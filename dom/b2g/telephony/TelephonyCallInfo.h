/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TelephonyCallInfo_h
#define mozilla_dom_TelephonyCallInfo_h

#include "nsITelephonyCallInfo.h"
#include "nsString.h"
#include "mozilla/Attributes.h"

namespace mozilla {
namespace dom {
namespace telephony {

class TelephonyCallInfo final : public nsITelephonyCallInfo {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYCALLINFO

  TelephonyCallInfo(uint32_t aClientId, uint32_t aCallIndex,
                    uint16_t aCallState, uint16_t aVoiceQuality,
                    uint32_t aCapabilities, uint16_t aVideoCallState,
                    const nsAString& aDisconnectedReason,
                    const nsAString& aNumber, uint16_t aNumberPresentation,
                    const nsAString& aName, uint16_t aNamePresentation,
                    uint32_t aRadioTech, bool aIsOutgoing, bool aIsEmergency,
                    bool aIsConference, bool aIsSwitchable, bool aIsMergeable,
                    bool aIsConferenceParent, bool aIsMarkable,
                    uint16_t aRttMode, uint32_t aVowifiQuality,
                    uint32_t aVerStatus);

 private:
  // Don't try to use the default constructor.
  TelephonyCallInfo() {}

  ~TelephonyCallInfo() {}

  uint32_t mClientId;
  uint32_t mCallIndex;
  uint16_t mCallState;
  uint16_t mVoiceQuality;
  uint32_t mCapabilities;
  uint16_t mVideoCallState;
  nsString mDisconnectedReason;

  nsString mNumber;
  uint16_t mNumberPresentation;
  nsString mName;
  uint16_t mNamePresentation;
  uint32_t mRadioTech;

  bool mIsOutgoing;
  bool mIsEmergency;
  bool mIsConference;
  bool mIsSwitchable;
  bool mIsMergeable;
  bool mIsConferenceParent;
  bool mIsMarkable;

  uint16_t mRttMode;
  uint32_t mVowifiQuality;
  uint32_t mVerStatus;
};

}  // namespace telephony
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_TelephonyCallInfo_h
