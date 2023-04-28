/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/SharedBufferManagerParent.h"
#include "base/message_loop.h"          // for MessageLoop
#include "base/process.h"               // for ProcessId
#include "base/task.h"                  // for CancelableTask, DeleteTask, etc
#include "base/thread.h"
#include "mozilla/ipc/MessageChannel.h" // for MessageChannel, etc
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/UniquePtr.h"          // for UniquePtr
#include "mozilla/Unused.h"
#include "nsIMemoryReporter.h"
#ifdef MOZ_WIDGET_GONK
// #include "GfxDebugger.h"
#include "mozilla/LinuxUtils.h"
#include "ui/PixelFormat.h"
#endif
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"
#include "transport/runnable_utils.h"
#include <dlfcn.h>

using namespace mozilla::ipc;
using std::map;

namespace mozilla {
namespace layers {

map<base::ProcessId, SharedBufferManagerParent* > SharedBufferManagerParent::sManagers;
StaticAutoPtr<Monitor> SharedBufferManagerParent::sManagerMonitor;
uint64_t SharedBufferManagerParent::sBufferKey(0);

#ifdef MOZ_WIDGET_GONK
class GrallocReporter final : public nsIMemoryReporter
{
public:
  NS_DECL_ISUPPORTS

  NS_IMETHOD CollectReports(nsIHandleReportCallback* aHandleReport,
                            nsISupports* aData, bool aAnonymize)
  {
    if (SharedBufferManagerParent::sManagerMonitor) {
      SharedBufferManagerParent::sManagerMonitor->Lock();
    }
    map<base::ProcessId, SharedBufferManagerParent*>::iterator it;
    for (it = SharedBufferManagerParent::sManagers.begin(); it != SharedBufferManagerParent::sManagers.end(); it++) {
      base::ProcessId pid = it->first;
      SharedBufferManagerParent *mgr = it->second;
      if (!mgr) {
        printf_stderr("GrallocReporter::CollectReports() mgr is nullptr");
        continue;
      }

      nsAutoCString pidName;
      LinuxUtils::GetThreadName(pid, pidName);

      MutexAutoLock lock(mgr->mLock);
      std::map<int64_t, android::sp<android::GraphicBuffer> >::iterator buf_it;
      for (buf_it = mgr->mBuffers.begin(); buf_it != mgr->mBuffers.end(); buf_it++) {
        nsresult rv;
        android::sp<android::GraphicBuffer> gb = buf_it->second;
        int bpp = android::bytesPerPixel(gb->getPixelFormat());
        int stride = gb->getStride();
        int height = gb->getHeight();
        int amount = bpp > 0
          ? (stride * height * bpp)
          // Special case for BSP specific formats (mainly YUV formats, count it as normal YUV buffer).
          : (stride * height * 3 / 2);

        nsPrintfCString gpath("gralloc/%s (pid=%d)/buffer(width=%d, height=%d, bpp=%d, stride=%d)",
            pidName.get(), pid, gb->getWidth(), height, bpp, stride);

        rv = aHandleReport->Callback(EmptyCString(), gpath, KIND_OTHER, UNITS_BYTES, amount,
              "Special RAM that can be shared between processes and directly accessed by "
              "both the CPU and GPU. Gralloc memory is usually a relatively precious "
              "resource, with much less available than generic RAM. When it's exhausted, "
              "graphics performance can suffer. This value can be incorrect because of race "
              "conditions."_ns,
            aData);
        if (rv != NS_OK) {
          if (SharedBufferManagerParent::sManagerMonitor) {
            SharedBufferManagerParent::sManagerMonitor->Unlock();
          }
          return rv;
        }
      }
    }
    if (SharedBufferManagerParent::sManagerMonitor) {
      SharedBufferManagerParent::sManagerMonitor->Unlock();
    }
    return NS_OK;
  }

protected:
  ~GrallocReporter() {}
};

NS_IMPL_ISUPPORTS(GrallocReporter, nsIMemoryReporter)
#endif

void InitGralloc() {
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread.");
#ifdef MOZ_WIDGET_GONK
  RegisterStrongMemoryReporter(new GrallocReporter());
#endif
}

/**
 * Task that deletes SharedBufferManagerParent on a specified thread.
 */
class DeleteSharedBufferManagerParentTask : public Runnable
{
public:
    explicit DeleteSharedBufferManagerParentTask(RefPtr<SharedBufferManagerParent> aSharedBufferManager)
        : Runnable("DeleteSharedBufferManagerParentTask")
        , mSharedBufferManager(aSharedBufferManager) {
    }
    NS_IMETHOD Run() override { return NS_OK; }
private:
    RefPtr<SharedBufferManagerParent> mSharedBufferManager;
};

/* static */
already_AddRefed<SharedBufferManagerParent> SharedBufferManagerParent::CreateSameProcess() {
  base::ProcessId pid = base::GetCurrentProcId();

  char thrname[128];
  SprintfLiteral(thrname, "BufMgrParent#%d", pid);

  RefPtr<SharedBufferManagerParent> parent =
      new SharedBufferManagerParent(pid, new base::Thread(thrname));
  return parent.forget();
}

/* static */
bool SharedBufferManagerParent::CreateForContent(
    Endpoint<PSharedBufferManagerParent>&& aEndpoint) {
  base::Thread* thread = nullptr;
  char thrname[128];
  SprintfLiteral(thrname, "BufMgrParent#%d", aEndpoint.OtherPid());
  thread = new base::Thread(thrname);

  RefPtr<SharedBufferManagerParent> bridge =
      new SharedBufferManagerParent(aEndpoint.OtherPid(), thread);
  MOZ_ASSERT(thread->IsRunning());
  thread->message_loop()->PostTask(NewRunnableMethod<Endpoint<PSharedBufferManagerParent>&&>(
      "layers::SharedBufferManagerParent::Bind", bridge, &SharedBufferManagerParent::Bind,
      std::move(aEndpoint)));

  return true;
}

void SharedBufferManagerParent::Bind(Endpoint<PSharedBufferManagerParent>&& aEndpoint) {
  aEndpoint.Bind(this);
}

SharedBufferManagerParent::SharedBufferManagerParent(base::ProcessId aOwner, base::Thread* aThread)
  : mThread(aThread)
  , mMainMessageLoop(MessageLoop::current())
  , mDestroyed(false)
  , mLock("SharedBufferManagerParent.mLock")
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!sManagerMonitor) {
    sManagerMonitor = new Monitor("Manager Monitor");
  }

  MonitorAutoLock lock(*sManagerMonitor.get());
  NS_ASSERTION(NS_IsMainThread(), "Should be on main thread");
  if (!aThread->IsRunning()) {
    aThread->Start();
  }

  if (sManagers.count(aOwner) != 0) {
    printf_stderr("SharedBufferManagerParent already exists.");
  }
  mOwner = aOwner;
  sManagers[aOwner] = this;

#ifdef MOZ_WIDGET_GONK
//  GfxDebugger::GetInstance();
#endif
}

SharedBufferManagerParent::~SharedBufferManagerParent()
{
  MonitorAutoLock lock(*sManagerMonitor.get());
  sManagers.erase(mOwner);

  // Delete mThread invokes Thread::Stop, it should wait Thread::ThreadMain
  // to finish before actually destroy mThread instance. If we do it not
  // in main thread, mThread instance will be destroyed before
  // Thread::ThreadMain finishes it's CleanUp. Crash occurres here.
  // Check and post DeleteTask to main thread once we need it.
  if (NS_IsMainThread()) {
    delete mThread;
  } else {
    RefPtr<DeleteTask<base::Thread>> task =
        new DeleteTask<base::Thread>(mThread);
    mMainMessageLoop->PostTask(task.forget());
  }
}

void
SharedBufferManagerParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MutexAutoLock lock(mLock);
  mDestroyed = true;
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  mBuffers.clear();
#endif
  RefPtr<Runnable> task = new DeleteSharedBufferManagerParentTask(this);
  mMainMessageLoop->PostTask(task.forget());
}

IPCResult SharedBufferManagerParent::RecvAllocateGrallocBuffer(const IntSize& aSize, const uint32_t& aFormat, const uint32_t& aUsage, mozilla::layers::MaybeMagicGrallocBufferHandle* aHandle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC

  *aHandle = null_t();

  if (aFormat == 0 || aUsage == 0) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- format and usage must be non-zero");
    return IPC_FAIL(this, "SharedBufferManagerParent::RecvAllocateGrallocBuffer -- format and usage must be non-zero");
  }

  if (aSize.width <= 0 || aSize.height <= 0) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- requested gralloc buffer size is invalid");
    return IPC_FAIL(this, "SharedBufferManagerParent::RecvAllocateGrallocBuffer -- requested gralloc buffer size is invalid");
  }

  // If the requested size is too big (i.e. exceeds the commonly used max GL texture size)
  // then we risk OOMing the parent process. It's better to just deny the allocation and
  // kill the child process, which is what the following code does.
  // TODO: actually use GL_MAX_TEXTURE_SIZE instead of hardcoding 4096
  if (aSize.width > 4096 || aSize.height > 4096) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- requested gralloc buffer is too big.");
    return IPC_FAIL(this, "SharedBufferManagerParent::RecvAllocateGrallocBuffer -- requested gralloc buffer is too big.");
  }

  android::sp<android::GraphicBuffer> outgoingBuffer = new android::GraphicBuffer(aSize.width, aSize.height, aFormat, aUsage);
  if (!outgoingBuffer.get() || outgoingBuffer->initCheck() != android::NO_ERROR) {
    printf_stderr("SharedBufferManagerParent::RecvAllocateGrallocBuffer -- gralloc buffer allocation failed");
    return IPC_FAIL(this, "SharedBufferManagerParent::RecvAllocateGrallocBuffer -- gralloc buffer allocation failed");
  }

  int64_t bufferKey;
  {
    MonitorAutoLock lock(*sManagerMonitor.get());
    bufferKey = ++sBufferKey;
  }
  GrallocBufferRef ref;
  ref.mOwner = mOwner;
  ref.mKey = bufferKey;
  *aHandle = MagicGrallocBufferHandle(outgoingBuffer, ref);

  {
    MutexAutoLock lock(mLock);
    mBuffers[bufferKey] = outgoingBuffer;
  }
#endif
  return IPC_OK();
}

IPCResult SharedBufferManagerParent::RecvDropGrallocBuffer(const mozilla::layers::MaybeMagicGrallocBufferHandle& handle)
{
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  NS_ASSERTION(handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef, "We shouldn't interact with the real buffer!");
  int64_t bufferKey = handle.get_GrallocBufferRef().mKey;
  android::sp<android::GraphicBuffer> buf = GetGraphicBuffer(bufferKey);
  MOZ_ASSERT(buf.get());
  MutexAutoLock lock(mLock);
  NS_ASSERTION(mBuffers.count(bufferKey) == 1, "No such buffer");
  mBuffers.erase(bufferKey);

  if(!buf.get()) {
    printf_stderr("SharedBufferManagerParent::RecvDropGrallocBuffer -- invalid buffer key.");
    return IPC_OK();
  }

#endif
  return IPC_OK();
}

void SharedBufferManagerParent::DropGrallocBufferSync(mozilla::layers::SurfaceDescriptor aDesc)
{
  DropGrallocBufferImpl(aDesc);
}

/*static*/
void SharedBufferManagerParent::DropGrallocBuffer(ProcessId id, mozilla::layers::SurfaceDescriptor aDesc)
{
  if (aDesc.type() != SurfaceDescriptor::TSurfaceDescriptorGralloc) {
    return;
  }

  MonitorAutoLock lock(*sManagerMonitor.get());
  SharedBufferManagerParent* mgr = SharedBufferManagerParent::GetInstance(id);
  if (!mgr) {
    return;
  }

  MutexAutoLock mgrlock(mgr->mLock);
  if (mgr->mDestroyed) {
    return;
  }

  if (PlatformThread::CurrentId() == mgr->mThread->thread_id()) {
    MOZ_CRASH("GFX: SharedBufferManagerParent::DropGrallocBuffer should not be called on SharedBufferManagerParent thread");
  } else {
    RefPtr<Runnable> runnable =
        WrapRunnable(RefPtr<SharedBufferManagerParent>(mgr),
                     &SharedBufferManagerParent::DropGrallocBufferSync, aDesc);
    // Dispatch the runnable.
    mgr->mThread->message_loop()->PostTask(runnable.forget());
  }
  return;
}

void SharedBufferManagerParent::DropGrallocBufferImpl(mozilla::layers::SurfaceDescriptor aDesc)
{
  MutexAutoLock lock(mLock);
  if (mDestroyed) {
    return;
  }
#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
  int64_t key = -1;
  MaybeMagicGrallocBufferHandle handle;
  if (aDesc.type() == SurfaceDescriptor::TSurfaceDescriptorGralloc) {
    handle = aDesc.get_SurfaceDescriptorGralloc().buffer();
  } else {
    return;
  }

  if (handle.type() == MaybeMagicGrallocBufferHandle::TGrallocBufferRef) {
    key = handle.get_GrallocBufferRef().mKey;
  } else if (handle.type() == MaybeMagicGrallocBufferHandle::TMagicGrallocBufferHandle) {
    key = handle.get_MagicGrallocBufferHandle().mRef.mKey;
  }

  NS_ASSERTION(key != -1, "Invalid buffer key");
  NS_ASSERTION(mBuffers.count(key) == 1, "No such buffer");
  mBuffers.erase(key);
  mozilla::Unused << SendDropGrallocBuffer(handle);
#endif
}

MessageLoop* SharedBufferManagerParent::GetMessageLoop()
{
  return mThread->message_loop();
}

SharedBufferManagerParent* SharedBufferManagerParent::GetInstance(ProcessId id)
{
  NS_ASSERTION(sManagers.count(id) == 1, "No BufferManager for the process");
  if (sManagers.count(id) == 1) {
    return sManagers[id];
  } else {
    return nullptr;
  }
}

#ifdef MOZ_HAVE_SURFACEDESCRIPTORGRALLOC
android::sp<android::GraphicBuffer>
SharedBufferManagerParent::GetGraphicBuffer(int64_t key)
{
  MutexAutoLock lock(mLock);
  if (mBuffers.count(key) == 1) {
    return mBuffers[key];
  } else {
    // The buffer can be dropped, or invalid
    printf_stderr("SharedBufferManagerParent::GetGraphicBuffer -- invalid key");
    return nullptr;
  }
}

android::sp<android::GraphicBuffer>
SharedBufferManagerParent::GetGraphicBuffer(GrallocBufferRef aRef)
{
  MonitorAutoLock lock(*sManagerMonitor.get());
  SharedBufferManagerParent* parent = GetInstance(aRef.mOwner);
  if (!parent) {
    return nullptr;
  }
  return parent->GetGraphicBuffer(aRef.mKey);
}

#ifdef MOZ_WIDGET_GONK

/* static */
int SharedBufferManagerParent::BufferTraversal(
  const std::function<int(int64_t key, android::sp<android::GraphicBuffer>& gb)> aAction)
{
  int ret = 0;

  if (SharedBufferManagerParent::sManagerMonitor) {
    SharedBufferManagerParent::sManagerMonitor->Lock();
  }

  int mgrIndex = 0;
  map<base::ProcessId, SharedBufferManagerParent*>::iterator it;

  bool quitTraversal = false;
  for (it = SharedBufferManagerParent::sManagers.begin();
       it != SharedBufferManagerParent::sManagers.end() && quitTraversal != true;
       it++) {
    SharedBufferManagerParent *mgr = it->second;
    if (!mgr) {
      printf_stderr("SMBP: manager is nullptr");
      continue;
    }

    std::map<int64_t, android::sp<android::GraphicBuffer> >::iterator buf_it;
    for (buf_it = mgr->mBuffers.begin();
         buf_it != mgr->mBuffers.end() && quitTraversal != true;
         buf_it++) {
      MutexAutoLock lock(mgr->mLock);
      ret = aAction(buf_it->first, buf_it->second);
      if (ret != BUFFER_TRAVERSAL_CONTINUE) {
        quitTraversal = true;
        }
    }
    mgrIndex++;
  }

  if (SharedBufferManagerParent::sManagerMonitor) {
    SharedBufferManagerParent::sManagerMonitor->Unlock();
  }

  ret = BUFFER_TRAVERSAL_OK;
  return ret;
}

/* static */
void SharedBufferManagerParent::ListGrallocBuffers(std::vector<int64_t> &aGrallocIndices) {
  BufferTraversal([&aGrallocIndices](int64_t key, android::sp<android::GraphicBuffer>& gb) -> int {
    int width   = gb->getWidth(),
        height  = gb->getHeight(),
        stride  = gb->getStride(),
        format  = gb->getPixelFormat(),
        bpp     = android::bytesPerPixel(gb->getPixelFormat());

    aGrallocIndices.push_back(key);
    printf_stderr("SBMP: gralloc[%llu]: w=%d, h=%d, format=%d, bpp=%d, stride=%d",
        key, width, height, format, bpp, stride);
    return BUFFER_TRAVERSAL_CONTINUE;
    });
}

/* static */
int SharedBufferManagerParent::DumpGrallocBuffer(int64_t aKey, char *filename) {
  int ret = 0;

  ret = BufferTraversal([&aKey, &filename](int64_t key, android::sp<android::GraphicBuffer>& gb) -> int{
    if (key != aKey) {
      return BUFFER_TRAVERSAL_CONTINUE;
    }

    int height  = gb->getHeight(),
        stride  = gb->getStride(),
        format  = gb->getPixelFormat(),
        bpp     = android::bytesPerPixel(gb->getPixelFormat());

    snprintf(filename, PATH_MAX, "/data/gb-%llu-%dx%d-f%d.raw", key, stride, height, format);
    printf_stderr("SBMP: dumping gralloc buffer as file %s", filename);

    uint8_t* img = NULL;
    gb->lock(GRALLOC_USAGE_SW_READ_OFTEN, (void**)(&img));
    if (img == NULL) {
      return BUFFER_TRAVERSAL_FAILURE;
    }

    int fd = creat(filename, O_WRONLY);
    if (fd == -1) {
      printf_stderr("SBMP: error on creating %s", filename);
      gb->unlock();
      return BUFFER_TRAVERSAL_FAILURE;
    }
    for (int i = 0; i < height; i++) {
      write(fd, img + stride * i * bpp, stride * bpp);
    }
    gb->unlock();
    close(fd);

    return BUFFER_TRAVERSAL_STOP;
  });

  return ret;
}
#endif  // MOZ_WIDGET_GONK

#endif

} /* namespace layers */
} /* namespace mozilla */
