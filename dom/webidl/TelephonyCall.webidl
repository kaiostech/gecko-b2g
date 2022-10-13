/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Pref="dom.telephony.enabled",
 Exposed=Window]
interface TelephonyCall : EventTarget {
  // Indicate which service the call comes from.
  readonly attribute unsigned long serviceId;

  readonly attribute TelephonyCallId id;

  // In CDMA networks, the 2nd waiting call shares the connection with the 1st
  // call. We need an additional attribute for the CDMA waiting call.
  readonly attribute TelephonyCallId? secondId;

  readonly attribute TelephonyCallState state;

  // The property "emergency" indicates whether the call number is an emergency
  // number. Only the outgoing call could have a value with true and it is
  // available after dialing state.
  readonly attribute boolean emergency;

  // Indicate whether the call state can be switched between "connected" and
  // "held".
  readonly attribute boolean switchable;

  // Indicate whether the call can be added into TelephonyCallGroup.
  // A RTT call is non-mergeable.
  readonly attribute boolean mergeable;

  readonly attribute DOMException? error;

  readonly attribute TelephonyCallDisconnectedReason? disconnectedReason;

  readonly attribute TelephonyCallGroup? group;

  // Indicate whether the voice quality is Normal or HD(High Definition).
  readonly attribute TelephonyCallVoiceQuality voiceQuality;

  /**
   * Indicate current video call state.
   */
  readonly attribute TelephonyVideoCallState videoCallState;

  /**
   * Indicate current call capabilities.
   */
  readonly attribute TelephonyCallCapabilities capabilities;

  /**
   * To indicate current call's radio tech.
   * ETA 3/24.
   */
  readonly attribute TelephonyCallRadioTech radioTech;

  /**
   * To indicate current voice over wifi call quality.
   */
  readonly attribute TelephonyVowifiQuality vowifiQuality;

  /**
   * To acquire the video call handler which helps app to operate video call related function.
   */
#ifdef MOZ_WIDGET_GONK
  //[Throws]
  //readonly attribute VideoCallProvider? videoCallProvider;
#endif

  /**
   * Indicate whether the call support RTT or not.
   */
  readonly attribute boolean supportRtt;

  /**
   * Current RTT mode.
   */
  readonly attribute TelephonyRttMode rttMode;

  /**
   * Indicate whether the call support mark as robocall.
   */
  readonly attribute boolean markable;

  /**
   * To indicate current call verification status.
   */
  readonly attribute TelephonyVerStatus verStatus;


  /**
   * To answer call with given type.
   * @param type
   *        The call type you are going to answer.
   *        One of Telephony.CALL_TYPE_* values.
   * @param isRtt
   *        To answer it with RTT enabled or not.
   *        We suggest answer as RTT by default.
   */
  [NewObject]
  Promise<undefined> answer(unsigned short type, optional boolean isRtt);

  /**
   * @param unwanted
   *        To reject/hangup current call and mark it as unwanted.
   *        Only applicable when markable is true
   */
  [NewObject]
  Promise<undefined> hangUp(optional boolean unwanted);
  [NewObject]
  Promise<undefined> hold();
  [NewObject]
  Promise<undefined> resume();

  /**
   * To request RTT upgrade/downgrade request.
   */
  [Throws]
  Promise<undefined> sendRttModifyRequest(boolean enable);
  /**
   * To response a RTT request.
   */
  [Throws]
  Promise<undefined> sendRttModifyResponse(boolean status);
  /**
   * To send out a RTT message.
   */
  [Throws]
  Promise<undefined> sendRttMessage(DOMString message);

  attribute EventHandler onstatechange;
  attribute EventHandler ondialing;
  attribute EventHandler onalerting;
  attribute EventHandler onconnected;
  attribute EventHandler ondisconnected;
  attribute EventHandler onheld;
  attribute EventHandler onerror;

  // Fired whenever the group attribute changes.
  attribute EventHandler ongroupchange;
  // Fired whenever the remote request a RTT upgrade request.
  attribute EventHandler onrttmodifyrequest;
  // Fired whenever the remote response to a RTT upgrade request local made.
  attribute EventHandler onrttmodifyresponse;
  // Fired whenever the remote sent RTT message.
  attribute EventHandler onrttmessage;
};

enum TelephonyCallVoiceQuality {
  "Normal",
  "HD"
};

enum TelephonyCallState {
  "dialing",
  "alerting",
  "connected",
  "held",
  "disconnected",
  "incoming",
};

enum TelephonyCallDisconnectedReason {
  "BadNumber",
  "NoRouteToDestination",
  "ChannelUnacceptable",
  "OperatorDeterminedBarring",
  "NormalCallClearing",
  "Busy",
  "NoUserResponding",
  "UserAlertingNoAnswer",
  "CallRejected",
  "NumberChanged",
  "CallRejectedDestinationFeature",
  "PreEmption",
  "DestinationOutOfOrder",
  "InvalidNumberFormat",
  "FacilityRejected",
  "ResponseToStatusEnquiry",
  "Congestion",
  "NetworkOutOfOrder",
  "NetworkTempFailure",
  "SwitchingEquipCongestion",
  "AccessInfoDiscarded",
  "RequestedChannelNotAvailable",
  "ResourceUnavailable",
  "QosUnavailable",
  "RequestedFacilityNotSubscribed",
  "IncomingCallsBarredWithinCug",
  "BearerCapabilityNotAuthorized",
  "BearerCapabilityNotAvailable",
  "BearerNotImplemented",
  "ServiceNotAvailable",
  "IncomingCallExceeded",
  "RequestedFacilityNotImplemented",
  "UnrestrictedBearerNotAvailable",
  "ServiceNotImplemented",
  "InvalidTransactionId",
  "NotCugMember",
  "IncompatibleDestination",
  "InvalidTransitNetworkSelection",
  "SemanticallyIncorrectMessage",
  "InvalidMandatoryInfo",
  "MessageTypeNotImplemented",
  "MessageTypeIncompatibleProtocolState",
  "InfoElementNotImplemented",
  "ConditionalIe",
  "MessageIncompatibleProtocolState",
  "RecoveryOnTimerExpiry",
  "Protocol",
  "Interworking",
  "Barred",
  "FDNBlocked",
  "SubscriberUnknown",
  "DeviceNotAccepted",
  "ModifiedDial",
  "CdmaLockedUntilPowerCycle",
  "CdmaDrop",
  "CdmaIntercept",
  "CdmaReorder",
  "CdmaSoReject",
  "CdmaRetryOrder",
  "CdmaAcess",
  "CdmaPreempted",
  "CdmaNotEmergency",
  "CdmaAccessBlocked",
  "LocalIllegalArgument",
  "LocalIllegalState",
  "LocalIllegalError",
  "LocalImsServiceDown",
  "LocalNoPendingCall",
  "LocalPowerOff",
  "LocalLowBattery",
  "LocalNetowrkNoService",
  "LocalNetworkNoLteCoverage",
  "LocalNetworkRoaming",
  "LocalNetworkIpChange",
  "LocalServiceUnavailable",
  "LocalNotRegistered",
  "LocalMaxCallExceeded",
  "LocalCallDecline",
  "LocalCallVccOnProgressing",
  "LocalCallResourceReservationFailed",
  "LocalCallCsRetryRequired",
  "LocalCallVolteRetryRequired",
  "LocalCallTerminated",
  "LocalCallHoNotFeasible",
  "Timeout1xxWaiting",
  "TimeoutNoAnswer",
  "TimeoutNoAnswerCallUpdate",
  "SipRedirected",
  "SipBadRequest",
  "SipForbidden",
  "SipNotFound",
  "SipNotSupported",
  "SipRequestTimeout",
  "SipTemprarilyUnavailable",
  "SipBadAddress",
  "SipBusy",
  "SipRequestCancelled",
  "SipNotAcceptable",
  "SipNotReachable",
  "SipClientError",
  "SipTransactionDoesNotExist",
  "SipServerInternalError",
  "SipServerUnavailable",
  "SipServerTimeOut",
  "SipServerError",
  "SipUserRejected",
  "SipGlobalError",
  "EmergencyTempFailure",
  "EmergencyPermFailure",
  "MediaInitFailed",
  "MediaNoData",
  "MediaNotAcceptable",
  "MediaUnspecified",
  "UserTerminated",
  "UserNoAnswer",
  "UserIgnore",
  "UserDecline",
  "LowBattery",
  "BlackListedCallId",
  "UserTerminatedByRemote",
  "UtNotSupported",
  "UtServiceUnavailable",
  "UtOperationNotAllowed",
  "UtNetowrkError",
  "UtCbPasswordMismatch",
  "ECBMNotSupported",
  "MultiendpointNotSupported",
  "CallDropIwlanToLteUnavailable",
  "AnswerdElsewhere",
  "CallPullOutOfSync",
  "CallPulled",
  "SuppSvcFailed",
  "SuppSvcCancelled",
  "SuppSvcReinviteCollision",
  "IwlanDpdFailure",
  "EpdgTunnelEstalbishFailure",
  "EpdgTunnelRekeyFailure",
  "EpdgTunnelLostConnection",
  "MaximumNumberOfCallReached",
  "RemoteCallDecline",
  "DataLimitReached",
  "DataDisabled",
  "WifiLost",
  "RetryOnImsWithoutRtt",
  "Unspecified",
};

enum TelephonyVideoCallState {
  // voice only call
  "Voice",
  // video transmitting + voice call
  "TxEnabled",
  // video receiving + voice call
  "RxEnabled",
  // bidirectional video + voice call
  "Bidirectional",
  // video is paused.
  // differs to call state HELD, the voice could still active.
  "Paused"
};

/**
 * Current calls' bearer.
 */
enum TelephonyCallRadioTech {
  // It is over circuit switch
  "cs",
  // It is over packet switch
  "ps",
  // It is over wifi.
  "wifi"
};

/**
 * Current voice over wifi call quality.
 */
enum TelephonyVowifiQuality {
  /**
   * Quality: None
   */
  "none",
  /**
   * Quality: Excellent
   */
  "excellent",
  /**
   * Quality: Fair
   */
  "fair",
  /**
   * Quality: Bad
   */
  "bad"
};

/**
 * Real-Time-Text mode.
 */
enum TelephonyRttMode {
  "off",
  "full"
};

/**
 * Verification status of incoming call
 */
enum TelephonyVerStatus {
  "none", //There's no verification info
  "fail", //Verification fail, this is a robocall
  "pass"  //Verification pass, this is a safe call
};
