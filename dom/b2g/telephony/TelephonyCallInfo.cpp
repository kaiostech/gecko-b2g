/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/telephony/TelephonyCallInfo.h"

namespace mozilla {
namespace dom {
namespace telephony {

NS_IMPL_ISUPPORTS(TelephonyCallInfo, nsITelephonyCallInfo)

TelephonyCallInfo::TelephonyCallInfo(
    uint32_t aClientId, uint32_t aCallIndex, uint16_t aCallState,
    uint16_t aVoiceQuality, uint32_t aCapabilities, uint16_t aVideoCallState,
    const nsAString& aDisconnectedReason, const nsAString& aNumber,
    uint16_t aNumberPresentation, const nsAString& aName,
    uint16_t aNamePresentation, uint32_t aRadioTech, bool aIsOutgoing,
    bool aIsEmergency, bool aIsConference, bool aIsSwitchable,
    bool aIsMergeable, bool aIsConferenceParent, bool aIsMarkable,
    uint16_t aRttMode, uint32_t aVowifiQuality, uint32_t aVerStatus)
    : mClientId(aClientId),
      mCallIndex(aCallIndex),
      mCallState(aCallState),
      mVoiceQuality(aVoiceQuality),
      mCapabilities(aCapabilities),
      mVideoCallState(aVideoCallState),
      mDisconnectedReason(aDisconnectedReason),

      mNumber(aNumber),
      mNumberPresentation(aNumberPresentation),
      mName(aName),
      mNamePresentation(aNamePresentation),
      mRadioTech(aRadioTech),
      mIsOutgoing(aIsOutgoing),
      mIsEmergency(aIsEmergency),
      mIsConference(aIsConference),
      mIsSwitchable(aIsSwitchable),
      mIsMergeable(aIsMergeable),
      mIsConferenceParent(aIsConferenceParent),
      mIsMarkable(aIsMarkable),
      mRttMode(aRttMode),
      mVowifiQuality(aVowifiQuality),
      mVerStatus(aVerStatus) {}

NS_IMETHODIMP
TelephonyCallInfo::GetClientId(uint32_t* aClientId) {
  *aClientId = mClientId;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetCallIndex(uint32_t* aCallIndex) {
  *aCallIndex = mCallIndex;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetCallState(uint16_t* aCallState) {
  *aCallState = mCallState;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetDisconnectedReason(nsAString& aDisconnectedReason) {
  aDisconnectedReason = mDisconnectedReason;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetNumber(nsAString& aNumber) {
  aNumber = mNumber;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetNumberPresentation(uint16_t* aNumberPresentation) {
  *aNumberPresentation = mNumberPresentation;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetName(nsAString& aName) {
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetNamePresentation(uint16_t* aNamePresentation) {
  *aNamePresentation = mNamePresentation;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsOutgoing(bool* aIsOutgoing) {
  *aIsOutgoing = mIsOutgoing;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsEmergency(bool* aIsEmergency) {
  *aIsEmergency = mIsEmergency;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsConference(bool* aIsConference) {
  *aIsConference = mIsConference;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsSwitchable(bool* aIsSwitchable) {
  *aIsSwitchable = mIsSwitchable;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsMergeable(bool* aIsMergeable) {
  *aIsMergeable = mIsMergeable;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetVoiceQuality(uint16_t* aVoiceQuality) {
  *aVoiceQuality = mVoiceQuality;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsConferenceParent(bool* aIsConferenceParent) {
  *aIsConferenceParent = mIsConferenceParent;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetIsMarkable(bool* aIsMarkable) {
  *aIsMarkable = mIsMarkable;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetCapabilities(uint32_t* aCapabilities) {
  *aCapabilities = mCapabilities;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetVideoCallState(uint16_t* aVideoCallState) {
  *aVideoCallState = mVideoCallState;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetRadioTech(uint32_t* aRadioTech) {
  *aRadioTech = mRadioTech;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetVowifiCallQuality(uint32_t* aVowifiQuality) {
  *aVowifiQuality = mVowifiQuality;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetRttMode(uint16_t* aRttMode) {
  *aRttMode = mRttMode;
  return NS_OK;
}

NS_IMETHODIMP
TelephonyCallInfo::GetVerStatus(uint32_t* aVerStatus) {
  *aVerStatus = mVerStatus;
  return NS_OK;
}

}  // namespace telephony
}  // namespace dom
}  // namespace mozilla
