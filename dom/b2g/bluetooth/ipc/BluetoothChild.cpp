/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothChild.h"

#include "BluetoothServiceChildProcess.h"

using mozilla::ipc::IPCResult;

USING_BLUETOOTH_NAMESPACE

namespace {

BluetoothServiceChildProcess* sBluetoothService;

}  // namespace

/*******************************************************************************
 * BluetoothChild
 ******************************************************************************/

BluetoothChild::BluetoothChild(BluetoothServiceChildProcess* aBluetoothService)
    : mShutdownState(Running) {
  MOZ_COUNT_CTOR(BluetoothChild);
  MOZ_ASSERT(!sBluetoothService);
  MOZ_ASSERT(aBluetoothService);

  sBluetoothService = aBluetoothService;
}

BluetoothChild::~BluetoothChild() {
  MOZ_COUNT_DTOR(BluetoothChild);
  MOZ_ASSERT(sBluetoothService);
  MOZ_ASSERT(mShutdownState == Dead);

  sBluetoothService = nullptr;
}

void BluetoothChild::BeginShutdown() {
  // Only do something here if we haven't yet begun the shutdown sequence.
  if (mShutdownState == Running) {
    SendStopNotifying();
    mShutdownState = SentStopNotifying;
  }
}

void BluetoothChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(sBluetoothService);

  sBluetoothService->NoteDeadActor();

#ifdef DEBUG
  mShutdownState = Dead;
#endif
}

IPCResult BluetoothChild::RecvNotify(const BluetoothSignal& aSignal) {
  MOZ_ASSERT(sBluetoothService);

  if (sBluetoothService) {
    sBluetoothService->DistributeSignal(aSignal);
  }
  return IPC_OK();
}

IPCResult BluetoothChild::RecvEnabled(const bool& aEnabled) {
  MOZ_ASSERT(sBluetoothService);

  if (sBluetoothService) {
    sBluetoothService->SetEnabled(aEnabled);
  }
  return IPC_OK();
}

IPCResult BluetoothChild::RecvBeginShutdown() {
  if (mShutdownState != Running && mShutdownState != SentStopNotifying) {
    MOZ_ASSERT(false, "Bad state!");
    return IPC_FAIL(this, "Bad state!");
  }

  SendStopNotifying();
  mShutdownState = SentStopNotifying;

  return IPC_OK();
}

IPCResult BluetoothChild::RecvNotificationsStopped() {
  if (mShutdownState != SentStopNotifying) {
    MOZ_ASSERT(false, "Bad state!");
    return IPC_FAIL(this, "Bad state!");
  }

  Send__delete__(this);
  return IPC_OK();
}

PBluetoothRequestChild* BluetoothChild::AllocPBluetoothRequestChild(
    const Request& aRequest) {
  MOZ_CRASH("Caller is supposed to manually construct a request!");
}

bool BluetoothChild::DeallocPBluetoothRequestChild(
    PBluetoothRequestChild* aActor) {
  delete aActor;
  return true;
}
