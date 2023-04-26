/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_ImsRegistrationChild_h
#define mozilla_dom_mobileconnection_ImsRegistrationChild_h

#include "mozilla/dom/mobileconnection/PImsRegistrationChild.h"
#include "mozilla/dom/mobileconnection/PImsRegistrationRequestChild.h"
#include "mozilla/dom/mobileconnection/PImsRegServiceFinderChild.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

/**
 * Child actor of PImsRegistration. The object is created by
 * ImsRegIPCService and destroyed after ImsRegIPCService is
 * shutdown. For multi-sim device, more than one instance will
 * be created and each instance represents the ImsRegHandler per sim slot.
 */
class ImsRegistrationChild final : public PImsRegistrationChild,
                                   public nsIImsRegHandler {
  friend class PImsRegistrationChild;
  // class PImsRegServiceFinderChild;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGHANDLER

  explicit ImsRegistrationChild(uint32_t aServiceId);

  void Init();

  void Shutdown();

 private:
  ImsRegistrationChild() = delete;

  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~ImsRegistrationChild() {}

 public:
  // protected:
  bool SendRequest(const ImsRegistrationRequest& aRequest,
                   nsIImsRegCallback* aCallback);

  virtual void ActorDestroy(ActorDestroyReason why) override;

  PImsRegistrationRequestChild* AllocPImsRegistrationRequestChild(
      const ImsRegistrationRequest& request);

  bool DeallocPImsRegistrationRequestChild(
      PImsRegistrationRequestChild* aActor);

  mozilla::ipc::IPCResult RecvNotifyEnabledStateChanged(const bool& aEnabled);

  mozilla::ipc::IPCResult RecvNotifyPreferredProfileChanged(
      const uint16_t& aProfile);

  mozilla::ipc::IPCResult RecvNotifyImsCapabilityChanged(
      const int16_t& aCapability, const nsString& UnregisteredReason);
  mozilla::ipc::IPCResult RecvNotifyRttEnabledStateChanged(
      const bool& aEnabled);

 private:
  bool mLive;
  nsCOMArray<nsIImsRegListener> mListeners;
  bool mEnabled;
  bool mRttEnabled;
  uint16_t mPreferredProfile;
  int16_t mCapability;
  nsString mUnregisteredReason;
  nsTArray<uint16_t> mSupportedBearers;
};

/******************************************************************************
 * PImsRegistrationRequestChild
 ******************************************************************************/

/**
 * Child actor of PImsRegistrationRequest. The object is created when an
 * asynchronous request is made and destroyed after receiving the response sent
 * by parent actor.
 */
class ImsRegistrationRequestChild : public PImsRegistrationRequestChild {
  friend class PImsRegistrationRequestChild;

 public:
  explicit ImsRegistrationRequestChild(nsIImsRegCallback* aRequestCallback)
      : mRequestCallback(aRequestCallback) {
    MOZ_ASSERT(mRequestCallback);
  }

  bool DoReply(const ImsRegistrationReplySuccess& aReply);

  bool DoReply(const ImsRegistrationReplyError& aReply);

  // protected:
  ~ImsRegistrationRequestChild() {}

  virtual void ActorDestroy(ActorDestroyReason why) override;

  mozilla::ipc::IPCResult Recv__delete__(const ImsRegistrationReply& aReply);

 private:
  nsCOMPtr<nsIImsRegCallback> mRequestCallback;
};

}  // namespace mobileconnection
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobileconnection_ImsRegistrationChild_h
