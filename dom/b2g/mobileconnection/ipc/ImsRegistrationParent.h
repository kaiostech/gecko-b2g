/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobileconnection_ImsRegistrationParent_h
#define mozilla_dom_mobileconnection_ImsRegistrationParent_h

#include "mozilla/dom/mobileconnection/PImsRegistrationParent.h"
#include "mozilla/dom/mobileconnection/PImsRegistrationRequestParent.h"
#include "mozilla/dom/mobileconnection/PImsRegServiceFinderParent.h"
#include "nsIImsRegService.h"

namespace mozilla {
namespace dom {
namespace mobileconnection {

class ImsRegServiceFinderParent : public PImsRegServiceFinderParent {
  friend class PImsRegServiceFinderParent;

 public:
  mozilla::ipc::IPCResult RecvCheckDeviceCapability(
      bool* aIsServiceInstalled, nsTArray<uint32_t>* aEnabledServiceIds);
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

/**
 * Parent actor of PImsRegistration. This object is created/destroyed along
 * with child actor.
 */
class ImsRegistrationParent : public PImsRegistrationParent,
                              public nsIImsRegListener {
  friend class PImsRegistrationParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGLISTENER

  explicit ImsRegistrationParent(uint32_t aServiceId);

 protected:
  virtual ~ImsRegistrationParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  mozilla::ipc::IPCResult RecvPImsRegistrationRequestConstructor(
      PImsRegistrationRequestParent* aActor,
      const ImsRegistrationRequest& aRequest) override;

 public:
  virtual PImsRegistrationRequestParent* AllocPImsRegistrationRequestParent(
      const ImsRegistrationRequest& request);

  virtual bool DeallocPImsRegistrationRequestParent(
      PImsRegistrationRequestParent* aActor);

  virtual mozilla::ipc::IPCResult RecvInit(
      bool* aEnabled, bool* aRttEnabled, uint16_t* aProfile,
      int16_t* aCapability, nsString* aUnregisteredReason,
      nsTArray<uint16_t>* aSupportedBearers);

 private:
  bool mLive;
  nsCOMPtr<nsIImsRegHandler> mHandler;
};

/******************************************************************************
 * PImsRegistrationRequestParent
 ******************************************************************************/

/**
 * Parent actor of PImsRegistrationRequestParent. The object is created along
 * with child actor and destroyed after the callback function of
 * nsIImsRegCallback is called. Child actor might be destroyed before
 * any callback is triggered. So we use mLive to maintain the status of child
 * actor in order to present sending data to a dead one.
 */
class ImsRegistrationRequestParent : public PImsRegistrationRequestParent,
                                     public nsIImsRegCallback {
  friend class PImsRegistrationRequestParent;

 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIMSREGCALLBACK

  explicit ImsRegistrationRequestParent(nsIImsRegHandler* aHandler)
      : mHandler(aHandler), mLive(true) {}

  bool DoRequest(const SetImsEnabledRequest& aRequest);

  bool DoRequest(const SetImsPreferredProfileRequest& aRequest);

  bool DoRequest(const SetImsRttEnabledRequest& aRequest);

 protected:
  virtual ~ImsRegistrationRequestParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  nsresult SendReply(const ImsRegistrationReply& aReply);

 private:
  nsCOMPtr<nsIImsRegHandler> mHandler;
  bool mLive;
};

}  // namespace mobileconnection
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobileconnection_ImsRegistrationParent_h
