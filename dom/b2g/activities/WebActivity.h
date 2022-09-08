/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebActivity_h
#define mozilla_dom_WebActivity_h

#include "mozilla/dom/WebActivityBinding.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"
#include "nsIActivityProxy.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {
namespace {
class ActivityInitRunnable;
}  // namespace

class Promise;
class WebActivity;

class WebActivityImpl {
 public:
  virtual void Start(Promise* aPromise) = 0;
  virtual void Cancel() = 0;
  virtual nsresult PermissionCheck() = 0;
  virtual nsresult Initialize(const GlobalObject& aOwner,
                              const WebActivityOptions& aOptions) = 0;
  virtual void AddOuter(WebActivity* aOuter) = 0;
  virtual void RemoveOuter(WebActivity* aOuter) = 0;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING
};

class WebActivityMain final : public WebActivityImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING(WebActivityMain, override)

  explicit WebActivityMain();
  virtual void Start(Promise* aPromise) override;
  virtual void Cancel() override;
  virtual nsresult PermissionCheck() override;
  virtual nsresult Initialize(const GlobalObject& aOwner,
                              const WebActivityOptions& aOptions) override;
  virtual void AddOuter(WebActivity* aOuter) override;
  virtual void RemoveOuter(WebActivity* aOuter) override;

 private:
  ~WebActivityMain();
  WebActivity* mOuter;
};

class ActivityStartCallback final : public nsIActivityStartCallback {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIACTIVITYSTARTCALLBACK
  explicit ActivityStartCallback(Promise* aPromise);

 protected:
  ~ActivityStartCallback();

 private:
  RefPtr<Promise> mPromise;
};

class WebActivity final : public nsISupports,
                          public nsWrapperCache

{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(WebActivity)

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  explicit WebActivity(nsIGlobalObject* aGlobal,
                       already_AddRefed<WebActivityImpl> aImpl);

  static already_AddRefed<nsIActivityProxy> GetOrCreateActivityProxy();

  // WebIDL methods
  static already_AddRefed<WebActivity> Constructor(const GlobalObject& aGlobal,
                                                   const nsAString& aName,
                                                   JS::Handle<JS::Value> aData,
                                                   ErrorResult& aRv);

  already_AddRefed<Promise> Start(ErrorResult& aRv);

  void Cancel();

 protected:
  friend class ActivityInitRunnable;
  friend class WebActivityMain;
  friend class WebActivityWorker;

  ~WebActivity();

  nsresult PermissionCheck();

  nsresult Initialize(const GlobalObject& aOwner,
                      const WebActivityOptions& aOptions);

  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<WebActivityImpl> mImpl;
  bool mIsStarted;
  nsString mId;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_WebActivity_h
