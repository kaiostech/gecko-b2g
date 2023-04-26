/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyIPCService_h
#define mozilla_dom_telephony_TelephonyIPCService_h

#include "mozilla/dom/telephony/TelephonyCommon.h"
#ifdef MOZ_WIDGET_GONK
// #include "mozilla/dom/videocallprovider/VideoCallProviderChild.h"
#endif
#include "mozilla/Attributes.h"
#include "nsIObserver.h"
#include "nsITelephonyService.h"

BEGIN_TELEPHONY_NAMESPACE

USING_VIDEOCALLPROVIDER_NAMESPACE

class IPCTelephonyRequest;
class PTelephonyChild;

class TelephonyIPCService final : public nsITelephonyService,
                                  public nsITelephonyListener,
                                  public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYSERVICE
  NS_DECL_NSITELEPHONYLISTENER
  NS_DECL_NSIOBSERVER

  TelephonyIPCService();

  void NoteActorDestroyed();

 private:
  ~TelephonyIPCService();

  nsTArray<nsCOMPtr<nsITelephonyListener> > mListeners;
  PTelephonyChild* mPTelephonyChild;
  uint32_t mDefaultServiceId;

  nsresult SendRequest(nsITelephonyListener* aListener,
                       nsITelephonyCallback* aCallback,
                       const IPCTelephonyRequest& aRequest);
  /*
  #ifdef MOZ_WIDGET_GONK
    nsresult GetLoopbackProvider(nsIVideoCallProvider **aProvider);
    void RemoveVideoCallProvider(nsITelephonyCallInfo *aInfo);
    void RemoveVideoCallProvider(uint32_t aClientId, uint32_t aCallIndex);
    void CleanupVideocallProviders();
    void CleanupLoopbackProvider();

    RefPtr<VideoCallProviderChild> mLoopbackProvider;
  #endif
  */
};

END_TELEPHONY_NAMESPACE

#endif  // mozilla_dom_telephony_TelephonyIPCService_h
