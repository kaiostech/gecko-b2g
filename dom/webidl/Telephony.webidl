/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * The possible values of TTY mode.
 *
 * "off"  - indicating TTY mode is disabled.
 * "full" - indicating both hearing carryover and voice carryover are enabled.
 * "hco"  - indicating hearing carryover is enabled.
 * "vco"  - indicating voice carryover is enabled.
 */
enum TtyMode { "off", "full", "hco", "vco" };

[Pref="dom.telephony.enabled",
 Exposed=Window]
interface Telephony : EventTarget {
  /**
   * There are multiple telephony services in multi-sim architecture. We use
   * |serviceId| to indicate the target telephony service. If not specified,
   * the implementation MUST use the default service.
   *
   * Possible values of |serviceId| are 0 ~ (number of services - 1), which is
   * simply the index of a service. Get number of services by acquiring
   * |navigator.mozMobileConnections.length|.
   */

  /**
   * The call types
   */
  const unsigned short CALL_TYPE_VOICE_N_VIDEO = 1;
  const unsigned short CALL_TYPE_VOICE = 2;
  const unsigned short CALL_TYPE_VIDEO_N_VOICE = 3;
  const unsigned short CALL_TYPE_VT = 4;
  const unsigned short CALL_TYPE_VT_TX = 5;
  const unsigned short CALL_TYPE_VT_RX = 6;
  const unsigned short CALL_TYPE_VT_NODIR = 7;
  const unsigned short CALL_TYPE_VS = 8;
  const unsigned short CALL_TYPE_VS_TX = 9;
  const unsigned short CALL_TYPE_VS_RX = 10;

  /**
   * Make a phone call with specific type or send the mmi code depending on the number provided.
   * @param type
   *        The call type you are going to dial.
   *        One of Telephony.CALL_TYPE_* values.
   * @param isRtt
   *        Is it a Real-Time-Text call.
   *        RTT call is only applicable to LTE only.
   * TelephonyCall - for call setup
   * MMICall - for MMI code
   */
  [Throws]
  Promise<(TelephonyCall or MMICall)> dial(DOMString number, unsigned short type,
                                           boolean isRtt,
                                           optional unsigned long serviceId);

  [Throws]
  /**
   * @param isRtt
   *        Is it a Real-Time-Text emergency call or not.
   *        RTT call is only applicable to LTE only.
   */
  Promise<TelephonyCall> dialEmergency(DOMString number, boolean isRtt, optional unsigned long serviceId);

  /**
   * Hangup all calls.
   */
  [Throws]
  Promise<undefined> hangUpAllCalls(optional unsigned long serviceId);

  /**
   * Send a series of DTMF tones.
   *
   * @param tones
   *    DTMF chars.
   * @param pauseDuraton (ms) [optional]
   *    Time to wait before sending tones. Default value is 3000 ms.
   * @param toneDuration (ms) [optional]
   *    Duration of each tone. Default value is 70 ms.
   * @param serviceId [optional]
   *    Default value is as user setting dom.telephony.defaultServiceId.
   */
  [Throws]
  Promise<undefined> sendTones(DOMString tones, optional unsigned long pauseDuration = 3000, optional unsigned long toneDuration = 70, optional unsigned long serviceId);

  [Throws]
  undefined startTone(DOMString tone, optional unsigned long serviceId);

  [Throws]
  undefined stopTone(optional unsigned long serviceId);

  // Calling this method, the app will be treated as owner of the telephony
  // calls from the AudioChannel policy.
  [Throws]
  // CheckAllPermissions="audio-channel-telephony"]
  undefined ownAudioChannel();

  [NewObject]
  Promise<undefined> sendUSSD(DOMString ussd, optional unsigned long serviceId);

  [NewObject]
  Promise<undefined> cancelUSSD(optional unsigned long serviceId);

  /**
   * Test purpose.
   * To test loopback mode, please setup preference telephony.vt.loopback.enabled properly.
   */
#ifdef MOZ_WIDGET_GONK
  //[Pref="telephony.vt.loopback.enabled"]
  //readonly attribute VideoCallProvider? loopbackProvider;
#endif

  /**
   * Return ECC list.
   *
   * @param serviceId [optional]
   *    serviceId that client is instereted to. If no specify, default service
   *    Id that user select from settings is used.
   *
   * @return a DOMString, ex: 112,911.
   */
  [Throws]
  Promise<DOMString> getEccList(optional unsigned long serviceId);

  [Throws]
  attribute boolean hacMode;

  [Throws]
  attribute boolean muted;

  [Throws]
  attribute boolean speakerEnabled;

  [Throws]
  attribute TtyMode ttyMode;

  readonly attribute (TelephonyCall or TelephonyCallGroup)? active;

  // A call is contained either in Telephony or in TelephonyCallGroup.
  readonly attribute CallsList calls;
  readonly attribute TelephonyCallGroup conferenceGroup;

  // Async notification that object initialization is done.
  [Throws]
  readonly attribute Promise<undefined> ready;

  attribute EventHandler onincoming;
  attribute EventHandler oncallschanged;
  attribute EventHandler onringbacktone;
  attribute EventHandler onsuppservicenotification;
  attribute EventHandler onttymodereceived;
  attribute EventHandler ontelephonycoveragelosing;
};
