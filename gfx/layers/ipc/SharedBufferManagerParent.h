/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedBufferManagerPARENT_H_
#define SharedBufferManagerPARENT_H_

#include "mozilla/Atomics.h"          // for Atomic
#include "mozilla/layers/PSharedBufferManagerParent.h"
#include "mozilla/StaticPtr.h"

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Mutex.h"            // for Mutex

namespace android {
class GraphicBuffer;
}
#endif

namespace base {
class Thread;
} // namespace base

namespace mozilla {
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
class Mutex;
#endif

namespace layers {

// Since Android O, SharedBufferManagerChild should not be necessary.
// android::Graphic buffer could be allocated via Binderized HAL.

class SharedBufferManagerParent : public PSharedBufferManagerParent
{
friend class GrallocReporter;
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedBufferManagerParent, final);

  static already_AddRefed<SharedBufferManagerParent> CreateSameProcess();
  static bool CreateForContent(Endpoint<PSharedBufferManagerParent>&& aEndpoint);

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  android::sp<android::GraphicBuffer> GetGraphicBuffer(int64_t key);
  static android::sp<android::GraphicBuffer> GetGraphicBuffer(GrallocBufferRef aRef);
#endif
  /**
   * Create a SharedBufferManagerParent but do not open the link
   */
  SharedBufferManagerParent(ProcessId aOwner, base::Thread* aThread);

  /**
   * When the IPC channel down or something bad make this Manager die, clear all the buffer reference!
   */
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  virtual ipc::IPCResult RecvAllocateGrallocBuffer(const IntSize&, const uint32_t&, const uint32_t&, mozilla::layers::MaybeMagicGrallocBufferHandle*);
  virtual ipc::IPCResult RecvDropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& handle);

  /**
   * Break the buffer's sharing state, decrease buffer reference for both side
   */
  static void DropGrallocBuffer(ProcessId id, mozilla::layers::SurfaceDescriptor aDesc);

  MessageLoop* GetMessageLoop();

#ifdef MOZ_WIDGET_GONK
  enum {
    BUFFER_TRAVERSAL_CONTINUE,
    BUFFER_TRAVERSAL_STOP,
    BUFFER_TRAVERSAL_OK,
    BUFFER_TRAVERSAL_FAILURE,
  };

  static int BufferTraversal(const std::function<int(int64_t key, android::sp<android::GraphicBuffer>&)> aAction);

  static void ListGrallocBuffers(std::vector<int64_t> &aGrallocIndices);

  static int DumpGrallocBuffer(int64_t key, char *filename);

#endif

protected:
  virtual ~SharedBufferManagerParent();

  void Bind(Endpoint<PSharedBufferManagerParent>&& aEndpoint);

  /**
   * Break the buffer's sharing state, decrease buffer reference for both side
   *
   * Must be called from SharedBufferManagerParent's thread
   */
  void DropGrallocBufferImpl(mozilla::layers::SurfaceDescriptor aDesc);

  // dispatched function
  void DropGrallocBufferSync(mozilla::layers::SurfaceDescriptor aDesc);

  /**
   * Function for find the buffer owner, most buffer passing on IPC contains only owner/key pair.
   * Use these function to access the real buffer.
   * Caller needs to hold sManagerMonitor.
   */
  static SharedBufferManagerParent* GetInstance(ProcessId id);

  /**
   * All living SharedBufferManager instances used to find the buffer owner, and parent->child IPCs
   */
  static std::map<base::ProcessId, SharedBufferManagerParent*> sManagers;

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  /**
   * Buffers owned by this SharedBufferManager pair
   */
  std::map<int64_t, android::sp<android::GraphicBuffer> > mBuffers;
#endif

  base::ProcessId mOwner;
  base::Thread* mThread;
  MessageLoop* mMainMessageLoop;
  bool mDestroyed;
  Mutex mLock;

  static uint64_t sBufferKey;
  static StaticAutoPtr<Monitor> sManagerMonitor;
};

} /* namespace layers */
} /* namespace mozilla */
#endif /* SharedBufferManagerPARENT_H_ */
