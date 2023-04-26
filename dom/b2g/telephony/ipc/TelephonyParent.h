/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_TelephonyParent_h
#define mozilla_dom_telephony_TelephonyParent_h

#include "mozilla/dom/telephony/TelephonyCommon.h"
#include "mozilla/dom/telephony/PTelephonyParent.h"
#include "mozilla/dom/telephony/PTelephonyRequestParent.h"
#include "nsITelephonyService.h"

BEGIN_TELEPHONY_NAMESPACE

class TelephonyParent : public PTelephonyParent, public nsITelephonyListener {
  friend class PTelephonyParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYLISTENER

  TelephonyParent();

 protected:
  virtual ~TelephonyParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual mozilla::ipc::IPCResult RecvPTelephonyRequestConstructor(
      PTelephonyRequestParent* aActor,
      const IPCTelephonyRequest& aRequest) override;

  PTelephonyRequestParent* AllocPTelephonyRequestParent(
      const IPCTelephonyRequest& aRequest);

  bool DeallocPTelephonyRequestParent(PTelephonyRequestParent* aActor);
  /*
  #ifdef MOZ_WIDGET_GONK
    PVideoCallProviderParent*
    AllocPVideoCallProviderParent(const uint32_t& clientId, const uint32_t&
  callIndex);

    bool
    DeallocPVideoCallProviderParent(PVideoCallProviderParent* aActor);
  #endif
  */
  virtual mozilla::ipc::IPCResult Recv__delete__() override;

  mozilla::ipc::IPCResult RecvRegisterListener();

  mozilla::ipc::IPCResult RecvUnregisterListener();

  mozilla::ipc::IPCResult RecvStartTone(const uint32_t& aClientId,
                                        const nsString& aTone);

  mozilla::ipc::IPCResult RecvStopTone(const uint32_t& aClientId);

  mozilla::ipc::IPCResult RecvGetHacMode(bool* aEnabled);

  mozilla::ipc::IPCResult RecvSetHacMode(const bool& aEnabled);

  mozilla::ipc::IPCResult RecvGetMicrophoneMuted(bool* aMuted);

  mozilla::ipc::IPCResult RecvSetMicrophoneMuted(const bool& aMuted);

  mozilla::ipc::IPCResult RecvGetSpeakerEnabled(bool* aEnabled);

  mozilla::ipc::IPCResult RecvSetSpeakerEnabled(const bool& aEnabled);

  mozilla::ipc::IPCResult RecvGetTtyMode(uint16_t* aEnabled);

  mozilla::ipc::IPCResult RecvSetTtyMode(const uint16_t& aEnabled);

 private:
  Atomic<bool> mActorDestroyed;
  bool mRegistered;
};

class TelephonyRequestParent : public PTelephonyRequestParent,
                               public nsITelephonyListener {
  friend class TelephonyParent;
  friend class PTelephonyRequestParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSITELEPHONYLISTENER

  class Callback : public nsITelephonyCallback {
    friend class TelephonyRequestParent;

   public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSITELEPHONYCALLBACK

   protected:
    explicit Callback(TelephonyRequestParent* aParent) : mParent(aParent) {}
    virtual ~Callback() {}

   private:
    nsresult SendResponse(const IPCTelephonyResponse& aResponse);
    RefPtr<TelephonyRequestParent> mParent;
  };

  class DialCallback final : public Callback, public nsITelephonyDialCallback {
    friend class TelephonyRequestParent;

   public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSITELEPHONYDIALCALLBACK
    NS_FORWARD_NSITELEPHONYCALLBACK(Callback::)

   private:
    explicit DialCallback(TelephonyRequestParent* aParent)
        : Callback(aParent) {}
    ~DialCallback() {}
  };

 protected:
  TelephonyRequestParent();
  virtual ~TelephonyRequestParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult SendResponse(const IPCTelephonyResponse& aResponse);

  Callback* GetCallback() { return mCallback; }

  DialCallback* GetDialCallback() { return mDialCallback; }

 private:
  bool mActorDestroyed;
  RefPtr<Callback> mCallback;
  RefPtr<DialCallback> mDialCallback;
};

END_TELEPHONY_NAMESPACE

#endif /* mozilla_dom_telephony_TelephonyParent_h */
