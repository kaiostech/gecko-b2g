/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDeviceStorage_h
#define nsDeviceStorage_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/LazyIdleThread.h"
#include "mozilla/Logging.h"
#include "mozilla/dom/devicestorage/DeviceStorageRequestChild.h"

#include "mozilla/dom/DOMRequest.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIClassInfo.h"
#include "nsIDOMWindow.h"
#include "nsIURI.h"
#include "nsIPrincipal.h"
#include "nsString.h"
#include "nsIWeakReferenceUtils.h"
#include "nsIDOMEventListener.h"
#include "nsIObserver.h"
#include "nsIStringBundle.h"
#include "nsIThread.h"
#include "mozilla/Mutex.h"
#include "prtime.h"
#include "DeviceStorage.h"
#include "mozilla/StaticPtr.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class BlobImpl;
class DeviceStorageParams;
}  // namespace dom
}  // namespace mozilla

class nsDOMDeviceStorage;
class DeviceStorageCursorRequest;

static mozilla::LazyLogModule sDSLogger("DeviceStorage");
#define DS_LOG_DEBUG(msg, ...)                 \
  MOZ_LOG(sDSLogger, mozilla::LogLevel::Debug, \
          ("[%s:%d] " msg, __func__, __LINE__, ##__VA_ARGS__))
#define DS_LOG_INFO(msg, ...)                 \
  MOZ_LOG(sDSLogger, mozilla::LogLevel::Info, \
          ("[%s:%d] " msg, __func__, __LINE__, ##__VA_ARGS__))
#define DS_LOG_WARN(msg, ...)                    \
  MOZ_LOG(sDSLogger, mozilla::LogLevel::Warning, \
          ("[%s:%d] " msg, __func__, __LINE__, ##__VA_ARGS__))
#define DS_LOG_ERROR(msg, ...)                 \
  MOZ_LOG(sDSLogger, mozilla::LogLevel::Error, \
          ("[%s:%d] " msg, __func__, __LINE__, ##__VA_ARGS__))

#define POST_ERROR_EVENT_FILE_EXISTS "NoModificationAllowedError"
#define POST_ERROR_EVENT_FILE_DOES_NOT_EXIST "NotFoundError"
#define POST_ERROR_EVENT_FILE_NOT_ENUMERABLE "TypeMismatchError"
#define POST_ERROR_EVENT_PERMISSION_DENIED "SecurityError"
#define POST_ERROR_EVENT_ILLEGAL_TYPE "TypeMismatchError"
#define POST_ERROR_EVENT_UNKNOWN "Unknown"

enum DeviceStorageRequestType {
  DEVICE_STORAGE_REQUEST_READ,
  DEVICE_STORAGE_REQUEST_WRITE,
  DEVICE_STORAGE_REQUEST_APPEND,
  DEVICE_STORAGE_REQUEST_CREATE,
  DEVICE_STORAGE_REQUEST_DELETE,
  DEVICE_STORAGE_REQUEST_WATCH,
  DEVICE_STORAGE_REQUEST_IS_DISK_FULL,
  DEVICE_STORAGE_REQUEST_FREE_SPACE,
  DEVICE_STORAGE_REQUEST_USED_SPACE,
  DEVICE_STORAGE_REQUEST_AVAILABLE,
  DEVICE_STORAGE_REQUEST_STATUS,
  DEVICE_STORAGE_REQUEST_FORMAT,
  DEVICE_STORAGE_REQUEST_MOUNT,
  DEVICE_STORAGE_REQUEST_UNMOUNT,
  DEVICE_STORAGE_REQUEST_CREATEFD,
  DEVICE_STORAGE_REQUEST_CURSOR
};

enum DeviceStorageAccessType {
  DEVICE_STORAGE_ACCESS_READ,
  DEVICE_STORAGE_ACCESS_WRITE,
  DEVICE_STORAGE_ACCESS_CREATE,
  DEVICE_STORAGE_ACCESS_UNDEFINED,
  DEVICE_STORAGE_ACCESS_COUNT
};

class DeviceStorageUsedSpaceCache final {
 public:
  static DeviceStorageUsedSpaceCache* CreateOrGet();

  DeviceStorageUsedSpaceCache();
  ~DeviceStorageUsedSpaceCache();

  class InvalidateRunnable final : public mozilla::Runnable {
   public:
    InvalidateRunnable(DeviceStorageUsedSpaceCache* aCache,
                       const nsAString& aStorageName)
        : mozilla::Runnable("devicestorage:InvalidateRunnable"),
          mCache(aCache),
          mStorageName(aStorageName) {}

    ~InvalidateRunnable() {}

    NS_IMETHOD Run() override {
      RefPtr<DeviceStorageUsedSpaceCache::CacheEntry> cacheEntry;
      cacheEntry = mCache->GetCacheEntry(mStorageName);
      if (cacheEntry) {
        cacheEntry->mDirty = true;
      }
      return NS_OK;
    }

   private:
    DeviceStorageUsedSpaceCache* mCache;
    nsString mStorageName;
  };

  void Invalidate(const nsAString& aStorageName) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mIOThread);

    mIOThread->Dispatch(new InvalidateRunnable(this, aStorageName),
                        NS_DISPATCH_NORMAL);
  }

  void Dispatch(already_AddRefed<nsIRunnable>&& aRunnable) {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(mIOThread);

    mIOThread->Dispatch(std::move(aRunnable), NS_DISPATCH_NORMAL);
  }

  nsresult AccumUsedSizes(const nsAString& aStorageName, uint64_t* aPictureSize,
                          uint64_t* aVideosSize, uint64_t* aMusicSize,
                          uint64_t* aTotalSize);

  void SetUsedSizes(const nsAString& aStorageName, uint64_t aPictureSize,
                    uint64_t aVideosSize, uint64_t aMusicSize,
                    uint64_t aTotalSize);

 private:
  friend class InvalidateRunnable;

  struct CacheEntry {
    // Technically, this doesn't need to be threadsafe, but the implementation
    // of the non-thread safe one causes ASSERTS due to the underlying thread
    // associated with a LazyIdleThread changing from time to time.
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CacheEntry)

    bool mDirty;
    nsString mStorageName;
    int64_t mFreeBytes;
    uint64_t mPicturesUsedSize;
    uint64_t mVideosUsedSize;
    uint64_t mMusicUsedSize;
    uint64_t mTotalUsedSize;

   private:
    ~CacheEntry() {}
  };
  already_AddRefed<CacheEntry> GetCacheEntry(const nsAString& aStorageName);

  nsTArray<RefPtr<CacheEntry>> mCacheEntries;

  RefPtr<mozilla::LazyIdleThread> mIOThread;

  static mozilla::StaticAutoPtr<DeviceStorageUsedSpaceCache>
      sDeviceStorageUsedSpaceCache;
};

class DeviceStorageTypeChecker final {
 public:
  static DeviceStorageTypeChecker* CreateOrGet();

  DeviceStorageTypeChecker();
  ~DeviceStorageTypeChecker();

  void InitFromBundle(nsIStringBundle* aBundle);

  bool Check(const nsAString& aType, mozilla::dom::BlobImpl* aBlob);
  bool Check(const nsAString& aType, nsIFile* aFile);
  bool Check(const nsAString& aType, const nsString& aPath);
  void GetTypeFromFile(nsIFile* aFile, nsAString& aType);
  void GetTypeFromFileName(const nsAString& aFileName, nsAString& aType);
  static nsresult GetPermissionForType(const nsAString& aType,
                                       nsACString& aPermissionResult);
  static nsresult GetAccessForRequest(
      const DeviceStorageRequestType aRequestType, nsACString& aAccessResult);
  static nsresult GetAccessForIndex(size_t aAccessIndex,
                                    nsACString& aAccessResult);
  static size_t GetAccessIndexForRequest(
      const DeviceStorageRequestType aRequestType);
  static bool IsVolumeBased(const nsAString& aType);
  static bool IsSharedMediaRoot(const nsAString& aType);

 private:
  nsString mPicturesExtensions;
  nsString mVideosExtensions;
  nsString mMusicExtensions;

  static mozilla::StaticAutoPtr<DeviceStorageTypeChecker>
      sDeviceStorageTypeChecker;
};

class DeviceStorageRequestManager final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DeviceStorageRequestManager)

  static const uint32_t INVALID_ID = 0;

  DeviceStorageRequestManager();

  bool IsOwningThread();
  nsresult DispatchToOwningThread(already_AddRefed<nsIRunnable>&& aRunnable);

  void StorePermission(size_t aAccess, bool aAllow);
  uint32_t CheckPermission(size_t aAccess);

  /* These must be called on the owning thread context of the device
     storage object. It will hold onto a device storage reference until
     all of the pending requests are completed or shutdown is called. */
  uint32_t Create(nsDOMDeviceStorage* aDeviceStorage,
                  mozilla::dom::DOMRequest** aRequest);

  uint32_t Create(nsDOMDeviceStorage* aDeviceStorage,
                  DeviceStorageCursorRequest* aRequest,
                  mozilla::dom::FileIterable** aIterable);

  /* These may be called from any thread context and post a request
     to the owning thread to resolve the underlying DOMRequest or
     FileIterable. In order to trigger FireDone for a FileIterable, one
     should call Resolve with only the request ID. */
  nsresult Resolve(uint32_t aId, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, const nsString& aValue, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, uint64_t aValue, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, bool aValue, bool aForceDispatch);
  nsresult Resolve(uint32_t aId, DeviceStorageFile* aValue,
                   bool aForceDispatch);
  nsresult Resolve(uint32_t aId, mozilla::dom::BlobImpl* aValue,
                   bool aForceDispatch);
  nsresult Reject(uint32_t aId, const nsString& aReason);
  nsresult Reject(uint32_t aId, const char* aReason);

  nsresult EnumeratePrepared(uint32_t aId);

  void Shutdown();

 private:
  DeviceStorageRequestManager(const DeviceStorageRequestManager&) = delete;
  DeviceStorageRequestManager& operator=(const DeviceStorageRequestManager&) =
      delete;

  struct ListEntry {
    RefPtr<mozilla::dom::DOMRequest> mRequest;
    RefPtr<mozilla::dom::FileIterable> mIterable;
    uint32_t mId;
    bool mIsIterable;
  };

  typedef nsTArray<ListEntry> ListType;
  typedef ListType::index_type ListIndex;

  virtual ~DeviceStorageRequestManager();
  uint32_t CreateInternal(mozilla::dom::DOMRequest* aRequest,
                          mozilla::dom::FileIterable* aIterable);
  nsresult ResolveInternal(ListIndex aIndex, JS::HandleValue aResult);
  nsresult RejectInternal(ListIndex aIndex, const nsString& aReason);
  nsresult DispatchOrAbandon(uint32_t aId,
                             already_AddRefed<nsIRunnable>&& aRunnable);
  ListType::index_type Find(uint32_t aId);

  nsCOMPtr<nsIThread> mOwningThread;
  ListType mPending;  // owning thread or destructor only

  mozilla::Mutex mMutex;
  uint32_t mPermissionCache[DEVICE_STORAGE_ACCESS_COUNT];
  bool mShutdown;

  static mozilla::Atomic<uint32_t> sLastRequestId;
};

class DeviceStorageRequest : public mozilla::Runnable {
 protected:
  DeviceStorageRequest();

 public:
  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          already_AddRefed<DeviceStorageFile>&& aFile,
                          uint32_t aRequest);

  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          already_AddRefed<DeviceStorageFile>&& aFile,
                          uint32_t aRequest, mozilla::dom::BlobImpl* aBlob);

  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          already_AddRefed<DeviceStorageFile>&& aFile,
                          uint32_t aRequest,
                          DeviceStorageFileDescriptor* aDSFileDescriptor);

  DeviceStorageAccessType GetAccess() const;
  void GetStorageType(nsAString& aType) const;
  DeviceStorageFile* GetFile() const;
  DeviceStorageFileDescriptor* GetFileDescriptor() const;
  DeviceStorageRequestManager* GetManager() const;

  uint32_t GetId() const { return mId; }

  void PermissionCacheMissed() { mPermissionCached = false; }

  nsresult Cancel();
  nsresult Allow();

  nsresult Resolve() {
    /* Always dispatch an empty resolve because that signals a cursor end
       and should not be executed directly from the caller's context due
       to the object potentially getting freed before we return. */
    uint32_t id = mId;
    mId = DeviceStorageRequestManager::INVALID_ID;
    return mManager->Resolve(id, true);
  }

  template <class T>
  nsresult Resolve(T aValue) {
    uint32_t id = mId;
    if (!mMultipleResolve) {
      mId = DeviceStorageRequestManager::INVALID_ID;
    }
    return mManager->Resolve(id, aValue, ForceDispatch());
  }

  template <class T>
  nsresult Reject(T aReason) {
    uint32_t id = mId;
    mId = DeviceStorageRequestManager::INVALID_ID;
    return mManager->Reject(id, aReason);
  }

 protected:
  bool ForceDispatch() const { return !mSendToParent && mPermissionCached; }

  virtual ~DeviceStorageRequest();
  virtual nsresult Prepare();
  virtual nsresult CreateSendParams(mozilla::dom::DeviceStorageParams& aParams);
  nsresult AllowInternal();
  nsresult SendToParentProcess();

  RefPtr<DeviceStorageRequestManager> mManager;
  RefPtr<DeviceStorageFile> mFile;
  uint32_t mId;
  RefPtr<mozilla::dom::BlobImpl> mBlob;
  RefPtr<DeviceStorageFileDescriptor> mDSFileDescriptor;
  DeviceStorageAccessType mAccess;
  bool mSendToParent;
  bool mUseMainThread;
  bool mUseStreamTransport;
  bool mCheckFile;
  bool mCheckBlob;
  bool mMultipleResolve;
  bool mPermissionCached;

 private:
  DeviceStorageRequest(const DeviceStorageRequest&) = delete;
  DeviceStorageRequest& operator=(const DeviceStorageRequest&) = delete;
};

class DeviceStorageCursorRequest final : public DeviceStorageRequest {
 public:
  DeviceStorageCursorRequest();

  using DeviceStorageRequest::Initialize;

  virtual void Initialize(DeviceStorageRequestManager* aManager,
                          already_AddRefed<DeviceStorageFile>&& aFile,
                          uint32_t aRequest, PRTime aSince);

  void AddFiles(size_t aSize);
  void AddFile(already_AddRefed<DeviceStorageFile> aFile);
  nsresult EnumeratePrepared();
  nsresult Continue();
  NS_IMETHOD Run() override;

 protected:
  virtual ~DeviceStorageCursorRequest(){};

  nsresult SendContinueToParentProcess();
  nsresult CreateSendParams(
      mozilla::dom::DeviceStorageParams& aParams) override;

  size_t mIndex;
  PRTime mSince;
  nsString mStorageType;
  nsTArray<RefPtr<DeviceStorageFile>> mFiles;
};

template <typename T>
class AutoCloseStream {
 public:
  AutoCloseStream(T* aStream) : mStream(aStream) {}
  ~AutoCloseStream() {
    if (mStream) mStream->Close();
  }

  explicit operator bool() { return mStream; }

  already_AddRefed<T> forget() { return mStream.forget(); }

  AutoCloseStream<T>& operator=(T* aStream) {
    mStream = aStream;
    return *this;
  }

  AutoCloseStream<T>& operator=(const nsCOMPtr<T>& aStream) {
    mStream = aStream;
    return *this;
  }

  T* get() const { return mStream; }

  operator T*() const { return get(); }

 private:
  void operator=(const AutoCloseStream<T>&) = delete;
  AutoCloseStream(const AutoCloseStream<T>&) = delete;

  nsCOMPtr<T> mStream;
};

#endif
