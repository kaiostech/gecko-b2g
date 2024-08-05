/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "base/task.h"    // for NewRunnableFunction, etc
#include "base/thread.h"  // for Thread
#if defined(ANDROID)
#  include "gfxAndroidPlatform.h"
#endif
#include "mozilla/gfx/Logging.h"  // for gfxDebug
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "mozilla/layers/SharedBufferManagerParent.h"
#include "mozilla/layers/SynchronousTask.h"
#include "mozilla/StaticPtr.h"         // for StaticRefPtr
#include "mozilla/ReentrantMonitor.h"  // for ReentrantMonitor, etc
#include "transport/runnable_utils.h"
#include "nsThreadUtils.h"  // fo NS_IsMainThread

#ifdef MOZ_WIDGET_GONK
#  define LOG(args...) __android_log_print(ANDROID_LOG_INFO, "SBMChild", ##args)
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

// Singleton
StaticMutex SharedBufferManagerChild::sSharedBufferManagerSingletonLock;
SharedBufferManagerChild*
    SharedBufferManagerChild::sSharedBufferManagerChildSingleton = nullptr;
base::Thread* SharedBufferManagerChild::sSharedBufferManagerChildThread =
    nullptr;

SharedBufferManagerChild::SharedBufferManagerChild()
    : mCanSend(false)
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
      ,
      mBufferMutex("BufferMonitor")
#endif
{
}

static bool InSharedBufferManagerChildThread() {
  return SharedBufferManagerChild::sSharedBufferManagerChildThread
             ->thread_id() == PlatformThread::CurrentId();
}

base::Thread* SharedBufferManagerChild::GetThread() const {
  return sSharedBufferManagerChildThread;
}

SharedBufferManagerChild* SharedBufferManagerChild::GetSingleton() {
  return sSharedBufferManagerChildSingleton;
}

bool SharedBufferManagerChild::IsCreated() { return GetSingleton() != nullptr; }

void SharedBufferManagerChild::ShutDown() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  if (RefPtr<SharedBufferManagerChild> child = GetSingleton()) {
    child->DestroyManager();
    delete sSharedBufferManagerChildThread;
    sSharedBufferManagerChildThread = nullptr;
  }
}

void SharedBufferManagerChild::InitSameProcess() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");

  MOZ_ASSERT(!sSharedBufferManagerChildSingleton);
  MOZ_ASSERT(!sSharedBufferManagerChildThread);

  sSharedBufferManagerChildThread = new base::Thread("BufferMgrChild");
  if (!sSharedBufferManagerChildThread->IsRunning()) {
    sSharedBufferManagerChildThread->Start();
  }

  RefPtr<SharedBufferManagerChild> child = new SharedBufferManagerChild();
  RefPtr<SharedBufferManagerParent> parent =
      SharedBufferManagerParent::CreateSameProcess();

  RefPtr<Runnable> runnable =
      WrapRunnable(child, &SharedBufferManagerChild::BindSameProcess, parent);
  child->GetMessageLoop()->PostTask(runnable.forget());

  // Assign this after so other threads can't post messages before we connect to
  // IPDL.
  {
    StaticMutexAutoLock lock(sSharedBufferManagerSingletonLock);
    sSharedBufferManagerChildSingleton = child;
  }
}

void SharedBufferManagerChild::Bind(
    Endpoint<PSharedBufferManagerChild>&& aEndpoint) {
  if (!aEndpoint.Bind(this)) {
    return;
  }

  // This reference is dropped in DeallocPSharedBufferManagerChild.
  this->AddRef();

  mCanSend = true;
}

void SharedBufferManagerChild::BindSameProcess(
    RefPtr<SharedBufferManagerParent> aParent) {
  MessageLoop* parentMsgLoop = aParent->GetMessageLoop();

  Open(aParent, parentMsgLoop->SerialEventTarget(), mozilla::ipc::ChildSide);

  // This reference is dropped in DeallocPSharedBufferManagerChild.
  this->AddRef();

  mCanSend = true;
}

bool SharedBufferManagerChild::InitForContent(
    Endpoint<PSharedBufferManagerChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());
#if defined(ANDROID)
  gfxPlatform::GetPlatform();
#endif

  if (!sSharedBufferManagerChildThread) {
    sSharedBufferManagerChildThread =
        new base::Thread("SharedBufferManagerChild");
    bool success = sSharedBufferManagerChildThread->Start();
    MOZ_RELEASE_ASSERT(success,
                       "Failed to start SharedBufferManagerChild thread!");
  }

  RefPtr<SharedBufferManagerChild> child = new SharedBufferManagerChild();

  RefPtr<Runnable> runnable =
      NewRunnableMethod<Endpoint<PSharedBufferManagerChild>&&>(
          "layers::SharedBufferManagerChild::Bind", child,
          &SharedBufferManagerChild::Bind, std::move(aEndpoint));
  child->GetMessageLoop()->PostTask(runnable.forget());

  // Assign this after so other threads can't post messages before we connect to
  // IPDL.
  {
    StaticMutexAutoLock lock(sSharedBufferManagerSingletonLock);
    sSharedBufferManagerChildSingleton = child;
  }

  return true;
}

bool SharedBufferManagerChild::ReinitForContent(
    Endpoint<PSharedBufferManagerChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());

  // Note that at this point, ActorDestroy may not have been called yet,
  // meaning mCanSend is still true. In this case we will try to send a
  // synchronous WillClose message to the parent, and will certainly get a
  // false result and a MsgDropped processing error. This is okay.
  ShutDown();

  return InitForContent(std::move(aEndpoint));
}

// dispatched function
void SharedBufferManagerChild::DoShutdown(SynchronousTask* aTask) {
  AutoCompleteTask complete(aTask);

  MOZ_ASSERT(InSharedBufferManagerChildThread(),
             "Should be in SharedBufferManagerChild thread.");
  MarkShutDown();
  StaticMutexAutoLock lock(sSharedBufferManagerSingletonLock);
  SharedBufferManagerChild::sSharedBufferManagerChildSingleton = nullptr;
}

void SharedBufferManagerChild::DestroyManager() {
  MOZ_ASSERT(!InSharedBufferManagerChildThread(),
             "This method must not be called in this thread.");
  // ...because we are about to dispatch synchronous messages to the
  // BufferManagerChild thread.

  {
    SynchronousTask task("SharedBufferManagerChild Shutdown lock");

    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<SharedBufferManagerChild>(this),
                     &SharedBufferManagerChild::DoShutdown, &task);
    GetMessageLoop()->PostTask(runnable.forget());

    task.Wait();
  }
}

void SharedBufferManagerChild::ActorDestroy(ActorDestroyReason aWhy) {
  MOZ_ASSERT(InSharedBufferManagerChildThread());
  mCanSend = false;
}

bool SharedBufferManagerChild::CanSend() const { return mCanSend; }

void SharedBufferManagerChild::MarkShutDown() {
  MOZ_ASSERT(InSharedBufferManagerChildThread());
  mCanSend = false;
}

MessageLoop* SharedBufferManagerChild::GetMessageLoop() const {
  return sSharedBufferManagerChildThread != nullptr
             ? sSharedBufferManagerChildThread->message_loop()
             : nullptr;
}

// dispatched function
void SharedBufferManagerChild::AllocGrallocBufferSync(
    const GrallocParam& aParam, SynchronousTask* aTask) {
  AutoCompleteTask complete(aTask);

  sSharedBufferManagerChildSingleton->AllocGrallocBufferNow(
      aParam.size, aParam.format, aParam.usage, aParam.buffer);
}

// dispatched function
void SharedBufferManagerChild::DeallocGrallocBufferSync(
    const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer) {
  SharedBufferManagerChild::sSharedBufferManagerChildSingleton
      ->DeallocGrallocBufferNow(aBuffer);
}

bool SharedBufferManagerChild::AllocGrallocBuffer(
    const gfx::IntSize& aSize, const uint32_t& aFormat, const uint64_t& aUsage,
    mozilla::layers::MaybeMagicGrallocBufferHandle* aBuffer) {
  if (aSize.width <= 0 || aSize.height <= 0) {
    gfxDebug() << "Asking for gralloc of invalid size " << aSize.width << "x"
               << aSize.height;
    return false;
  }

  if (InSharedBufferManagerChildThread()) {
    return SharedBufferManagerChild::AllocGrallocBufferNow(aSize, aFormat,
                                                           aUsage, aBuffer);
  }

  {
    SynchronousTask task("SharedBufferManagerChild AllocGrallocBuffer lock");

    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<SharedBufferManagerChild>(this),
                     &SharedBufferManagerChild::AllocGrallocBufferSync,
                     GrallocParam(aSize, aFormat, aUsage, aBuffer), &task);
    GetMessageLoop()->PostTask(runnable.forget());

    task.Wait();
  }
  return true;
}

bool SharedBufferManagerChild::AllocGrallocBufferNow(
    const IntSize& aSize, const uint32_t& aFormat, const uint64_t& aUsage,
    mozilla::layers::MaybeMagicGrallocBufferHandle* aHandle) {
  // These are protected functions, we can just assert and ask the caller to
  // test
  MOZ_ASSERT(aSize.width >= 0 && aSize.height >= 0);

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  mozilla::layers::MaybeMagicGrallocBufferHandle handle;
  if (!SendAllocateGrallocBuffer(aSize, aFormat, aUsage, &handle)) {
    return false;
  }
  if (handle.type() != mozilla::layers::MaybeMagicGrallocBufferHandle::
                           TMagicGrallocBufferHandle) {
    return false;
  }
  *aHandle = handle.get_MagicGrallocBufferHandle().mRef;

  {
    MutexAutoLock lock(mBufferMutex);
    MOZ_ASSERT(
        mBuffers.count(handle.get_MagicGrallocBufferHandle().mRef.mKey) == 0);
    mBuffers[handle.get_MagicGrallocBufferHandle().mRef.mKey] =
        handle.get_MagicGrallocBufferHandle().mGraphicBuffer;
  }
  return true;
#else
  MOZ_CRASH("No GrallocBuffer for you");
  return true;
#endif
}

void SharedBufferManagerChild::DeallocGrallocBuffer(
    const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer) {
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  NS_ASSERTION(aBuffer.type() !=
                   mozilla::layers::MaybeMagicGrallocBufferHandle::
                       TMagicGrallocBufferHandle,
               "We shouldn't try to do IPC with real buffer");
  if (aBuffer.type() !=
      mozilla::layers::MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
    return;
  }
#endif

  if (InSharedBufferManagerChildThread()) {
    return SharedBufferManagerChild::DeallocGrallocBufferNow(aBuffer);
  }

  RefPtr<Runnable> runnable = WrapRunnable(
      RefPtr<SharedBufferManagerChild>(this),
      &SharedBufferManagerChild::DeallocGrallocBufferSync, aBuffer);
  GetMessageLoop()->PostTask(runnable.forget());
}

void SharedBufferManagerChild::DeallocGrallocBufferNow(
    const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer) {
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  NS_ASSERTION(aBuffer.type() !=
                   mozilla::layers::MaybeMagicGrallocBufferHandle::
                       TMagicGrallocBufferHandle,
               "We shouldn't try to do IPC with real buffer");

  {
    MutexAutoLock lock(mBufferMutex);
    mBuffers.erase(aBuffer.get_GrallocBufferRef().mKey);
  }
  SendDropGrallocBuffer(aBuffer);
#else
  MOZ_CRASH("No GrallocBuffer for you");
#endif
}

void SharedBufferManagerChild::DropGrallocBuffer(
    const mozilla::layers::MaybeMagicGrallocBufferHandle& aHandle) {
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  int64_t bufferKey = -1;
  if (aHandle.type() == mozilla::layers::MaybeMagicGrallocBufferHandle::
                            TMagicGrallocBufferHandle) {
    bufferKey = aHandle.get_MagicGrallocBufferHandle().mRef.mKey;
  } else if (aHandle.type() == mozilla::layers::MaybeMagicGrallocBufferHandle::
                                   TGrallocBufferRef) {
    bufferKey = aHandle.get_GrallocBufferRef().mKey;
  } else {
    return;
  }

  {
    MutexAutoLock lock(mBufferMutex);
    mBuffers.erase(bufferKey);
  }
#endif
}

IPCResult SharedBufferManagerChild::RecvDropGrallocBuffer(
    const mozilla::layers::MaybeMagicGrallocBufferHandle& aHandle) {
  DropGrallocBuffer(aHandle);
  return IPC_OK();
}

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
android::sp<android::GraphicBuffer> SharedBufferManagerChild::GetGraphicBuffer(
    int64_t key) {
  MutexAutoLock lock(mBufferMutex);
  if (mBuffers.count(key) == 0) {
    printf_stderr("SharedBufferManagerChild::GetGraphicBuffer -- invalid key");
    return nullptr;
  }
  return mBuffers[key];
}
#endif

} /* namespace layers */
} /* namespace mozilla */
