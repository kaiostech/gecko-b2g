/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Preferences.h"
#include "VirtualCursorService.h"
#include "CursorSimulator.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

extern mozilla::LazyLogModule gVirtualCursorLog;

namespace mozilla {
namespace dom {

StaticRefPtr<VirtualCursorService> sSingleton;

NS_IMPL_ISUPPORTS(VirtualCursorService, nsIVirtualCursorService)

/* static */
already_AddRefed<VirtualCursorService> VirtualCursorService::GetService() {
  MOZ_ASSERT(NS_IsMainThread());
  if (!sSingleton) {
    sSingleton = new VirtualCursorService();
  }
  RefPtr<VirtualCursorService> service = sSingleton.get();
  return service.forget();
}

VirtualCursorService::~VirtualCursorService() { mCursorMap.Clear(); }

/* static */
already_AddRefed<VirtualCursorProxy> VirtualCursorService::GetOrCreateCursor(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<VirtualCursorService> service = GetService();

  return service->mCursorMap
      .WithEntryHandle(
          aWindow,
          [&](auto&& entry) {
            return entry.OrInsertWith([&] {
              RefPtr<dom::BrowserChild> browser =
                  BrowserChild::GetFrom(aWindow);
              LayoutDeviceIntPoint offset(browser->GetChromeOffset());
              VirtualCursorProxy* proxy;
              if (XRE_GetProcessType() == GeckoProcessType_Content) {
                proxy = new VirtualCursorProxy(
                    aWindow, new CursorRemote(aWindow), offset);
              } else {
                proxy = new VirtualCursorProxy(aWindow, service, offset);
              }
              ScreenIntSize size = browser->GetInnerSize();
              proxy->UpdateScreenSize(size.width, size.height);
              return proxy;
            });
          })
      .forget();
}

/* static */
already_AddRefed<VirtualCursorProxy> VirtualCursorService::GetCursor(
    nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<VirtualCursorService> service = GetService();
  auto entry = service->mCursorMap.Lookup(aWindow);
  if (!entry) {
    return nullptr;
  }
  RefPtr<VirtualCursorProxy> cursor = entry.Data();
  return cursor.forget();
}

/* static */
void VirtualCursorService::RemoveCursor(nsPIDOMWindowOuter* aWindow) {
  MOZ_ASSERT(NS_IsMainThread());
  if (aWindow) {
    RefPtr<VirtualCursorService> service = GetService();
    service->mCursorMap.Remove(aWindow);
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("VirtualCursorProxy RemoveCursor, remains %d",
             service->mCursorMap.Count()));
  }
}

NS_IMETHODIMP VirtualCursorService::Init(nsIDOMWindow* aWindow) {
  NS_ENSURE_ARG_POINTER(aWindow);

  if (!Preferences::GetBool("dom.virtualcursor.enabled", false) ||
      XRE_GetProcessType() == GeckoProcessType_Content) {
    return NS_OK;
  }
  nsCOMPtr<nsPIDOMWindowInner> inner = do_QueryInterface(aWindow);
  NS_ENSURE_TRUE(inner, NS_ERROR_FAILURE);
  mWindow = nsPIDOMWindowOuter::GetFromCurrentInner(inner);
  RefPtr<nsGlobalWindowOuter> win = nsGlobalWindowOuter::Cast(mWindow);
  mWindowUtils = win->WindowUtils();
  NS_ENSURE_TRUE(mWindowUtils, NS_ERROR_FAILURE);

  if (Preferences::GetBool("dom.virtualcursor.pan_simulator.enabled", false)) {
    mPanSimulator = new PanSimulator(mWindow, mWindowUtils);
  }
  return NS_OK;
}

NS_IMETHODIMP VirtualCursorService::RemoveCursor(nsFrameLoader* aFrameLoader) {
  if (mCurFrameLoader == aFrameLoader) {
    CursorOut();
  }
  return NS_OK;
}

/* static */
void VirtualCursorService::Shutdown() {
  if (!sSingleton) {
    return;
  }
  sSingleton = nullptr;
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::Shutdown"));
}

void VirtualCursorService::SendCursorEvent(const nsAString& aType,
                                           int32_t aButton, int32_t aButtons) {
  NS_ENSURE_TRUE_VOID(mWindowUtils);
  bool preventDefault;
  mWindowUtils->SendMouseEvent(aType, mCSSCursorPoint.x, mCSSCursorPoint.y,
                               aButton, 1, 0, false, 0.0, 0, false, false,
                               aButtons, 0, 6, &preventDefault);
}

void VirtualCursorService::UpdatePos(const LayoutDeviceIntPoint& aPoint) {
  if (!mWindow) {
    return;
  }

  if (IsPanning()) {
    // There may be a timing that we already start panning on the b2g process
    // then receive the move requests from content. Ignore the request.
    return;
  }

  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::UpdatePos"));
  nsIDocShell* docShell = mWindow->GetDocShell();
  NS_ENSURE_TRUE_VOID(docShell);
  RefPtr<nsPresContext> presContext = docShell->GetPresContext();
  NS_ENSURE_TRUE_VOID(presContext);
  PresShell* presShell = docShell->GetPresShell();
  NS_ENSURE_TRUE_VOID(presShell);

  float resolution = presShell->GetResolution();
  mCSSCursorPoint.x =
      presContext->DevPixelsToFloatCSSPixels(aPoint.x) / resolution;
  mCSSCursorPoint.y =
      presContext->DevPixelsToFloatCSSPixels(aPoint.y) / resolution;
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::UpdatePos (%.2f,%.2f), resolution: %.2f",
           mCSSCursorPoint.x, mCSSCursorPoint.y, resolution));
  CursorMove();
}

void VirtualCursorService::CursorClick() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::CursorClick"));
  CursorDown();
  CursorUp();
}

void VirtualCursorService::CursorDown() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::CursorDown"));
  SendCursorEvent(NS_LITERAL_STRING_FROM_CSTRING("mousedown"), 0, 1);
}

void VirtualCursorService::CursorUp() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::CursorUp"));
  SendCursorEvent(NS_LITERAL_STRING_FROM_CSTRING("mouseup"), 0, 0);
}

void VirtualCursorService::CursorMove() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::CursorMove"));
  if (IsPanning()) {
    // There may be a timing that we already start panning on the b2g process
    // then receive the move requests from content. Ignore the request.
    return;
  }
  SendCursorEvent(NS_LITERAL_STRING_FROM_CSTRING("mousemove"), 0, 0);
}

void VirtualCursorService::CursorOut(bool aCheckActive) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorService::CursorOut"));
  mCurFrameLoader = nullptr;
  SendCursorEvent(NS_LITERAL_STRING_FROM_CSTRING("mouseout"), 0, 0);
}

void VirtualCursorService::ShowContextMenu() {
  NS_ENSURE_TRUE_VOID(mWindowUtils);
  bool preventDefault;
  mWindowUtils->SendMouseEvent(NS_LITERAL_STRING_FROM_CSTRING("contextmenu"),
                               mCSSCursorPoint.x, mCSSCursorPoint.y, 2, 1, 0,
                               false, 0.0, 0, false, false, 0, 0, 6,
                               &preventDefault);
}

void VirtualCursorService::SetCurFrameLoader(nsFrameLoader* aFrameLoader) {
  mCurFrameLoader = aFrameLoader;
}

void VirtualCursorService::StartPanning() {
  if (mPanSimulator) {
    if (mPanSimulator->IsPanning()) {
      MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
              ("Try to start panning while already in panning state"));
      mPanSimulator->Finish();
    }

    // Bring the cursor to the center of the screen when the user starts
    // panning outside of the window.
    if (mCSSCursorPoint.x == -1 || mCSSCursorPoint.y == -1) {
      double width, height;
      mWindow->GetInnerWidth(&width);
      mWindow->GetInnerHeight(&height);

      CSSPoint centerPos(width, height);
      mCSSCursorPoint.x = centerPos.x / 2;
      mCSSCursorPoint.y = centerPos.y / 2;
    }

    nsIntPoint point((int)mCSSCursorPoint.x, (int)mCSSCursorPoint.y);
    mPanSimulator->Start(point);
    CursorOut();
  }
}

void VirtualCursorService::StopPanning() {
  if (mPanSimulator && mPanSimulator->IsPanning()) {
    mPanSimulator->Finish();
    CursorMove();
  }
}

bool VirtualCursorService::IsPanning() {
  if (mPanSimulator) {
    return mPanSimulator->IsPanning();
  }
  return false;
}

}  // namespace dom
}  // namespace mozilla
