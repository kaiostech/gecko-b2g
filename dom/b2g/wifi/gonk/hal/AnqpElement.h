/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AnqpElement_H
#define AnqpElement_H

#include "nsTHashMap.h"
#include "WifiCommon.h"

BEGIN_WIFI_NAMESPACE

/**
 * Detail elements to query ANQP data
 */
enum class AnqpElementType : uint32_t {
  ANQPQueryList = 256,
  ANQPVenueName = 258,
  ANQPRoamingConsortium = 261,
  ANQPIPAddrAvailability = 262,
  ANQPNAIRealm = 263,
  ANQP3GPPNetwork = 264,
  ANQPDomainName = 268,
  ANQPVendorSpec = 56797,
  HSQueryList = 1,
  HSFriendlyName = 3,
  HSWANMetrics = 4,
  HSConnCapability = 5,
  HSNAIHomeRealmQuery = 6,
  HSOSUProviders = 8,
  HSIconRequest = 10,
  HSIconFile = 11
};

/**
 * An Authentication parameter, part of the NAI Realm ANQP element, specified in
 * IEEE802.11-2012 section 8.4.4.10, table 8-188
 */
enum class AuthParam : uint32_t {
  ANQPQueryList = 256,
  EXPANDED_EAP_METHOD = 1,
  NON_EAP_INNER_AUTH_TYPE = 2,
  INNER_AUTH_EAP_METHOD_TYPE = 3,
  EXPANDED_INNER_EAP_METHOD = 4,
  CREDENTIAL_TYPE = 5,
  TUNNELED_EAP_METHOD_CREDENTIAL_TYPE = 6,
  VENDOR_SPECIFIC = 221
};

/**
 * Constants for IPv4 availability
 */
enum class V4 : int32_t {
  NOT_AVAILABLE = 0,
  PUBLIC = 1,
  PORT_RESTRICTED = 2,
  SINGLE_NAT = 3,
  DOUBLE_NAT = 4,
  PORT_RESTRICTED_AND_SINGLE_NAT = 5,
  PORT_RESTRICTED_AND_DOUBLE_NAT = 6,
  UNKNOWN = 7
};

/**
 * Constants for IPv6 availability
 */
enum class V6 : int32_t { NOT_AVAILABLE = 0, AVAILABLE = 1, UNKNOWN = 2 };

/**
 * Non-EAP Inner Authentication Type
 */
enum class AuthType : int32_t {
  UNKNOWN = 0,
  PAP = 1,
  CHAP = 2,
  MSCHAP = 3,
  MSCHAPV2 = 4
};

constexpr static uint32_t R1_ANQP_SET[] = {
    (uint32_t)AnqpElementType::ANQPVenueName,
    (uint32_t)AnqpElementType::ANQPIPAddrAvailability,
    (uint32_t)AnqpElementType::ANQPNAIRealm,
    (uint32_t)AnqpElementType::ANQP3GPPNetwork,
    (uint32_t)AnqpElementType::ANQPDomainName};

constexpr static uint32_t R2_ANQP_SET[] = {
    (uint32_t)AnqpElementType::HSFriendlyName,
    (uint32_t)AnqpElementType::HSWANMetrics,
    (uint32_t)AnqpElementType::HSConnCapability,
    (uint32_t)AnqpElementType::HSOSUProviders};

typedef std::vector<uint8_t> VecByte;

typedef nsTHashMap<nsUint32HashKey, VecByte> AnqpResponseMap;

/**
 * Define parser function for each ANQP response items.
 */
typedef void (*AnqpParserFn)(const VecByte& aPayload,
                             nsAnqpResponse* aResponse);

struct AnqpParser {
  static void ParseVenueName(const VecByte& aPayload,
                             nsAnqpResponse* aResponse);
  static void ParseRoamingConsortium(const VecByte& aPayload,
                                     nsAnqpResponse* aResponse);
  static void ParseIpAddrAvailability(const VecByte& aPayload,
                                      nsAnqpResponse* aResponse);
  static void ParseNaiRealm(const VecByte& aPayload, nsAnqpResponse* aResponse);
  static void Parse3gppNetwork(const VecByte& aPayload,
                               nsAnqpResponse* aResponse);
  static void ParseDomainName(const VecByte& aPayload,
                              nsAnqpResponse* aResponse);
  static void ParseHSFriendlyName(const VecByte& aPayload,
                                  nsAnqpResponse* aResponse);
  static void ParseHSWanMetrics(const VecByte& aPayload,
                                nsAnqpResponse* aResponse);
  static void ParseHSConnectionCapability(const VecByte& aPayload,
                                          nsAnqpResponse* aResponse);
  static void ParseHSOsuProviders(const VecByte& aPayload,
                                  nsAnqpResponse* aResponse);

  static std::string ParsePlmn(const VecByte& aPayload);
  static already_AddRefed<nsI18Name> parseI18Name(
      VecByte::const_iterator& aIter);
  static void printBuffer(uint32_t key, const VecByte& buf);
};

/**
 * Define parser function for EAP methods.
 */
struct EapMethodParser {
  static already_AddRefed<nsExpandedEapMethod> ParseExpandedEapMethod(
      VecByte::const_iterator& aIter, int32_t aLength);
  static already_AddRefed<nsNonEapInnerAuth> ParseNonEapInnerAuth(
      VecByte::const_iterator& aIter, int32_t aLength);
  static already_AddRefed<nsInnerAuth> ParseInnerAuthEap(
      VecByte::const_iterator& aIter, int32_t aLength);
  static already_AddRefed<nsCredentialType> ParseCredentialType(
      VecByte::const_iterator& aIter, int32_t aLength);
  static already_AddRefed<nsVendorSpecificAuth> ParseVendorSpecificAuth(
      VecByte::const_iterator& aIter, int32_t aLength);
  static void ParseEapMethod(VecByte::const_iterator& aIter,
                             nsEapMethod* aEapMethod);
};

END_WIFI_NAMESPACE

#endif  // AnqpElement_H
