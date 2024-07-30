/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum MobileConnectionState {"notSearching", "searching", "denied", "registered"};
enum MobileConnectionType {"gsm", "gprs", "edge", "umts", "hsdpa", "hsupa",
                           "hspa", "hspa+", "is95a", "is95b", "1xrtt", "evdo0",
                           "evdoa", "evdob", "ehrpd", "lte", "tdscdma", "iwlan",
                           "lte_ca", "nr"};

[Pref="dom.mobileconnection.enabled",
 Exposed=Window]
interface MobileConnectionInfo
{
  /**
   * State of the connection.
   */
  readonly attribute MobileConnectionState? state;

  /**
   * Indicates whether the connection is ready.
   *
   * Note: The meaning of "connection ready" for data and voice are different.
   *       - Data: the "default" data connection is established or not.
   *       - Voice: voice is registered to network or not.
   */
  readonly attribute boolean connected;

  /**
   * Indicates whether only emergency calls are possible.
   *
   * This flag is only relevant to voice connections and when 'connected' is
   * false.
   */
  readonly attribute boolean emergencyCallsOnly;

  /**
   * Indicates whether the connection is going through a foreign operator
   * (roaming) or not.
   */
  readonly attribute boolean roaming;

  /**
   * Network operator information.
   */
  readonly attribute DOMMobileNetworkInfo? network;

  /**
   * Type of connection.
   */
  readonly attribute MobileConnectionType? type;

  /**
   * Cell location information.
   */
  readonly attribute MobileCellInfo? cell;

  /**
   * Reason Data Denied.
   *
   * if state is 'denied' this is an enumerated reason why
   * registration was denied. See 3GPP TS 24.008,
   * Annex G.6 "Additional cause codes for GMM".
   * 7 == GPRS services not allowed
   * 8 == GPRS services and non-GPRS services not allowed
   * 9 == MS identity cannot be derived by the network
   * 10 == Implicitly detached
   * 14 == GPRS services not allowed in this PLMN
   * 16 == MSC temporarily not reachable
   * 40 == No PDP context activated
  */

  readonly attribute long reasonDataDenied;

  /**
   * Indicates whether device should show 5G in LTE.
   *
   */
  readonly attribute boolean isNSA5GAvailable;
};
