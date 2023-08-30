/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "NetworkUtils.h"
#include "nsINetworkInterface.h"

#include "mozilla/Sprintf.h"
#include "SystemProperty.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsCOMPtr.h"

#include <android/log.h>
#include <limits>
#include "mozilla/fallible.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Unused.h"
#include "nsPrintfCString.h"
#include <errno.h>
#include <string.h>
#include <sys/types.h>   // struct addrinfo
#include <sys/socket.h>  // getaddrinfo(), freeaddrinfo()
#include <netdb.h>
#include <arpa/inet.h>                          // inet_ntop()
#include <sys/stat.h>                           // chmod()
#include <cutils/properties.h>                  // PROPERTY_VALUE_MAX
#include <private/android_filesystem_config.h>  // AID_SYSTEM
#include <android/net/INetd.h>
#include <android/net/InterfaceConfigurationParcel.h>
#include <android/net/TetherStatsParcel.h>
#include <binder/IServiceManager.h>
#include <utils/String16.h>
#include <binder/IBinder.h>
#include <android/net/IDnsResolver.h>
#include "NetdUnsolService.h"
#include "NetdEventListener.h"

#define WARN(args...) \
  __android_log_print(ANDROID_LOG_WARN, "NetworkUtils", ##args)
#define ERROR(args...) \
  __android_log_print(ANDROID_LOG_ERROR, "NetworkUtils", ##args)

namespace binder = android::binder;
using android::IBinder;
using android::IServiceManager;
using android::sp;
using android::binder::Status;
using android::net::INetd;
using namespace mozilla::dom;
using namespace mozilla::ipc;
using android::net::IDnsResolver;
using android::net::ResolverParamsParcel;
using android::net::TetherStatsParcel;
using mozilla::system::Property;

static bool ENABLE_NU_DEBUG = false;

static const int32_t SUCCESS = 0;

static const char* TCP_BUFFER_DELIMIT = ",";

static const char* DEFAULT_DNS_FORWARDER1 = "8.8.4.4";
static const char* DEFAULT_DNS_FORWARDER2 = "8.8.8.8";

// Default resolver parameters.
static const uint32_t DNS_RESOLVER_DEFAULT_SAMPLE_VALIDITY_SECONDS = 1800;
static const uint32_t DNS_RESOLVER_DEFAULT_SUCCESS_THRESHOLD_PERCENT = 25;
static const uint32_t DNS_RESOLVER_DEFAULT_MIN_SAMPLES = 8;
static const uint32_t DNS_RESOLVER_DEFAULT_MAX_SAMPLES = 64;

// Wake up event for driver specific constants.
static const int32_t WAKEUP_PACKET_MARK = 0x80000000;
static const int32_t WAKEUP_PACKET_MASK = 0x80000000;

// IPv6 addressing mode.
static int32_t IPV6_ADDR_GEN_MODE = INetd::IPV6_ADDR_GEN_MODE_DEFAULT;

static const char RADVD_CONF_FILE[] = "/data/misc/radvd/radvd.conf";

struct IFProperties {
  char gateway[Property::VALUE_MAX_LENGTH];
  char dns1[Property::VALUE_MAX_LENGTH];
  char dns2[Property::VALUE_MAX_LENGTH];
};

// A macro for native function call return value check.
// For native function call, non-zero return value means failure.
#define RETURN_IF_FAILED(rv)    \
  do {                          \
    if (SUCCESS != rv) {        \
      return CommandResult(rv); \
    }                           \
  } while (0);

#define WARN_IF_FAILED(rv)                                               \
  do {                                                                   \
    if (SUCCESS != rv) {                                                 \
      WARN("Error (%d) occurred in %s (%s:%d)", rv, __PRETTY_FUNCTION__, \
           __FILE__, __LINE__);                                          \
    }                                                                    \
  } while (0);

sp<INetd> gNetd = nullptr;
sp<IDnsResolver> gDnsResolver = nullptr;
nsCOMPtr<nsIThread> gNetworkUtilsThread;
static NetworkUtils* gNetworkUtils;
static NetdUnsolService* gNetdUnsolService;
static NetdEventListener* gNetdEventListener;
static bool gIpv6TetheringIfaceEnabled = false;
static nsTArray<nsCString> gIpv6TetheringInterfaces;

static nsTArray<CommandChain*> gCommandChainQueue;

static void NU_DBG(const char* format, ...) {
  if (!ENABLE_NU_DEBUG) {
    return;
  }
  va_list args;
  va_start(args, format);
  __android_log_vprint(ANDROID_LOG_INFO, "NetworkUtils", format, args);
  va_end(args);
}

/*
 * Helper function to dispatch callback to NetworkWorker.
 */

static void postMessage(NetworkResultOptions& aResult) {
  MOZ_ASSERT(gNetworkUtils);
  MOZ_ASSERT(gNetworkUtils->getMessageCallback());

  if (*(gNetworkUtils->getMessageCallback()))
    (*(gNetworkUtils->getMessageCallback()))(aResult);
}

static void postMessage(NetworkParams& aOptions,
                        NetworkResultOptions& aResult) {
  MOZ_ASSERT(gNetworkUtils);
  MOZ_ASSERT(gNetworkUtils->getMessageCallback());

  aResult.mId = aOptions.mId;

  if (*(gNetworkUtils->getMessageCallback()))
    (*(gNetworkUtils->getMessageCallback()))(aResult);
}

const CommandFunc NetworkUtils::sWifiEnableChain[] = {
    NetworkUtils::setConfig,
    NetworkUtils::tetherInterface,
    NetworkUtils::addRouteToLocalNetwork,
    NetworkUtils::setIpForwardingEnabled,
    NetworkUtils::startTethering,
    NetworkUtils::setDnsForwarders,
    NetworkUtils::enableNat,
    NetworkUtils::addIpv6TetheringInterfaces,
    NetworkUtils::updateIpv6Tethering,
    NetworkUtils::wifiTetheringSuccess};

const CommandFunc NetworkUtils::sWifiDisableChain[] = {
    NetworkUtils::stopIpv6Tethering,
    NetworkUtils::removeIpv6TetheringInterfaces,
    NetworkUtils::updateIpv6Tethering,
    NetworkUtils::untetherInterface,
    NetworkUtils::removeRouteFromLocalNetwork,
    NetworkUtils::disableNat,
    NetworkUtils::setIpForwardingDisabled,
    NetworkUtils::stopTethering,
    NetworkUtils::wifiTetheringSuccess};

const CommandFunc NetworkUtils::sWifiFailChain[] = {
    NetworkUtils::stopIpv6Tethering,
    NetworkUtils::removeIpv6TetheringInterfaces,
    NetworkUtils::setIpForwardingDisabled, NetworkUtils::stopTethering};

const CommandFunc NetworkUtils::sUSBEnableChain[] = {
    NetworkUtils::setConfig,
    NetworkUtils::tetherInterface,
    NetworkUtils::addRouteToLocalNetwork,
    NetworkUtils::setIpForwardingEnabled,
    NetworkUtils::startTethering,
    NetworkUtils::setDnsForwarders,
    NetworkUtils::enableNat,
    NetworkUtils::addIpv6TetheringInterfaces,
    NetworkUtils::updateIpv6Tethering,
    NetworkUtils::usbTetheringSuccess};

const CommandFunc NetworkUtils::sUSBDisableChain[] = {
    NetworkUtils::stopIpv6Tethering,
    NetworkUtils::removeIpv6TetheringInterfaces,
    NetworkUtils::updateIpv6Tethering,
    NetworkUtils::untetherInterface,
    NetworkUtils::removeRouteFromLocalNetwork,
    NetworkUtils::disableNat,
    NetworkUtils::setIpForwardingDisabled,
    NetworkUtils::stopTethering,
    NetworkUtils::usbTetheringSuccess};

const CommandFunc NetworkUtils::sUSBFailChain[] = {
    NetworkUtils::stopIpv6Tethering,
    NetworkUtils::removeIpv6TetheringInterfaces,
    NetworkUtils::setIpForwardingDisabled, NetworkUtils::stopTethering};

const CommandFunc NetworkUtils::sUpdateUpStreamChain[] = {
    NetworkUtils::stopIpv6Tethering,
    NetworkUtils::cleanUpStream,
    NetworkUtils::cleanUpStreamInterfaceForwarding,
    NetworkUtils::createUpStream,
    NetworkUtils::createUpStreamInterfaceForwarding,
    NetworkUtils::setDnsForwarders,
    NetworkUtils::updateIpv6Tethering,
    NetworkUtils::updateUpStreamSuccess};

/*
 * Netd aidl initialize process.
 */
static bool IsNetdRunning() {
  char value[Property::VALUE_MAX_LENGTH];

  Property::Get("init.svc.netd", value, "");
  if (strncmp(value, "running", 7)) {
    return false;
  }
  return true;
}

class NetdInitRunnable : public mozilla::Runnable {
 public:
  explicit NetdInitRunnable(bool aEnable)
      : mozilla::Runnable("NetdInitRunnable"), mEnable(aEnable) {}

  NS_IMETHOD Run() override {
    NU_DBG("NetdInitRunnable enter");
    if (mEnable) {
      if (!IsNetdRunning()) {
        Property::Set("ctl.start", "netd");
      }

      sp<IServiceManager> sm = android::defaultServiceManager();
      sp<IBinder> binderNetd;
      sp<IBinder> binderDnsResolver;
      bool isNetdAlive = false;

      do {
        binderNetd = sm->getService(android::String16("netd"));
        binderDnsResolver = sm->getService(android::String16("dnsresolver"));

        if (binderNetd != nullptr && binderDnsResolver != nullptr) {
          gNetd = android::interface_cast<INetd>(binderNetd);
          gDnsResolver =
              android::interface_cast<IDnsResolver>(binderDnsResolver);
          gNetd->isAlive(&isNetdAlive);
          if (isNetdAlive) {
            NU_DBG("Netd is alived, continue rest registration.");
            break;
          }
        }

        usleep(1000000);
        NU_DBG("Retrying connect with Netd...");
      } while (true);

      // Register netlink unsolicited event listener.
      gNetdUnsolService = new NetdUnsolService(NetworkUtils::NotifyNetdEvent);
      Status netdStatus = gNetd->registerUnsolicitedEventListener(
          android::interface_cast<android::net::INetdUnsolicitedEventListener>(
              gNetdUnsolService));
      NU_DBG("Registered NetdUnsolService %s",
             netdStatus.isOk() ? "Successful" : "Failed");

      // Register netd event listener.
      gNetdEventListener = new NetdEventListener(NetworkUtils::NotifyNetdEvent);
      Status dnsStatus = gDnsResolver->registerEventListener(
          android::interface_cast<android::net::metrics::INetdEventListener>(
              gNetdEventListener));
      NU_DBG("Registered NetdEventListener %s",
             dnsStatus.isOk() ? "Successful" : "Failed");
    } else {
      gNetd = nullptr;
      gDnsResolver = nullptr;
      gNetdUnsolService = nullptr;
      gNetdEventListener = nullptr;
      if (IsNetdRunning()) {
        Property::Set("ctl.stop", "netd");
      }
    }

    if (gNetworkUtils) {
      gNetworkUtils->updateDebug();
    }
    return NS_OK;
  }

 private:
  bool mEnable;
};

/**
 * Dispatch netd command on networktuls thread.
 */
class NetworkCommandDispatcher : public mozilla::Runnable {
 public:
  explicit NetworkCommandDispatcher(const NetworkParams& aParams)
      : mozilla::Runnable("NetworkCommandDispatcher"), mParams(aParams) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!NS_IsMainThread());

    if (gNetworkUtils) {
      gNetworkUtils->ExecuteCommand(std::move(mParams));
    }
    return NS_OK;
  }

 private:
  NetworkParams mParams;
};

void NetworkUtils::DispatchCommand(const NetworkParams& aParams) {
  nsCOMPtr<nsIRunnable> runnable =
      new NetworkCommandDispatcher(std::move(aParams));
  if (gNetworkUtilsThread) {
    gNetworkUtilsThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  }
}

/**
 * Helper function to get the mask from given prefix length.
 */
static uint32_t makeMask(const uint32_t prefixLength) {
  uint32_t mask = 0;
  for (uint32_t i = 0; i < prefixLength; ++i) {
    mask |= (0x80000000 >> i);
  }
  return ntohl(mask);
}

/**
 * Helper function to get the network part of an ip from prefix.
 * param ip must be in network byte order.
 */
static char* getNetworkAddr(const uint32_t ip, const uint32_t prefix) {
  uint32_t mask = 0, subnet = 0;

  mask = ~mask << (32 - prefix);
  mask = htonl(mask);
  subnet = ip & mask;

  struct in_addr addr;
  addr.s_addr = subnet;

  return inet_ntoa(addr);
}

/**
 * Helper function to split string by seperator, store split result as an
 * nsTArray.
 */
static void split(char* str, const char* sep, nsTArray<nsCString>& result) {
  char* s = strtok(str, sep);
  while (s != nullptr) {
    result.AppendElement(s);
    s = strtok(nullptr, sep);
  }
}

[[maybe_unused]] static void split(char* str, const char* sep,
                                   nsTArray<nsString>& result) {
  char* s = strtok(str, sep);
  while (s != nullptr) {
    result.AppendElement(NS_ConvertUTF8toUTF16(s));
    s = strtok(nullptr, sep);
  }
}

[[maybe_unused]] static void convertUTF8toUTF16(nsTArray<nsCString>& narrow,
                                                nsTArray<nsString>& wide,
                                                uint32_t length) {
  for (uint32_t i = 0; i < length; i++) {
    wide.AppendElement(NS_ConvertUTF8toUTF16(narrow[i].get()));
  }
}

static int getIpType(const char* aIp) {
  struct addrinfo hint, *ip_info = NULL;

  memset(&hint, 0, sizeof(hint));
  hint.ai_family = AF_UNSPEC;
  hint.ai_flags = AI_NUMERICHOST;

  if (getaddrinfo(aIp, NULL, &hint, &ip_info)) {
    return AF_UNSPEC;
  }

  int type = ip_info->ai_family;
  freeaddrinfo(ip_info);

  return type;
}

void NetworkUtils::runNextQueuedCommandChain() {
  if (gCommandChainQueue.IsEmpty()) {
    NU_DBG("No command chain left in the queue. Done!");
    return;
  }
  NU_DBG("Process the queued command chain.");
  CommandChain* nextChain = gCommandChainQueue[0];
  NetworkResultOptions newResult;
  next(nextChain, false, newResult);
}

void NetworkUtils::next(CommandChain* aChain, bool aError,
                        NetworkResultOptions& aResult) {
  if (aError) {
    ErrorCallback onError = aChain->getErrorCallback();
    if (onError) {
      aResult.mError = true;
      (*onError)(aChain->getParams(), aResult);
    }
    delete aChain;
    gCommandChainQueue.RemoveElementAt(0);
    runNextQueuedCommandChain();
    return;
  }
  CommandFunc f = aChain->getNextCommand();
  if (!f) {
    delete aChain;
    gCommandChainQueue.RemoveElementAt(0);
    runNextQueuedCommandChain();
    return;
  }

  (*f)(aChain, CommandCallback(next), aResult);
}

nsCString NetworkUtils::getSubnetIp(const nsCString& aIp, int aPrefixLength) {
  int type = getIpType(aIp.get());

  if (AF_INET6 == type) {
    struct in6_addr in6;
    if (inet_pton(AF_INET6, aIp.get(), &in6) != 1) {
      return nsCString();
    }

    uint32_t p, i, p1, mask;
    p = aPrefixLength;
    i = 0;
    while (i < 4) {
      p1 = p > 32 ? 32 : p;
      p -= p1;
      mask = p1 ? ~0x0 << (32 - p1) : 0;
      in6.s6_addr32[i++] &= htonl(mask);
    }

    char subnetStr[INET6_ADDRSTRLEN];
    if (!inet_ntop(AF_INET6, &in6, subnetStr, sizeof subnetStr)) {
      return nsCString();
    }

    return nsCString(subnetStr);
  }

  if (AF_INET == type) {
    uint32_t ip = inet_addr(aIp.get());
    uint32_t netmask = makeMask(aPrefixLength);
    uint32_t subnet = ip & netmask;
    struct in_addr addr;
    addr.s_addr = subnet;
    return nsCString(inet_ntoa(addr));
  }

  return nsCString();
}

CommandResult::CommandResult(int32_t aResultCode) : mIsPending(false) {
  // This is usually not a netd command. We treat the return code
  // typical linux convention, which uses 0 to indicate success.
  mResult.mError = (aResultCode == SUCCESS ? false : true);
  mResult.mResultCode = aResultCode;
  if (aResultCode != SUCCESS) {
    // The returned value is sometimes negative, make sure we pass a positive
    // error number to strerror.
    enum {
      STRERROR_R_BUF_SIZE = 1024,
    };
    char strerrorBuf[STRERROR_R_BUF_SIZE];
    strerror_r(abs(aResultCode), strerrorBuf, STRERROR_R_BUF_SIZE);
    mResult.mReason = NS_ConvertUTF8toUTF16(strerrorBuf);
  }
}

CommandResult::CommandResult(const mozilla::dom::NetworkResultOptions& aResult)
    : mResult(aResult), mIsPending(false) {}

CommandResult::CommandResult(const Pending&) : mIsPending(true) {}

bool CommandResult::isPending() const { return mIsPending; }

/*
 * Netd command function
 */
#define GET_CHAR(prop) NS_ConvertUTF16toUTF8(aChain->getParams().prop).get()
#define GET_FIELD(prop) aChain->getParams().prop

void NetworkUtils::clearAddrForInterface(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult) {
  Status status = gNetd->interfaceClearAddrs(std::string(GET_CHAR(mIfname)));
  NU_DBG("clearAddrForInterface %s", status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}


void NetworkUtils::createNetwork(CommandChain* aChain,
                                 CommandCallback aCallback,
                                 NetworkResultOptions& aResult) {
  Status networkStatus =
      gNetd->networkCreatePhysical(GET_FIELD(mNetId), INetd::PERMISSION_NONE);
  Status dnsStatus = gDnsResolver->createNetworkCache(GET_FIELD(mNetId));
  NU_DBG("createNetwork physical %s, networkCache %s",
         networkStatus.isOk() ? "success" : "failed",
         dnsStatus.isOk() ? "success" : "failed");

  bool createNetworkResult = (networkStatus.isOk() && dnsStatus.isOk());
  if (!createNetworkResult) {
    // Removed the fail network.
    gNetd->networkDestroy(GET_FIELD(mNetId));
    gDnsResolver->destroyNetworkCache(GET_FIELD(mNetId));
  }
  next(aChain, !createNetworkResult, aResult);
}

void NetworkUtils::destroyNetwork(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult) {
  Status networkStatus = gNetd->networkDestroy(GET_FIELD(mNetId));
  Status dnsStatus = gDnsResolver->destroyNetworkCache(GET_FIELD(mNetId));
  NU_DBG("destroyNetwork %s", networkStatus.isOk() ? "success" : "failed");
  next(aChain, !networkStatus.isOk(), aResult);
}

void NetworkUtils::setIpv6AddrGenMode(CommandChain* aChain,
                                      CommandCallback aCallback,
                                      NetworkResultOptions& aResult) {
  Status status = gNetd->setIPv6AddrGenMode(std::string(GET_CHAR(mIfname)),
                                            IPV6_ADDR_GEN_MODE);
  NU_DBG("setIpv6AddrGenMode %d %s", IPV6_ADDR_GEN_MODE,
         status.isOk() ? "success" : "failed but continue");
  next(aChain, false, aResult);
}

void NetworkUtils::setIpv6PrivacyExtensions(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult) {
  bool enable = GET_FIELD(mEnable);
  Status status = gNetd->interfaceSetIPv6PrivacyExtensions(
      std::string(GET_CHAR(mIfname)), enable);
  NU_DBG("setIpv6PrivacyExtensions %s %s", enable ? "enable" : "disable",
         status.isOk() ? "success" : "failed but continue");
  next(aChain, false, aResult);
}

void NetworkUtils::setIpv6Enabled(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult) {
  bool enable = GET_FIELD(mEnable);
  Status status =
      gNetd->interfaceSetEnableIPv6(std::string(GET_CHAR(mIfname)), enable);
  NU_DBG("setIpv6Enabled %s %s", enable ? "enable" : "disable",
         status.isOk() ? "success" : "failed");
  aCallback(aChain, !status.isOk(), aResult);
}

void NetworkUtils::wakeupAddInterface(CommandChain* aChain,
                                      CommandCallback aCallback,
                                      NetworkResultOptions& aResult) {
  // Marks only available on wlan interfaces.
  if (nsINetworkInfo::NETWORK_TYPE_WIFI != GET_FIELD(mNetworkType)) {
    NU_DBG("%s : ignore network type = %ld", __FUNCTION__,
           GET_FIELD(mNetworkType));
    aCallback(aChain, false, aResult);
    return;
  }

  std::string interface(GET_CHAR(mIfname));
  std::string prefix = "iface:" + interface;
  Status status = gNetd->wakeupAddInterface(
      interface, prefix, WAKEUP_PACKET_MARK, WAKEUP_PACKET_MASK);
  NU_DBG("%s : %s", status.isOk() ? "success" : "failed but continue",
         __FUNCTION__);
  aCallback(aChain, false, aResult);
}

void NetworkUtils::wakeupDelInterface(CommandChain* aChain,
                                      CommandCallback aCallback,
                                      NetworkResultOptions& aResult) {
  // Marks only available on wlan interfaces.
  if (nsINetworkInfo::NETWORK_TYPE_WIFI != GET_FIELD(mNetworkType)) {
    NU_DBG("%s : ignore network type = %ld", __FUNCTION__,
           GET_FIELD(mNetworkType));
    aCallback(aChain, false, aResult);
    return;
  }

  std::string interface(GET_CHAR(mIfname));
  std::string prefix = "iface:" + interface;
  Status status = gNetd->wakeupDelInterface(
      interface, prefix, WAKEUP_PACKET_MARK, WAKEUP_PACKET_MASK);
  NU_DBG("%s : %s", status.isOk() ? "success" : "failed but continue",
         __FUNCTION__);
  aCallback(aChain, false, aResult);
}

void NetworkUtils::addInterfaceToNetwork(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult) {
  Status status =
      gNetd->networkAddInterface(GET_FIELD(mNetId), GET_CHAR(mIfname));
  NU_DBG("addInterfaceToNetwork %s", status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::removeInterfaceToNetwork(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult) {
  Status status =
      gNetd->networkRemoveInterface(GET_FIELD(mNetId), GET_CHAR(mIfname));
  NU_DBG("removeInterfaceToNetwork %s", status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::addDefaultRouteToNetwork(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult) {
  nsTArray<nsString>& gateways = GET_FIELD(mGateways);

  for (GET_FIELD(mLoopIndex) = 0;
       GET_FIELD(mLoopIndex) < GET_FIELD(mGateways).Length();
       GET_FIELD(mLoopIndex)++) {
    char destination[BUF_SIZE];

    NS_ConvertUTF16toUTF8 autoGateway(gateways[GET_FIELD(mLoopIndex)]);
    int type = getIpType(autoGateway.get());
    snprintf(destination, BUF_SIZE - 1,
             type == AF_INET6 ? "::/0" : "0.0.0.0/0");
    Status status = gNetd->networkAddRoute(GET_FIELD(mNetId), GET_CHAR(mIfname),
                                           destination, autoGateway.get());
    NU_DBG("addDefaultRouteToNetwork %s", status.isOk() ? "success" : "failed");
    if (!status.isOk()) {
      next(aChain, true, aResult);
      return;
    }
  }
  aCallback(aChain, false, aResult);
}

void NetworkUtils::removeDefaultRoute(CommandChain* aChain,
                                      CommandCallback aCallback,
                                      NetworkResultOptions& aResult) {
  for (GET_FIELD(mLoopIndex) = 0;
       GET_FIELD(mLoopIndex) < GET_FIELD(mGateways).Length();
       GET_FIELD(mLoopIndex)++) {
    char destination[BUF_SIZE];
    nsTArray<nsString>& gateways = GET_FIELD(mGateways);
    NS_ConvertUTF16toUTF8 autoGateway(gateways[GET_FIELD(mLoopIndex)]);
    int type = getIpType(autoGateway.get());
    snprintf(destination, BUF_SIZE - 1,
             type == AF_INET6 ? "::/0" : "0.0.0.0/0");
    Status status = gNetd->networkRemoveRoute(
        GET_FIELD(mNetId), GET_CHAR(mIfname), destination, autoGateway.get());
    NU_DBG("removeDefaultRoute %s",
           status.isOk() ? "success" : "failed but continue");
  }

  // Ignore remove route failed since some routing might already been removed.
  aCallback(aChain, false, aResult);
}

void NetworkUtils::setInterfaceDns(CommandChain* aChain,
                                   CommandCallback aCallback,
                                   NetworkResultOptions& aResult) {
  std::vector<std::string> servers;
  std::vector<std::string> domains;
  nsTArray<nsString>& dnses = GET_FIELD(mDnses);
  uint32_t length = dnses.Length();
  for (uint32_t i = 0; i < length; i++) {
    NS_ConvertUTF16toUTF8 autoDns(dnses[i]);
    servers.push_back(autoDns.get());
  }
  if (!GET_FIELD(mDomain).IsEmpty()) {
    domains.push_back(GET_CHAR(mDomain));
  }
  ResolverParamsParcel resolverParams;
  resolverParams.netId = GET_FIELD(mNetId);
  resolverParams.sampleValiditySeconds =
      DNS_RESOLVER_DEFAULT_SAMPLE_VALIDITY_SECONDS;
  resolverParams.successThreshold =
      DNS_RESOLVER_DEFAULT_SUCCESS_THRESHOLD_PERCENT;
  resolverParams.minSamples = DNS_RESOLVER_DEFAULT_MIN_SAMPLES;
  resolverParams.maxSamples = DNS_RESOLVER_DEFAULT_MAX_SAMPLES;
  resolverParams.baseTimeoutMsec = 0;
  resolverParams.retryCount = 3;
  resolverParams.servers = servers;
  resolverParams.domains = domains;
  // TODO: Strict mode.
  resolverParams.tlsName = "";
  resolverParams.tlsServers = {};
  resolverParams.tlsFingerprints = {};

  Status status = gDnsResolver->setResolverConfiguration(resolverParams);
  NU_DBG("setInterfaceDns %s", status.isOk() ? "success" : "failed");
  if (!status.isOk()) {
    aResult.mReason = NS_ConvertUTF8toUTF16("Resolver command failed");
  }
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::addRouteToInterface(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult) {
  nsCString gatewayOrEmpty;
  nsCString ipOrSubnetIp = NS_ConvertUTF16toUTF8(GET_FIELD(mIp));
  Status status;
  char destination[BUF_SIZE];
  char errorReason[BUF_SIZE];

  if (GET_FIELD(mGateway).IsEmpty()) {
    ipOrSubnetIp = getSubnetIp(ipOrSubnetIp, GET_FIELD(mPrefixLength));
    snprintf(destination, BUF_SIZE - 1, "%s/%d", ipOrSubnetIp.get(),
             GET_FIELD(mPrefixLength));
    status = gNetd->networkAddRoute(GET_FIELD(mNetId), GET_CHAR(mIfname),
                                    destination, gatewayOrEmpty.get());
  } else {
    snprintf(destination, BUF_SIZE - 1, "%s/%d", ipOrSubnetIp.get(),
             GET_FIELD(mPrefixLength));
    gatewayOrEmpty = NS_ConvertUTF16toUTF8(GET_FIELD(mGateway));
    // TODO: uid might need to get from b2g.
    status = gNetd->networkAddLegacyRoute(GET_FIELD(mNetId), GET_CHAR(mIfname),
                                          destination, gatewayOrEmpty.get(), 0);
  }
  if (!status.isOk() && status.serviceSpecificErrorCode() == EEXIST) {
    NU_DBG("addRouteToInterface failed, Ignore \"File exists\" error");
    aCallback(aChain, false, aResult);
    return;
  }

  NU_DBG("addRouteToInterface %s", status.isOk() ? "success" : "failed");
  if (!status.isOk()) {
    snprintf(errorReason, BUF_SIZE - 1, "addRoute() failed : %d",
             status.serviceSpecificErrorCode());
    aResult.mReason = NS_ConvertUTF8toUTF16(errorReason);
  }
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::removeRouteFromInterface(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult) {
  nsCString gatewayOrEmpty;
  nsCString ipOrSubnetIp = NS_ConvertUTF16toUTF8(GET_FIELD(mIp));
  Status status;
  char destination[BUF_SIZE];
  char errorReason[BUF_SIZE];

  if (GET_FIELD(mGateway).IsEmpty()) {
    ipOrSubnetIp = getSubnetIp(ipOrSubnetIp, GET_FIELD(mPrefixLength));
    snprintf(destination, BUF_SIZE - 1, "%s/%d", ipOrSubnetIp.get(),
             GET_FIELD(mPrefixLength));
    status = gNetd->networkRemoveRoute(GET_FIELD(mNetId), GET_CHAR(mIfname),
                                       destination, gatewayOrEmpty.get());
  } else {
    snprintf(destination, BUF_SIZE - 1, "%s/%d", ipOrSubnetIp.get(),
             GET_FIELD(mPrefixLength));
    gatewayOrEmpty = NS_ConvertUTF16toUTF8(GET_FIELD(mGateway));
    // TODO: uid might need to get from b2g.
    status =
        gNetd->networkRemoveLegacyRoute(GET_FIELD(mNetId), GET_CHAR(mIfname),
                                        destination, gatewayOrEmpty.get(), 0);
  }
  NU_DBG("removeRouteFromInterface %s", status.isOk() ? "success" : "failed");
  if (!status.isOk()) {
    snprintf(errorReason, BUF_SIZE - 1, "removeRoute() failed : %d",
             status.serviceSpecificErrorCode());
    aResult.mReason = NS_ConvertUTF8toUTF16(errorReason);
  }
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::addRouteToSecondaryTable(CommandChain* aChain,
                                            CommandCallback aCallback,
                                            NetworkResultOptions& aResult) {
  char destination[BUF_SIZE];
  snprintf(destination, BUF_SIZE - 1, "%s/%s", GET_CHAR(mIp),
           GET_CHAR(mPrefix));
  Status status = gNetd->networkAddRoute(GET_FIELD(mNetId), GET_CHAR(mIfname),
                                         destination, GET_CHAR(mGateway));
  NU_DBG("addRouteToSecondaryTable %s", status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::removeRouteFromSecondaryTable(
    CommandChain* aChain, CommandCallback aCallback,
    NetworkResultOptions& aResult) {
  char destination[BUF_SIZE];
  snprintf(destination, BUF_SIZE - 1, "%s/%s", GET_CHAR(mIp),
           GET_CHAR(mPrefix));
  Status status = gNetd->networkRemoveRoute(
      GET_FIELD(mNetId), GET_CHAR(mIfname), destination, GET_CHAR(mGateway));
  NU_DBG("removeRouteFromSecondaryTable %s",
         status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::setMtu(CommandChain* aChain, CommandCallback aCallback,
                          NetworkResultOptions& aResult) {
  Status status =
      gNetd->interfaceSetMtu(std::string(GET_CHAR(mIfname)), GET_FIELD(mMtu));
  NU_DBG("setMtu %s", status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::setDefaultNetwork(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult) {
  Status status = gNetd->networkSetDefault(GET_FIELD(mNetId));
  NU_DBG("setDefaultNetwork %s", status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::setConfig(CommandChain* aChain, CommandCallback aCallback,
                             NetworkResultOptions& aResult) {
  android::net::InterfaceConfigurationParcel interfaceCfg;
  struct in_addr addr;

  Status status = gNetd->interfaceGetCfg(GET_CHAR(mIfname), &interfaceCfg);
  if (!status.isOk()) {
    NU_DBG("%s: Unable to get interface: %s", __FUNCTION__, GET_CHAR(mIfname));
    next(aChain, !status.isOk(), aResult);
    return;
  }

  if (!inet_aton(GET_CHAR(mIp), &addr)) {
    interfaceCfg.ipv4Addr = "";
    interfaceCfg.prefixLength = 0;
  } else if (addr.s_addr != 0) {
    interfaceCfg.ipv4Addr = GET_CHAR(mIp);
    interfaceCfg.prefixLength = atoi(GET_CHAR(mPrefix));
  }

  Status bringStatus;
  if (!strcmp(GET_CHAR(mLink), "up")) {
    interfaceCfg.flags.push_back(
        std::string(android::String8((INetd::IF_STATE_UP().string()))));
    bringStatus = gNetd->interfaceSetCfg(interfaceCfg);

  } else if (!strcmp(GET_CHAR(mLink), "down")) {
    interfaceCfg.flags.push_back(
        std::string(android::String8((INetd::IF_STATE_DOWN().string()))));
    bringStatus = gNetd->interfaceSetCfg(interfaceCfg);
  } else {
    NU_DBG("%s: Unknown link config.", __FUNCTION__);
    next(aChain, true, aResult);
    return;
  }

  NU_DBG("%s: %s", __FUNCTION__, bringStatus.isOk() ? "success" : "failed");
  next(aChain, !bringStatus.isOk(), aResult);
}

void NetworkUtils::enableNat(CommandChain* aChain, CommandCallback aCallback,
                             NetworkResultOptions& aResult) {
  Status status = gNetd->tetherAddForward(GET_CHAR(mInternalIfname),
                                          GET_CHAR(mExternalIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::disableNat(CommandChain* aChain, CommandCallback aCallback,
                              NetworkResultOptions& aResult) {
  Status status = gNetd->tetherRemoveForward(GET_CHAR(mInternalIfname),
                                             GET_CHAR(mExternalIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::setIpForwardingEnabled(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult) {
  Status status = gNetd->ipfwdEnableForwarding("tethering");
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  if (!status.isOk()) {
    next(aChain, true, aResult);
    return;
  }

  if (!strcmp(GET_CHAR(mExternalIfname), "rmnet0")) {
    NU_DBG("%s: ignore interface - %s", __FUNCTION__,
           GET_CHAR(mExternalIfname));
    next(aChain, false, aResult);
    return;
  }

  Status fwdStatus = gNetd->ipfwdAddInterfaceForward(GET_CHAR(mInternalIfname),
                                                     GET_CHAR(mExternalIfname));
  NU_DBG("%s: for iface %s", __FUNCTION__,
         fwdStatus.isOk() ? "success" : "failed");

  next(aChain, !fwdStatus.isOk(), aResult);
}

void NetworkUtils::setIpForwardingDisabled(CommandChain* aChain,
                                           CommandCallback aCallback,
                                           NetworkResultOptions& aResult) {
  std::vector<std::string> ifList;
  gNetd->tetherInterfaceList(&ifList);
  // Only disable forwarding if it left single tethering interface.
  if (ifList.size() == 1) {
    Status status = gNetd->ipfwdDisableForwarding("tethering");
    if (!status.isOk()) {
      next(aChain, true, aResult);
      return;
    }
  }

  if (!strcmp(GET_CHAR(mExternalIfname), "rmnet0")) {
    NU_DBG("%s : ignore interface - %s", __FUNCTION__,
           GET_CHAR(mExternalIfname));
    next(aChain, false, aResult);
    return;
  }

  Status fwdStatus = gNetd->ipfwdRemoveInterfaceForward(
      GET_CHAR(mInternalIfname), GET_CHAR(mExternalIfname));
  // We don't care about remove forwarding result
  // since rule might already removed from table list.
  aCallback(aChain, false, aResult);
}

void NetworkUtils::tetherInterface(CommandChain* aChain,
                                   CommandCallback aCallback,
                                   NetworkResultOptions& aResult) {
  Status status = gNetd->tetherInterfaceAdd(GET_CHAR(mIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::untetherInterface(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult) {
  Status status = gNetd->tetherInterfaceRemove(GET_CHAR(mIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::addRouteToLocalNetwork(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult) {
  Status status = gNetd->networkAddInterface(INetd::LOCAL_NET_ID,
                                             GET_CHAR(mInternalIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  if (!status.isOk()) {
    next(aChain, true, aResult);
    return;
  }

  uint32_t prefix = atoi(GET_CHAR(mPrefix));
  uint32_t ip = inet_addr(GET_CHAR(mIp));
  char* networkAddr = getNetworkAddr(ip, prefix);
  char route[BUF_SIZE];
  snprintf(route, BUF_SIZE, "%s/%s", networkAddr, GET_CHAR(mPrefix));

  Status routeStatus = gNetd->networkAddRoute(
      INetd::LOCAL_NET_ID, GET_CHAR(mInternalIfname), route, "");
  NU_DBG("%s: add route %s", __FUNCTION__,
         routeStatus.isOk() ? "success" : "failed");

  next(aChain, !routeStatus.isOk(), aResult);
}

void NetworkUtils::removeRouteFromLocalNetwork(CommandChain* aChain,
                                               CommandCallback aCallback,
                                               NetworkResultOptions& aResult) {
  Status status =
      gNetd->networkRemoveInterface(INetd::LOCAL_NET_ID, GET_CHAR(mIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::startTethering(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult) {
  bool tetherEnabled;
  Status status;
  gNetd->tetherIsEnabled(&tetherEnabled);
  // We don't need to start tethering again.
  if (tetherEnabled) {
    aCallback(aChain, false, aResult);
    return;
  } else {
    std::vector<std::string> dhcpRanges;
    dhcpRanges.push_back(GET_CHAR(mWifiStartIp));
    dhcpRanges.push_back(GET_CHAR(mWifiEndIp));
    if (!GET_FIELD(mUsbStartIp).IsEmpty() && !GET_FIELD(mUsbEndIp).IsEmpty()) {
      dhcpRanges.push_back(GET_CHAR(mUsbStartIp));
      dhcpRanges.push_back(GET_CHAR(mUsbEndIp));
    }
    status = gNetd->tetherStart(dhcpRanges);
    NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  }
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::stopTethering(CommandChain* aChain,
                                 CommandCallback aCallback,
                                 NetworkResultOptions& aResult) {
  std::vector<std::string> ifList;
  gNetd->tetherInterfaceList(&ifList);
  // Don't stop tethering because others interface still need it.
  if (ifList.size() > 1) {
    aCallback(aChain, false, aResult);
    return;
  }

  Status status = gNetd->tetherStop();
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::setDnsForwarders(CommandChain* aChain,
                                    CommandCallback aCallback,
                                    NetworkResultOptions& aResult) {
  std::vector<std::string> tetherDnsAddrs;
  nsTArray<nsString>& dnses = GET_FIELD(mDnses);
  if (!dnses.Length()) {
    tetherDnsAddrs.push_back(DEFAULT_DNS_FORWARDER1);
    tetherDnsAddrs.push_back(DEFAULT_DNS_FORWARDER2);
  } else {
    NU_DBG("%s: list dnses:", __FUNCTION__);
    for (uint32_t i = 0; i < dnses.Length(); i++) {
      NS_ConvertUTF16toUTF8 dns(dnses[i]);
      tetherDnsAddrs.push_back(dns.get());
      NU_DBG("%s: %s", __FUNCTION__, dns.get());
    }
  }

  Status status = gNetd->tetherDnsSet(GET_FIELD(mNetId), tetherDnsAddrs);
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::cleanUpStream(CommandChain* aChain,
                                 CommandCallback aCallback,
                                 NetworkResultOptions& aResult) {
  Status status = gNetd->tetherRemoveForward(GET_CHAR(mPreInternalIfname),
                                             GET_CHAR(mPreExternalIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  // We don't care about remove upstream result
  // since it might not exist tethering rule.
  next(aChain, false, aResult);
}

void NetworkUtils::createUpStream(CommandChain* aChain,
                                  CommandCallback aCallback,
                                  NetworkResultOptions& aResult) {
  Status status = gNetd->tetherAddForward(GET_CHAR(mCurInternalIfname),
                                          GET_CHAR(mCurExternalIfname));
  NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::cleanUpStreamInterfaceForwarding(
    CommandChain* aChain, CommandCallback aCallback,
    NetworkResultOptions& aResult) {
  if (!strcmp(GET_CHAR(mPreExternalIfname), "rmnet0")) {
    NU_DBG("%s : ignore interface - %s", __FUNCTION__,
           GET_CHAR(mPreExternalIfname));
    next(aChain, false, aResult);
    return;
  }

  Status status = gNetd->ipfwdRemoveInterfaceForward(
      GET_CHAR(mPreInternalIfname), GET_CHAR(mPreExternalIfname));
  // We don't care about remove forwarding result
  // since rule might already removed from table list.
  next(aChain, false, aResult);
}

void NetworkUtils::createUpStreamInterfaceForwarding(
    CommandChain* aChain, CommandCallback aCallback,
    NetworkResultOptions& aResult) {
  if (!strcmp(GET_CHAR(mCurExternalIfname), "rmnet0")) {
    NU_DBG("%s : ignore interface - %s", __FUNCTION__,
           GET_CHAR(mCurExternalIfname));
    next(aChain, false, aResult);
    return;
  }

  Status status = gNetd->ipfwdAddInterfaceForward(GET_CHAR(mCurInternalIfname),
                                                  GET_CHAR(mCurExternalIfname));
  NU_DBG("%s: for iface %s", __FUNCTION__,
         status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

void NetworkUtils::addIpv6TetheringInterfaces(CommandChain* aChain,
                                              CommandCallback aCallback,
                                              NetworkResultOptions& aResult) {
  nsCString internalIface(GET_CHAR(mInternalIfname));

  if (!gIpv6TetheringInterfaces.Contains(internalIface)) {
    gIpv6TetheringInterfaces.AppendElement(internalIface);
  }

  // Proceed next cmd.
  next(aChain, false, aResult);
}

void NetworkUtils::removeIpv6TetheringInterfaces(
    CommandChain* aChain, CommandCallback aCallback,
    NetworkResultOptions& aResult) {
  nsCString internalIface(GET_CHAR(mInternalIfname));

  if (gIpv6TetheringInterfaces.Contains(internalIface)) {
    gIpv6TetheringInterfaces.RemoveElement(internalIface);
  }

  // Proceed next cmd.
  next(aChain, false, aResult);
}

void NetworkUtils::stopIpv6Tethering(CommandChain* aChain,
                                     CommandCallback aCallback,
                                     NetworkResultOptions& aResult) {
  nsCString ipv6Prefix(GET_CHAR(mIPv6Prefix));

  if (!ipv6Prefix.IsEmpty() && !gIpv6TetheringInterfaces.IsEmpty() &&
      gIpv6TetheringIfaceEnabled) {
    NU_DBG("disable Ipv6 Tethering %s", gIpv6TetheringInterfaces[0].get());
    Property::Set("ctl.stop", "radvd");
    gIpv6TetheringIfaceEnabled = false;
    GET_FIELD(mLoopIndex) = 0;
    return removeIpv6LocalNetworkRoute(aChain, aCallback, aResult);
  }
  // Proceed next cmd.
  next(aChain, false, aResult);
}

void NetworkUtils::updateIpv6Tethering(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult) {
  nsCString externalIface(GET_CHAR(mExternalIfname));
  nsCString internalIface(GET_CHAR(mInternalIfname));
  nsCString ipv6Prefix(GET_CHAR(mIPv6Prefix));
  nsTArray<nsString>& dnses = GET_FIELD(mDnses);
  uint32_t dnsLength = dnses.Length();

  if (!externalIface.get()[0]) {
    externalIface = GET_CHAR(mCurExternalIfname);
  }
  if (!internalIface.get()[0]) {
    internalIface = GET_CHAR(mCurInternalIfname);
  }

  if (!strcmp(externalIface.get(), "wlan0")) {
    NU_DBG("Do not enable Ipv6 Tethering if Wi-Fi as external interface");
    next(aChain, false, aResult);
    return;
  }

  if (ipv6Prefix.IsEmpty() || !dnsLength) {
    NU_DBG("Do not enable Ipv6 Tethering since no %s",
           !dnsLength ? "DNS" : "Prefix");
    next(aChain, false, aResult);
    return;
  }

  if (gIpv6TetheringInterfaces.IsEmpty() || gIpv6TetheringIfaceEnabled) {
    NU_DBG("Do not enable Ipv6 Tethering since %s",
           gIpv6TetheringIfaceEnabled ? "ongoing Ipv6 Tethering"
                                      : "no available interface");
    next(aChain, false, aResult);
    return;
  }

  NU_DBG("enable Ipv6 Tethering for %s %s %s", externalIface.get(),
         gIpv6TetheringInterfaces[0].get(), ipv6Prefix.get());

  if (composeIpv6TetherConf(gIpv6TetheringInterfaces[0].get(), ipv6Prefix.get(),
                            dnses)) {
    NU_DBG("radvd configured, start radvd");
    Property::Set("ctl.start", "radvd");
    gIpv6TetheringIfaceEnabled = true;
    GET_FIELD(mLoopIndex) = 0;
    return addIpv6RouteToLocalNetwork(aChain, aCallback, aResult);
  }
  // Proceed next cmd.
  next(aChain, false, aResult);
}

void NetworkUtils::addIpv6RouteToLocalNetwork(CommandChain* aChain,
                                              CommandCallback aCallback,
                                              NetworkResultOptions& aResult) {
  for (GET_FIELD(mLoopIndex) = 0;
       GET_FIELD(mLoopIndex) < GET_FIELD(mIPv6Routes).Length();
       GET_FIELD(mLoopIndex)++) {
    nsTArray<nsString>& ipv6routes = GET_FIELD(mIPv6Routes);
    NS_ConvertUTF16toUTF8 autoIpv6Routes(ipv6routes[GET_FIELD(mLoopIndex)]);
    Status status = gNetd->networkAddRoute(INetd::LOCAL_NET_ID,
                                           gIpv6TetheringInterfaces[0].get(),
                                           autoIpv6Routes.get(), "");
    NU_DBG("%s: %s", __FUNCTION__, status.isOk() ? "success" : "failed");
    if (!status.isOk()) {
      next(aChain, true, aResult);
      return;
    }
  }
  aCallback(aChain, false, aResult);
}

void NetworkUtils::removeIpv6LocalNetworkRoute(CommandChain* aChain,
                                               CommandCallback aCallback,
                                               NetworkResultOptions& aResult) {
  for (GET_FIELD(mLoopIndex) = 0;
       GET_FIELD(mLoopIndex) < GET_FIELD(mIPv6Routes).Length();
       GET_FIELD(mLoopIndex)++) {
    nsTArray<nsString>& ipv6routes = GET_FIELD(mIPv6Routes);
    NS_ConvertUTF16toUTF8 autoIpv6Routes(ipv6routes[GET_FIELD(mLoopIndex)]);
    Status status = gNetd->networkRemoveRoute(INetd::LOCAL_NET_ID,
                                              gIpv6TetheringInterfaces[0].get(),
                                              autoIpv6Routes.get(), "");
    // We don't really care about remove ipv6 local network route result
    NU_DBG("%s: %s", __FUNCTION__,
           status.isOk() ? "success" : "failed but ignore");
  }
  aCallback(aChain, false, aResult);
}

bool NetworkUtils::composeIpv6TetherConf(const char* aInternalIface,
                                         const char* aNetworkPrefix,
                                         nsTArray<nsString>& aDnses) {
  int type;
  char dnsesList[256] = {0};
  char dnsBuffer[64] = {0};
  char dnsSpace[64] = {0};
  FILE* radvdFile;
  char buffer[512] = {0};

  for (uint32_t i = 0; i < aDnses.Length(); i++) {
    NS_ConvertUTF16toUTF8 autoDns(aDnses[i]);
    SprintfLiteral(dnsBuffer, "%s", autoDns.get());
    type = getIpType(dnsBuffer);
    if (type == AF_INET6) {
      snprintf(dnsSpace, strlen(dnsBuffer) + 2, " %s", dnsBuffer);
      strcat(dnsesList, dnsSpace);
    }
    memset(&dnsBuffer, 0, sizeof(dnsBuffer));
    memset(&dnsSpace, 0, sizeof(dnsSpace));
  }

  if (strlen(dnsesList) == 0) {
    NU_DBG("Do not enable Ipv6 Tethering since no Ipv6 DNS");
    return false;
  }

  // Compose radvd config file
  radvdFile = fopen(RADVD_CONF_FILE, "w");

  if (radvdFile == NULL) {
    NU_DBG("Cannot open %s", RADVD_CONF_FILE);
    return false;
  }

  SprintfLiteral(buffer,
                 "interface %s\n{\n\tAdvSendAdvert on;\n"
                 "\tMinRtrAdvInterval 3;\n\tMaxRtrAdvInterval 10;\n"
                 "\tAdvManagedFlag off;\n\tAdvOtherConfigFlag off;\n"
                 "\tprefix %s\n"
                 "\t{\n\t\tAdvOnLink off;\n"
                 "\t\tAdvAutonomous on;\n"
                 "\t\tAdvRouterAddr off;\n"
                 "\t};\n\tRDNSS%s\n"
                 "\t{\n"
                 "\t\tAdvRDNSSLifetime 3600;\n"
                 "\t};"
                 "\n};\n",
                 aInternalIface, aNetworkPrefix, dnsesList);

  if (!fwrite(buffer, sizeof(char), strlen(buffer), radvdFile)) {
    NU_DBG("Write %s fail", RADVD_CONF_FILE);
    fclose(radvdFile);
    return false;
  }

  fclose(radvdFile);

  if (chmod(RADVD_CONF_FILE, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH) <
          0 ||
      chown(RADVD_CONF_FILE, AID_SYSTEM, AID_SYSTEM)) {
    NU_DBG("Cannot chmod %s", RADVD_CONF_FILE);
    return false;
  }

  return true;
}

bool NetworkUtils::getIpv6Prefix(const char* aIpv6Addr, char* aIpv6Prefix) {
  unsigned char ipv6AddrNetwork[sizeof(struct in6_addr)];
  char ipv6PrefixBuf[64] = {0};

  if (inet_pton(AF_INET6, aIpv6Addr, ipv6AddrNetwork) < 1) {
    NU_DBG("Failed to transfer Ipv6 address to binary");
    return false;
  }

  for (uint32_t i = sizeof(struct in6_addr) / 2; i < sizeof(struct in6_addr);
       i++) {
    ipv6AddrNetwork[i] = 0;
  }

  if (!inet_ntop(AF_INET6, ipv6AddrNetwork, ipv6PrefixBuf, INET6_ADDRSTRLEN)) {
    NU_DBG("Failed to transfer Ipv6 binary to address");
    return false;
  }

  snprintf(aIpv6Prefix, sizeof(ipv6PrefixBuf), "%s/64", ipv6PrefixBuf);

  return true;
}

void NetworkUtils::setupPrefix64Discovery(CommandChain* aChain,
                                          CommandCallback aCallback,
                                          NetworkResultOptions& aResult) {
  bool enable = GET_FIELD(mEnable);
  int netId = GET_FIELD(mNetId);
  Status status;
  if (enable) {
    status = gDnsResolver->startPrefix64Discovery(netId);
  } else {
    status = gDnsResolver->stopPrefix64Discovery(netId);
  }
  NU_DBG("setupPrefix64Discovery %s on %d %s", enable ? "enable" : "disable",
         netId, status.isOk() ? "success" : "failed");
  next(aChain, !status.isOk(), aResult);
}

#undef GET_CHAR
#undef GET_FIELD

/*
 * Netd command success/fail function
 */
#define ASSIGN_FIELD(prop) aResult.prop = aChain->getParams().prop;
#define ASSIGN_FIELD_VALUE(prop, value) aResult.prop = value;

template <size_t N>
void NetworkUtils::runChain(const NetworkParams& aParams,
                            const CommandFunc (&aCmds)[N],
                            ErrorCallback aError) {
  CommandChain* chain = new CommandChain(aParams, aCmds, N, aError);
  gCommandChainQueue.AppendElement(chain);

  if (gCommandChainQueue.Length() > 1) {
    NU_DBG("%d command chains are queued. Wait!", gCommandChainQueue.Length());
    return;
  }

  NetworkResultOptions result;
  NetworkUtils::next(gCommandChainQueue[0], false, result);
}

// Called to clean up the command chain and process the queued command chain if
// any.
void NetworkUtils::finalizeSuccess(CommandChain* aChain,
                                   NetworkResultOptions& aResult) {
  next(aChain, false, aResult);
}

void NetworkUtils::defaultAsyncSuccessHandler(CommandChain* aChain,
                                              CommandCallback aCallback,
                                              NetworkResultOptions& aResult) {
  NU_DBG("defaultAsyncSuccessHandler");
  aResult.mRet = true;
  postMessage(aChain->getParams(), aResult);
  finalizeSuccess(aChain, aResult);
}

void NetworkUtils::defaultAsyncFailureHandler(NetworkParams& aOptions,
                                              NetworkResultOptions& aResult) {
  aResult.mRet = false;
  postMessage(aOptions, aResult);
}

void NetworkUtils::usbTetheringFail(NetworkParams& aOptions,
                                    NetworkResultOptions& aResult) {
  // Notify the main thread.
  aResult.mResult = false;
  postMessage(aOptions, aResult);
  // Try to roll back to ensure
  // we don't leave the network systems in limbo.
  // This parameter is used to disable ipforwarding.
  {
    aOptions.mEnable = false;
    runChain(aOptions, sUSBFailChain, nullptr);
  }
}

void NetworkUtils::usbTetheringSuccess(CommandChain* aChain,
                                       CommandCallback aCallback,
                                       NetworkResultOptions& aResult) {
  ASSIGN_FIELD(mEnable)
  aResult.mResult = true;
  postMessage(aChain->getParams(), aResult);
  finalizeSuccess(aChain, aResult);
}

void NetworkUtils::wifiTetheringFail(NetworkParams& aOptions,
                                     NetworkResultOptions& aResult) {
  // Notify the main thread.
  postMessage(aOptions, aResult);

  // If one of the stages fails, we try roll back to ensure
  // we don't leave the network systems in limbo.
  ASSIGN_FIELD_VALUE(mEnable, false)
  runChain(aOptions, sWifiFailChain, nullptr);
}

void NetworkUtils::wifiTetheringSuccess(CommandChain* aChain,
                                        CommandCallback aCallback,
                                        NetworkResultOptions& aResult) {
  ASSIGN_FIELD(mEnable)
  aResult.mResult = true;
  postMessage(aChain->getParams(), aResult);
  finalizeSuccess(aChain, aResult);
}

void NetworkUtils::updateUpStreamFail(NetworkParams& aOptions,
                                      NetworkResultOptions& aResult) {
  postMessage(aOptions, aResult);
}

void NetworkUtils::updateUpStreamSuccess(CommandChain* aChain,
                                         CommandCallback aCallback,
                                         NetworkResultOptions& aResult) {
  ASSIGN_FIELD(mCurExternalIfname)
  ASSIGN_FIELD(mCurInternalIfname)
  postMessage(aChain->getParams(), aResult);
  finalizeSuccess(aChain, aResult);
}

#undef ASSIGN_FIELD
#undef ASSIGN_FIELD_VALUE

NetworkUtils::NetworkUtils(MessageCallback aCallback)
    : mMessageCallback(aCallback) {
  mNetUtils.reset(new NetUtils());

  nsresult rv =
      NS_NewNamedThread("NetworkUtils", getter_AddRefs(gNetworkUtilsThread));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    NU_DBG("Failed to create NetworkUtils handle thread");
    return;
  }

  // Restart netd if b2g restart.
  char value[Property::VALUE_MAX_LENGTH];
  Property::Get("dev.b2g.pid", value, "0");
  unsigned int prePid = atoi(value);
  unsigned int curPid = getpid();
  snprintf(value, Property::VALUE_MAX_LENGTH - 1, "%d", curPid);
  Property::Set("dev.b2g.pid", value);
  if (prePid > 0) {
    NU_DBG("Stop Netd due to b2g restart");
    setupNetd(false);
  }

  NU_DBG("Start Netd Service");
  setupNetd(true);

  IPV6_ADDR_GEN_MODE = mozilla::StaticPrefs::dom_b2g_ipv6_addr_mode();
  updateDebug();

  gNetworkUtils = this;
}

NetworkUtils::~NetworkUtils() {
  setupNetd(false);
  NU_DBG("destroy NetworkUtils");
  gNetworkUtilsThread->Shutdown();
  gNetworkUtilsThread = nullptr;
}

void NetworkUtils::setupNetd(bool aEnable) {
  nsCOMPtr<nsIRunnable> runnable = new NetdInitRunnable(aEnable);
  if (gNetworkUtilsThread) {
    NU_DBG("Dispatch Netd thread");
    gNetworkUtilsThread->Dispatch(runnable, nsIEventTarget::DISPATCH_NORMAL);
  }
}

#define GET_CHAR(prop) NS_ConvertUTF16toUTF8(aOptions.prop).get()
#define GET_FIELD(prop) aOptions.prop

// Hoist this type definition to global to avoid template
// instantiation error on gcc 4.4 used by ICS emulator.
typedef CommandResult (NetworkUtils::*CommandHandler)(NetworkParams&);
struct CommandHandlerEntry {
  const char* mCommandName;
  CommandHandler mCommandHandler;
};

void NetworkUtils::ExecuteCommand(NetworkParams aOptions) {
  const static CommandHandlerEntry COMMAND_HANDLER_TABLE[] = {

// For command 'testCommand', BUILD_ENTRY(testCommand) will generate
// {"testCommand", NetworkUtils::testCommand}
#define BUILD_ENTRY(c) {#c, &NetworkUtils::c}
      BUILD_ENTRY(removeNetworkRoute),
      BUILD_ENTRY(setDNS),
      BUILD_ENTRY(getNetId),
      BUILD_ENTRY(createNetwork),
      BUILD_ENTRY(destroyNetwork),
      BUILD_ENTRY(setDefaultRoute),
      BUILD_ENTRY(removeDefaultRoute),
      BUILD_ENTRY(addHostRoute),
      BUILD_ENTRY(removeHostRoute),
      BUILD_ENTRY(addSecondaryRoute),
      BUILD_ENTRY(removeSecondaryRoute),
      BUILD_ENTRY(setMtu),
      BUILD_ENTRY(setDefaultNetwork),
      BUILD_ENTRY(addInterfaceToNetwork),
      BUILD_ENTRY(removeInterfaceToNetwork),
      BUILD_ENTRY(setIpv6Status),
      BUILD_ENTRY(dhcpRequest),
      BUILD_ENTRY(stopDhcp),
      BUILD_ENTRY(getInterfaces),
      BUILD_ENTRY(getInterfaceConfig),
      BUILD_ENTRY(setInterfaceConfig),
      BUILD_ENTRY(startClatd),
      BUILD_ENTRY(stopClatd),
      BUILD_ENTRY(setTcpBufferSize),
      BUILD_ENTRY(setNetworkInterfaceAlarm),
      BUILD_ENTRY(enableNetworkInterfaceAlarm),
      BUILD_ENTRY(disableNetworkInterfaceAlarm),
      BUILD_ENTRY(setTetheringAlarm),
      BUILD_ENTRY(removeTetheringAlarm),
      BUILD_ENTRY(setDhcpServer),
      BUILD_ENTRY(getTetheringStatus),
      BUILD_ENTRY(getTetherStats),
      BUILD_ENTRY(updateUpStream),
      BUILD_ENTRY(removeUpStream),
      BUILD_ENTRY(setUSBTethering),
      BUILD_ENTRY(setWifiTethering),
      BUILD_ENTRY(setupPrefix64Discovery),
#undef BUILD_ENTRY
  };

  // Loop until we find the command name which matches aOptions.mCmd.
  CommandHandler handler = nullptr;
  for (size_t i = 0; i < mozilla::ArrayLength(COMMAND_HANDLER_TABLE); i++) {
    if (aOptions.mCmd.EqualsASCII(COMMAND_HANDLER_TABLE[i].mCommandName)) {
      handler = COMMAND_HANDLER_TABLE[i].mCommandHandler;
      break;
    }
  }

  if (!handler) {
    // Command not found in COMMAND_HANDLER_TABLE.
    WARN("unknown message: %s", NS_ConvertUTF16toUTF8(aOptions.mCmd).get());
    return;
  }

  // The handler would return one of the following 3 values
  // to be wrapped to CommandResult:
  //
  //   1) |int32_t| for mostly synchronous native function calls.
  //   2) |NetworkResultOptions| to populate additional results. (e.g.
  //   dhcpRequest) 3) |CommandResult::Pending| to indicate the result is not
  //      obtained yet.
  //
  // If the handler returns "Pending", the handler should take the
  // responsibility for posting result to main thread.
  CommandResult commandResult = (this->*handler)(aOptions);
  if (!commandResult.isPending()) {
    postMessage(aOptions, commandResult.mResult);
  }
}

/**
 * Set DNS servers for given network interface.
 */
CommandResult NetworkUtils::setDNS(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {setInterfaceDns,
                                        defaultAsyncSuccessHandler};
  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
    return CommandResult(-1);
  }
  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

/**
 * Query the netId associated with the given network interface name.
 */
CommandResult NetworkUtils::getNetId(NetworkParams& aOptions) {
  NetworkResultOptions result;
  result.mResult = false;
  char errorReason[BUF_SIZE];

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    snprintf(errorReason, BUF_SIZE - 1, "No such interface: %s",
             GET_CHAR(mIfname));
    result.mReason = NS_ConvertUTF8toUTF16(errorReason);
    return CommandResult(result);
  }
  result.mNetId.AppendInt(netIdInfo.mNetId, 10);
  result.mResult = true;
  return CommandResult(result);
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::createNetwork(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      createNetwork,
      addInterfaceToNetwork,
      wakeupAddInterface,
      defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
    NU_DBG("Interface %s (%d) has been created.", GET_CHAR(mIfname),
           netIdInfo.mNetId);
    mNetIdManager.acquire(GET_FIELD(mIfname), &netIdInfo,
                          GET_FIELD(mNetworkType));
    return CommandResult(SUCCESS);
  }

  mNetIdManager.acquire(GET_FIELD(mIfname), &netIdInfo,
                        GET_FIELD(mNetworkType));
  NU_DBG("Request netd to create a network with netid %d", netIdInfo.mNetId);
  // Newly created netid. Ask netd to create network.
  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::destroyNetwork(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      wakeupDelInterface,
      removeInterfaceToNetwork,
      destroyNetwork,
      defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.release(GET_FIELD(mIfname), &netIdInfo,
                             GET_FIELD(mNetworkType))) {
    ERROR("No existing netid for %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  if (netIdInfo.mTypes != 0) {
    // Still be referenced. Just return.
    NU_DBG("Someone is still using this interface.");
    return CommandResult(SUCCESS);
  }

  NU_DBG("Interface %s (%d) is no longer used. Tell netd to destroy.",
         GET_CHAR(mIfname), netIdInfo.mNetId);

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

/**
 * Set default route for given network interface..
 */
CommandResult NetworkUtils::setDefaultRoute(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {addDefaultRouteToNetwork,
                                        defaultAsyncSuccessHandler};

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  NU_DBG("Calling NetworkUtils::setDefaultRoute mIfname= %s",
         GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

/*
 * Remove default route for given network interface.
 */
CommandResult NetworkUtils::removeDefaultRoute(NetworkParams& aOptions) {
  NU_DBG("Calling NetworkUtils::removeDefaultRoute");

  static CommandFunc COMMAND_CHAIN[] = {
      removeDefaultRoute,
      defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId,
         GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

/**
 * Add host route for given network interface.
 */
CommandResult NetworkUtils::addHostRoute(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      addRouteToInterface,
      defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId,
         GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

/**
 * Remove host route for given network interface.
 */
CommandResult NetworkUtils::removeHostRoute(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      removeRouteFromInterface,
      defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId,
         GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

CommandResult NetworkUtils::removeNetworkRoute(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      clearAddrForInterface,
      defaultAsyncSuccessHandler,
  };

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("interface %s is not present in any network", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  NU_DBG("Obtained netid %d for interface %s", netIdInfo.mNetId,
         GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

CommandResult NetworkUtils::addSecondaryRoute(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {addRouteToSecondaryTable,
                                        defaultAsyncSuccessHandler};

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }
  aOptions.mNetId = netIdInfo.mNetId;

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

CommandResult NetworkUtils::removeSecondaryRoute(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {removeRouteFromSecondaryTable,
                                        defaultAsyncSuccessHandler};

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }
  aOptions.mNetId = netIdInfo.mNetId;

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

CommandResult NetworkUtils::setMtu(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      setMtu,
      defaultAsyncSuccessHandler,
  };

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

/**
 * Set default for given network interface..
 */
CommandResult NetworkUtils::setDefaultNetwork(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {setDefaultNetwork,
                                        defaultAsyncSuccessHandler};

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(GET_FIELD(mIfname), &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mIfname));
    return CommandResult(-1);
  }

  NU_DBG("Calling NetworkUtils::setDefaultNetwork mIfname= %s",
         GET_CHAR(mIfname));

  aOptions.mNetId = netIdInfo.mNetId;
  aOptions.mLoopIndex = 0;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

  return CommandResult(CommandResult::Pending());
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::addInterfaceToNetwork(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      addInterfaceToNetwork,
      defaultAsyncSuccessHandler,
  };

  if (GET_FIELD(mNetId) == -1) {
    NU_DBG("addInterfaceToNetwork no mNetId");
    return CommandResult(-1);
  }

  if (GET_FIELD(mIfname).IsEmpty()) {
    NU_DBG("addInterfaceToNetwork no mIfname");
    return CommandResult(-1);
  }

  NetIdManager::NetIdInfo netIdInfo;
  if (mNetIdManager.lookupByNetId(uint32_t(GET_FIELD(mNetId)), &netIdInfo)) {
    NU_DBG("Add %s to network(%d).", GET_CHAR(mIfname), netIdInfo.mNetId);
    mNetIdManager.addInterfaceToNetwork(GET_FIELD(mIfname), &netIdInfo);
    runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

    return CommandResult(CommandResult::Pending());
  }

  NU_DBG("addInterfaceToNetwork no exist netID");
  return CommandResult(-1);
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::removeInterfaceToNetwork(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {
      removeInterfaceToNetwork,
      defaultAsyncSuccessHandler,
  };

  if (GET_FIELD(mNetId) == -1) {
    NU_DBG("removeInterfaceToNetwork no mNetId");
    return CommandResult(-1);
  }

  if (GET_FIELD(mIfname).IsEmpty()) {
    NU_DBG("removeInterfaceToNetwork no mIfname");
    return CommandResult(-1);
  }

  NetIdManager::NetIdInfo netIdInfo;

  if (mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
    NU_DBG("Remove %s to network(%d).", GET_CHAR(mIfname), netIdInfo.mNetId);
    mNetIdManager.removeInterfaceToNetwork(GET_FIELD(mIfname), &netIdInfo);
    runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);

    return CommandResult(CommandResult::Pending());
  }
  NU_DBG("removeInterfaceToNetwork no exist mIfname");
  return CommandResult(-1);
}

CommandResult NetworkUtils::setIpv6Status(NetworkParams& aOptions) {
  bool enable = aOptions.mEnable;
  if (enable) {
    static CommandFunc COMMAND_CHAIN[] = {
        setIpv6PrivacyExtensions,
        setIpv6AddrGenMode,
        setIpv6Enabled,
        defaultAsyncSuccessHandler,
    };
    runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  } else {
    static CommandFunc COMMAND_CHAIN[] = {
        setIpv6Enabled,
        defaultAsyncSuccessHandler,
    };
    runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  }
  return CommandResult(CommandResult::Pending());
}

CommandResult NetworkUtils::dhcpRequest(NetworkParams& aOptions) {
  mozilla::dom::NetworkResultOptions result;

  NS_ConvertUTF16toUTF8 autoIfname(aOptions.mIfname);
  char ipaddr[Property::VALUE_MAX_LENGTH];
  char gateway[Property::VALUE_MAX_LENGTH];
  uint32_t prefixLength;
  char dns1[Property::VALUE_MAX_LENGTH];
  char dns2[Property::VALUE_MAX_LENGTH];
  char server[Property::VALUE_MAX_LENGTH];
  uint32_t lease;
  char vendorinfo[Property::VALUE_MAX_LENGTH];
  int32_t ret = mNetUtils->do_dhcp_do_request(autoIfname.get(), ipaddr, gateway,
                                              &prefixLength, dns1, dns2, server,
                                              &lease, vendorinfo);

  RETURN_IF_FAILED(ret);

  result.mIpaddr_str = NS_ConvertUTF8toUTF16(ipaddr);
  result.mGateway_str = NS_ConvertUTF8toUTF16(gateway);
  result.mDns1_str = NS_ConvertUTF8toUTF16(dns1);
  result.mDns2_str = NS_ConvertUTF8toUTF16(dns2);
  result.mServer_str = NS_ConvertUTF8toUTF16(server);
  result.mVendor_str = NS_ConvertUTF8toUTF16(vendorinfo);
  result.mLease = lease;
  result.mPrefixLength = prefixLength;
  result.mMask = makeMask(prefixLength);

  uint32_t inet4;  // only support IPv4 for now.

#define INET_PTON(var, field)      \
  PR_BEGIN_MACRO                   \
  inet_pton(AF_INET, var, &inet4); \
  result.field = inet4;            \
  PR_END_MACRO

  INET_PTON(ipaddr, mIpaddr);
  INET_PTON(gateway, mGateway);

  if (dns1[0] != '\0') {
    INET_PTON(dns1, mDns1);
  }

  if (dns2[0] != '\0') {
    INET_PTON(dns2, mDns2);
  }

  INET_PTON(server, mServer);

  char inet_str[64];
  if (inet_ntop(AF_INET, &result.mMask, inet_str, sizeof(inet_str))) {
    result.mMask_str = NS_ConvertUTF8toUTF16(inet_str);
  }

  return CommandResult(result);
}

CommandResult NetworkUtils::stopDhcp(NetworkParams& aOptions) {
  return CommandResult(mNetUtils->do_dhcp_stop(GET_CHAR(mIfname)));
}

/**
 * Get existing network interfaces.
 */
CommandResult NetworkUtils::getInterfaces(NetworkParams& aOptions) {
  NetworkResultOptions result;

  std::vector<std::string> interfaceGetList;
  Status status = gNetd->interfaceGetList(&interfaceGetList);
  if (status.isOk()) {
    result.mInterfaceList.Construct();
    for (uint32_t i = 0; i < interfaceGetList.size(); i++) {
      mozilla::Unused << result.mInterfaceList.Value().AppendElement(
          NS_ConvertUTF8toUTF16(interfaceGetList[i].c_str()),
          mozilla::fallible_t());
    }
  }
  result.mResult = status.isOk();

  NU_DBG("getInterfaces: %s", result.mResult ? "success" : "failed");
  return CommandResult(result);
}

/**
 * Get network config of a specified interface.
 */
CommandResult NetworkUtils::getInterfaceConfig(NetworkParams& aOptions) {
  NetworkResultOptions result;
  android::net::InterfaceConfigurationParcel interfaceCfgResult;
  Status status =
      gNetd->interfaceGetCfg(GET_CHAR(mIfname), &interfaceCfgResult);
  if (status.isOk()) {
    result.mMacAddr = NS_ConvertUTF8toUTF16(interfaceCfgResult.hwAddr.c_str());
    result.mIpAddr = NS_ConvertUTF8toUTF16(interfaceCfgResult.ipv4Addr.c_str());
    result.mPrefixLength = interfaceCfgResult.prefixLength;
    if (std::find(interfaceCfgResult.flags.begin(),
                  interfaceCfgResult.flags.end(),
                  std::string("up")) != interfaceCfgResult.flags.end()) {
      result.mFlag = NS_ConvertUTF8toUTF16("up");
    } else {
      result.mFlag = NS_ConvertUTF8toUTF16("down");
    }
  }
  result.mResult = status.isOk();

  NU_DBG("getInterfaceConfig: %s", result.mResult ? "success" : "failed");
  return CommandResult(result);
}

/**
 * Set network config for a specified interface.
 */
CommandResult NetworkUtils::setInterfaceConfig(NetworkParams& aOptions) {
  NetworkResultOptions result;
  android::net::InterfaceConfigurationParcel interfaceCfg;
  struct in_addr addr;

  result.mResult = false;
  interfaceCfg.ifName = GET_CHAR(mIfname);
  interfaceCfg.hwAddr = "";

  if (!inet_aton(GET_CHAR(mIp), &addr)) {
    interfaceCfg.ipv4Addr = "";
    interfaceCfg.prefixLength = 0;
  } else if (addr.s_addr != 0) {
    interfaceCfg.ipv4Addr = GET_CHAR(mIp);
    interfaceCfg.prefixLength = atoi(GET_CHAR(mPrefix));
    Status status = gNetd->interfaceSetCfg(interfaceCfg);
    if (!status.isOk()) {
      NU_DBG("setInterfaceConfig: failed to set configuration.");
      return CommandResult(result);
    }
  }

  Status bringStatus;
  if (!strcmp(GET_CHAR(mLink), "up")) {
    interfaceCfg.flags.push_back(
        std::string(android::String8((INetd::IF_STATE_UP().string()))));
    bringStatus = gNetd->interfaceSetCfg(interfaceCfg);

  } else if (!strcmp(GET_CHAR(mLink), "down")) {
    interfaceCfg.flags.push_back(
        std::string(android::String8((INetd::IF_STATE_DOWN().string()))));
    bringStatus = gNetd->interfaceSetCfg(interfaceCfg);
  } else {
    NU_DBG("setInterfaceConfig: Unknown link config.");
    return CommandResult(result);
  }

  result.mResult = bringStatus.isOk();
  NU_DBG("setInterfaceConfig: %s", result.mResult ? "success" : "failed");
  return CommandResult(result);
}

/**
 * Request to start Clat464.
 */
CommandResult NetworkUtils::startClatd(NetworkParams& aOptions) {
  NU_DBG("startClatd: %s %s", GET_CHAR(mIfname), GET_CHAR(mNat64Prefix));
  NetworkResultOptions result;
  result.mResult = false;

  if (GET_FIELD(mIfname).IsEmpty() || GET_FIELD(mNat64Prefix).IsEmpty()) {
    NU_DBG("startClatd argument is empty");
    return CommandResult(result);
  }

// FIXME: 'clatdStart' is deprecated:
#if 0
  std::string clatAddress;
  Status status = gNetd->clatdStart(GET_CHAR(mIfname), GET_CHAR(mNat64Prefix),
                                    &clatAddress);
  result.mResult = status.isOk();

  if (result.mResult) {
    result.mClatdAddress = NS_ConvertUTF8toUTF16(clatAddress.c_str());
  }
  NU_DBG("startClatd result: %s %s", clatAddress.c_str(),
         result.mResult ? "success" : "false");
#endif
  return CommandResult(result);
}

/**
 * Request to stop Clat464.
 */
CommandResult NetworkUtils::stopClatd(NetworkParams& aOptions) {
  NU_DBG("stopClatd: %s", GET_CHAR(mIfname));
  NetworkResultOptions result;
  result.mResult = false;

  if (GET_FIELD(mIfname).IsEmpty()) {
    NU_DBG("stopClatd argument is empty");
    return CommandResult(result);
  }

// FIXME:  'clatdStop' is deprecated.
#if 0
  Status status = gNetd->clatdStop(GET_CHAR(mIfname));
  result.mResult = status.isOk();
  NU_DBG("stopClatd result: %s", result.mResult ? "success" : "false");
#endif

  return CommandResult(result);
}

/**
 * Set dynamic TCP buffer size based on connection type.
 */
CommandResult NetworkUtils::setTcpBufferSize(NetworkParams& aOptions) {
  NU_DBG("setTcpBufferSizes: %s", GET_CHAR(mTcpBufferSizes));
  NetworkResultOptions result;
  result.mResult = false;

  if (GET_FIELD(mTcpBufferSizes).IsEmpty()) {
    NU_DBG("mTcpBufferSizes is empty");
    return CommandResult(result);
  }

  char buf[BUF_SIZE];
  SprintfLiteral(buf, "%s", GET_CHAR(mTcpBufferSizes));
  nsTArray<nsCString> bufferResult;
  split(buf, TCP_BUFFER_DELIMIT, bufferResult);

  if (bufferResult.Length() < 6) {
    NU_DBG("mTcpBufferSizes length is not enough");
    return CommandResult(result);
  }

  std::string rmemValues = std::string(bufferResult[0].get()) + " " +
                           std::string(bufferResult[1].get()) + " " +
                           std::string(bufferResult[2].get());
  std::string wmemValues = std::string(bufferResult[3].get()) + " " +
                           std::string(bufferResult[4].get()) + " " +
                           std::string(bufferResult[5].get());

  Status status = gNetd->setTcpRWmemorySize(rmemValues, wmemValues);
  result.mResult = status.isOk();
  NU_DBG("setTcpBufferSizes result: %s", result.mResult ? "success" : "failed");

  return CommandResult(result);
}

CommandResult NetworkUtils::setNetworkInterfaceAlarm(NetworkParams& aOptions) {
  NU_DBG("setNetworkInterfaceAlarms: %s", GET_CHAR(mIfname));
  NetworkResultOptions result;

  Status status = gNetd->bandwidthSetInterfaceAlert(GET_CHAR(mIfname),
                                                    GET_FIELD(mThreshold));

  result.mResult = status.isOk() ? true : false;
  NU_DBG("setNetworkInterfaceAlarms %s %ld %s", GET_CHAR(mIfname),
         GET_FIELD(mThreshold), result.mResult ? "success" : "failed");
  return CommandResult(result);
}

CommandResult NetworkUtils::enableNetworkInterfaceAlarm(
    NetworkParams& aOptions) {
  NU_DBG("enableNetworkInterfaceAlarm: %s", GET_CHAR(mIfname));
  NetworkResultOptions result;

  Status status =
      gNetd->bandwidthSetInterfaceQuota(GET_CHAR(mIfname), INT64_MAX);
  result.mResult = status.isOk() ? true : false;

  NU_DBG("enableNetworkInterfaceAlarm %s",
         result.mResult ? "success" : "failed");
  return CommandResult(result);
}

CommandResult NetworkUtils::disableNetworkInterfaceAlarm(
    NetworkParams& aOptions) {
  NU_DBG("disableNetworkInterfaceAlarms: %s", GET_CHAR(mIfname));
  NetworkResultOptions result;

  Status status = gNetd->bandwidthRemoveInterfaceQuota(GET_CHAR(mIfname));

  result.mResult = status.isOk() ? true : false;
  NU_DBG("disableNetworkInterfaceAlarm %s",
         result.mResult ? "success" : "failed");
  return CommandResult(result);
}

CommandResult NetworkUtils::setTetheringAlarm(NetworkParams& aOptions) {
  NU_DBG("setTetheringAlarm");
  NetworkResultOptions result;
  result.mResult = false;

  Status status = gNetd->bandwidthSetGlobalAlert(GET_FIELD(mThreshold));
  result.mResult = status.isOk() ? true : false;
  NU_DBG("setTetheringAlarm %s", result.mResult ? "success" : "failed");
  return CommandResult(result);
}

CommandResult NetworkUtils::removeTetheringAlarm(NetworkParams& aOptions) {
  NU_DBG("removeTetheringAlarm");
  NetworkResultOptions result;
  result.mResult = false;

  Status status = gNetd->bandwidthSetInterfaceAlert(GET_CHAR(mIfname),
                                                    GET_FIELD(mThreshold));
  result.mResult = status.isOk() ? true : false;
  NU_DBG("removeTetheringAlarm %s", result.mResult ? "success" : "failed");
  return CommandResult(result);
}

/**
 * Start/Stop DHCP server.
 */
CommandResult NetworkUtils::setDhcpServer(NetworkParams& aOptions) {
  NetworkResultOptions result;
  result.mResult = false;
  Status status;

  if (aOptions.mEnabled) {
    android::net::InterfaceConfigurationParcel interfaceCfg;
    struct in_addr addr;

    interfaceCfg.ifName = GET_CHAR(mIfname);
    interfaceCfg.hwAddr = "";

    if (!inet_aton(GET_CHAR(mIp), &addr)) {
      interfaceCfg.ipv4Addr = "";
      interfaceCfg.prefixLength = 0;
    } else if (addr.s_addr != 0) {
      interfaceCfg.ipv4Addr = GET_CHAR(mIp);
      interfaceCfg.prefixLength = atoi(GET_CHAR(mPrefix));
      status = gNetd->interfaceSetCfg(interfaceCfg);
      if (status.isOk()) {
        NU_DBG("setDhcpServer: failed to set configuration.");
        return CommandResult(result);
      }
    }

    interfaceCfg.flags.push_back(
        std::string(android::String8((INetd::IF_STATE_UP().string()))));
    status = gNetd->interfaceSetCfg(interfaceCfg);

    if (!status.isOk()) {
      NU_DBG("setDhcpServer: failed to bring up interface.");
      return CommandResult(result);
    }

    std::vector<std::string> dhcpRanges;
    dhcpRanges.push_back(GET_CHAR(mWifiStartIp));
    dhcpRanges.push_back(GET_CHAR(mWifiEndIp));
    if (!GET_FIELD(mUsbStartIp).IsEmpty() && !GET_FIELD(mUsbEndIp).IsEmpty()) {
      dhcpRanges.push_back(GET_CHAR(mUsbStartIp));
      dhcpRanges.push_back(GET_CHAR(mUsbEndIp));
    }
    status = gNetd->tetherStart(dhcpRanges);

    result.mResult = status.isOk();
    return CommandResult(result);
  }

  std::vector<std::string> ifList;
  gNetd->tetherInterfaceList(&ifList);
  if (ifList.size() > 1) {
    // Don't stop tethering because others interface still need it.
    // Send the dummy to continue the function chain.
    result.mResult = true;
    return CommandResult(result);
  }
  status = gNetd->tetherStop();
  result.mResult = status.isOk();
  return CommandResult(result);
}

CommandResult NetworkUtils::getTetheringStatus(NetworkParams& aOptions) {
  NU_DBG("getTetheringStatus");
  NetworkResultOptions result;
  result.mResult = false;

  bool tetherEnabled;
  gNetd->tetherIsEnabled(&tetherEnabled);
  result.mResult = tetherEnabled;

  return CommandResult(result);
}

CommandResult NetworkUtils::getTetherStats(NetworkParams& aOptions) {
  NetworkResultOptions result;
  Status status;
  result.mResult = false;

  std::vector<TetherStatsParcel> tetherStatsParcelVec;
  status = gNetd->tetherGetStats(&tetherStatsParcelVec);
  NU_DBG("getTetherStats %s", status.isOk() ? "success" : "failed");
  result.mResult = status.isOk();
  result.mTetherStats.Construct();

  if (tetherStatsParcelVec.size()) {
    for (auto statsParcel : tetherStatsParcelVec) {
      if (!strcmp(statsParcel.iface.c_str(), "rmnet0")) {
        // Just ignore the dummy upstream interface.
        continue;
      }
      TetherStats tetherStats;
      tetherStats.mIfname =
          nsString(NS_ConvertUTF8toUTF16(statsParcel.iface.c_str()));
      tetherStats.mRxBytes = statsParcel.rxBytes;
      tetherStats.mRxPackets = statsParcel.rxPackets;
      tetherStats.mTxBytes = statsParcel.txBytes;
      tetherStats.mTxPackets = statsParcel.txPackets;
      mozilla::Unused << result.mTetherStats.Value().AppendElement(
          std::move(tetherStats), mozilla::fallible);
    }
  }
  return CommandResult(result);
}

CommandResult NetworkUtils::setUSBTethering(NetworkParams& aOptions) {
  bool enable = aOptions.mEnable;
  char ipv6Prefix[64] = {0};
  nsCString externalIface(GET_CHAR(mExternalIfname));

  NetIdManager::NetIdInfo netIdInfo;
  aOptions.mNetId = mNetIdManager.lookup(aOptions.mExternalIfname, &netIdInfo)
                        ? netIdInfo.mNetId
                        : -1;
  if (enable && aOptions.mNetId < 0) {
    ERROR("No such interface to enable: %s", GET_CHAR(mExternalIfname));
    return CommandResult(-1);
  }

  // Collect external interface Ipv6 prefix.
  if (!aOptions.mIpv6Ip.IsEmpty() &&
      getIpv6Prefix(GET_CHAR(mIpv6Ip), ipv6Prefix)) {
    aOptions.mIPv6Prefix = NS_ConvertUTF8toUTF16(ipv6Prefix);
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16(ipv6Prefix));
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16("fe80::/64"));
  }
  aOptions.mLoopIndex = 0;

  dumpParams(aOptions, "USB");

  if (enable) {
    NU_DBG("Starting USB Tethering on %s <-> %s", GET_CHAR(mInternalIfname),
           GET_CHAR(mExternalIfname));
    runChain(aOptions, sUSBEnableChain, usbTetheringFail);
  } else {
    NU_DBG("Stopping USB Tethering on %s <-> %s", GET_CHAR(mInternalIfname),
           GET_CHAR(mExternalIfname));
    runChain(aOptions, sUSBDisableChain, usbTetheringFail);
  }
  return CommandResult(CommandResult::Pending());
}

CommandResult NetworkUtils::setWifiTethering(NetworkParams& aOptions) {
  bool enable = aOptions.mEnable;
  char ipv6Prefix[64] = {0};
  nsCString externalIface(GET_CHAR(mExternalIfname));

  // Collect external interface Ipv6 prefix.
  if (!aOptions.mIpv6Ip.IsEmpty() &&
      getIpv6Prefix(GET_CHAR(mIpv6Ip), ipv6Prefix)) {
    aOptions.mIPv6Prefix = NS_ConvertUTF8toUTF16(ipv6Prefix);
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16(ipv6Prefix));
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16("fe80::/64"));
  }
  aOptions.mLoopIndex = 0;

  NetIdManager::NetIdInfo netIdInfo;
  aOptions.mNetId = mNetIdManager.lookup(aOptions.mExternalIfname, &netIdInfo)
                        ? netIdInfo.mNetId
                        : -1;
  if (enable && aOptions.mNetId < 0) {
    ERROR("No such interface to enable: %s", GET_CHAR(mExternalIfname));
    return CommandResult(-1);
  }

  dumpParams(aOptions, "WIFI");

  if (enable) {
    NU_DBG("Starting Wifi Tethering on %s <-> %s", GET_CHAR(mInternalIfname),
           GET_CHAR(mExternalIfname));
    runChain(aOptions, sWifiEnableChain, wifiTetheringFail);
  } else {
    NU_DBG("Stopping Wifi Tethering on %s <-> %s", GET_CHAR(mInternalIfname),
           GET_CHAR(mExternalIfname));
    runChain(aOptions, sWifiDisableChain, wifiTetheringFail);
  }
  return CommandResult(CommandResult::Pending());
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::updateUpStream(NetworkParams& aOptions) {
  char ipv6Prefix[64] = {0};
  nsCString externalIface(GET_CHAR(mExternalIfname));

  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(aOptions.mCurExternalIfname, &netIdInfo)) {
    ERROR("No such interface: %s", GET_CHAR(mCurExternalIfname));
    return CommandResult(-1);
  }
  aOptions.mNetId = netIdInfo.mNetId;

  if (!externalIface.get()[0]) {
    externalIface = GET_CHAR(mCurExternalIfname);
  }

  // Collect external interface Ipv6 prefix.
  if (!aOptions.mIpv6Ip.IsEmpty() &&
      getIpv6Prefix(GET_CHAR(mIpv6Ip), ipv6Prefix)) {
    aOptions.mIPv6Prefix = NS_ConvertUTF8toUTF16(ipv6Prefix);
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16(ipv6Prefix));
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16("fe80::/64"));
  }
  aOptions.mLoopIndex = 0;

  dumpParams(aOptions, GET_CHAR(mType));

  runChain(aOptions, sUpdateUpStreamChain, updateUpStreamFail);
  return CommandResult(CommandResult::Pending());
}

/**
 * handling upstream interface change event.
 */
CommandResult NetworkUtils::removeUpStream(NetworkParams& aOptions) {
  char ipv6Prefix[64] = {0};

  // Collect external interface Ipv6 prefix.
  if (!aOptions.mIpv6Ip.IsEmpty() &&
      getIpv6Prefix(GET_CHAR(mIpv6Ip), ipv6Prefix)) {
    aOptions.mIPv6Prefix = NS_ConvertUTF8toUTF16(ipv6Prefix);
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16(ipv6Prefix));
    aOptions.mIPv6Routes.AppendElement(NS_ConvertUTF8toUTF16("fe80::/64"));
  }
  aOptions.mLoopIndex = 0;

  static CommandFunc COMMAND_CHAIN[] = {
      stopIpv6Tethering,
      cleanUpStreamInterfaceForwarding,
      defaultAsyncSuccessHandler,
  };

  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

/**
 * Setup NAT64 prefix discovery on the given network.
 */
CommandResult NetworkUtils::setupPrefix64Discovery(NetworkParams& aOptions) {
  static CommandFunc COMMAND_CHAIN[] = {setupPrefix64Discovery,
                                        defaultAsyncSuccessHandler};
  NetIdManager::NetIdInfo netIdInfo;
  if (!mNetIdManager.lookup(aOptions.mIfname, &netIdInfo)) {
    return CommandResult(-1);
  }
  aOptions.mNetId = netIdInfo.mNetId;
  runChain(aOptions, COMMAND_CHAIN, defaultAsyncFailureHandler);
  return CommandResult(CommandResult::Pending());
}

/**
 * callback from NetdUnsolService.
 */
void NetworkUtils::NotifyNetdEvent(
    mozilla::dom::NetworkResultOptions& aResult) {
  postMessage(aResult);
}

void NetworkUtils::dumpParams(NetworkParams& aOptions, const char* aType) {
  NU_DBG("Dump params:");
  NU_DBG("     ifname: %s", GET_CHAR(mIfname));
  NU_DBG("     networktype %ld", GET_FIELD(mNetworkType));
  NU_DBG("     ip: %s", GET_CHAR(mIp));
  NU_DBG("     link: %s", GET_CHAR(mLink));
  NU_DBG("     prefix: %s", GET_CHAR(mPrefix));
  NU_DBG("     wifiStartIp: %s", GET_CHAR(mWifiStartIp));
  NU_DBG("     wifiEndIp: %s", GET_CHAR(mWifiEndIp));
  NU_DBG("     usbStartIp: %s", GET_CHAR(mUsbStartIp));
  NU_DBG("     usbEndIp: %s", GET_CHAR(mUsbEndIp));
  NU_DBG("     internalIfname: %s", GET_CHAR(mInternalIfname));
  NU_DBG("     externalIfname: %s", GET_CHAR(mExternalIfname));
  NU_DBG("     netId: %d", GET_FIELD(mNetId));
  nsTArray<nsString>& dnses = GET_FIELD(mDnses);
  for (uint32_t i = 0; i < dnses.Length(); i++) {
    NS_ConvertUTF16toUTF8 autoDns(dnses[i]);
    NU_DBG("     dnsserver %d: %s", i + 1, autoDns.get());
  }

  if (!strcmp(aType, "WIFI")) {
    NU_DBG("     wifictrlinterfacename: %s", GET_CHAR(mWifictrlinterfacename));
    NU_DBG("     ssid: %s", GET_CHAR(mSsid));
    NU_DBG("     security: %s", GET_CHAR(mSecurity));
    NU_DBG("     key: %s", GET_CHAR(mKey));
  }
  NU_DBG("     preInternalIfname: %s", GET_CHAR(mPreInternalIfname));
  NU_DBG("     preExternalIfname: %s", GET_CHAR(mPreExternalIfname));
  NU_DBG("     curInternalIfname: %s", GET_CHAR(mCurInternalIfname));
  NU_DBG("     curExternalIfname: %s", GET_CHAR(mCurExternalIfname));
  NU_DBG("     ipv6Prefix: %s", GET_CHAR(mIPv6Prefix));
}

void NetworkUtils::updateDebug() {
  ENABLE_NU_DEBUG = mozilla::StaticPrefs::network_debugging_enabled();
  if (gNetdUnsolService) {
    gNetdUnsolService->updateDebug(ENABLE_NU_DEBUG);
  }
  if (gNetdEventListener) {
    gNetdEventListener->updateDebug(ENABLE_NU_DEBUG);
  }
}

#undef GET_CHAR
#undef GET_FIELD
