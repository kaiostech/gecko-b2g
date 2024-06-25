/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothSocket.h"
#include <fcntl.h>
#include <sys/socket.h>
#include "BluetoothSocketObserver.h"
#include "BluetoothInterface.h"
#include "BluetoothUtils.h"
#include "mozilla/ipc/UnixSocketWatcher.h"
#include "mozilla/Assertions.h"
#include "mozilla/FileUtils.h"
#include "mozilla/RefPtr.h"
#include "nsISupportsImpl.h"  // for MOZ_COUNT_CTOR_INHERITED, MOZ_COUNT_DTOR_INHERITED
#include "nsXULAppAPI.h"

using namespace mozilla::ipc;
USING_BLUETOOTH_NAMESPACE

static const size_t MAX_READ_SIZE = 1 << 16;

class mozilla::dom::bluetooth::DroidSocketImpl
    : public mozilla::ipc::UnixFdWatcher,
      public DataSocketIO {
 public:
  /* The connection status in DroidSocketImpl indicates the current
   * phase of the socket connection. The initial settign should always
   * be DISCONNECTED, when no connection is present.
   *
   * To establish a connection on the server, DroidSocketImpl moves
   * to LISTENING. It now waits for incoming connection attempts by
   * installing a read watcher on the I/O thread. When its socket file
   * descriptor becomes readable, DroidSocketImpl accepts the connection
   * and finally moves DroidSocketImpl to CONNECTED. DroidSocketImpl now
   * uses read and write watchers during data transfers. Any socket setup
   * is handled internally by the accept method.
   *
   * On the client side, DroidSocketImpl moves to CONNECTING and installs
   * a write watcher on the I/O thread to wait until the connection is
   * ready. The socket setup is handled internally by the connect method.
   * Installing the write handler makes the code compatible with POSIX
   * semantics for non-blocking connects and gives a clear signal when the
   * conncetion is ready. DroidSocketImpl then moves to CONNECTED and uses
   * read and write watchers during data transfers.
   */
  enum ConnectionStatus {
    SOCKET_IS_DISCONNECTED = 0,
    SOCKET_IS_LISTENING,
    SOCKET_IS_CONNECTING,
    SOCKET_IS_CONNECTED
  };

  DroidSocketImpl(MessageLoop* aConsumerLoop, MessageLoop* aIOLoop,
                  BluetoothSocket* aConsumer)
      : mozilla::ipc::UnixFdWatcher(aIOLoop),
        DataSocketIO(aConsumerLoop),
        mConsumer(aConsumer),
        mShuttingDownOnIOThread(false),
        mConnectionStatus(SOCKET_IS_DISCONNECTED) {
    MOZ_COUNT_CTOR_INHERITED(DroidSocketImpl, DataSocketIO);
  }

  ~DroidSocketImpl() {
    MOZ_ASSERT(IsConsumerThread());

    MOZ_COUNT_DTOR_INHERITED(DroidSocketImpl, DataSocketIO);
  }

  void Send(UnixSocketIOBuffer* aBuffer) {
    EnqueueData(aBuffer);
    AddWatchers(WRITE_WATCHER, false);
  }

  void Connect(int aFd);
  void Listen(int aFd);
  void Accept(int aFd);

  void ConnectClientFd() {
    // Stop current read watch
    RemoveWatchers(READ_WATCHER);

    mConnectionStatus = SOCKET_IS_CONNECTED;

    // Restart read & write watch on client fd
    AddWatchers(READ_WATCHER, true);
    AddWatchers(WRITE_WATCHER, false);
  }

  BluetoothSocket* GetBluetoothSocket() { return mConsumer; }

  DataSocket* GetDataSocket() { return GetBluetoothSocket(); }

  /**
   * Consumer pointer. Non-thread-safe pointer, so should only be manipulated
   * directly from consumer thread. All non-consumer-thread accesses should
   * happen with mImpl as container.
   */
  BluetoothSocket* mConsumer;

  // Methods for |DataSocket|
  //

  nsresult QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer) override;
  void ConsumeBuffer() override;
  void DiscardBuffer() override;

  // Methods for |SocketIOBase|
  //

  SocketBase* GetSocketBase() override { return GetDataSocket(); }

  bool IsShutdownOnConsumerThread() const override {
    MOZ_ASSERT(IsConsumerThread());

    return mConsumer == nullptr;
  }

  bool IsShutdownOnIOThread() const override { return mShuttingDownOnIOThread; }

  void ShutdownOnConsumerThread() override {
    MOZ_ASSERT(IsConsumerThread());
    MOZ_ASSERT(!IsShutdownOnConsumerThread());

    mConsumer = nullptr;
  }

  void ShutdownOnIOThread() override {
    MOZ_ASSERT(!IsConsumerThread());
    MOZ_ASSERT(!mShuttingDownOnIOThread);

    Close();  // will also remove fd from I/O loop
    mShuttingDownOnIOThread = true;
  }

 private:
  class ReceiveTask;

  /**
   * libevent triggered functions that reads data from socket when available and
   * guarenteed non-blocking. Only to be called on IO thread.
   *
   * @param aFd [in] File descriptor to read from
   */
  virtual void OnFileCanReadWithoutBlocking(int aFd) override;

  /**
   * libevent or developer triggered functions that writes data to socket when
   * available and guarenteed non-blocking. Only to be called on IO thread.
   *
   * @param aFd [in] File descriptor to read from
   */
  virtual void OnFileCanWriteWithoutBlocking(int aFd) override;

  void OnSocketCanReceiveWithoutBlocking(int aFd);
  void OnSocketCanAcceptWithoutBlocking(int aFd);
  void OnSocketCanSendWithoutBlocking(int aFd);
  void OnSocketCanConnectWithoutBlocking(int aFd);

  /**
   * If true, do not requeue whatever task we're running
   */
  bool mShuttingDownOnIOThread;

  ConnectionStatus mConnectionStatus;

  /**
   * I/O buffer for received data
   */
  UniquePtr<UnixSocketRawData> mBuffer;
};

class SocketConnectTask final : public SocketIOTask<DroidSocketImpl> {
 public:
  SocketConnectTask(DroidSocketImpl* aDroidSocketImpl, int aFd)
      : SocketIOTask<DroidSocketImpl>(aDroidSocketImpl), mFd(aFd) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Connect(mFd);
    return NS_OK;
  }

 private:
  int mFd;
};

class SocketListenTask final : public SocketIOTask<DroidSocketImpl> {
 public:
  SocketListenTask(DroidSocketImpl* aDroidSocketImpl, int aFd)
      : SocketIOTask<DroidSocketImpl>(aDroidSocketImpl), mFd(aFd) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());

    if (!IsCanceled()) {
      GetIO()->Listen(mFd);
    }
    return NS_OK;
  }

 private:
  int mFd;
};

class SocketConnectClientFdTask final : public SocketIOTask<DroidSocketImpl> {
  explicit SocketConnectClientFdTask(DroidSocketImpl* aImpl)
      : SocketIOTask<DroidSocketImpl>(aImpl) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());

    if (!IsCanceled()) {
      GetIO()->ConnectClientFd();
    }
    return NS_OK;
  }
};

void DroidSocketImpl::Connect(int aFd) {
  MOZ_ASSERT(aFd >= 0);

  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  NS_ENSURE_TRUE_VOID(flags >= 0);

  if (!(flags & O_NONBLOCK)) {
    int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags | O_NONBLOCK));
    NS_ENSURE_TRUE_VOID(!res);
  }

  SetFd(aFd);
  mConnectionStatus = SOCKET_IS_CONNECTING;

  AddWatchers(WRITE_WATCHER, false);
}

void DroidSocketImpl::Listen(int aFd) {
  MOZ_ASSERT(aFd >= 0);

  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  NS_ENSURE_TRUE_VOID(flags >= 0);

  if (!(flags & O_NONBLOCK)) {
    int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags | O_NONBLOCK));
    NS_ENSURE_TRUE_VOID(!res);
  }

  SetFd(aFd);
  mConnectionStatus = SOCKET_IS_LISTENING;

  AddWatchers(READ_WATCHER, true);
}

void DroidSocketImpl::Accept(int aFd) {
  Close();

  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  NS_ENSURE_TRUE_VOID(flags >= 0);

  if (!(flags & O_NONBLOCK)) {
    int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags | O_NONBLOCK));
    NS_ENSURE_TRUE_VOID(!res);
  }

  SetFd(aFd);
  mConnectionStatus = SOCKET_IS_CONNECTED;

  RefPtr<SocketEventTask> task =
      new SocketEventTask(this, SocketEventTask::CONNECT_SUCCESS);
  GetConsumerThread()->PostTask(task.forget());

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void DroidSocketImpl::OnFileCanReadWithoutBlocking(int aFd) {
  if (mConnectionStatus == SOCKET_IS_CONNECTED) {
    OnSocketCanReceiveWithoutBlocking(aFd);
  } else if (mConnectionStatus == SOCKET_IS_LISTENING) {
    OnSocketCanAcceptWithoutBlocking(aFd);
  } else {
    MOZ_ASSERT_UNREACHABLE("invalid connection state for reading");
  }
}

void DroidSocketImpl::OnSocketCanReceiveWithoutBlocking(int aFd) {
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  ssize_t res = ReceiveData(aFd);
  if (res < 0) {
    /* I/O error */
    RemoveWatchers(READ_WATCHER | WRITE_WATCHER);
  } else if (!res) {
    /* EOF or peer shutdown */
    RemoveWatchers(READ_WATCHER);
  }
}

class AcceptTask final : public SocketIOTask<DroidSocketImpl> {
 public:
  AcceptTask(DroidSocketImpl* aDroidSocketImpl, int aFd)
      : SocketIOTask<DroidSocketImpl>(aDroidSocketImpl), mFd(aFd) {}

  NS_IMETHOD Run() override {
    MOZ_ASSERT(!GetIO()->IsConsumerThread());
    MOZ_ASSERT(!IsCanceled());

    GetIO()->Accept(mFd);
    return NS_OK;
  }

 private:
  int mFd;
};

class AcceptResultHandler final : public BluetoothSocketResultHandler {
 public:
  explicit AcceptResultHandler(DroidSocketImpl* aImpl) : mImpl(aImpl) {
    MOZ_ASSERT(mImpl);
  }

  void Accept(int aFd, const BluetoothAddress& aBdAddress,
              int aConnectionStatus) override {
    MOZ_ASSERT(mImpl->IsConsumerThread());

    mozilla::ScopedClose fd(aFd);  // Close received socket fd on error

    if (mImpl->IsShutdownOnConsumerThread()) {
      BT_LOGD("mConsumer is null, aborting receive!");
      return;
    }

    if (aConnectionStatus != 0) {
      mImpl->mConsumer->NotifyError();
      return;
    }

    mImpl->mConsumer->SetAddress(aBdAddress);
    RefPtr<AcceptTask> task = new AcceptTask(mImpl, fd.forget());
    mImpl->GetIOLoop()->PostTask(task.forget());
  }

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(mImpl->IsConsumerThread());
    BT_LOGD("BluetoothSocketInterface::Accept failed: %d", (int)aStatus);

    if (!mImpl->IsShutdownOnConsumerThread()) {
      // Instead of NotifyError(), call NotifyDisconnect() to trigger
      // BluetoothOppManager::OnSocketDisconnect() as
      // DroidSocketImpl::OnFileCanReadWithoutBlocking() in Firefox OS 2.0 in
      // order to keep the same behavior and reduce regression risk.
      mImpl->mConsumer->NotifyDisconnect();
    }
  }

 private:
  DroidSocketImpl* mImpl;
};

class InvokeAcceptTask final : public SocketTask<DroidSocketImpl> {
 public:
  InvokeAcceptTask(DroidSocketImpl* aImpl, int aListenFd)
      : SocketTask<DroidSocketImpl>(aImpl), mListenFd(aListenFd) {}

  NS_IMETHOD Run() override {
    DroidSocketImpl* io = SocketTask<DroidSocketImpl>::GetIO();

    MOZ_ASSERT(io->IsConsumerThread());

    // mConsumer is a non-thread-safe pointer which should only be accessed by
    // consumer-thread. This function should only run on consumer-thread to
    // avoid the illegal access.
    if (!io->IsConsumerThread()) {
      BT_WARNING("InvokeAcceptTask isn't running on the consumer thread.");
      return NS_ERROR_FAILURE;
    }

    if (io->IsShutdownOnConsumerThread()) {
      BT_LOGD("mConsumer is null, abort from Accept invoking!");
      return NS_ERROR_FAILURE;
    }
    io->mConsumer->Accept(mListenFd, new AcceptResultHandler(io));
    return NS_OK;
  }

 private:
  int mListenFd;
};

void DroidSocketImpl::OnSocketCanAcceptWithoutBlocking(int aFd) {
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  /* When a listening socket is ready for receiving data,
   * we can call |Accept| on it.
   */

  RemoveWatchers(READ_WATCHER);
  RefPtr<InvokeAcceptTask> task = new InvokeAcceptTask(this, aFd);
  GetConsumerThread()->PostTask(task.forget());
}

void DroidSocketImpl::OnFileCanWriteWithoutBlocking(int aFd) {
  if (mConnectionStatus == SOCKET_IS_CONNECTED) {
    OnSocketCanSendWithoutBlocking(aFd);
  } else if (mConnectionStatus == SOCKET_IS_CONNECTING) {
    OnSocketCanConnectWithoutBlocking(aFd);
  } else {
    MOZ_ASSERT_UNREACHABLE("invalid connection state for writing");
  }
}

void DroidSocketImpl::OnSocketCanSendWithoutBlocking(int aFd) {
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);
  MOZ_ASSERT(aFd >= 0);

  nsresult rv = SendPendingData(aFd);
  if (NS_FAILED(rv)) {
    return;
  }

  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

void DroidSocketImpl::OnSocketCanConnectWithoutBlocking(int aFd) {
  MOZ_ASSERT(!IsConsumerThread());
  MOZ_ASSERT(!mShuttingDownOnIOThread);

  /* We follow Posix behaviour here: Connect operations are
   * complete once we can write to the connecting socket.
   */

  mConnectionStatus = SOCKET_IS_CONNECTED;

  RefPtr<SocketEventTask> task =
      new SocketEventTask(this, SocketEventTask::CONNECT_SUCCESS);
  GetConsumerThread()->PostTask(task.forget());

  AddWatchers(READ_WATCHER, true);
  if (HasPendingData()) {
    AddWatchers(WRITE_WATCHER, false);
  }
}

// |DataSocketIO|

nsresult DroidSocketImpl::QueryReceiveBuffer(UnixSocketIOBuffer** aBuffer) {
  MOZ_ASSERT(aBuffer);

  if (!mBuffer) {
    mBuffer = MakeUnique<UnixSocketRawData>(MAX_READ_SIZE);
  }
  *aBuffer = mBuffer.get();

  return NS_OK;
}

/**
 * |ReceiveRunnable| transfers data received on the I/O thread
 * to an instance of |BluetoothSocket| on the consumer thread.
 */
class DroidSocketImpl::ReceiveTask final : public SocketTask<DroidSocketImpl> {
 public:
  ReceiveTask(DroidSocketImpl* aIO, UnixSocketBuffer* aBuffer)
      : SocketTask<DroidSocketImpl>(aIO), mBuffer(aBuffer) {}

  NS_IMETHOD Run() override {
    DroidSocketImpl* io = SocketTask<DroidSocketImpl>::GetIO();

    MOZ_ASSERT(io->IsConsumerThread());

    if (NS_WARN_IF(io->IsShutdownOnConsumerThread())) {
      // Since we've already explicitly closed and the close
      // happened before this, this isn't really an error.
      // return;
      return NS_ERROR_FAILURE;
    }

    BluetoothSocket* bluetoothSocket = io->GetBluetoothSocket();
    MOZ_ASSERT(bluetoothSocket);

    bluetoothSocket->ReceiveSocketData(mBuffer);
    return NS_OK;
  }

 private:
  UniquePtr<UnixSocketBuffer> mBuffer;
};

void DroidSocketImpl::ConsumeBuffer() {
  RefPtr<ReceiveTask> task = new ReceiveTask(this, mBuffer.release());
  GetConsumerThread()->PostTask(task.forget());
}

void DroidSocketImpl::DiscardBuffer() {
  // Nothing to do.
}

//
// |BluetoothSocket|
//

BluetoothSocket::BluetoothSocket(BluetoothSocketObserver* aObserver)
    : mSocketInterface(nullptr),
      mObserver(aObserver),
      mCurrentRes(nullptr),
      mImpl(nullptr) {
  MOZ_COUNT_CTOR_INHERITED(BluetoothSocket, DataSocket);
}

BluetoothSocket::~BluetoothSocket() {
  MOZ_ASSERT(!mImpl);  // Socket is closed

  MOZ_COUNT_DTOR_INHERITED(BluetoothSocket, DataSocket);
}

void BluetoothSocket::SetObserver(BluetoothSocketObserver* aObserver) {
  mObserver = aObserver;
}

class ConnectSocketResultHandler final : public BluetoothSocketResultHandler {
 public:
  explicit ConnectSocketResultHandler(DroidSocketImpl* aImpl) : mImpl(aImpl) {
    MOZ_ASSERT(mImpl);
  }

  void Connect(int aFd, const BluetoothAddress& aBdAddress,
               int aConnectionStatus) override {
    MOZ_ASSERT(mImpl->IsConsumerThread());

    if (mImpl->IsShutdownOnConsumerThread()) {
      BT_LOGD("mConsumer is null, aborting send!");
      return;
    }

    if (aConnectionStatus != 0) {
      mImpl->mConsumer->NotifyError();
      return;
    }

    mImpl->mConsumer->SetAddress(aBdAddress);
    RefPtr<SocketConnectTask> task = new SocketConnectTask(mImpl, aFd);
    mImpl->GetIOLoop()->PostTask(task.forget());
  }

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(mImpl->IsConsumerThread());
    BT_WARNING("Connect failed: %d", (int)aStatus);

    if (!mImpl->IsShutdownOnConsumerThread()) {
      // Instead of NotifyError(), call NotifyDisconnect() to trigger
      // BluetoothOppManager::OnSocketDisconnect() as
      // DroidSocketImpl::OnFileCanReadWithoutBlocking() in Firefox OS 2.0 in
      // order to keep the same behavior and reduce regression risk.
      mImpl->mConsumer->NotifyDisconnect();
    }
  }

 private:
  DroidSocketImpl* mImpl;
};

nsresult BluetoothSocket::Connect(const BluetoothAddress& aDeviceAddress,
                                  const BluetoothUuid& aServiceUuid,
                                  BluetoothSocketType aType, int aChannel,
                                  bool aAuth, bool aEncrypt,
                                  MessageLoop* aConsumerLoop,
                                  MessageLoop* aIOLoop) {
  MOZ_ASSERT(!mImpl);

  auto rv = LoadSocketInterface();
  if (NS_FAILED(rv)) {
    return rv;
  }

  SetConnectionStatus(SOCKET_CONNECTING);

  mImpl = new DroidSocketImpl(aConsumerLoop, aIOLoop, this);

  BluetoothSocketResultHandler* res = new ConnectSocketResultHandler(mImpl);
  SetCurrentResultHandler(res);

  mSocketInterface->Connect(aDeviceAddress, aType, aServiceUuid, aChannel,
                            aEncrypt, aAuth, res);

  return NS_OK;
}

nsresult BluetoothSocket::Connect(const BluetoothAddress& aDeviceAddress,
                                  const BluetoothUuid& aServiceUuid,
                                  BluetoothSocketType aType, int aChannel,
                                  bool aAuth, bool aEncrypt) {
  return Connect(aDeviceAddress, aServiceUuid, aType, aChannel, aAuth, aEncrypt,
                 MessageLoop::current(), XRE_GetIOMessageLoop());
}

class ListenResultHandler final : public BluetoothSocketResultHandler {
 public:
  explicit ListenResultHandler(DroidSocketImpl* aImpl) : mImpl(aImpl) {
    MOZ_ASSERT(mImpl);
  }

  void Listen(int aFd) override {
    MOZ_ASSERT(mImpl->IsConsumerThread());

    RefPtr<SocketListenTask> task = new SocketListenTask(mImpl, aFd);
    mImpl->GetIOLoop()->PostTask(task.forget());
  }

  void OnError(BluetoothStatus aStatus) override {
    MOZ_ASSERT(mImpl->IsConsumerThread());

    BT_WARNING("Listen failed: %d", (int)aStatus);
  }

 private:
  DroidSocketImpl* mImpl;
};

nsresult BluetoothSocket::Listen(const nsAString& aServiceName,
                                 const BluetoothUuid& aServiceUuid,
                                 BluetoothSocketType aType, int aChannel,
                                 bool aAuth, bool aEncrypt,
                                 MessageLoop* aConsumerLoop,
                                 MessageLoop* aIOLoop) {
  MOZ_ASSERT(!mImpl);

  auto rv = LoadSocketInterface();
  if (NS_FAILED(rv)) {
    return rv;
  }

  BluetoothServiceName serviceName;
  rv = StringToServiceName(aServiceName, serviceName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  SetConnectionStatus(SOCKET_LISTENING);

  mImpl = new DroidSocketImpl(aConsumerLoop, aIOLoop, this);

  BluetoothSocketResultHandler* res = new ListenResultHandler(mImpl);
  SetCurrentResultHandler(res);

  mSocketInterface->Listen(aType, serviceName, aServiceUuid, aChannel, aEncrypt,
                           aAuth, res);

  return NS_OK;
}

nsresult BluetoothSocket::Listen(const nsAString& aServiceName,
                                 const BluetoothUuid& aServiceUuid,
                                 BluetoothSocketType aType, int aChannel,
                                 bool aAuth, bool aEncrypt) {
  return Listen(aServiceName, aServiceUuid, aType, aChannel, aAuth, aEncrypt,
                MessageLoop::current(), XRE_GetIOMessageLoop());
}

nsresult BluetoothSocket::Accept(int aListenFd,
                                 BluetoothSocketResultHandler* aRes) {
  auto rv = LoadSocketInterface();
  if (NS_FAILED(rv)) {
    return rv;
  }

  SetCurrentResultHandler(aRes);
  mSocketInterface->Accept(aListenFd, aRes);

  return NS_OK;
}

void BluetoothSocket::ReceiveSocketData(UniquePtr<UnixSocketBuffer>& aBuffer) {
  if (mObserver) {
    mObserver->ReceiveSocketData(this, aBuffer);
  }
}

// |DataSocket|

void BluetoothSocket::SendSocketData(UnixSocketIOBuffer* aBuffer) {
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(mImpl->IsConsumerThread());
  MOZ_ASSERT(!mImpl->IsShutdownOnConsumerThread());

  RefPtr<SocketIOSendTask<DroidSocketImpl, UnixSocketIOBuffer>> task =
      new SocketIOSendTask<DroidSocketImpl, UnixSocketIOBuffer>(mImpl, aBuffer);
  mImpl->GetIOLoop()->PostTask(task.forget());
}

// |SocketBase|

void BluetoothSocket::Close() {
  if (!mImpl) {
    return;
  }

  NotifyDisconnect();
}

void BluetoothSocket::OnConnectSuccess() {
  SetCurrentResultHandler(nullptr);

  if (mObserver) {
    mObserver->OnSocketConnectSuccess(this);
  }
}

void BluetoothSocket::OnConnectError() {
  auto observer = mObserver;

  Cleanup();

  if (observer) {
    observer->OnSocketConnectError(this);
  }
}

void BluetoothSocket::OnDisconnect() {
  auto observer = mObserver;

  Cleanup();

  if (observer) {
    observer->OnSocketDisconnect(this);
  }
}

nsresult BluetoothSocket::LoadSocketInterface() {
  if (mSocketInterface) {
    return NS_OK;
  }

  auto interface = BluetoothInterface::GetInstance();
  NS_ENSURE_TRUE(!!interface, NS_ERROR_FAILURE);

  auto socketInterface = interface->GetBluetoothSocketInterface();
  NS_ENSURE_TRUE(!!socketInterface, NS_ERROR_FAILURE);

  mSocketInterface = socketInterface;

  return NS_OK;
}

void BluetoothSocket::Cleanup() {
  MOZ_ASSERT(mSocketInterface);
  MOZ_ASSERT(mImpl);
  MOZ_ASSERT(mImpl->IsConsumerThread());

  // Stop any watching |SocketMessageWatcher|
  if (mCurrentRes) {
    mSocketInterface->Close(mCurrentRes);
  }

  // From this point on, we consider mImpl as being deleted. We
  // sever the relationship here so any future calls to listen
  // or connect will create a new implementation.
  mImpl->ShutdownOnConsumerThread();
  RefPtr<SocketIOShutdownTask> task = new SocketIOShutdownTask(mImpl);
  mImpl->GetIOLoop()->PostTask(task.forget());
  mImpl = nullptr;

  mSocketInterface = nullptr;
  mObserver = nullptr;
  mCurrentRes = nullptr;
  mDeviceAddress.Clear();
}

// static function
void BluetoothSocket::Uninit(RefPtr<BluetoothSocket> aSocket) {
  if (aSocket == nullptr) {
    return;
  }

  aSocket->SetObserver(nullptr);

  if (aSocket->GetConnectionStatus() != SOCKET_DISCONNECTED) {
    aSocket->Close();
  }
}
