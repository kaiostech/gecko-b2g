/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_BluetoothChild_h
#define mozilla_dom_bluetooth_ipc_BluetoothChild_h

#include "mozilla/dom/bluetooth/PBluetoothChild.h"
#include "mozilla/dom/bluetooth/PBluetoothRequestChild.h"

namespace mozilla {
namespace dom {
namespace bluetooth {

class BluetoothServiceChildProcess;

}  // namespace bluetooth
}  // namespace dom
}  // namespace mozilla

using mozilla::ipc::IPCResult;

BEGIN_BLUETOOTH_NAMESPACE

/*******************************************************************************
 * BluetoothChild
 ******************************************************************************/

class BluetoothChild : public PBluetoothChild {
  friend class mozilla::dom::bluetooth::BluetoothServiceChildProcess;

  enum ShutdownState {
    Running = 0,
    SentStopNotifying,
    ReceivedNotificationsStopped,
    Dead
  };

  ShutdownState mShutdownState;

 public:
  virtual void ActorDestroy(ActorDestroyReason aWhy);

  virtual IPCResult RecvNotify(const BluetoothSignal& aSignal);

  virtual IPCResult RecvEnabled(const bool& aEnabled);

  virtual IPCResult RecvBeginShutdown();

  virtual IPCResult RecvNotificationsStopped();

  virtual PBluetoothRequestChild* AllocPBluetoothRequestChild(
      const Request& aRequest);

  virtual bool DeallocPBluetoothRequestChild(PBluetoothRequestChild* aActor);

 protected:
  explicit BluetoothChild(BluetoothServiceChildProcess* aBluetoothService);
  virtual ~BluetoothChild();

  void BeginShutdown();
};

END_BLUETOOTH_NAMESPACE

#endif  // mozilla_dom_bluetooth_ipc_BluetoothChild_h
