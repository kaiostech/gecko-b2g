/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SharedBufferManagerCHILD_H_
#define SharedBufferManagerCHILD_H_

#include "mozilla/layers/PSharedBufferManagerChild.h"
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
#include "mozilla/Mutex.h"
#endif

namespace base {
class Thread;
} // namespace base

namespace mozilla {

namespace layers {
class SharedBufferManagerParent;
class SynchronousTask;

struct GrallocParam {
  gfx::IntSize size;
  uint32_t format;
  uint32_t usage;
  mozilla::layers::MaybeMagicGrallocBufferHandle* buffer;

  GrallocParam(const gfx::IntSize& aSize,
               const uint32_t& aFormat,
               const uint32_t& aUsage,
               mozilla::layers::MaybeMagicGrallocBufferHandle* aBuffer)
    : size(aSize)
    , format(aFormat)
    , usage(aUsage)
    , buffer(aBuffer)
  {}
};

// Since Android O, SharedBufferManagerChild should not be necessary.
// android::Graphic buffer could be allocated via Binderized HAL.

class SharedBufferManagerChild : public PSharedBufferManagerChild {
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SharedBufferManagerChild, final);

  SharedBufferManagerChild();
  /**
   * Creates the gralloc buffer manager with a dedicated thread for SharedBufferManagerChild.
   *
   * We may want to use a specific thread in the future. In this case, use
   * CreateWithThread instead.
   */

  static void InitSameProcess();

  static bool InitForContent(Endpoint<PSharedBufferManagerChild>&& aEndpoint);
  static bool ReinitForContent(Endpoint<PSharedBufferManagerChild>&& aEndpoint);

  /**
   * Destroys The SharedBufferManager protocol.
   *
   * The actual destruction happens synchronously on the SharedBufferManagerChild thread
   * which means that if this function is called from another thread, the current
   * thread will be paused until the destruction is done.
   */
  void DestroyManager();

  /**
   * Destroys the grallob buffer manager calling DestroyManager, and destroys the
   * SharedBufferManager's thread.
   *
   * If you don't want to destroy the thread, call DestroyManager directly
   * instead.
   */
  static void ShutDown();

  /**
   * returns the singleton instance.
   *
   * can be called from any thread.
   */
  static SharedBufferManagerChild* GetSingleton();

  /**
   * Dispatches a task to the SharedBufferManagerChild thread to do the connection
   */
  void ConnectAsync(SharedBufferManagerParent* aParent);

  /**
   * Allocate GrallocBuffer remotely.
  */
  bool
  AllocGrallocBuffer(const gfx::IntSize&, const uint32_t&, const uint32_t&, mozilla::layers::MaybeMagicGrallocBufferHandle*);

  /**
   * Deallocate a remotely allocated gralloc buffer.
   * As gralloc buffer life cycle controlled by sp, this just break the sharing status of the underlying buffer
   * and decrease the reference count on both side.
   */
  void
  DeallocGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& aBuffer);

  void
  DropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& aHandle);

  virtual ipc::IPCResult RecvDropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& aHandle);

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  android::sp<android::GraphicBuffer> GetGraphicBuffer(int64_t key);
#endif

  base::Thread* GetThread() const;

  MessageLoop* GetMessageLoop() const;

  static bool IsCreated();

  // Singleton
  static StaticMutex sSharedBufferManagerSingletonLock;
  static base::Thread* sSharedBufferManagerChildThread;
  static SharedBufferManagerChild* sSharedBufferManagerChildSingleton;

protected:

  /**
   * This must be called by the static function DeleteSharedBufferManagerSync defined
   * in ImageBridgeChild.cpp ONLY.
   */
  ~SharedBufferManagerChild() { }

  void ActorDestroy(ActorDestroyReason aWhy) override;

  bool CanSend() const;
  void MarkShutDown();

  void Bind(Endpoint<PSharedBufferManagerChild>&& aEndpoint);
  void BindSameProcess(RefPtr<SharedBufferManagerParent> aParent);

  void DoShutdown(SynchronousTask* aTask);

  /**
   * Part of the allocation of gralloc SurfaceDescriptor that is
   * executed on the SharedBufferManagerChild thread after invoking
   * AllocSurfaceDescriptorGralloc.
   *
   * Must be called from the SharedBufferManagerChild thread.
   */
  bool
  AllocGrallocBufferNow(const gfx::IntSize& aSize,
                        const uint32_t& aFormat,
                        const uint32_t& aUsage,
                        mozilla::layers::MaybeMagicGrallocBufferHandle* aBuffer);

  // Dispatched function
  void
  AllocGrallocBufferSync(const GrallocParam& aParam,
                         SynchronousTask* aTask);

  /**
   * Part of the deallocation of gralloc SurfaceDescriptor that is
   * executed on the SharedBufferManagerChild thread after invoking
   * DeallocSurfaceDescriptorGralloc.
   *
   * Must be called from the SharedBufferManagerChild thread.
   */
  void
  DeallocGrallocBufferNow(const mozilla::layers::MaybeMagicGrallocBufferHandle& handle);

  // dispatched function
  void
  DeallocGrallocBufferSync(const mozilla::layers::MaybeMagicGrallocBufferHandle& handle);

  bool mCanSend;

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  std::map<int64_t, android::sp<android::GraphicBuffer> > mBuffers;
  Mutex mBufferMutex;
#endif
};

} /* namespace layers */
} /* namespace mozilla */
#endif /* SharedBufferManagerCHILD_H_*/
