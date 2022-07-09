/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_usb_UsbManager_h
#define mozilla_dom_usb_UsbManager_h

#include "mozilla/dom/usb/Types.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "nsCycleCollectionParticipant.h"
#include "nsICancelable.h"
#include "nsITimer.h"

class nsPIDOMWindowInner;
class nsIScriptContext;

namespace mozilla {

namespace hal {
class UsbStatus;
}  // namespace hal

namespace dom {

class UsbManager : public DOMEventTargetHelper,
                   public UsbObserver,
                   public nsITimerCallback,
                   public nsINamed {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  explicit UsbManager(nsPIDOMWindowInner* aWindow);

  void Init();
  void Shutdown();

  // For IObserver.
  void Notify(const hal::UsbStatus& aUsbStatus) override;

  /**
   * WebIDL Interface
   */
  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  bool DeviceAttached() const { return mDeviceAttached; }

  bool DeviceConfigured() const { return mDeviceConfigured; }

  IMPL_EVENT_HANDLER(usbstatuschange);

 private:
  ~UsbManager() {}

  /**
   * Update the usb device status stored in the usb manager object using
   * a usbDeviceStatusobject.
   */
  void UpdateFromUsbStatus(const hal::UsbStatus& aUsbStatus);

  /**
   * Delay for debouncing USB disconnects event during usb function swtiching
   */
  nsCOMPtr<nsITimer> mDebouncingTimer;

  bool mDeviceAttached;
  bool mDeviceConfigured;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_usb_UsbManager_h
