/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_ipc_BluetoothParent_h
#define mozilla_dom_bluetooth_ipc_BluetoothParent_h

#include "mozilla/Observer.h"
#include "mozilla/dom/bluetooth/PBluetoothParent.h"
#include "mozilla/dom/bluetooth/PBluetoothRequestParent.h"

namespace mozilla {
namespace dom {

class ContentParent;

}  // namespace dom
}  // namespace mozilla

using mozilla::ipc::IPCResult;

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothService;

/*******************************************************************************
 * BluetoothParent
 ******************************************************************************/

class BluetoothParent : public PBluetoothParent,
                        public BluetoothSignalObserver {
  friend class mozilla::dom::ContentParent;

  enum ShutdownState {
    Running = 0,
    SentBeginShutdown,
    ReceivedStopNotifying,
    SentNotificationsStopped,
    Dead
  };

  RefPtr<BluetoothService> mService;
  ShutdownState mShutdownState;
  bool mReceivedStopNotifying;
  bool mSentBeginShutdown;

 public:
  void BeginShutdown();

  bool InitWithService(BluetoothService* aService);

  virtual IPCResult RecvRegisterSignalHandler(const nsString& aNode);

  virtual IPCResult RecvUnregisterSignalHandler(const nsString& aNode);

  virtual IPCResult RecvStopNotifying();

  virtual IPCResult RecvPBluetoothRequestConstructor(
      PBluetoothRequestParent* aActor, const Request& aRequest) override;

  virtual PBluetoothRequestParent* AllocPBluetoothRequestParent(
      const Request& aRequest);

  virtual bool DeallocPBluetoothRequestParent(PBluetoothRequestParent* aActor);

 protected:
  BluetoothParent();
  virtual ~BluetoothParent();

  virtual void Notify(const BluetoothSignal& aSignal) override;

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  void UnregisterAllSignalHandlers();
};

END_BLUETOOTH_NAMESPACE

#endif  // mozilla_dom_bluetooth_ipc_BluetoothParent_h
