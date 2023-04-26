/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyIPCSerializer_h
#define mozilla_dom_telephony_TelephonyIPCSerializer_h

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/telephony/TelephonyCallInfo.h"
#include "nsITelephonyCallInfo.h"

using mozilla::AutoJSContext;
using mozilla::dom::telephony::TelephonyCallInfo;

typedef nsITelephonyCallInfo* nsTelephonyCallInfo;

namespace IPC {

/**
 * nsITelephonyCallInfo Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsITelephonyCallInfo*> {
  typedef nsITelephonyCallInfo* paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done.
    if (isNull) {
      return;
    }

    uint32_t clientId;
    uint32_t callIndex;
    uint16_t callState;
    uint16_t voiceQuality;
    uint32_t capabilities;
    uint16_t videoCallState;
    nsString disconnectedReason;

    nsString number;
    uint16_t numberPresentation;
    nsString name;
    uint16_t namePresentation;
    uint32_t radioTech;
    uint32_t vowifiQuality;
    uint16_t rttMode;
    uint32_t verStatus;

    bool isOutgoing;
    bool isEmergency;
    bool isConference;
    bool isSwitchable;
    bool isMergeable;
    bool isConferenceParent;
    bool isMarkable;

    aParam->GetClientId(&clientId);
    aParam->GetCallIndex(&callIndex);
    aParam->GetCallState(&callState);
    aParam->GetVoiceQuality(&voiceQuality);
    aParam->GetCapabilities(&capabilities);
    aParam->GetVideoCallState(&videoCallState);
    aParam->GetDisconnectedReason(disconnectedReason);

    aParam->GetNumber(number);
    aParam->GetNumberPresentation(&numberPresentation);
    aParam->GetName(name);
    aParam->GetNamePresentation(&namePresentation);
    aParam->GetRadioTech(&radioTech);
    aParam->GetVerStatus(&verStatus);

    aParam->GetIsOutgoing(&isOutgoing);
    aParam->GetIsEmergency(&isEmergency);
    aParam->GetIsConference(&isConference);
    aParam->GetIsSwitchable(&isSwitchable);
    aParam->GetIsMergeable(&isMergeable);
    aParam->GetIsConferenceParent(&isConferenceParent);
    aParam->GetIsMarkable(&isMarkable);

    aParam->GetRttMode(&rttMode);
    aParam->GetVowifiCallQuality(&vowifiQuality);

    WriteParam(aWriter, clientId);
    WriteParam(aWriter, callIndex);
    WriteParam(aWriter, callState);
    WriteParam(aWriter, voiceQuality);
    WriteParam(aWriter, capabilities);
    WriteParam(aWriter, videoCallState);
    WriteParam(aWriter, disconnectedReason);

    WriteParam(aWriter, number);
    WriteParam(aWriter, numberPresentation);
    WriteParam(aWriter, name);
    WriteParam(aWriter, namePresentation);
    WriteParam(aWriter, radioTech);
    WriteParam(aWriter, verStatus);

    WriteParam(aWriter, isOutgoing);
    WriteParam(aWriter, isEmergency);
    WriteParam(aWriter, isConference);
    WriteParam(aWriter, isSwitchable);
    WriteParam(aWriter, isMergeable);
    WriteParam(aWriter, isConferenceParent);
    WriteParam(aWriter, isMarkable);

    WriteParam(aWriter, rttMode);
    WriteParam(aWriter, vowifiQuality);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transfered.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    uint32_t clientId;
    uint32_t callIndex;
    uint16_t callState;
    uint16_t voiceQuality;
    uint32_t capabilities;
    uint16_t videoCallState;
    nsString disconnectedReason;

    nsString number;
    uint16_t numberPresentation;
    nsString name;
    uint16_t namePresentation;
    uint32_t radioTech;
    uint32_t verStatus;

    bool isOutgoing;
    bool isEmergency;
    bool isConference;
    bool isSwitchable;
    bool isMergeable;
    bool isConferenceParent;
    bool isMarkable;

    uint16_t rttMode;
    uint32_t vowifiQuality;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &clientId) && ReadParam(aReader, &callIndex) &&
          ReadParam(aReader, &callState) && ReadParam(aReader, &voiceQuality) &&
          ReadParam(aReader, &capabilities) &&
          ReadParam(aReader, &videoCallState) &&
          ReadParam(aReader, &disconnectedReason) &&

          ReadParam(aReader, &number) &&
          ReadParam(aReader, &numberPresentation) &&
          ReadParam(aReader, &name) && ReadParam(aReader, &namePresentation) &&
          ReadParam(aReader, &radioTech) && ReadParam(aReader, &verStatus) &&

          ReadParam(aReader, &isOutgoing) && ReadParam(aReader, &isEmergency) &&
          ReadParam(aReader, &isConference) &&
          ReadParam(aReader, &isSwitchable) &&
          ReadParam(aReader, &isMergeable) &&
          ReadParam(aReader, &isConferenceParent) &&
          ReadParam(aReader, &isMarkable) && ReadParam(aReader, &rttMode) &&
          ReadParam(aReader, &vowifiQuality))) {
      return false;
    }

    nsCOMPtr<nsITelephonyCallInfo> info = new TelephonyCallInfo(
        clientId, callIndex, callState, voiceQuality, capabilities,
        videoCallState, disconnectedReason, number, numberPresentation, name,
        namePresentation, radioTech, isOutgoing, isEmergency, isConference,
        isSwitchable, isMergeable, isConferenceParent, rttMode, vowifiQuality,
        isMarkable, verStatus);

    info.forget(aResult);

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_telephony_TelephonyIPCSerializer_h
