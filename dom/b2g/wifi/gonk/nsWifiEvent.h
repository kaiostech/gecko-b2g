/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWifiEvent_H
#define nsWifiEvent_H

#include "nsAnqpResponse.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsIWifiEvent.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsWifiElement.h"

class nsWifiEvent final : public nsIWifiEvent {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWIFIEVENT

  nsWifiEvent();
  explicit nsWifiEvent(const nsAString& aName);

  void updateStateChanged(nsStateChanged* aStateChanged);
  void updateGsmRands(const nsTArray<nsString>& aGsmRands);
  void updateAnqpResponse(nsAnqpResponse* aAnqpResponse);

  nsString mName;
  nsString mBssid;
  bool mLocallyGenerated;
  uint32_t mReason;
  uint32_t mStatusCode;
  uint32_t mNumStations;
  int32_t mErrorCode;
  bool mTimeout;
  RefPtr<nsStateChanged> mStateChanged;
  nsString mRand;
  nsString mAutn;
  nsTArray<nsString> mGsmRands;
  nsString mAnqpNetworkKey;
  RefPtr<nsAnqpResponse> mAnqpResponse;
  uint16_t mWpsConfigError;
  uint16_t mWpsErrorIndication;

 private:
  ~nsWifiEvent() {}
};

#endif  // nsWifiEvent_H
