/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilIndicationResult_H
#define nsRilIndicationResult_H

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIRilIndicationResult.h"
#include "nsISupports.h"
#include "nsRilResult.h"
#include "nsTArray.h"
#include <nsISupportsImpl.h>

class nsRilIndicationResult final
  : public nsRilResult
  , public nsIRilIndicationResult
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  // NS_DECL_ISUPPORTS
  NS_DECL_NSIRILINDICATIONRESULT
  // For those has no parameter notify.
  explicit nsRilIndicationResult(const nsAString& aRilMessageType);
  // For radioStateChanged
  void updateRadioStateChanged(int32_t aRadioState);
  // For newSms
  void updateOnSms(nsTArray<int32_t>& aPdu);
  // For newSmsOnSim
  void updateNewSmsOnSim(int32_t aRecordNumber);
  // For onUssd
  void updateOnUssd(int32_t aTypeCode, const nsAString& aMessage);
  // For nitzTimeReceived
  void updateNitzTimeReceived(const nsAString& aDateString,
                              int64_t aReceiveTimeInMS);
  // For currentSignalStrength
  void updateCurrentSignalStrength(nsSignalStrength* aSignalStrength);
  // For dataCallListChanged
  void updateDataCallListChanged(
    nsTArray<RefPtr<nsSetupDataCallResult>>& aDatacalls);
  // For suppSvcNotify
  void updateSuppSvcNotify(nsSuppSvcNotification* aSuppSvc);
  // For stkProactiveCommand & stkEventNotify
  void updateStkProactiveCommand(const nsAString& aCmd);
  // For stkEventNotify
  void updateStkEventNotify(const nsAString& aCmd);
  // For stkCallSetup
  void updateStkCallSetup(int32_t aTimeout);
  // For simRefresh
  void updateSimRefresh(nsSimRefreshResult* aRefreshResult);
  // For callRing
  void updateCallRing(bool aIsGsm);
  // For newBroadcastSms
  void updateNewBroadcastSms(nsTArray<int32_t>& aData);
  // For restrictedStateChanged
  void updateRestrictedStateChanged(int32_t aRestrictedState);
  // For indicateRingbackTone
  void updateIndicateRingbackTone(bool aPlayRingbackTone);
  // For voiceRadioTechChanged
  void updateVoiceRadioTechChanged(int32_t aRat);
  // For cellInfoList
  void updateCellInfoList(nsTArray<RefPtr<nsRilCellInfo>>& aRecords);
  // For subscriptionStatusChanged
  void updateSubscriptionStatusChanged(bool aActivate);
  // For srvccStateNotify
  void updateSrvccStateNotify(int32_t aSrvccState);
  // For hardwareConfigChanged
  void updateHardwareConfigChanged(
    nsTArray<RefPtr<nsHardwareConfig>>& aConfigs);
  // For radioCapabilityIndication
  void updateRadioCapabilityIndication(nsRadioCapability* aRc);
  // For stkCallControlAlphaNotify
  void updateStkCallControlAlphaNotify(const nsAString& aAlpha);
  // For lceData
  void updateLceData(nsILceDataInfo* aLce);
  // For pcoData
  void updatePcoData(nsIPcoDataInfo* aPco);
  // For modemReset
  void updateModemReset(const nsAString& aReason);
  // For currentLinkCapacityEstimate
  void updateCurrentLinkCapacityEstimate(nsLinkCapacityEstimate* aClce);
  // For physicalChannelConfig
  void updatePhysicalChannelConfig(
    nsTArray<RefPtr<nsPhysicalChannelConfig>>& aPcc);
  // For EmergencyNumber
  void updateCurrentEmergencyNumberList(
    nsTArray<RefPtr<nsEmergencyNumber>>& aEmergNums);
  // For AllowedCarriers
  void updateAllowedCarriers(nsAllowedCarriers* aAllowedCarriers);
  // For NetworkScan
  void updateScanResult(nsNetworkScanResult* aNetworkScanResult);
  // For networksice
  void updateSlicingConfig(nsSlicingConfig* aSlicingConfig);
  // For UiccApplications whether enabled or not
  void updateUiccApplicationsEnabled(bool aUiccApplicationsEnabled);
  // For registrationFailed
  void updateRegistrationFailed(nsRegistrationFailedEvent* aRegFailedEvent);
  // For barringInfoChanged
  void updateBarringInfoChanged(nsBarringInfoChanged* aBarringInfoChangedEvent);
  // For unthrottleApn
  void updateUnthrottleApn(const nsAString& aUnthrottleApn);
  // For simPhonebookRecordsReceived
  void updateSimPhonebookRecords(
    nsSimPhonebookRecordsEvent* aSimPBRecordsEvent);
  void updateSimSlotStatus(nsTArray<RefPtr<nsISimSlotStatus>>&);

  int32_t mRadioState;
  int32_t mRecordNumber;
  int32_t mTypeCode;
  nsString mMessage;
  nsString mDateString;
  int64_t mReceiveTimeInMS;
  RefPtr<nsSignalStrength> mSignalStrength;
  nsTArray<RefPtr<nsSetupDataCallResult>> mDatacalls;
  RefPtr<nsSuppSvcNotification> mSuppSvc;
  nsString mCmd;
  int32_t mTimeout;
  RefPtr<nsSimRefreshResult> mRefreshResult;
  bool mIsGsm;
  nsTArray<int32_t> mData;
  int32_t mRestrictedState;
  bool mPlayRingbackTone;
  int32_t mRadioTech;
  nsTArray<RefPtr<nsRilCellInfo>> mRecords;
  bool mActivate;
  int32_t mSrvccState;
  nsTArray<RefPtr<nsHardwareConfig>> mConfigs;
  RefPtr<nsRadioCapability> mRc;
  nsString mAlpha;
  nsCOMPtr<nsILceDataInfo> mLce;
  nsCOMPtr<nsIPcoDataInfo> mPco;
  nsString mReason;
  nsTArray<int32_t> mPdu;
  RefPtr<nsLinkCapacityEstimate> mClce;
  nsTArray<RefPtr<nsPhysicalChannelConfig>> mPccs;
  nsTArray<RefPtr<nsEmergencyNumber>> mEmergNums;
  RefPtr<nsAllowedCarriers> mAllowedCarriers;
  RefPtr<nsNetworkScanResult> mNetworkScanResult;
  bool mUiccApplicationsEnabled;
  RefPtr<nsRegistrationFailedEvent> mRegFailedEvent;
  RefPtr<nsBarringInfoChanged> mBarringInfoChangedEvent;
  nsString mUnthrottleApn;
  RefPtr<nsSimPhonebookRecordsEvent> mSimPBRecordsEvent;
  RefPtr<nsSlicingConfig> mSlicingConfig;
  nsTArray<RefPtr<nsISimSlotStatus>> mSimSlotStatus;

private:
  ~nsRilIndicationResult();
};
#endif // nsRilIndicationResult_H
