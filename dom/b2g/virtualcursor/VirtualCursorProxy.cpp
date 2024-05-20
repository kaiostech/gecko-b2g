/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VirtualCursorProxy.h"

#include "nsLayoutUtils.h"
#include "nsSupportsPrimitives.h"

extern mozilla::LazyLogModule gVirtualCursorLog;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(VirtualCursorProxy, nsIObserver)

VirtualCursorProxy::VirtualCursorProxy(nsPIDOMWindowOuter* aWindow,
                                       nsIVirtualCursor* aDelegate,
                                       const LayoutDeviceIntPoint& aOffset)
    : mOuterWindow(aWindow) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorProxy construct win id=%lu this=%p",
           aWindow->WindowID(), this));
  mSimulator = new CursorSimulator(aWindow, aDelegate);
  mSimulator->UpdateChromeOffset(aOffset);
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "outer-window-destroyed", false);
  }
}

VirtualCursorProxy::~VirtualCursorProxy() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorProxy::~VirtualCursorProxy"));
}

void VirtualCursorProxy::DestroyCursor() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "outer-window-destroyed");
  }
  mSimulator->Destroy();
  VirtualCursorService::RemoveCursor(mOuterWindow);
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug, ("VirtualCursorProxy::Destroy"));
}

void VirtualCursorProxy::RequestEnable() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorProxy::RequestEnable"));
  mSimulator->Enable();
};

void VirtualCursorProxy::RequestDisable() {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorProxy::RequestDisable"));
  mSimulator->Disable();
};

void VirtualCursorProxy::IsEnabled(bool* aEnabled) {
  *aEnabled = mSimulator->isEnabled();
}

void VirtualCursorProxy::UpdateScreenSize(int32_t aWidth, int32_t aHeight) {
  mSimulator->UpdateScreenSize(aWidth, aHeight);
}

void VirtualCursorProxy::UpdateChromeOffset(
    const LayoutDeviceIntPoint& aChromeOffset) {
  mSimulator->UpdateChromeOffset(aChromeOffset);
}

void VirtualCursorProxy::SetSelectionMode(
    CursorSimulator::SelectionMode aMode) {
  mSimulator->SetSelectionMode(aMode);
}

bool VirtualCursorProxy::GetSelectionActive() {
  return mSimulator->GetSelectionActive();
}

NS_IMETHODIMP VirtualCursorProxy::Observe(nsISupports* aSubject,
                                          const char* aTopic,
                                          const char16_t* aData) {
  MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
          ("VirtualCursorProxy observed %s", aTopic));
  if (!strcmp(aTopic, "outer-window-destroyed")) {
    nsCOMPtr<nsISupportsPRUint64> wrapper = do_QueryInterface(aSubject);
    NS_ENSURE_TRUE(wrapper, NS_ERROR_FAILURE);
    uint64_t outerID;
    Unused << wrapper->GetData(&outerID);
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("outer-window-destroyed id %lu self %lu", outerID,
             mOuterWindow->WindowID()));
    if (mOuterWindow->WindowID() == outerID) {
      DestroyCursor();
    }
  }
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
