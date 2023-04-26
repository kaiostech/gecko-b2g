/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ImsRegHandler.h"

#include "ImsRegCallback.h"
#include "nsIImsRegService.h"

using mozilla::ErrorResult;
using mozilla::dom::mobileconnection::ImsRegCallback;

namespace mozilla {
namespace dom {

class ImsRegHandler::Listener final : public nsIImsRegListener {
  ImsRegHandler* mImsRegHandler;

 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIIMSREGLISTENER(mImsRegHandler)

  explicit Listener(ImsRegHandler* aImsRegHandler)
      : mImsRegHandler(aImsRegHandler) {
    MOZ_ASSERT(mImsRegHandler);
  }

  void Disconnect() {
    MOZ_ASSERT(mImsRegHandler);
    mImsRegHandler = nullptr;
  }

 private:
  ~Listener() { MOZ_ASSERT(!mImsRegHandler); }
};

NS_IMPL_ISUPPORTS(ImsRegHandler::Listener, nsIImsRegListener)

NS_IMPL_ADDREF_INHERITED(ImsRegHandler, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(ImsRegHandler, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(ImsRegHandler)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ImsRegHandler,
                                                  DOMEventTargetHelper)
  // Don't traverse mListener because it doesn't keep any reference to
  // ImsRegHandler but a raw pointer instead. Neither does mImsRegHandler
  // because it's an xpcom service owned object and is only released at shutting
  // down.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mHandler)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDeviceConfig)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(ImsRegHandler,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mHandler)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mDeviceConfig)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ImsRegHandler)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

ImsRegHandler::ImsRegHandler(nsPIDOMWindowInner* aWindow,
                             nsIImsRegHandler* aHandler)
    : DOMEventTargetHelper(aWindow), mHandler(aHandler) {
  MOZ_ASSERT(mHandler);

  mUnregisteredReason.SetIsVoid(true);

  int16_t capability = nsIImsRegHandler::IMS_CAPABILITY_UNKNOWN;
  mHandler->GetCapability(&capability);
  nsAutoString reason;
  mHandler->GetUnregisteredReason(reason);

  UpdateCapability(capability, reason);

  // GetSupportedBearers
  uint16_t* bearers = nullptr;
  uint32_t count = 0;
  nsTArray<ImsBearer> supportedBearers;
  nsresult rv = mHandler->GetSupportedBearers(&count, &bearers);
  NS_ENSURE_SUCCESS_VOID(rv);
  for (uint32_t i = 0; i < count; ++i) {
    uint16_t bearer = bearers[i];
    MOZ_ASSERT(bearer < static_cast<uint16_t>(ImsBearer::EndGuard_));
    supportedBearers.AppendElement(static_cast<ImsBearer>(bearer));
  }
  free(bearers);
  mDeviceConfig = new ImsDeviceConfiguration(GetOwner(), supportedBearers);

  mListener = new Listener(this);
  mHandler->RegisterListener(mListener);

  mRttEnabled = false;
}

ImsRegHandler::~ImsRegHandler() { Shutdown(); }

void ImsRegHandler::DisconnectFromOwner() {
  DOMEventTargetHelper::DisconnectFromOwner();
  // Event listeners can't be handled anymore, so we can shutdown
  // the ImsRegHandler.
  Shutdown();
}

void ImsRegHandler::Shutdown() {
  if (mListener) {
    if (mHandler) {
      mHandler->UnregisterListener(mListener);
    }

    mListener->Disconnect();
    mListener = nullptr;
  }

  if (mHandler) {
    mHandler = nullptr;
  }
}

void ImsRegHandler::UpdateCapability(int16_t aCapability,
                                     const nsAString& aReason) {
  // IMS is not registered
  if (aCapability == nsIImsRegHandler::IMS_CAPABILITY_UNKNOWN) {
    mCapability.SetNull();
    mUnregisteredReason = aReason;
    return;
  }

  // IMS is registered
  MOZ_ASSERT(aCapability >= 0 &&
             aCapability < static_cast<int16_t>(ImsCapability::EndGuard_));
  mCapability.SetValue(static_cast<ImsCapability>(aCapability));
  mUnregisteredReason.SetIsVoid(true);
}

JSObject* ImsRegHandler::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return ImsRegHandler_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<ImsDeviceConfiguration> ImsRegHandler::DeviceConfig() const {
  RefPtr<ImsDeviceConfiguration> result = mDeviceConfig;
  return result.forget();
}

already_AddRefed<Promise> ImsRegHandler::SetEnabled(bool aEnabled,
                                                    ErrorResult& aRv) {
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ImsRegCallback> requestCallback = new ImsRegCallback(promise);

  nsresult rv = mHandler->SetEnabled(aEnabled, requestCallback);

  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    // fall-through to return promise.
  }

  return promise.forget();
}

bool ImsRegHandler::GetEnabled(ErrorResult& aRv) const {
  bool result = false;

  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return result;
  }

  nsresult rv = mHandler->GetEnabled(&result);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return result;
  }

  return result;
}

already_AddRefed<Promise> ImsRegHandler::SetPreferredProfile(
    ImsProfile aProfile, ErrorResult& aRv) {
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ImsRegCallback> requestCallback = new ImsRegCallback(promise);

  nsresult rv = mHandler->SetPreferredProfile(static_cast<uint16_t>(aProfile),
                                              requestCallback);

  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    // fall-through to return promise.
  }

  return promise.forget();
}

ImsProfile ImsRegHandler::GetPreferredProfile(ErrorResult& aRv) const {
  ImsProfile result = ImsProfile::Cellular_preferred;

  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return result;
  }

  uint16_t profile = nsIImsRegHandler::IMS_PROFILE_CELLULAR_PREFERRED;
  nsresult rv = mHandler->GetPreferredProfile(&profile);
  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return result;
  }

  MOZ_ASSERT(profile < static_cast<uint16_t>(ImsProfile::EndGuard_));
  result = static_cast<ImsProfile>(profile);

  return result;
}

Nullable<ImsCapability> ImsRegHandler::GetCapability() const {
  return mCapability;
}

void ImsRegHandler::GetUnregisteredReason(nsString& aReason) const {
  aReason = mUnregisteredReason;
  return;
}

already_AddRefed<Promise> ImsRegHandler::SetRttEnabled(bool aEnabled,
                                                       ErrorResult& aRv) {
  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  RefPtr<ImsRegCallback> requestCallback = new ImsRegCallback(promise);

  nsresult rv = mHandler->SetRttEnabled(aEnabled, requestCallback);

  if (NS_FAILED(rv)) {
    promise->MaybeReject(rv);
    // fall-through to return promise.
  } else {
    mRttEnabled = aEnabled;
  }

  return promise.forget();
}

bool ImsRegHandler::GetRttEnabled(ErrorResult& aRv) const {
  bool result = false;

  if (!mHandler) {
    aRv.Throw(NS_ERROR_FAILURE);
    return result;
  }

  nsresult rv = mHandler->GetRttEnabled(&result);

  if (NS_FAILED(rv)) {
    aRv.Throw(rv);
    return result;
  }

  return result;
}

// nsIImsRegListener

NS_IMETHODIMP
ImsRegHandler::NotifyEnabledStateChanged(bool aEnabled) {
  // Add |enabledstatechanged| when needed:
  // The enabled state is expected to be changed when set request is resolved,
  // so the caller knows when to get the updated enabled state.
  // If the change observed by multiple apps is expected,
  // then |enabledstatechanged| is required. Return NS_OK intentionally.
  return NS_OK;
}

NS_IMETHODIMP
ImsRegHandler::NotifyPreferredProfileChanged(uint16_t aProfile) {
  // Add |profilechanged| when needed:
  // The preferred profile is expected to be changed when set request is
  // resolved, so the caller knows when to get the updated enabled state. If the
  // change observed by multiple apps is expected, then |profilechanged| is
  // required. Return NS_OK intentionally.
  return NS_OK;
}

NS_IMETHODIMP
ImsRegHandler::NotifyCapabilityChanged(int16_t aCapability,
                                       const nsAString& aUnregisteredReason) {
  UpdateCapability(aCapability, aUnregisteredReason);

  return DispatchTrustedEvent(u"capabilitychange"_ns);
}

NS_IMETHODIMP
ImsRegHandler::NotifyRttEnabledStateChanged(bool aEnabled) {
  mRttEnabled = aEnabled;  // georgia, dispatch rttenabled event?
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
