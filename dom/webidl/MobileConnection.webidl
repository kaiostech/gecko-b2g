/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

enum MobileNetworkSelectionMode {"automatic", "manual"};
enum MobileRadioState {"enabling", "enabled", "disabling", "disabled"};
enum MobileNetworkType {"gsm", "wcdma", "cdma", "evdo", "lte", "tdscdma", "nr"};
enum MobilePreferredNetworkType {
  "wcdma/gsm",                              //  0
  "gsm",                                    //  1
  "wcdma",                                  //  2
  "wcdma/gsm-auto",                         //  3
  "cdma/evdo",                              //  4
  "cdma",                                   //  5
  "evdo",                                   //  6
  "wcdma/gsm/cdma/evdo",                    //  7
  "lte/cdma/evdo",                          //  8
  "lte/wcdma/gsm",                          //  9
  "lte/wcdma/gsm/cdma/evdo",                // 10
  "lte",                                    // 11
  "lte/wcdma",                              // 12
  "tdscdma",                                // 13
  "wcdma/tdscdma",                          // 14
  "lte/tdscdma",                            // 15
  "tdscdma/gsm",                            // 16
  "lte/tdscdma/gsm",                        // 17
  "wcdma/tdscdma/gsm",                      // 18
  "lte/wcdma/tdscdma",                      // 19
  "lte/wcdma/tdscdma/gsm",                  // 20
  "wcdma/tdscdma/gsm/cdma/evdo",            // 21
  "lte/wcdma/tdscdma/gsm/cdma/evdo",        // 22
  "nr",                                     // 23
  "nr/lte",                                 // 24
  "nr/lte/cdma/evdo",                       // 25
  "nr/lte/wcdma/gsm",                       // 26
  "nr/lte/wcdma/gsm/cdma/evdo",             // 27
  "nr/lte/wcdma",                           // 28
  "nr/lte/tdscdma",                         // 29
  "nr/lte/tdscdma/gsm",                     // 30
  "nr/lte/wcdma/tdscdma",                   // 31
  "nr/lte/wcdma/tdscdma/gsm",               // 32
  "nr/lte/wcdma/tdscdma/gsm/cdma/evdo"      // 33
};

enum MobileRoamingMode {"home", "affiliated", "any"};

enum NrDualConnectivityState {"ENABLE", "DISABLE", "DISABLE_IMMEDIATE"};

[Pref="dom.mobileconnection.enabled",
Exposed=Window]
interface MobileConnection : EventTarget
{
  /**
   * Call forwarding service class.
   *
   * @see 3GPP TS 27.007 7.11 "class".
   */
  const long ICC_SERVICE_CLASS_VOICE      = 0x01; // (1 << 0)
  const long ICC_SERVICE_CLASS_DATA       = 0x02; // (1 << 1)
  const long ICC_SERVICE_CLASS_FAX        = 0x04; // (1 << 2)
  const long ICC_SERVICE_CLASS_SMS        = 0x08; // (1 << 3)
  const long ICC_SERVICE_CLASS_DATA_SYNC  = 0x10; // (1 << 4)
  const long ICC_SERVICE_CLASS_DATA_ASYNC = 0x20; // (1 << 5)
  const long ICC_SERVICE_CLASS_PACKET     = 0x40; // (1 << 6)
  const long ICC_SERVICE_CLASS_PAD        = 0x80; // (1 << 7)
  const long ICC_SERVICE_CLASS_MAX        = 0x80; // (1 << 7)

  /**
   * Call forwarding action.
   *
   * @see 3GPP TS 27.007 7.11 "mode".
   */
  const long CALL_FORWARD_ACTION_DISABLE      = 0;
  const long CALL_FORWARD_ACTION_ENABLE       = 1;
  const long CALL_FORWARD_ACTION_QUERY_STATUS = 2;
  const long CALL_FORWARD_ACTION_REGISTRATION = 3;
  const long CALL_FORWARD_ACTION_ERASURE      = 4;

  /**
   * Call forwarding reason.
   *
   * @see 3GPP TS 27.007 7.11 "reason".
   */
  const long CALL_FORWARD_REASON_UNCONDITIONAL                   = 0;
  const long CALL_FORWARD_REASON_MOBILE_BUSY                     = 1;
  const long CALL_FORWARD_REASON_NO_REPLY                        = 2;
  const long CALL_FORWARD_REASON_NOT_REACHABLE                   = 3;
  const long CALL_FORWARD_REASON_ALL_CALL_FORWARDING             = 4;
  const long CALL_FORWARD_REASON_ALL_CONDITIONAL_CALL_FORWARDING = 5;

  /**
   * Call barring program.
   */
  const long CALL_BARRING_PROGRAM_ALL_OUTGOING                       = 0;
  const long CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL             = 1;
  const long CALL_BARRING_PROGRAM_OUTGOING_INTERNATIONAL_EXCEPT_HOME = 2;
  const long CALL_BARRING_PROGRAM_ALL_INCOMING                       = 3;
  const long CALL_BARRING_PROGRAM_INCOMING_ROAMING                   = 4;
  const long CALL_BARRING_PROGRAM_ALL_SERVICE                        = 5;
  const long CALL_BARRING_PROGRAM_OUTGOING_SERVICE                   = 6;
  const long CALL_BARRING_PROGRAM_INCOMING_SERVICE                   = 7;

  /**
   * Calling line identification restriction constants.
   *
   * @see 3GPP TS 27.007 7.7 Defined values.
   */
  const long CLIR_DEFAULT     = 0;
  const long CLIR_INVOCATION  = 1;
  const long CLIR_SUPPRESSION = 2;

  /**
   * These two fields can be accessed by privileged applications with the
   * 'mobilenetwork' permission.
   */
  [Func="B2G::HasMobileNetworkSupport"]
  readonly attribute DOMString lastKnownNetwork;
  [Func="B2G::HasMobileNetworkSupport"]
  readonly attribute DOMString lastKnownHomeNetwork;

  /**
   * Information about the voice connection.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute MobileConnectionInfo voice;

  /**
   * Information about the data connection.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute MobileConnectionInfo data;

  /**
   * Integrated Circuit Card Identifier of the SIM this mobile connection
   * corresponds to.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute DOMString? iccId;

  /**
   * The current state of emergency callback mode.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  readonly attribute boolean isInEmergencyCbMode;

  /**
   * The selection mode of the voice and data networks.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute MobileNetworkSelectionMode? networkSelectionMode;

  /**
   * The current radio state.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute MobileRadioState? radioState;

  /**
   * Array of network types that are supported by this radio.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  Promise<sequence<MobileNetworkType>> getSupportedNetworkTypes();

  /**
   * The mobile device identities.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  DOMMobileConnectionDeviceIds getDeviceIdentities();

  /**
   * Signal strength information.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute DOMMobileSignalStrength signalStrength;

  /**
   * IMS registration handler.
   *
   * Only available if supported.
   */
  [Func="B2G::HasMobileConnectionSupport"]
  readonly attribute ImsRegHandler? imsHandler;

  /**
   * Search for available networks.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an array of DOMMobileNetworkInfo.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported', or
   * 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getNetworks();

  /**
   * Manually selects the passed in network, overriding the radio's current
   * selection.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   * Note: If the network was actually changed by this request,
   * the 'voicechange' and 'datachange' events will also be fired.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest selectNetwork(DOMMobileNetworkInfo network);

  /**
   * Tell the radio to automatically select a network.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   * Note: If the network was actually changed by this request, the
   * 'voicechange' and 'datachange' events will also be fired.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest selectNetworkAutomatically();

  /**
   * Set preferred network type.
   *
   * @param type
   *        PreferredNetworkType indicates the desired preferred network type.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'ModeNotSupported', 'IllegalSIMorME', or
   * 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setPreferredNetworkType(MobilePreferredNetworkType type);

  /**
   * Query current preferred network type.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a string indicating the current preferred network type.
   * The value will be either 'wcdma/gsm', 'gsm', 'wcdma', 'wcdma/gsm-auto',
   * 'cdma/evdo', 'cdma', 'evdo', 'wcdma/gsm/cdma/evdo', 'lte/cdma/evdo',
   * 'lte/wcdma/gsm', 'lte/wcdma/gsm/cdma/evdo', 'lte' or 'lte/wcdma'.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getPreferredNetworkType();

  /**
   * Set roaming preference.
   *
   * @param mode
   *        RoamingPreferenceMode indicates the desired roaming preference.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setRoamingPreference(MobileRoamingMode mode);

  /**
   * Query current roaming preference.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a string indicating the current roaming preference.
   * The value will be either 'home', 'affiliated', or 'any'.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getRoamingPreference();

  /**
   * Enable or disable VoNr.
   *
   * @param enabled
   *   Enable(true) or disable(false) VONR.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setVoNrEnabled(boolean enabled);

  /**
   * Is VONR enabled
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a boolean to show the current VONR state.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest isVoNrEnabled();

  /**
   * Enable or disable E-UTRA-NR dual connectivity. If disabled then UE will not connect
   * to secondary carrier.
   *
   * @param state
   * nrDualConnectivityState expected NR dual connectivity state.
   *   1. Enable NR dual connectivity {NrDualConnectivityState:ENABLE}
   *   2. Disable NR dual connectivity {NrDualConnectivityState:DISABLE}
   *   3. Disable NR dual connectivity and force secondary cell to be released
   *   {NrDualConnectivityState:DISABLE_IMMEDIATE}
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setNrDualConnectivityState(NrDualConnectivityState state);

  /**
   * Is E-UTRA-NR Dual Connectivity enabled
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a boolean indicating the current roaming preference.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest isNrDualConnectivityEnabled();

  /**
   * Starts a network scan
   *
   * @param scanType
   *        ONE_SHOT(0) or PERIODIC(1).
   * @param interval
   *        Time interval in seconds between the completion of one scan and the start of a subsequent scan.
   *        Only valid when 'scanType' is 'SCAN_PERIODIC'. Range {5,300}.
   * @param maxSearchTime
   *        Maximum duration of the periodic search (in seconds).
   *        Only valid when 'scanType' is 'SCAN_PERIODIC'. Range {60,3600}.
   * @param incrementalResults
   *        Indicates whether the modem must report incremental results of the network scan
   *        to the client.
   * @param incrementalResultsPeriodicity
   *        Indicates the periodicity with which the modem must report incremental results to  the client (in seconds).
   *        Range {1,10}.This value must be less than or equal to maxSearchTime. If incremental results are
   *        not requested, implementations may ignore this value.
   * @param mccMncs
   *        Describes the List of PLMN ids (MCC-MNC).Separate each mccmnc with semicolon(;).
   * @param specifiers
   *        Networks with bands/channels to scan.
   *        The max count is 8.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest startNetworkScan(optional MobileNetworkScan options={});

  /**
   * Set voice privacy preference.
   *
   * @param enabled
   *        Boolean indicates the preferred voice privacy mode used in voice
   *        scrambling in CDMA networks. 'True' means the enhanced voice security
   *        is required.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setVoicePrivacyMode(boolean enabled);

  /**
   * Query current voice privacy mode.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a boolean indicating the current voice privacy mode.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getVoicePrivacyMode();

  /**
   * Configures call forward options.
   *
   * @param options
   *        An object containing the call forward rule to set.
   * @see CallForwardingOptions for the detail of options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setCallForwardingOption(optional CallForwardingOptions options={});

  /**
   * Queries current call forward options.
   *
   * @param serviceClass
   *        Indicates the service for the call forward status want to query.
   *        It should be combination of the MobileConnection.ICC_SERVICE_CLASS_*
   *        For example:
   *        Voice call - MobileConnection.ICC_SERVICE_CLASS_VOICE
   *        Video call - MobileConnection.ICC_SERVICE_CLASS_PACKET |
   *                     MobileConnection.ICC_SERVICE_CLASS_DATA_SYNC
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a CallForwardingOptions object.
   * @see CallForwardingOptions for the detail of result.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getCallForwardingOption(unsigned short reason, unsigned short serviceClass);

  /**
   * Configures call barring options.
   *
   * @param options
   *        An object containing the call barring rule to set.
   * @see CallBarringOptions for the detail of options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure',
   * 'IncorrectPassword'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setCallBarringOption(optional CallBarringOptions options={});

  /**
   * Queries current call barring status.
   *
   * @param options
   *        An object containing the call barring rule to query. No need to
   *        specify 'enabled' property.
   * @see CallBarringOptions for the detail of options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an object of CallBarringOptions with correct 'enabled'
   * property indicating the status of this rule.
   * @see CallBarringOptions for the detail of result.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getCallBarringOption(optional CallBarringOptions options={});

  /**
   * Change call barring facility password.
   *
   * @param options
   *        An object containing information about pin and newPin, and,
   *        this object must have both "pin" and "newPin" attributes
   *        to change the call barring facility password.
   * @see CallBarringOptions for the detail of options.
   *
   * Example:
   *
   *   changeCallBarringPassword({pin: "...",
   *                              newPin: "..."});
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest changeCallBarringPassword(optional CallBarringOptions options={});

  /**
   * Configures call waiting options.
   *
   * @param enabled
   *        Boolean indicates the desired call waiting status.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setCallWaitingOption(boolean enabled);

  /**
   * Queries current call waiting options.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be a boolean indicating the call waiting status.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getCallWaitingOption();

  /**
   * Enables or disables the presentation of the calling line identity (CLI) to
   * the called party when originating a call.
   *
   * @param mode
   *        It shall be one of the MobileConnection.CLIR_* values.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'InvalidParameter', 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setCallingLineIdRestriction(unsigned short mode);

  /**
   * Queries current CLIR status.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called. And the request's
   * result will be an object containing containing CLIR 'n' and 'm' parameter.
   * @see ClirStatus for the detail of result.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest getCallingLineIdRestriction();

  /**
   * Exit emergency callback mode.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest exitEmergencyCbMode();

  /**
   * Set radio enabled/disabled.
   *
   * @param enabled
   *        True to enable the radio.
   *
   * @return a DOMRequest
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'InvalidStateError', 'RadioNotAvailable',
   * 'IllegalSIMorME', or 'GenericFailure'.
   *
   * Note: Request is not available when radioState is null, 'enabling', or
   * 'disabling'. Calling the function in above conditions will receive
   * 'InvalidStateError' error.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest setRadioEnabled(boolean enabled);

  /**
   * Stop network scan.
   *
   * @return a DOMRequest.
   *
   * If successful, the request's onsuccess will be called.
   *
   * Otherwise, the request's onerror will be called, and the request's error
   * will be either 'RadioNotAvailable', 'RequestNotSupported',
   * 'IllegalSIMorME', or 'GenericFailure'.
   */
  [Throws, Func="B2G::HasMobileConnectionSupport"]
  DOMRequest stopNetworkScan();

  /**
   * The 'voicechange' event is notified whenever the voice connection object
   * changes.
   */
  attribute EventHandler onvoicechange;

  /**
   * The 'datachange' event is notified whenever the data connection object
   * changes values.
   */
  attribute EventHandler ondatachange;

  /**
   * The 'dataerror' event is notified whenever the data connection object
   * receives an error from the RIL.
   */
  attribute EventHandler ondataerror;

  /**
   * The 'oncfstatechange' event is notified whenever the call forwarding
   * state changes.
   */
  attribute EventHandler oncfstatechange;

  /**
   * The 'emergencycbmodechange' event is notified whenever the emergency
   * callback mode changes.
   */
  attribute EventHandler onemergencycbmodechange;

  /**
   * The 'onotastatuschange' event is notified whenever the ota provision status
   * changes.
   */
  attribute EventHandler onotastatuschange;

  /**
   * The 'oniccchange' event is notified whenever the iccid value
   * changes.
   */
  attribute EventHandler oniccchange;

  /**
   * The 'onradiostatechange' event is notified whenever the radio state
   * changes.
   */
  attribute EventHandler onradiostatechange;

  /**
   * The 'onclirmodechange' event is notified whenever the mode of the calling
   * line id restriction (CLIR) changes.
   */
  attribute EventHandler onclirmodechange;

  /** The 'onnetworkselectionmodechange' event is notified whenever
   * network selection mode changes.
   */
  attribute EventHandler onnetworkselectionmodechange;

  /**
   * The 'onsignalstrengthchange' event is notified whenever the signal strength
   * value changes.
   */
  attribute EventHandler onsignalstrengthchange;

  /**
   * The 'onmodemrestart' event is notified whenever the modem restarting.
   */
  attribute EventHandler onmodemrestart;

  /**
   * The 'onnetworkscanresult' event is notified whenever the network scan result received.
   */
  attribute EventHandler onnetworkscanresult;
};

[GenerateConversionToJS]
dictionary CallForwardingOptions
{
  /**
   * Call forwarding rule status.
   *
   * It will be either not active (false), or active (true).
   *
   * Note: Unused for setting call forwarding options. It reports
   *       the status of the rule when getting how the rule is
   *       configured.
   *
   * @see 3GPP TS 27.007 7.11 "status".
   */
  boolean? active = null;

  /**
   * Indicates what to do with the rule. It shall be one of the
   * MobileConnection.CALL_FORWARD_ACTION_* values.
   */
  unsigned short? action = null;

  /**
   * Indicates the reason the call is being forwarded. It shall be one of the
   * MobileConnection.CALL_FORWARD_REASON_* values.
   */
  unsigned short? reason = null;

  /**
   * Phone number of forwarding address.
   */
  DOMString? number = null;

  /**
   * When "no reply" is enabled or queried, this gives the time in
   * seconds to wait before call is forwarded.
   */
  unsigned short? timeSeconds = null;

  /**
   * Service for which the call forward is set up. It should be combination of
   * MobileConnection.ICC_SERVICE_CLASS_*. For example:
   * Voice call - MobileConnection.ICC_SERVICE_CLASS_VOICE
   * Video call - MobileConnection.ICC_SERVICE_CLASS_PACKET |
   *              MobileConnection.ICC_SERVICE_CLASS_DATA_SYNC
   */
  unsigned short? serviceClass = null;
};

[GenerateConversionToJS]
dictionary CallBarringOptions
{
  /**
   * Indicates the program the call is being barred. It shall be one of the
   * MobileConnection.CALL_BARRING_PROGRAM_* values.
   */
  unsigned short? program = null;

  /**
   * Enable or disable the call barring program.
   */
  boolean? enabled = null;

  /**
   * Barring password. Use "" if no password specified.
   */
  DOMString? password = null;

  /**
   * Service for which the call barring is set up. It shall be one of the
   * MobileConnection.ICC_SERVICE_CLASS_* values.
   */
  unsigned short? serviceClass = null;

  /**
   * Old call barring password.
   *
   * Note: Only used for changeCallBarringPassword().
   */
  // TODO: Combine this with |password| and rename |newPin| to |newPassword|.
  //       But it needs to modify the gaia side as well, so we could consider
  //       doing this in bug 987541.
  DOMString? pin = null;

  /**
   * New call barring password.
   *
   * Note: Only used for changeCallBarringPassword().
   */
  DOMString? newPin = null;
};

[GenerateConversionToJS]
dictionary MMIResult
{
  /**
   * Indicate whether the result is successful or not.
   */
  boolean success = true;

  /**
   * String key that identifies the service associated with the MMI code
   * request. The UI is supposed to handle the localization of the strings
   * associated with this string key.
   */
  DOMString serviceCode = "";

  /**
   * String key containing the status message of the associated MMI request or
   * the error message when the request fails.

   * The UI is supposed to handle the localization of the strings associated
   * with this string key.
   */
  DOMString statusMessage = "";

  /**
   * Some MMI requests like call forwarding or PIN/PIN2/PUK/PUK2 related
   * requests provide extra information along with the status message, this
   * information can be a number, an array of string keys or an array of
   * CallForwardingOptions.
   *
   * And it should be
   * (unsigned short or sequence<DOMString> or sequence<CallForwardingOptions>)
   * But we cannot yet use sequences as union member types (please see bug 767924)
   * ,so we use object here.
   */
  (unsigned short or object) additionalInformation;
};

[GenerateConversionToJS]
dictionary ClirStatus
{
  /**
   * CLIR parameter 'n': parameter sets the adjustment for outgoing calls.
   *
   * It shall be one of the MobileConnection.CLIR_* values.
   */
  unsigned short n = 0;

  /**
   * CLIR parameter 'm': parameter shows the subscriber CLIR service status in
   *                     the network.
   * (0) CLIR not provisioned.
   * (1) CLIR provisioned in permanent mode.
   * (2) unknown (e.g. no network, etc.).
   * (3) CLIR temporary mode presentation restricted.
   * (4) CLIR temporary mode presentation allowed.
   * m = 1, 3, 4 are treated as enabled.
   *
   * @see 3GPP TS 27.007 7.7 defined values.
   */
  unsigned short m = 0;
};

[GenerateConversionToJS]
dictionary MobileDeviceIds
{
  DOMString imei = "";
  DOMString imeisv = "";
  DOMString esn = "";
  DOMString meid = "";
};

[GenerateConversionToJS]
dictionary MobileRadioAccessSpecifier
{
  unsigned short radioAccessNetwork = 0;
  DOMString bands = "";
  DOMString channels = "";
};

[GenerateConversionToJS]
dictionary MobileNetworkScan
{
  unsigned short? scanType = null;
  unsigned short? interval = null;
  unsigned short? maxSearchTime = null;
  boolean? incrementalResults = null;
  unsigned short? incrementalResultsPeriodicity = null;
  DOMString? mccMncs = null;
  sequence<MobileRadioAccessSpecifier>? specifiers = null;
};
