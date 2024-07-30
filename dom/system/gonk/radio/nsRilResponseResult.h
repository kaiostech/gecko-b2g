/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilResponseResult_H
#define nsRilResponseResult_H

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIRilResponseResult.h"
#include "nsISupports.h"
#include "nsRilResult.h"
#include "nsTArray.h"
#include <nsISupportsImpl.h>

class nsRilResponseResult final
  : public nsRilResult
  , public nsIRilResponseResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  // NS_DECL_ISUPPORTS
  NS_DECL_NSIRILRESPONSERESULT
  // For those has no parameter notify.
  nsRilResponseResult(const nsAString& aRilMessageType,
                      int32_t aRilMessageToken,
                      int32_t aErrorMsg);

  // For DeviceIdentity
  void updateDeviceIdentity(const nsAString& aImei,
                            const nsAString& aImeisv,
                            const nsAString& aEsn,
                            const nsAString& aMeid);

  // For VoiceRadioTechnology
  void updateVoiceRadioTechnology(int32_t aRadioTech);

  // For updateBasebandVersion
  void updateBasebandVersion(const nsAString& aBasebandVersion);

  // For VoiceRegStatus
  void updateVoiceRegStatus(nsVoiceRegState* aVoiceRegState);

  // For DataRegStatus
  void updateDataRegStatus(nsDataRegState* aDataRegState);

  // For Operator
  void updateOperator(nsOperatorInfo* aOperatorInfo);

  // For NetworkSelectionMode
  void updateNetworkSelectionMode(bool aNwModeManual);

  // For SignalStrength
  void updateSignalStrength(nsSignalStrength* aSignalStrength);

  // For GetSmscAddress
  void updateSmscAddress(const nsAString& aSmsc);

  // For getCurrentCalls
  void updateCurrentCalls(nsTArray<RefPtr<nsCall>>& aCalls);

  // For updateLastCallsFailCause
  void updateFailCause(int32_t aCauseCode, const nsAString& aVendorCause);

  // For getPreferredNetworkType
  void updatePreferredNetworkType(int32_t aType);

  // For getAvailableNetwork
  void updateAvailableNetworks(
    nsTArray<RefPtr<nsOperatorInfo>>& aAvailableNetworks);

  // For setupDataCall
  void updateDataCallResponse(nsSetupDataCallResult* aDcResponse);

  // For getDataCallList
  void updateDcList(nsTArray<RefPtr<nsSetupDataCallResult>>& aDcLists);

  // For getCellInfoList
  void updateCellInfoList(nsTArray<RefPtr<nsRilCellInfo>>& aCellInfoLists);

  // For getIMSI
  void updateIMSI(const nsAString& aIMSI);

  // For IccIOForApp
  void updateIccIoResult(nsIccIoResult* aIccIoResult);

  // For getCLIR
  void updateClir(int32_t aN, int32_t aM);

  // For getCallForwardStatus
  void updateCallForwardStatusList(
    nsTArray<RefPtr<nsCallForwardInfo>>& aCallForwardInfoLists);

  // For getCallWaiting
  void updateCallWaiting(bool aEnable, int32_t aServiceClass);

  // For getFacilityLockForApp
  void updateServiceClass(int32_t aServiceClass);

  // For GetClip
  void updateClip(int32_t aProvisioned);

  // For getNeighboringCellIds
  void updateNeighboringCells(
    nsTArray<RefPtr<nsNeighboringCell>>& aNeighboringCell);

  // For quertTtyMode
  void updateTtyMode(int32_t aTtyMode);

  // For getMute
  void updateMute(bool aMuteEnabled);

  // For Icc pin/puk
  void updateRemainRetries(int32_t aRemainingRetries);

  // For GetPreferredVoicePrivacy
  void updateVoicePrivacy(bool aEnhancedVoicePrivacy);

  void updateRadioCapability(nsRadioCapability* aRadioCapability);

  // For sendSms
  void updateSendSmsResponse(nsSendSmsResult* aSendSmsResult);

  void updateModemStackStatusReponse(bool aModemStackEnabled);

  void updateUiccAppEnabledReponse(bool aEanbled);

  void updateAllowedNetworkTypesBitmask(int32_t aAllowedNetworkTypesBitmask);

  void updateIccCardStatus(nsCardStatus* aCardStatus);

  void updateAllowedCarriers(nsAllowedCarriers* aAllowedCarriers);

  void updateVoNREnabled(bool aVoNREnabled);

  void updateKeppAliveStatus(nsIKeepAliveStatus* aStatus);

  void updateSimDepersonalizationResult(
    nsISupplySimDepersonalizationResult* aResult);

  void updateNrDualConnectivityEnabled(bool aEanbled);

  void updateUpdatedRecordIndex(int32_t aIndex);

  void updatePhonebookCapacity(nsIPhonebookCapacity* aParm);

  void updateSlicingConfig(nsISlicingConfig* aConfig);

  void updateAllocatedId(int32_t aId);

  void updateRadioAccessSpecifiers(
    nsTArray<RefPtr<nsIRadioAccessSpecifier>>& aSpecifiers);

  void updateGetBarringInfoResult(nsIGetBarringInfoResult* aInfo);

  void updateSimSlotStatus(nsTArray<RefPtr<nsISimSlotStatus>>& aStatus);

  nsString mImei;
  nsString mImeisv;
  nsString mEsn;
  nsString mMeid;
  int32_t mRadioTech;
  nsString mBasebandVersion;
  RefPtr<nsCardStatus> mCardStatus;
  RefPtr<nsVoiceRegState> mVoiceRegState;
  RefPtr<nsDataRegState> mDataRegState;
  RefPtr<nsOperatorInfo> mOperatorInfo;
  bool mNwModeManual;
  RefPtr<nsSignalStrength> mSignalStrength;
  nsString mSmsc;
  nsTArray<RefPtr<nsCall>> mCalls;
  int32_t mCauseCode;
  nsString mVendorCause;
  int32_t mType;
  nsTArray<RefPtr<nsOperatorInfo>> mAvailableNetworks;
  RefPtr<nsSetupDataCallResult> mDcResponse;
  nsTArray<RefPtr<nsSetupDataCallResult>> mDcLists;
  nsTArray<RefPtr<nsRilCellInfo>> mCellInfoLists;
  nsString mIMSI;
  RefPtr<nsIccIoResult> mIccIoResult;
  int32_t mCLIR_N;
  int32_t mCLIR_M;
  nsTArray<RefPtr<nsCallForwardInfo>> mCallForwardInfoLists;
  bool mCWEnable;
  int32_t mServiceClass;
  int32_t mProvisioned;
  nsTArray<RefPtr<nsNeighboringCell>> mNeighboringCell;
  nsTArray<RefPtr<nsIRadioAccessSpecifier>> mSpecifiers;
  int32_t mTtyMode;
  bool mMuteEnabled;
  int32_t mRemainingRetries;
  int32_t mUpdatedRecordIndex;
  int32_t mAllocatedId;
  bool mEnhancedVoicePrivacy;
  RefPtr<nsIRadioCapability> mRadioCapability;
  RefPtr<nsISendSmsResult> mSendSmsResult;
  bool mModemStackEnabled;
  bool mUiccAppEnabled;
  bool mNrDualConnectivityEnabled;
  int32_t mAllowedNetworkTypesBitmask;
  RefPtr<nsAllowedCarriers> mAllowedCarriers;
  bool mVoNrEnabled;
  RefPtr<nsIKeepAliveStatus> mKeepAliveStatus;
  RefPtr<nsISupplySimDepersonalizationResult> mSimDepersonalizationResult;
  RefPtr<nsIPhonebookCapacity> mPhoneBookCapacity;
  RefPtr<nsISlicingConfig> mSlicingConfig;
  RefPtr<nsIGetBarringInfoResult> mBarringInfoResult;
  nsTArray<RefPtr<nsISimSlotStatus>> mSimSlotStatus;

private:
  ~nsRilResponseResult();
};
#endif // nsRilResponseResult_H
