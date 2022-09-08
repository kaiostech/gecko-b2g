/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_AlarmManager_h
#define mozilla_dom_AlarmManager_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/AlarmManagerBinding.h"
#include "mozilla/dom/Promise.h"
#include "nsIAlarmProxy.h"
#include "nsWrapperCache.h"

/*
  - AlarmManager:
    The AlarmManager webidl implementation.
  - AlarmManagerImpl:
    The abstract class for AlarmManager to handle requests from both main
    threads and worker threads.
  - AlarmManagerMain:
    The AlarmManagerImpl implementation on main threads, creates an AlarmProxy
    instance by itself, and uses the AlarmProxy to send requests to
    AlarmService.
  - AlarmManagerWorker:
    The AlarmManagerImpl implementation on worker threads, cannot create
    AlarmProxy instances by itself, so dispatches all requests to main thread
    by runnables.
*/

namespace mozilla {
namespace dom {

class AlarmManagerImpl {
 public:
  AlarmManagerImpl() = default;
  virtual nsresult Init(nsIGlobalObject* aGlobal) = 0;

  virtual void GetAll(Promise* aPromise) = 0;
  virtual void Add(Promise* aPromise, JSContext* aCx,
                   const AlarmOptions& aOptions) = 0;
  virtual void Remove(long aId) = 0;

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

 protected:
  nsCString mUrl;
};

class AlarmManagerMain final : public AlarmManagerImpl {
 public:
  NS_INLINE_DECL_REFCOUNTING(AlarmManagerMain, override)
  AlarmManagerMain() = default;
  virtual nsresult Init(nsIGlobalObject* aGlobal) override;

  virtual void GetAll(Promise* aPromise) override;
  virtual void Add(Promise* aPromise, JSContext* aCx,
                   const AlarmOptions& aOptions) override;
  virtual void Remove(long aId) override;

 private:
  ~AlarmManagerMain() = default;
};

class AlarmManager final : public nsISupports,
                           public nsWrapperCache

{
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(AlarmManager)

  static already_AddRefed<AlarmManager> Create(nsIGlobalObject* aGlobal,
                                               nsresult& aRv);

  nsIGlobalObject* GetParentObject() const { return mGlobal; }

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  explicit AlarmManager(nsIGlobalObject* aGlobal);
  nsresult Init();

  already_AddRefed<Promise> GetAll();
  already_AddRefed<Promise> Add(JSContext* aCx, const AlarmOptions& options);
  void Remove(long aId);

 protected:
  ~AlarmManager() = default;
  nsCOMPtr<nsIGlobalObject> mGlobal;
  RefPtr<AlarmManagerImpl> mImpl;
};

namespace alarm {
already_AddRefed<nsIAlarmProxy> CreateAlarmProxy();
nsresult Initialize(const nsCOMPtr<nsIPrincipal>& aPrincipal, nsCString& aUrl);
}  // namespace alarm
}  // namespace dom
}  // namespace mozilla

#endif
