/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Utilities for managing the dhcpcd DHCP client daemon */

#include "DhcpUtils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "SystemProperty.h"
#include "mozilla/Sprintf.h"

using mozilla::system::Property;

static const char DAEMON_NAME[] = "dhcpcd";
static const char DAEMON_PROP_NAME[] = "init.svc.dhcpcd";
static const char DHCP_PROP_NAME_PREFIX[] = "dhcp";
static const int NAP_TIME = 200; /* wait for 200ms at a time */
                                 /* when polling for property values */
static const char DAEMON_NAME_RENEW[] = "iprenew";
static char errMsg[100] = "\0";
/* interface length for dhcpcd daemon start (dhcpcd_<interface> as defined in
 * init.rc file) or for filling up system properties
 * dhcpcd.<interface>.ipaddress, dhcpcd.<interface>.dns1 and other properties on
 * a successful bind
 */
#define MAX_INTERFACE_LENGTH 25

/*
 * P2p interface names increase sequentially p2p-p2p0-1, p2p-p2p0-2.. after
 * group formation. This does not work well with system properties which can
 * quickly exhaust or for specifiying a dhcp start target in init which requires
 * interface to be pre-defined in init.rc file.
 *
 * This function returns a common string p2p for all p2p interfaces.
 */
void P2pIfaceReplace(const char* interface, char* p2pInterface) {
  // Use p2p for any interface starting with p2p
  if (strncmp(interface, "p2p", 3) == 0) {
    strncpy(p2pInterface, "p2p", MAX_INTERFACE_LENGTH);
  } else {
    strncpy(p2pInterface, interface, MAX_INTERFACE_LENGTH);
  }
}

/*
 * Wait for a system property to be assigned a specified value.
 * If desiredValue is NULL, then just wait for the property to
 * be created with any value. maxWait is the maximum amount of
 * time in seconds to wait before giving up.
 */
static int WaitForProperty(const char* name, const char* desiredValue,
                           int maxWait) {
  char value[Property::VALUE_MAX_LENGTH] = {'\0'};
  int maxNaps = (maxWait * 1000) / NAP_TIME;

  if (maxNaps < 1) {
    maxNaps = 1;
  }

  while (maxNaps-- >= 0) {
    if (Property::Get(name, value, NULL)) {
      if (desiredValue == NULL || strcmp(value, desiredValue) == 0) {
        return 0;
      }
    }
    if (maxNaps >= 0) {
      usleep(NAP_TIME * 1000);
    }
  }
  return -1; /* failure */
}

static int GetIpInfo(const char* interface, char* ipAddr, char* gateway,
                     uint32_t* prefixLength, char* dns[], char* server,
                     uint32_t* lease, char* vendorInfo, char* domain,
                     char* mtu) {
  char propName[Property::KEY_MAX_LENGTH];
  char propValue[Property::VALUE_MAX_LENGTH];
  /* Interface name after converting p2p0-p2p0-X to p2p to reuse system
   * properties */
  char p2pInterface[MAX_INTERFACE_LENGTH];
  int x;

  P2pIfaceReplace(interface, p2pInterface);

  SprintfLiteral(propName, "%s.%s.ipaddress", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);
  Property::Get(propName, ipAddr, NULL);

  SprintfLiteral(propName, "%s.%s.gateway", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);
  Property::Get(propName, gateway, NULL);

  SprintfLiteral(propName, "%s.%s.server", DHCP_PROP_NAME_PREFIX, p2pInterface);
  Property::Get(propName, server, NULL);

  // TODO: Handle IPv6 when we change system property usage
  if (gateway[0] == '\0' || strncmp(gateway, "0.0.0.0", 7) == 0) {
    // DHCP server is our best bet as gateway
    strncpy(gateway, server, Property::VALUE_MAX_LENGTH);
  }

  SprintfLiteral(propName, "%s.%s.mask", DHCP_PROP_NAME_PREFIX, p2pInterface);
  if (Property::Get(propName, propValue, NULL)) {
    int p;
    // this conversion is v4 only, but this dhcp client is v4 only anyway
    in_addr_t mask = ntohl(inet_addr(propValue));
    // Check netmask is a valid IP address.  ntohl gives NONE response (all 1's)
    // for non 255.255.255.255 inputs.  if we get that value check if it is
    // legit..
    if (mask == INADDR_NONE && strcmp(propValue, "255.255.255.255") != 0) {
      SprintfLiteral(errMsg, "DHCP gave invalid net mask %s", propValue);
      return -1;
    }
    for (p = 0; p < 32; p++) {
      if (mask == 0) break;
      // check for non-contiguous netmask, e.g., 255.254.255.0
      if ((mask & 0x80000000) == 0) {
        SprintfLiteral(errMsg, "DHCP gave invalid net mask %s", propValue);
        return -1;
      }
      mask = mask << 1;
    }
    *prefixLength = p;
  }

  for (x = 0; dns[x] != NULL; x++) {
    SprintfLiteral(propName, "%s.%s.dns%d", DHCP_PROP_NAME_PREFIX, p2pInterface,
                   x + 1);
    Property::Get(propName, dns[x], NULL);
  }

  SprintfLiteral(propName, "%s.%s.leasetime", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);
  if (Property::Get(propName, propValue, NULL)) {
    *lease = atol(propValue);
  }

  SprintfLiteral(propName, "%s.%s.vendorInfo", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);
  Property::Get(propName, vendorInfo, NULL);

  SprintfLiteral(propName, "%s.%s.domain", DHCP_PROP_NAME_PREFIX, p2pInterface);
  Property::Get(propName, domain, NULL);

  SprintfLiteral(propName, "%s.%s.mtu", DHCP_PROP_NAME_PREFIX, p2pInterface);
  Property::Get(propName, mtu, NULL);

  return 0;
}

DhcpUtils::DhcpUtils() {}

/*
 * Get any available DHCP results.
 */
int DhcpUtils::GetDhcpResults(const char* interface, char* ipAddr,
                              char* gateway, uint32_t* prefixLength,
                              char* dns[], char* server, uint32_t* lease,
                              char* vendorInfo, char* domain, char* mtu) {
  char resultPropName[Property::KEY_MAX_LENGTH];
  char propValue[Property::VALUE_MAX_LENGTH];

  /* Interface name after converting p2p0-p2p0-X to p2p to reuse system
   * properties */
  char p2pInterface[MAX_INTERFACE_LENGTH];
  P2pIfaceReplace(interface, p2pInterface);
  SprintfLiteral(resultPropName, "%s.%s.result", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);

  memset(propValue, '\0', Property::VALUE_MAX_LENGTH);
  if (!Property::Get(resultPropName, propValue, NULL)) {
    SprintfLiteral(errMsg, "%s", "DHCP result property was not set");
    return -1;
  }
  if (strcmp(propValue, "ok") == 0) {
    if (GetIpInfo(interface, ipAddr, gateway, prefixLength, dns, server, lease,
                  vendorInfo, domain, mtu) == -1) {
      return -1;
    }
    return 0;
  } else {
    SprintfLiteral(errMsg, "DHCP result was %s", propValue);
    return -1;
  }
}

/*
 * Start the dhcp client daemon, and wait for it to finish
 * configuring the interface.
 *
 * The device init.rc file needs a corresponding entry for this work.
 *
 * Example:
 * service dhcpcd_<interface> /system/bin/dhcpcd -ABKL -f dhcpcd.conf
 */
int DhcpUtils::DhcpStart(const char* interface) {
  char resultPropName[Property::KEY_MAX_LENGTH];
  char daemonPropName[Property::KEY_MAX_LENGTH];
  char propValue[Property::VALUE_MAX_LENGTH] = {'\0'};
  char daemonCmd[Property::VALUE_MAX_LENGTH * 2];
  const char* ctrlProp = "ctl.start";
  const char* desiredStatus = "running";
  /* Interface name after converting p2p0-p2p0-X to p2p to reuse system
   * properties */
  char p2pInterface[MAX_INTERFACE_LENGTH];

  P2pIfaceReplace(interface, p2pInterface);

  SprintfLiteral(resultPropName, "%s.%s.result", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);

  SprintfLiteral(daemonPropName, "%s_%s", DAEMON_PROP_NAME, p2pInterface);

  /* Erase any previous setting of the dhcp result property */
  Property::Set(resultPropName, "");

  /* Start the daemon and wait until it's ready */
  SprintfLiteral(daemonCmd, "%s_%s", DAEMON_NAME, p2pInterface);

  memset(propValue, '\0', Property::VALUE_MAX_LENGTH);
  Property::Set(ctrlProp, daemonCmd);
  if (WaitForProperty(daemonPropName, desiredStatus, 10) < 0) {
    SprintfLiteral(errMsg, "%s", "Timed out waiting for dhcpcd to start");
    return -1;
  }

  /* Wait for the daemon to return a result */
  if (WaitForProperty(resultPropName, NULL, 30) < 0) {
    SprintfLiteral(errMsg, "%s", "Timed out waiting for DHCP to finish");
    return -1;
  }

  return 0;
}

/**
 * Stop the DHCP client daemon.
 */
int DhcpUtils::DhcpStop(const char* interface) {
  char resultPropName[Property::KEY_MAX_LENGTH];
  char daemonPropName[Property::KEY_MAX_LENGTH];
  char daemonCmd[Property::VALUE_MAX_LENGTH * 2];
  const char* ctrlProp = "ctl.stop";
  const char* desiredStatus = "stopped";

  char p2pInterface[MAX_INTERFACE_LENGTH];

  P2pIfaceReplace(interface, p2pInterface);

  SprintfLiteral(resultPropName, "%s.%s.result", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);

  SprintfLiteral(daemonPropName, "%s_%s", DAEMON_PROP_NAME, p2pInterface);

  SprintfLiteral(daemonCmd, "%s_%s", DAEMON_NAME, p2pInterface);

  /* Stop the daemon and wait until it's reported to be stopped */
  Property::Set(ctrlProp, daemonCmd);
  if (WaitForProperty(daemonPropName, desiredStatus, 5) < 0) {
    return -1;
  }
  Property::Set(resultPropName, "failed");
  return 0;
}

/**
 * Release the current DHCP client lease.
 */
int DhcpUtils::DhcpRelease(const char* interface) {
  char daemonPropName[Property::KEY_MAX_LENGTH];
  char daemonCmd[Property::VALUE_MAX_LENGTH * 2];
  const char* ctrlProp = "ctl.stop";
  const char* desiredStatus = "stopped";

  char p2pInterface[MAX_INTERFACE_LENGTH];

  P2pIfaceReplace(interface, p2pInterface);

  SprintfLiteral(daemonPropName, "%s_%s", DAEMON_PROP_NAME, p2pInterface);

  SprintfLiteral(daemonCmd, "%s_%s", DAEMON_NAME, p2pInterface);

  /* Stop the daemon and wait until it's reported to be stopped */
  Property::Set(ctrlProp, daemonCmd);
  if (WaitForProperty(daemonPropName, desiredStatus, 5) < 0) {
    return -1;
  }
  return 0;
}

char* DhcpUtils::GetErrMsg() { return errMsg; }

/**
 * The device init.rc file needs a corresponding entry.
 *
 * Example:
 * service iprenew_<interface> /system/bin/dhcpcd -n
 *
 */
int DhcpUtils::DhcpRenew(const char* interface) {
  char resultPropName[Property::KEY_MAX_LENGTH];
  char propValue[Property::VALUE_MAX_LENGTH] = {'\0'};
  char daemonCmd[Property::VALUE_MAX_LENGTH * 2];
  const char* ctrlProp = "ctl.start";

  char p2pInterface[MAX_INTERFACE_LENGTH];

  P2pIfaceReplace(interface, p2pInterface);

  SprintfLiteral(resultPropName, "%s.%s.result", DHCP_PROP_NAME_PREFIX,
                 p2pInterface);

  // Erase any previous setting of the dhcp result property
  Property::Set(resultPropName, "");

  // Start the renew daemon and wait until it's ready
  SprintfLiteral(daemonCmd, "%s_%s:%s", DAEMON_NAME_RENEW, p2pInterface,
                 interface);
  memset(propValue, '\0', Property::VALUE_MAX_LENGTH);
  Property::Set(ctrlProp, daemonCmd);

  // Wait for the daemon to return a result
  if (WaitForProperty(resultPropName, NULL, 30) < 0) {
    SprintfLiteral(errMsg, "%s", "Timed out waiting for DHCP Renew to finish");
    return -1;
  }

  return 0;
}
