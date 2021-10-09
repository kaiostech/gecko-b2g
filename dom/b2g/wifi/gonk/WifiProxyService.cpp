/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WifiProxyService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "WifiNative.h"

#ifdef MOZ_TASK_TRACER
#  include "GeckoTaskTracer.h"
using namespace mozilla::tasktracer;
#endif

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {

// The singleton Wifi service, to be used on the main thread.
static StaticRefPtr<WifiProxyService> gWifiProxyService;

static UniquePtr<WifiNative> gWifiNative;

// Runnable used dispatch the WaitForEvent result on the main thread.
class WifiEventDispatcher : public Runnable {
 public:
  WifiEventDispatcher(nsWifiEvent* aEvent, const nsACString& aInterface)
      : Runnable("WifiEventDispatcher"),
        mEvent(aEvent),
        mInterface(aInterface) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (gWifiProxyService->GetListener()) {
      gWifiProxyService->GetListener()->OnEventCallback(mEvent, mInterface);
    }
    return NS_OK;
  }

 private:
  RefPtr<nsWifiEvent> mEvent;
  nsCString mInterface;
};

// Runnable used dispatch the Command result on the main thread.
class WifiResultDispatcher : public Runnable {
 public:
  WifiResultDispatcher(nsWifiResult* aResult, const nsACString& aInterface)
      : Runnable("WifiResultDispatcher"),
        mResult(aResult),
        mInterface(aInterface) {
    MOZ_ASSERT(!NS_IsMainThread());
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread());

    if (gWifiProxyService->GetListener()) {
      gWifiProxyService->GetListener()->OnCommandResult(mResult, mInterface);
    }
    return NS_OK;
  }

 private:
  RefPtr<nsWifiResult> mResult;
  nsCString mInterface;
};

// Runnable used to call SendCommand on the control thread.
class ControlRunnable : public Runnable {
 public:
  ControlRunnable(CommandOptions aOptions, const nsACString& aInterface)
      : Runnable("ControlRunnable"),
        mOptions(aOptions),
        mInterface(aInterface) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  NS_IMETHOD Run() {
    RefPtr<nsWifiResult> result = new nsWifiResult();
    if (gWifiNative->ExecuteCommand(mOptions, result, mInterface)) {
      nsCOMPtr<nsIRunnable> runnable =
          new WifiResultDispatcher(result, mInterface);
      NS_DispatchToMainThread(runnable);
    }
    return NS_OK;
  }

 private:
  CommandOptions mOptions;
  nsCString mInterface;
};

NS_IMPL_ISUPPORTS(WifiProxyService, nsIWifiProxyService)

WifiProxyService::WifiProxyService() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!gWifiProxyService);
}

WifiProxyService::~WifiProxyService() { MOZ_ASSERT(!gWifiProxyService); }

already_AddRefed<WifiProxyService> WifiProxyService::FactoryCreate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gWifiProxyService) {
    gWifiProxyService = new WifiProxyService();
    ClearOnShutdown(&gWifiProxyService);

    gWifiNative = MakeUnique<WifiNative>();
    ClearOnShutdown(&gWifiNative);
  }

  RefPtr<WifiProxyService> service = gWifiProxyService.get();
  return service.forget();
}

NS_IMETHODIMP
WifiProxyService::Start(nsIWifiEventListener* aListener,
                        const char** aInterfaces, uint32_t aNumOfInterfaces) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aListener);

  nsresult rv;
  rv = NS_NewNamedThread("Wifi Control", getter_AddRefs(mControlThread));
  if (NS_FAILED(rv)) {
    NS_WARNING("Can't create wifi control thread");
    Shutdown();
    return NS_ERROR_FAILURE;
  }

  gWifiNative->RegisterEventCallback(this);
  mListener = aListener;
  return NS_OK;
}

NS_IMETHODIMP
WifiProxyService::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());

  gWifiNative->UnregisterEventCallback();

  if (mControlThread) {
    mControlThread->Shutdown();
    mControlThread = nullptr;
  }

  mListener = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
WifiProxyService::SendCommand(JS::Handle<JS::Value> aOptions,
                              const nsACString& aInterface, JSContext* aCx) {
  MOZ_ASSERT(NS_IsMainThread());
  WifiCommandOptions options;

  if (!options.Init(aCx, aOptions)) {
    NS_WARNING("Bad dictionary passed to WifiProxyService::SendCommand");
    return NS_ERROR_FAILURE;
  }

  if (!mControlThread) {
    return NS_ERROR_FAILURE;
  }

  // Dispatch the command to the control thread.
  nsCOMPtr<nsIRunnable> runnable =
      new ControlRunnable(CommandOptions(options), aInterface);
  MOZ_ALWAYS_SUCCEEDS(mControlThread->Dispatch(
      runnable.forget(), nsIEventTarget::DISPATCH_NORMAL));
  return NS_OK;
}

void WifiProxyService::Notify(nsWifiEvent* aEvent,
                              const nsACString& aInterface) {
  nsCOMPtr<nsIRunnable> runnable = new WifiEventDispatcher(aEvent, aInterface);
  NS_DispatchToMainThread(runnable);
}

}  // namespace mozilla
