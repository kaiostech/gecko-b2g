/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>
#include "UsbManager.h"
#include "base/message_loop.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Hal.h"
#include "mozilla/dom/UsbEvent.h"
#include "mozilla/dom/UsbManagerBinding.h"
#include "nsComponentManagerUtils.h"  // for do_CreateInstance

/**
 * We have to use macros here because our leak analysis tool things we are
 * leaking strings when we have |static const nsString|. Sad :(
 */
#define USB_STATUS_CHANGE_NAME u"usbstatuschange"_ns

const uint32_t UPDATE_DELAY = 1000;

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(UsbManager)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(UsbManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(UsbManager, DOMEventTargetHelper)

UsbManager::UsbManager(nsPIDOMWindowInner* aWindow)
    : DOMEventTargetHelper(aWindow),
      mDeviceAttached(false),
      mDeviceConfigured(false) {}

void UsbManager::Init() {
  hal::RegisterUsbObserver(this);

  hal::UsbStatus usbStatus;
  hal::GetCurrentUsbStatus(&usbStatus);

  UpdateFromUsbStatus(usbStatus);
}

void UsbManager::Shutdown() {
  if (mDebouncingTimer) {
    mDebouncingTimer->Cancel();
    mDebouncingTimer = nullptr;
  }
  hal::UnregisterUsbObserver(this);
}

JSObject* UsbManager::WrapObject(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return UsbManager_Binding::Wrap(aCx, this, aGivenProto);
}

void UsbManager::UpdateFromUsbStatus(const hal::UsbStatus& aUsbStatus) {
  mDeviceAttached = aUsbStatus.deviceAttached();
  mDeviceConfigured = aUsbStatus.deviceConfigured();
}

void UsbManager::Notify(const hal::UsbStatus& aUsbStatus) {
  bool previousDeviceAttached = mDeviceAttached;
  bool previousDeviceConfigured = mDeviceConfigured;

  UpdateFromUsbStatus(aUsbStatus);

  if (previousDeviceAttached != mDeviceAttached ||
      previousDeviceConfigured != mDeviceConfigured) {
    UsbEventInit init;
    init.mDeviceAttached = aUsbStatus.deviceAttached();
    init.mDeviceConfigured = aUsbStatus.deviceConfigured();

    RefPtr<UsbEvent> event =
        UsbEvent::Constructor(this, USB_STATUS_CHANGE_NAME, init);
    // Delay for debouncing USB disconnects.
    // We often get rapid connect/disconnect events
    // when enabling USB functions which need debouncing.
    if (!mDeviceAttached && !mDebouncingTimer) {
      mDebouncingTimer = NS_NewTimer();
      mDebouncingTimer->InitWithCallback(this, UPDATE_DELAY,
                                         nsITimer::TYPE_ONE_SHOT);
      return;
    }

    // Remove ongoing debouncing event.
    if (mDebouncingTimer) {
      mDebouncingTimer->Cancel();
      mDebouncingTimer = nullptr;
    }

    DispatchTrustedEvent(event);
  }
}

NS_IMETHODIMP
UsbManager::Notify(nsITimer* aTimer) {
  UsbEventInit init;
  init.mDeviceAttached = mDeviceAttached;
  init.mDeviceConfigured = mDeviceConfigured;
  RefPtr<UsbEvent> event =
      UsbEvent::Constructor(this, USB_STATUS_CHANGE_NAME, init);
  DispatchTrustedEvent(event);
  return NS_OK;
}

NS_IMETHODIMP
UsbManager::GetName(nsACString& aName) {
  aName.AssignLiteral("UsbManager");
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
