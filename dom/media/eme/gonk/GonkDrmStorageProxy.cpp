/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GonkDrmStorageProxy.h"

#include "GonkDrmUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Variant.h"
#include "nsComponentManagerUtils.h"
#include "nsThreadUtils.h"

namespace mozilla {

GonkDrmStorageProxy::GonkDrmStorageProxy(const nsAString& aOrigin,
                                         const nsAString& aKeySystem)
    : mOrigin(aOrigin), mKeySystem(aKeySystem) {}

bool GonkDrmStorageProxy::Init() {
  GD_ASSERT(NS_IsMainThread());

  nsresult rv;
  mStorage = do_CreateInstance("@mozilla.org/gonkdrm/storage;1", &rv);
  if (NS_FAILED(rv) || !mStorage) {
    GD_LOGE("GonkDrmStorageProxy::Init, do_CreateInstance failed.");
    Shutdown();
    return false;
  }

  mStorage->Init(NS_ConvertUTF16toUTF8(mOrigin),
                 NS_ConvertUTF16toUTF8(mKeySystem));
  return true;
}

void GonkDrmStorageProxy::Shutdown() {
  GD_ASSERT(NS_IsMainThread());

  if (mStorage) {
    mStorage->Uninit();
    mStorage = nullptr;
  }
}

class GonkDrmStorageCallback final : public nsIGonkDrmStorageCallback {
  typedef GonkDrmStorageProxy::GenericCallback GenericCallback;
  typedef GonkDrmStorageProxy::GetCallback GetCallback;
  typedef Variant<GenericCallback, GetCallback> CallbackVariant;

  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

 public:
  template <typename T>
  static already_AddRefed<GonkDrmStorageCallback> Create(const T& aCallback) {
    nsCOMPtr<nsISerialEventTarget> currentThread =
        GetCurrentSerialEventTarget();
    if (!currentThread) {
      GD_LOGE("GonkDrmStorageCallback::Create, failed to get current thread.");
      return nullptr;
    }

    return MakeAndAddRef<GonkDrmStorageCallback>(currentThread,
                                                 CallbackVariant(aCallback));
  }

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHODIMP
  OnAdd(nsresult aStatus, JSContext* cx) override {
    auto callback = mCallback.as<GenericCallback>();
    GD_ASSERT(callback);

    mCbThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmStorageCallback::OnAdd",
        [aStatus, callback]() { callback(NS_SUCCEEDED(aStatus)); }));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnGet(nsresult aStatus, const nsACString& aMimeType,
        const nsACString& aSessionType, const nsACString& aKeySetId,
        JSContext* cx) override {
    auto callback = mCallback.as<GetCallback>();
    GD_ASSERT(callback);

    mCbThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmStorageCallback::OnGet",
        [aStatus, mimeType = nsCString(aMimeType),
         sessionType = nsCString(aSessionType), keySetId = nsCString(aKeySetId),
         callback]() {
          callback(NS_SUCCEEDED(aStatus), mimeType, sessionType, keySetId);
        }));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnRemove(nsresult aStatus, JSContext* cx) override {
    auto callback = mCallback.as<GenericCallback>();
    GD_ASSERT(callback);

    mCbThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmStorageCallback::OnRemove",
        [aStatus, callback]() { callback(NS_SUCCEEDED(aStatus)); }));
    return NS_OK;
  }

  NS_IMETHODIMP
  OnClear(nsresult aStatus, JSContext* cx) override {
    auto callback = mCallback.as<GenericCallback>();
    GD_ASSERT(callback);

    mCbThread->Dispatch(NS_NewRunnableFunction(
        "GonkDrmStorageCallback::OnClear",
        [aStatus, callback]() { callback(NS_SUCCEEDED(aStatus)); }));
    return NS_OK;
  }

 private:
  GonkDrmStorageCallback(nsISerialEventTarget* aCbThread,
                         CallbackVariant aCallback)
      : mCbThread(aCbThread), mCallback(aCallback) {}

  ~GonkDrmStorageCallback() = default;

  nsCOMPtr<nsISerialEventTarget> mCbThread;
  CallbackVariant mCallback;
};

NS_IMPL_ISUPPORTS(GonkDrmStorageCallback, nsIGonkDrmStorageCallback)

void GonkDrmStorageProxy::Add(const nsACString& aEmeSessionId,
                              const nsACString& aMimeType,
                              const nsACString& aSessionType,
                              const nsACString& aKeySetId,
                              const GenericCallback& aCallback) {
  GD_ASSERT(mStorage);

  RefPtr<GonkDrmStorageCallback> storageCb =
      GonkDrmStorageCallback::Create(aCallback);
  if (!storageCb) {
    GD_LOGE("GonkDrmStorageProxy::Add, failed to create storage callback.");
    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "GonkDrmStorageProxy::Add",
      [sessionId = nsCString(aEmeSessionId), mimeType = nsCString(aMimeType),
       sessionType = nsCString(aSessionType), keySetId = nsCString(aKeySetId),
       storageCb, self = RefPtr<GonkDrmStorageProxy>(this)]() {
        self->mStorage->Add(sessionId, mimeType, sessionType, keySetId,
                            storageCb);
      }));
}

void GonkDrmStorageProxy::Get(const nsACString& aEmeSessionId,
                              const GetCallback& aCallback) {
  GD_ASSERT(mStorage);

  RefPtr<GonkDrmStorageCallback> storageCb =
      GonkDrmStorageCallback::Create(aCallback);
  if (!storageCb) {
    GD_LOGE("GonkDrmStorageProxy::Add, failed to create storage callback.");
    return;
  }

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("GonkDrmStorageProxy::Get",
                             [sessionId = nsCString(aEmeSessionId), storageCb,
                              self = RefPtr<GonkDrmStorageProxy>(this)]() {
                               self->mStorage->Get(sessionId, storageCb);
                             }));
}

void GonkDrmStorageProxy::Remove(const nsACString& aEmeSessionId,
                                 const GenericCallback& aCallback) {
  GD_ASSERT(mStorage);

  RefPtr<GonkDrmStorageCallback> storageCb =
      GonkDrmStorageCallback::Create(aCallback);
  if (!storageCb) {
    GD_LOGE("GonkDrmStorageProxy::Add, failed to create storage callback.");
    return;
  }

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("GonkDrmStorageProxy::Remove",
                             [sessionId = nsCString(aEmeSessionId), storageCb,
                              self = RefPtr<GonkDrmStorageProxy>(this)]() {
                               self->mStorage->Remove(sessionId, storageCb);
                             }));
}

void GonkDrmStorageProxy::Clear(const GenericCallback& aCallback) {
  GD_ASSERT(mStorage);

  RefPtr<GonkDrmStorageCallback> storageCb =
      GonkDrmStorageCallback::Create(aCallback);
  if (!storageCb) {
    GD_LOGE("GonkDrmStorageProxy::Add, failed to create storage callback.");
    return;
  }

  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "GonkDrmStorageProxy::Clear",
      [storageCb, self = RefPtr<GonkDrmStorageProxy>(this)]() {
        self->mStorage->Clear(storageCb);
      }));
}

}  // namespace mozilla
