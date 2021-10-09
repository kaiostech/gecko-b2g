/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsWifiResult.h"

#define WIFIRESULT_CID                               \
  {                                                  \
    0x5c610641, 0x1da9, 0x4741, {                    \
      0xa2, 0x6a, 0x8d, 0xff, 0x3c, 0x5e, 0x55, 0xf5 \
    }                                                \
  }

/**
 * nsWifiResult
 */
NS_IMPL_ISUPPORTS(nsWifiResult, nsIWifiResult)

nsWifiResult::nsWifiResult() : mId(0), mStatus(false) {}

void nsWifiResult::updateWifiConfiguration(nsWifiConfiguration* aWifiConfig) {
  mWifiConfig = aWifiConfig;
}

void nsWifiResult::updateScanResults(
    const nsTArray<RefPtr<nsScanResult>>& aScanResults) {
  mScanResults = aScanResults.Clone();
}

void nsWifiResult::updateChannels(const nsTArray<int32_t>& aChannels) {
  mChannels = aChannels.Clone();
}

void nsWifiResult::updateSignalPoll(const nsTArray<int32_t>& aSignalPoll) {
  mSignalPoll = aSignalPoll.Clone();
}

void nsWifiResult::updateLinkLayerStats(nsLinkLayerStats* aLinkLayerStats) {
  mLinkLayerStats = aLinkLayerStats;
}

NS_IMETHODIMP
nsWifiResult::GetId(uint32_t* aId) {
  *aId = mId;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetStatus(uint32_t* aStatus) {
  *aStatus = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetDriverVersion(nsAString& aDriverVersion) {
  aDriverVersion = mDriverVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetFirmwareVersion(nsAString& aFirmwareVersion) {
  aFirmwareVersion = mFirmwareVersion;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetMacAddress(nsAString& aMacAddress) {
  aMacAddress = mMacAddress;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetStaInterface(nsAString& aStaInterface) {
  aStaInterface = mStaInterface;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetApInterface(nsAString& aApInterface) {
  aApInterface = mApInterface;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetSupportedFeatures(uint32_t* aSupportedFeatures) {
  *aSupportedFeatures = mSupportedFeatures;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetDebugLevel(uint32_t* aDebugLevel) {
  *aDebugLevel = mDebugLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetNumStations(uint32_t* aNumStations) {
  *aNumStations = mNumStations;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetUsageFlag(uint16_t* aUsageFlag) {
  *aUsageFlag = mUsageFlag;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetNickname(nsAString& aNickname) {
  aNickname = mNickname;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetDuplicated(bool* aDuplicated) {
  *aDuplicated = mDuplicated;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetGeneratedPin(nsAString& aGeneratedPin) {
  aGeneratedPin = mGeneratedPin;
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetWifiConfig(nsIWifiConfiguration** aWifiConfig) {
  RefPtr<nsIWifiConfiguration> configuration(mWifiConfig);
  configuration.forget(aWifiConfig);
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetChannels(nsTArray<int32_t>& aChannels) {
  aChannels = mChannels.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::SignalPoll(nsTArray<int32_t>& aSignalPoll) {
  aSignalPoll = mSignalPoll.Clone();
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetScanResults(nsTArray<RefPtr<nsIScanResult>>& aScanResults) {
  for (uint32_t i = 0; i < mScanResults.Length(); i++) {
    aScanResults.AppendElement(mScanResults[i]);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWifiResult::GetLinkLayerStats(nsILinkLayerStats** aLinkLayerStats) {
  RefPtr<nsILinkLayerStats> stats(mLinkLayerStats);
  stats.forget(aLinkLayerStats);
  return NS_OK;
}

NS_DEFINE_NAMED_CID(WIFIRESULT_CID);
