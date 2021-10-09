/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "AnqpElement"

#include "AnqpElement.h"
#include <iomanip>
#include <set>

#define IPAVAILABILITY_BUFFER_LENGTH 1
#define WANMETRICS_BUFFER_LENGTH 13
#define VENUE_INFO_LENGTH 2
#define MAXIMUM_I18N_STRING_LENGTH 252
#define LANGUAGE_CODE_LENGTH 3
#define EXPANDED_EAP_BUFFER_LENGTH 7
#define NON_EAP_INNER_AUTH_BUFFER_LENGTH 1
#define INNER_AUTH_BUFFER_LENGTH 1
#define CREDENTIAL_BUFFER_LENGTH 1
#define MAXIMUM_OSU_SSID_LENGTH 32
#define MINIMUM_OSU_PROVIDER_INFO_LENGTH 9

#define IPV4_AVAILABILITY_MASK 0x3F
#define IPV6_AVAILABILITY_MASK 0x3

#define NAI_ENCODING_UTF8_MASK 0x1
#define NAI_REALM_STRING_SEPARATOR ";"

#define IEI_TYPE_PLMN_LIST 0
#define IEI_CONTENT_LENGTH_MASK 0x7F
#define PLMN_DATA_BYTES 3
#define MNC_2DIGIT_VALUE 0xF

#define GUD_VERSION_1 0

#define LINK_STATUS_MASK (1 << 0 | 1 << 1)
#define SYMMETRIC_LINK_MASK (1 << 2)
#define AT_CAPACITY_MASK (1 << 3)

#define VERBOSE_ANQP_DUMP 0

using namespace mozilla;
using namespace mozilla::dom::wifi;

bool gVerboseDump = false;

const static std::set<int32_t> IPV4_AVAILABILITY_SET = {
    (int32_t)V4::NOT_AVAILABLE,
    (int32_t)V4::PUBLIC,
    (int32_t)V4::PORT_RESTRICTED,
    (int32_t)V4::SINGLE_NAT,
    (int32_t)V4::DOUBLE_NAT,
    (int32_t)V4::PORT_RESTRICTED_AND_SINGLE_NAT,
    (int32_t)V4::PORT_RESTRICTED_AND_DOUBLE_NAT,
    (int32_t)V4::UNKNOWN,
};

const static std::set<int32_t> IPV6_AVAILABILITY_SET = {
    (int32_t)V6::NOT_AVAILABLE,
    (int32_t)V6::AVAILABLE,
    (int32_t)V6::UNKNOWN,
};

const static std::map<uint32_t, AnqpParserFn> ANQP_PARSER_TABLE = {
    {(uint32_t)AnqpElementType::ANQPVenueName, AnqpParser::ParseVenueName},
    {(uint32_t)AnqpElementType::ANQPRoamingConsortium,
     AnqpParser::ParseRoamingConsortium},
    {(uint32_t)AnqpElementType::ANQPIPAddrAvailability,
     AnqpParser::ParseIpAddrAvailability},
    {(uint32_t)AnqpElementType::ANQPNAIRealm, AnqpParser::ParseNaiRealm},
    {(uint32_t)AnqpElementType::ANQP3GPPNetwork, AnqpParser::Parse3gppNetwork},
    {(uint32_t)AnqpElementType::ANQPDomainName, AnqpParser::ParseDomainName},
    {(uint32_t)AnqpElementType::HSFriendlyName,
     AnqpParser::ParseHSFriendlyName},
    {(uint32_t)AnqpElementType::HSWANMetrics, AnqpParser::ParseHSWanMetrics},
    {(uint32_t)AnqpElementType::HSConnCapability,
     AnqpParser::ParseHSConnectionCapability},
    {(uint32_t)AnqpElementType::HSOSUProviders,
     AnqpParser::ParseHSOsuProviders}};

const static std::map<int32_t, nsString> AUTH_TYPE_MAP = {
    {(int32_t)AuthType::UNKNOWN, u"UNKNOWN"_ns},
    {(int32_t)AuthType::PAP, u"PAP"_ns},
    {(int32_t)AuthType::CHAP, u"CHAP"_ns},
    {(int32_t)AuthType::MSCHAP, u"MS-CHAP"_ns},
    {(int32_t)AuthType::MSCHAPV2, u"MS-CHAP-V2"_ns}};

/**
 * AnqpParser implementation
 */
void AnqpParser::printBuffer(uint32_t key, const VecByte& data) {
  if (!gVerboseDump) {
    return;
  }

  size_t width = 5;
  size_t bufferSize = data.size();
  size_t lines = bufferSize / width + 1;
  std::ostringstream stream;

  for (size_t line = 0; line < lines; line++) {
    stream << std::hex << std::uppercase;
    for (size_t i = 0; i < width; i++) {
      size_t index = line * width + i;
      if (index == bufferSize) break;
      stream << std::setw(2) << std::setfill('0') << unsigned(data[index]);
      stream << " ";
    }
    WIFI_LOGD(LOG_TAG, "[%d]: %s", key, stream.str().c_str());
    stream.str("");
  }
}

void AnqpParser::ParseVenueName(const VecByte& aPayload,
                                nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::ANQPVenueName, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  // | Venue Info | Venue Name Duple #1 (optional) | ...
  //      2                  variable
  // | Venue Name Duple #N (optional) |
  //             variable

  // Skip Venue Info field
  pos += VENUE_INFO_LENGTH;

  nsTArray<RefPtr<nsI18Name>> venueName;
  // parse each I18Name
  while (pos < aPayload.cend()) {
    RefPtr<nsI18Name> name = parseI18Name(pos);
    if (!name) {
      WIFI_LOGE(LOG_TAG, "Parse I18Name failed");
      return;
    }
    venueName.AppendElement(name);
  }

  aResponse->updateVenueName(venueName);
}

void AnqpParser::ParseRoamingConsortium(const VecByte& aPayload,
                                        nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::ANQPRoamingConsortium, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  // | OI Duple #1 (optional) | ...
  //         variable
  // | OI Length |     OI     |
  //       1        variable
  nsTArray<int64_t> OIs;
  while (pos < aPayload.cend()) {
    size_t length = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

    if (length < sizeof(int8_t) || length > sizeof(int32_t)) {
      WIFI_LOGE(LOG_TAG, "Invalid OI length: %d", length);
      return;
    }

    int64_t OI = ByteToInteger(pos, length, BIG_ENDIAN);
    OIs.AppendElement(OI);
  }

  aResponse->updateRoamingConsortiumOIs(OIs);
}

void AnqpParser::ParseIpAddrAvailability(const VecByte& aPayload,
                                         nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::ANQPIPAddrAvailability, aPayload);
  if (aPayload.size() != IPAVAILABILITY_BUFFER_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid IP availability field length");
    return;
  }

  std::set<int32_t>::const_iterator it;
  VecByte::const_iterator pos = aPayload.cbegin();
  // | IP Address |
  //       1
  // b0                           b7
  // | IPv6 Address | IPv4 Address |
  //     2 bits          6 bits
  int32_t field = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

  int32_t v6Availability = field & IPV6_AVAILABILITY_MASK;
  it = IPV6_AVAILABILITY_SET.find(v6Availability);
  if (it == IPV6_AVAILABILITY_SET.end()) {
    v6Availability = (int32_t)V6::UNKNOWN;
  }

  int32_t v4Availability = field & IPV4_AVAILABILITY_MASK;
  it = IPV4_AVAILABILITY_SET.find(v4Availability);
  if (it == IPV4_AVAILABILITY_SET.end()) {
    v4Availability = (int32_t)V4::UNKNOWN;
  }

  RefPtr<nsIpAvailability> ipAvailability =
      new nsIpAvailability(v4Availability, v6Availability);
  aResponse->updateIpAvailability(ipAvailability);
}

void AnqpParser::ParseNaiRealm(const VecByte& aPayload,
                               nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::ANQPNAIRealm, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();

  if (aPayload.cend() <= pos) {
    return;
  }
  // | NAI Realm Count (optional) | NAI Realm Data #1 (optional) | ....
  //             2                         variable
  nsTArray<RefPtr<nsNAIRealmData>> naiRealms;
  int32_t count = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
  while (count > 0) {
    nsTArray<nsString> realms;
    nsTArray<RefPtr<nsEapMethod>> eapMethods;
    RefPtr<nsNAIRealmData> naiRealm = new nsNAIRealmData();
    // | Len | Encoding | NAIRealm Len | NAIRealm | EAPMethod Count |
    //    2       1            1         variable         1
    // | EAPMethod #1 (optional) |
    //         variable
    int32_t length = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
    if (length > aPayload.cend() - pos) {
      WIFI_LOGE(LOG_TAG, "Invalid nai data length: %d", length);
      return;
    }

    // encoding field
    [[maybe_unused]] bool utf8 =
        (ByteToInteger(pos, 1, LITTLE_ENDIAN) & NAI_ENCODING_UTF8_MASK) != 0;
    // parse realm string
    int32_t realmLen = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    // TODO: should support UTF-8 or ASCII
    std::string realmList = ByteToString(pos, realmLen);

    std::stringstream stream(realmList);
    std::string realm;
    while (getline(stream, realm, ';')) {
      realms.AppendElement(NS_ConvertUTF8toUTF16(realm.c_str()));
    }
    naiRealm->updateRealms(realms);

    // | Length | EAP Method | Auth Param Count | Auth Param #1 (optional) | ...
    //     1          1               1                 variable
    int32_t methodCount = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    while (methodCount > 0) {
      length = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
      if (length > aPayload.cend() - pos) {
        WIFI_LOGE(LOG_TAG, "Invalid auth parameter length: %d", length);
        return;
      }

      int32_t methodID = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
      RefPtr<nsEapMethod> eapMethod = new nsEapMethod(methodID);
      // parse each authentication parameter
      EapMethodParser::ParseEapMethod(pos, eapMethod);

      eapMethods.AppendElement(eapMethod);
      methodCount--;
    }

    naiRealm->updateEapMethods(eapMethods);
    naiRealms.AppendElement(naiRealm);
    count--;
  }

  aResponse->updateNaiRealmList(naiRealms);
}

void AnqpParser::Parse3gppNetwork(const VecByte& aPayload,
                                  nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::ANQP3GPPNetwork, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  // Format:
  // | GUD Version | Length | IEI 1 | ... | IEI N|
  //        1           1    variable
  int32_t gudVersion = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  if (gudVersion != GUD_VERSION_1) {
    WIFI_LOGE(LOG_TAG, "Unsupported GUD version: %d", gudVersion);
    return;
  }

  int32_t length = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  int32_t remaining = aPayload.cend() - pos;
  if (length != remaining) {
    WIFI_LOGE(LOG_TAG, "Mismatch length and buffer size: %d, %d", length,
              remaining);
    return;
  }

  nsTArray<RefPtr<nsCellularNetwork>> networks;
  while (pos < aPayload.cend()) {
    // parse cellular network
    int32_t ieiType = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    int32_t ieiSize =
        ByteToInteger(pos, 1, LITTLE_ENDIAN) & IEI_CONTENT_LENGTH_MASK;

    if (ieiType != IEI_TYPE_PLMN_LIST) {
      WIFI_LOGE(LOG_TAG, "Ignore unsupported IEI Type: %d", ieiType);
      // go on to next IEI
      pos = pos + ieiSize;
      continue;
    }

    int32_t plmnCount = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    if (ieiSize != (plmnCount * PLMN_DATA_BYTES + 1)) {
      WIFI_LOGE(LOG_TAG, "IEI size and PLMN count mismatched");
      return;
    }

    nsTArray<nsString> plmns;
    while (plmnCount > 0) {
      VecByte plmnBuffer(pos, pos + PLMN_DATA_BYTES);
      plmns.AppendElement(NS_ConvertUTF8toUTF16(ParsePlmn(plmnBuffer).c_str()));

      pos += PLMN_DATA_BYTES;
      plmnCount--;
    }

    RefPtr<nsCellularNetwork> plmnList = new nsCellularNetwork();
    plmnList->updatePlmnList(plmns);
    networks.AppendElement(plmnList);
  }

  aResponse->updateCellularNetwork(networks);
}

void AnqpParser::ParseDomainName(const VecByte& aPayload,
                                 nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::ANQPDomainName, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  VecByte::const_iterator end = aPayload.cend();
  // | Domain Name Field #1 (optional) | ...
  //            variable
  nsTArray<nsString> domains;
  while (pos < end) {
    // | Length | Domain Name |
    //    1       variable
    int32_t length = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    std::string name = ByteToString(pos, length);

    domains.AppendElement(NS_ConvertUTF8toUTF16(name.c_str()));
  }

  aResponse->updateDomainName(domains);
}

void AnqpParser::ParseHSFriendlyName(const VecByte& aPayload,
                                     nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::HSFriendlyName, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  // | Operator Name Duple #1 (optional) | ...
  //          variable
  // | Operator Name Duple #N (optional) |
  //          variable
  nsTArray<RefPtr<nsI18Name>> operatorFriendlyName;
  // parse each I18Name
  while (pos < aPayload.cend()) {
    RefPtr<nsI18Name> name = parseI18Name(pos);
    if (!name) {
      WIFI_LOGE(LOG_TAG, "Parse I18Name failed");
      return;
    }
    operatorFriendlyName.AppendElement(name);
  }

  aResponse->updateOperatorFriendlyName(operatorFriendlyName);
}

void AnqpParser::ParseHSWanMetrics(const VecByte& aPayload,
                                   nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::HSWANMetrics, aPayload);
  if (aPayload.size() != WANMETRICS_BUFFER_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid IP availability field length");
    return;
  }

  VecByte::const_iterator pos = aPayload.cbegin();
  // | WAN Info | DL Speed | UL Speed | DL Load | UL Load | LMD |
  //      1          4          4          1         1       2
  // WAN Info Format:
  // | Link Status | Symmetric Link | At Capacity | Reserved |
  //      B0 B1            B2             B3        B4 - B7
  int32_t wanInfo = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  int32_t status = wanInfo & LINK_STATUS_MASK;
  bool symmetric = (wanInfo & SYMMETRIC_LINK_MASK) != 0;
  bool capped = (wanInfo & AT_CAPACITY_MASK) != 0;
  int64_t downlinkSpeed = ByteToInteger(pos, 4, LITTLE_ENDIAN) & 0xFFFFFFFFL;
  int64_t uplinkSpeed = ByteToInteger(pos, 4, LITTLE_ENDIAN) & 0xFFFFFFFFL;
  int32_t downlinkLoad = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  int32_t uplinkLoad = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  int32_t lmd = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;

  RefPtr<nsWanMetrics> wanMetrics =
      new nsWanMetrics(status, symmetric, capped, downlinkSpeed, uplinkSpeed,
                       downlinkLoad, uplinkLoad, lmd);
  aResponse->updateWanMetrics(wanMetrics);
}

void AnqpParser::ParseHSConnectionCapability(const VecByte& aPayload,
                                             nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::HSConnCapability, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  // | ProtoPort Tuple #1 (optiional) | ....
  //                4
  nsTArray<RefPtr<nsProtocolPortTuple>> statusList;
  while (pos < aPayload.cend()) {
    int32_t protocol = ByteToInteger(pos, 1, LITTLE_ENDIAN);
    int32_t port = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
    int32_t status = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

    RefPtr<nsProtocolPortTuple> protocalPortTuple =
        new nsProtocolPortTuple(protocol, port, status);
    statusList.AppendElement(protocalPortTuple);
  }

  aResponse->updateConnectionCapability(statusList);
}

void AnqpParser::ParseHSOsuProviders(const VecByte& aPayload,
                                     nsAnqpResponse* aResponse) {
  printBuffer((uint32_t)AnqpElementType::HSOSUProviders, aPayload);
  VecByte::const_iterator pos = aPayload.cbegin();
  VecByte::const_iterator it;
  // | OSU SSID Length | OSU SSID | Number of OSU Providers | Provider #1 | ...
  //          1          variable             1                 variable
  int32_t ssidLen = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  if (ssidLen > MAXIMUM_OSU_SSID_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid ssid length: %d", ssidLen);
    return;
  }
  std::string ssid = ByteToString(pos, ssidLen);

  // number of OSU providers
  int32_t numProviders = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  // | Length | Friendly Name Len | Friendly Name #1 | ... | Friendly Name #n |
  //     2               2              variable                 variable
  // | Server URI length | Server URI | Method List Length | Method List |
  //          1             variable             1             variable
  // | Icon Available Len | Icon Available | NAI Length | NAI | Desc Length |
  //            2              variable           1     variable     2
  // | Description #1 | ... | Description #n |
  //      variable               variable
  // | Operator Name Duple #N (optional) |
  //             variable
  nsTArray<RefPtr<nsOsuProviderInfo>> providers;
  // parse each OSU provider info
  while (numProviders > 0) {
    // parse length field
    int32_t length = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
    if (length < MINIMUM_OSU_PROVIDER_INFO_LENGTH) {
      WIFI_LOGE(LOG_TAG, "Invalid osu provider info length: %d", length);
      return;
    }

    // parse friendly names
    int32_t friendlyNameLen = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
    VecByte friendlyName(pos, pos + friendlyNameLen);
    it = friendlyName.cbegin();

    nsTArray<RefPtr<nsFriendlyNameMap>> friendlyNameList;
    while (it != friendlyName.cend()) {
      RefPtr<nsI18Name> name = parseI18Name(it);
      if (!name) {
        WIFI_LOGE(LOG_TAG, "Parse I18Name failed");
        return;
      }

      nsString language;
      name->GetLanguage(language);
      nsCString friendlyName;
      name->GetText(friendlyName);

      RefPtr<nsFriendlyNameMap> friendlyNameMap =
          new nsFriendlyNameMap(language, friendlyName);
      friendlyNameList.AppendElement(friendlyNameMap);
    }
    pos += friendlyNameLen;

    // parse server URI
    int32_t uriLen = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    // TODO: should construct object as nsIURI
    std::string uri = ByteToString(pos, uriLen);

    // parse method list
    int32_t methodListLen = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    nsTArray<int32_t> methodList;
    while (methodListLen > 0) {
      int32_t method = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
      methodList.AppendElement(method);
      methodListLen--;
    }

    // parse list of icon info
    int32_t iconLen = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
    VecByte iconInfos(pos, pos + iconLen);
    it = iconInfos.cbegin();
    pos += iconLen;
    // | Width | Height | Language | Type Len | Type | Filename Len | Filename |
    //     2       2         3           1    variable       1        variable
    nsTArray<RefPtr<nsIconInfo>> iconInfoList;
    while (it != iconInfos.cend()) {
      int32_t width = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
      int32_t height = ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;

      std::string language = ByteToString(it, LANGUAGE_CODE_LENGTH);

      int32_t iconTypeLen = ByteToInteger(it, 1, LITTLE_ENDIAN) & 0xFF;
      std::string iconType = ByteToString(it, iconTypeLen);

      // TODO: file name should be UTF-8 format
      int32_t fileNameLen = ByteToInteger(it, 1, LITTLE_ENDIAN) & 0xFF;
      std::string fileName = ByteToString(it, fileNameLen);

      RefPtr<nsIconInfo> iconInfo = new nsIconInfo(
          width, height, NS_ConvertUTF8toUTF16(language.c_str()),
          NS_ConvertUTF8toUTF16(iconType.c_str()), nsCString(fileName.c_str()));
      iconInfoList.AppendElement(iconInfo);
    }

    // parse network access identifier
    int32_t naiLen = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    // TODO: NAI should be UTF-8 format
    std::string nai = ByteToString(pos, naiLen);

    // parse service descriptions
    int32_t serviceDescriptionLen =
        ByteToInteger(pos, 2, LITTLE_ENDIAN) & 0xFFFF;
    VecByte serviceDescription(pos, pos + serviceDescriptionLen);
    it = serviceDescription.cbegin();

    nsTArray<RefPtr<nsI18Name>> serviceDescriptions;
    while (it != serviceDescription.cend()) {
      RefPtr<nsI18Name> description = parseI18Name(it);
      if (!description) {
        WIFI_LOGE(LOG_TAG, "Parse I18Name for service description failed");
        return;
      }
      serviceDescriptions.AppendElement(description);
    }
    pos += serviceDescriptionLen;

    // parse osu provider end, update and go to next
    RefPtr<nsOsuProviderInfo> provider =
        new nsOsuProviderInfo(nsCString(uri.c_str()), nsCString(nai.c_str()));
    provider->updateFriendlyNames(friendlyNameList);
    provider->updateMethodList(methodList);
    provider->updateIconInfoList(iconInfoList);
    provider->updateServiceDescriptions(serviceDescriptions);

    providers.AppendElement(provider);
    numProviders--;
  }

  RefPtr<nsOsuProviders> osuProviders =
      new nsOsuProviders(NS_ConvertUTF8toUTF16(ssid.c_str()));
  osuProviders->updateProviders(providers);
}

std::string AnqpParser::ParsePlmn(const VecByte& aPayload) {
  // b7                         b0
  // | MCC Digit 2 | MCC Digit 1 |
  // | MNC Digit 3 | MCC Digit 3 |
  // | MNC Digit 2 | MNC Digit 1 |
  VecByte::const_iterator pos = aPayload.cbegin();

  // Formatted as | MCC Digit 1 | MCC Digit 2 | MCC Digit 3 |
  int32_t mcc = ((*pos << 8) & 0xF00) | (*pos & 0x0F0) | (*(pos + 1) & 0x00F);

  // Formated as |MNC Digit 1 | MNC Digit 2 |
  int32_t mnc = ((*(pos + 2) << 4) & 0xF0) | ((*(pos + 2) >> 4) & 0x0F);

  // The digit 3 of MNC decides if the MNC is 2 or 3 digits number.  When it is
  // equal to 0xF, MNC is a 2 digit value. Otherwise, it is a 3 digit number.
  int32_t mncDigit3 = (*(pos + 1) >> 4) & 0x0F;

  std::ostringstream strStream;
  if (mncDigit3 != MNC_2DIGIT_VALUE) {
    strStream << std::setw(3) << std::setfill('0') << std::hex << mcc;
    strStream << std::setw(3) << std::setfill('0') << std::hex
              << ((mnc << 4) | mncDigit3);
  } else {
    strStream << std::setw(3) << std::setfill('0') << std::hex << mcc;
    strStream << std::setw(3) << std::setfill('0') << std::hex << mnc;
  }

  return strStream.str();
}

already_AddRefed<nsI18Name> AnqpParser::parseI18Name(
    VecByte::const_iterator& aIter) {
  VecByte::const_iterator& pos = aIter;
  int32_t length = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

  if (length < LANGUAGE_CODE_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid I18name data length: %d", length);
    return nullptr;
  }

  std::string language = ByteToString(pos, LANGUAGE_CODE_LENGTH);
  // TODO: create locale
  std::string locale;
  // TODO: the byte should be in UTF-8 format
  std::string text = ByteToString(pos, length - LANGUAGE_CODE_LENGTH);

  if (text.length() > MAXIMUM_I18N_STRING_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Name text exceeds the maximum value: %d",
              text.length());
    return nullptr;
  }

  RefPtr<nsI18Name> name = new nsI18Name(
      NS_ConvertUTF8toUTF16(language.c_str()),
      NS_ConvertUTF8toUTF16(locale.c_str()), nsCString(text.c_str()));
  return name.forget();
}

/**
 * EapMethodParser implementation
 */
already_AddRefed<nsExpandedEapMethod> EapMethodParser::ParseExpandedEapMethod(
    VecByte::const_iterator& aIter, int32_t aLength) {
  if (aLength != EXPANDED_EAP_BUFFER_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid expanded EAP method buffer length: %d",
              aLength);
    return nullptr;
  }

  VecByte::const_iterator& pos = aIter;
  int32_t vendorID = ByteToInteger(pos, 3, BIG_ENDIAN) & 0xFFFFFF;
  int64_t vendorType = ByteToInteger(pos, 4, BIG_ENDIAN) & 0xFFFFFFFF;

  RefPtr<nsExpandedEapMethod> method =
      new nsExpandedEapMethod(vendorID, vendorType);
  return method.forget();
}

already_AddRefed<nsNonEapInnerAuth> EapMethodParser::ParseNonEapInnerAuth(
    VecByte::const_iterator& aIter, int32_t aLength) {
  if (aLength != NON_EAP_INNER_AUTH_BUFFER_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid non EAP inner auth buffer length: %d", aLength);
    return nullptr;
  }

  VecByte::const_iterator& pos = aIter;
  int32_t type = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

  nsString authType;
  authType = (AUTH_TYPE_MAP.find(type) != AUTH_TYPE_MAP.end())
                 ? AUTH_TYPE_MAP.at(type)
                 : AUTH_TYPE_MAP.at(0);

  RefPtr<nsNonEapInnerAuth> out = new nsNonEapInnerAuth(authType);
  return out.forget();
}

already_AddRefed<nsInnerAuth> EapMethodParser::ParseInnerAuthEap(
    VecByte::const_iterator& aIter, int32_t aLength) {
  if (aLength != INNER_AUTH_BUFFER_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid inner auth buffer length: %d", aLength);
    return nullptr;
  }

  VecByte::const_iterator& pos = aIter;
  int32_t methodId = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

  RefPtr<nsInnerAuth> out = new nsInnerAuth(methodId);
  return out.forget();
}

already_AddRefed<nsCredentialType> EapMethodParser::ParseCredentialType(
    VecByte::const_iterator& aIter, int32_t aLength) {
  if (aLength != CREDENTIAL_BUFFER_LENGTH) {
    WIFI_LOGE(LOG_TAG, "Invalid credential buffer length: %d", aLength);
    return nullptr;
  }

  VecByte::const_iterator& pos = aIter;
  int32_t credType = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
  // CREDENTIAL_TYPE_SIM = 1
  // CREDENTIAL_TYPE_USIM = 2
  // CREDENTIAL_TYPE_NFC = 3
  // CREDENTIAL_TYPE_HARDWARE_TOKEN = 4
  // CREDENTIAL_TYPE_SOFTWARE_TOKEN = 5
  // CREDENTIAL_TYPE_CERTIFICATE = 6
  // CREDENTIAL_TYPE_USERNAME_PASSWORD = 7
  // CREDENTIAL_TYPE_NONE = 8
  // CREDENTIAL_TYPE_ANONYMOUS = 9
  // CREDENTIAL_TYPE_VENDOR_SPECIFIC = 10
  RefPtr<nsCredentialType> credential = new nsCredentialType(credType);
  return credential.forget();
}

already_AddRefed<nsVendorSpecificAuth> EapMethodParser::ParseVendorSpecificAuth(
    VecByte::const_iterator& aIter, int32_t aLength) {
  VecByte::const_iterator& pos = aIter;
  std::string data = ByteToString(pos, aLength);

  RefPtr<nsVendorSpecificAuth> vendor =
      new nsVendorSpecificAuth(NS_ConvertUTF8toUTF16(data.c_str()));
  return vendor.forget();
}

void EapMethodParser::ParseEapMethod(VecByte::const_iterator& aIter,
                                     nsEapMethod* aEapMethod) {
  VecByte::const_iterator& pos = aIter;
  int32_t authCount = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

  RefPtr<nsAuthParams> authParams = new nsAuthParams();
  nsTArray<uint32_t> authTypeIds;
  nsTArray<RefPtr<nsExpandedEapMethod>> expandedEapMethod;
  nsTArray<RefPtr<nsNonEapInnerAuth>> nonEapInnerAuth;
  nsTArray<RefPtr<nsInnerAuth>> innerAuth;
  nsTArray<RefPtr<nsExpandedEapMethod>> expandedInnerEapMethod;
  nsTArray<RefPtr<nsCredentialType>> credential;
  nsTArray<RefPtr<nsCredentialType>> tunneledCredential;
  nsTArray<RefPtr<nsVendorSpecificAuth>> vendorSpecificAuth;

  while (authCount > 0) {
    // | Auth ID | Length | Value |
    //      1         1    variable
    uint32_t authID = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;
    uint32_t length = ByteToInteger(pos, 1, LITTLE_ENDIAN) & 0xFF;

    if (!authTypeIds.Contains(authID)) {
      authTypeIds.AppendElement(authID);
    }

#define INVOKE_EAPMETHOD_PARSER(func, type, list) \
  do {                                            \
    if (func) {                                   \
      RefPtr<type> item = (func)(pos, length);    \
      if (item) {                                 \
        list.AppendElement(item);                 \
      }                                           \
    }                                             \
  } while (0)

    switch (authID) {
      case (uint32_t)AuthParam::EXPANDED_EAP_METHOD:
        INVOKE_EAPMETHOD_PARSER(ParseExpandedEapMethod, nsExpandedEapMethod,
                                expandedEapMethod);
        break;
      case (uint32_t)AuthParam::NON_EAP_INNER_AUTH_TYPE:
        INVOKE_EAPMETHOD_PARSER(ParseNonEapInnerAuth, nsNonEapInnerAuth,
                                nonEapInnerAuth);
        break;
      case (uint32_t)AuthParam::INNER_AUTH_EAP_METHOD_TYPE:
        INVOKE_EAPMETHOD_PARSER(ParseInnerAuthEap, nsInnerAuth, innerAuth);
        break;
      case (uint32_t)AuthParam::EXPANDED_INNER_EAP_METHOD:
        INVOKE_EAPMETHOD_PARSER(ParseExpandedEapMethod, nsExpandedEapMethod,
                                expandedInnerEapMethod);
        break;
      case (uint32_t)AuthParam::CREDENTIAL_TYPE:
        INVOKE_EAPMETHOD_PARSER(ParseCredentialType, nsCredentialType,
                                credential);
        break;
      case (uint32_t)AuthParam::TUNNELED_EAP_METHOD_CREDENTIAL_TYPE:
        INVOKE_EAPMETHOD_PARSER(ParseCredentialType, nsCredentialType,
                                tunneledCredential);
        break;
      case (uint32_t)AuthParam::VENDOR_SPECIFIC:
        INVOKE_EAPMETHOD_PARSER(ParseVendorSpecificAuth, nsVendorSpecificAuth,
                                vendorSpecificAuth);
        break;
      default:
        WIFI_LOGE(LOG_TAG, "Unknown auth type ID: %d", authID);
        return;
    }

#undef INVOKE_EAPMETHOD_PARSER

    authCount--;
  }

  authParams->updateAuthTypeId(authTypeIds);
  authParams->updateExpandedEapMethod(expandedEapMethod);
  authParams->updateNonEapInnerAuth(nonEapInnerAuth);
  authParams->updateInnerAuth(innerAuth);
  authParams->updateExpandedInnerEapMethod(expandedInnerEapMethod);
  authParams->updateCredential(credential);
  authParams->updateTunneledCredential(tunneledCredential);
  authParams->updateVendorSpecificAuth(vendorSpecificAuth);
  aEapMethod->updateAuthParams(authParams);
}
