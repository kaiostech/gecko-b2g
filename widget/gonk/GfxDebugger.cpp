/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */

/* (c) 2017 KAI OS TECHNOLOGIES (HONG KONG) LIMITED All rights reserved. This
 * file or any portion thereof may not be reproduced or used in any manner
 * whatsoever without the express written permission of KAI OS TECHNOLOGIES
 * (HONG KONG) LIMITED. KaiOS is the trademark of KAI OS TECHNOLOGIES (HONG
 * KONG) LIMITED or its affiliate company and may be registered in some
 * jurisdictions. All other trademarks are the property of their respective
 * owners.
 */

#include <binder/Parcel.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/un.h>

#include "GfxDebugger.h"
#include "GonkScreenRecord.h"
#include "GonkScreenShot.h"
#include "mozilla/layers/SharedBufferManagerParent.h"

#ifdef LOG_TAG
#  undef LOG_TAG
#endif
#define LOG_TAG "GfxDebugger"

#include <android/log.h>
#define GD_LOGD(args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, args)
#define GD_LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, args)
#define GD_LOGI(args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, args)
#define GD_LOGW(args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, args)

#define GD_SOCKET_NAME "/dev/socket/gfxdebugger-ipc"

using namespace android;
using mozilla::layers::APZCTreeManager;
//using mozilla::layers::AsyncPanZoomController;
using mozilla::layers::SharedBufferManagerParent;

namespace mozilla {
namespace ipc {

GfxDebuggerConnector::GfxDebuggerConnector(const nsACString& aSocketName,
                                           const char** const aAllowedUsers)
    : mSocketName(aSocketName), mAllowedUsers(aAllowedUsers){};

nsresult GfxDebuggerConnector::CreateSocket(int& aFd) const {
  unlink(mSocketName.get());

  aFd = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (aFd < 0) {
    GD_LOGE("Could not open GFX debugger socket!");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult GfxDebuggerConnector::SetSocketFlags(int aFd) const {
  static const int sReuseAddress = 1;

  // Set close-on-exec bit.
  int flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFD));
  if (flags < 0) {
    return NS_ERROR_FAILURE;
  }
  flags |= FD_CLOEXEC;
  int res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFD, flags));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  // Set non-blocking status flag.
  flags = TEMP_FAILURE_RETRY(fcntl(aFd, F_GETFL));
  if (flags < 0) {
    return NS_ERROR_FAILURE;
  }
  flags |= O_NONBLOCK;
  res = TEMP_FAILURE_RETRY(fcntl(aFd, F_SETFL, flags));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  // Set socket addr to be reused even if kernel is still waiting to close.
  res = setsockopt(aFd, SOL_SOCKET, SO_REUSEADDR, &sReuseAddress,
                   sizeof(sReuseAddress));
  if (res < 0) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult GfxDebuggerConnector::CheckPermission(int aFd) const {
  struct ucred userCred;
  socklen_t len = sizeof(userCred);

  if (getsockopt(aFd, SOL_SOCKET, SO_PEERCRED, &userCred, &len)) {
    return NS_ERROR_FAILURE;
  }

  const struct passwd* userInfo = getpwuid(userCred.uid);
  if (!userInfo) {
    return NS_ERROR_FAILURE;
  }

  GD_LOGD("uid=%d, name=%s", userCred.uid, userInfo->pw_name);
  if (!mAllowedUsers) {
    return NS_ERROR_FAILURE;
  }

  for (const char** user = mAllowedUsers; *user; ++user) {
    if (!strcmp(*user, userInfo->pw_name)) {
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

nsresult GfxDebuggerConnector::CreateAddress(struct sockaddr& aAddress,
                                             socklen_t& aAddressLength) const {
  struct sockaddr_un* address =
      reinterpret_cast<struct sockaddr_un*>(&aAddress);

  size_t namesiz = strlen(mSocketName.get()) + 1;  // include trailing '\0'

  if (namesiz > sizeof(address->sun_path)) {
    GD_LOGE("Address too long for socket struct!");
    return NS_ERROR_FAILURE;
  }

  address->sun_family = AF_UNIX;
  memcpy(address->sun_path, mSocketName.get(), namesiz);

  aAddressLength = offsetof(struct sockaddr_un, sun_path) + namesiz;

  return NS_OK;
}

nsresult GfxDebuggerConnector::ConvertAddressToString(
    const struct sockaddr& aAddress, socklen_t aAddressLength,
    nsACString& aAddressString) {
  MOZ_ASSERT(aAddress.sa_family == AF_UNIX);

  const struct sockaddr_un* un =
      reinterpret_cast<const struct sockaddr_un*>(&aAddress);

  size_t len = aAddressLength - offsetof(struct sockaddr_un, sun_path);

  aAddressString.Assign(un->sun_path, len);

  return NS_OK;
}

nsresult GfxDebuggerConnector::CreateListenSocket(struct sockaddr* aAddress,
                                                  socklen_t* aAddressLength,
                                                  int& aListenFd) {
  ScopedClose fd;

  nsresult rv = CreateSocket(fd.rwget());
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (aAddress && aAddressLength) {
    rv = CreateAddress(*aAddress, *aAddressLength);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  aListenFd = fd.forget();
  return NS_OK;
}

nsresult GfxDebuggerConnector::AcceptStreamSocket(int aListenFd,
                                                  struct sockaddr* aAddress,
                                                  socklen_t* aAddressLength,
                                                  int& aStreamFd) {
  ScopedClose fd(
      TEMP_FAILURE_RETRY(accept(aListenFd, aAddress, aAddressLength)));
  if (fd < 0) {
    NS_WARNING("Cannot accept file descriptor!");
    return NS_ERROR_FAILURE;
  }
  nsresult rv = SetSocketFlags(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = CheckPermission(fd);
  if (NS_FAILED(rv)) {
    return rv;
  }

  mStreamFd = aStreamFd = fd.forget();

  return NS_OK;
}

nsresult GfxDebuggerConnector::CreateStreamSocket(struct sockaddr* aAddress,
                                                  socklen_t* aAddressLength,
                                                  int& aStreamFd) {
  return NS_ERROR_FAILURE;
}

nsresult GfxDebuggerConnector::Duplicate(UnixSocketConnector*& aConnector) {
  aConnector = new GfxDebuggerConnector(*this);

  return NS_OK;
}

//-----------------------------------------------------------------------------
static const char* GFXDEBUGGER_ALLOWED_USERS[] = {"root", "system", NULL};

GfxDebugger::GfxDebugger() : mShutdown(false) {
  mSocketName.AssignLiteral(GD_SOCKET_NAME);
  Listen();
}

/* static */
GfxDebugger* GfxDebugger::GetInstance() {
  static GfxDebugger* mInstance = nullptr;

  if (mInstance == nullptr) {
    mInstance = new GfxDebugger;
  }
  return mInstance;
}

void GfxDebugger::Shutdown() {
  // We set mShutdown first, so that |OnDisconnect| won't try to reconnect.
  mShutdown = true;

  if (mStreamSocket) {
    mStreamSocket->Close();
    mStreamSocket = nullptr;
  }
  if (mListenSocket) {
    mListenSocket->Close();
    mListenSocket = nullptr;
  }
}

void GfxDebugger::Listen() {
  if (mStreamSocket) {
    mStreamSocket->Close();
  } else {
    mStreamSocket = new StreamSocket(this, STREAM_SOCKET);
  }

  if (!mListenSocket) {
    // We only ever allocate one |ListenSocket|...
    mListenSocket = new ListenSocket(this, LISTEN_SOCKET);
    mConnector =
        new GfxDebuggerConnector(mSocketName, GFXDEBUGGER_ALLOWED_USERS);
    mListenSocket->Listen(mConnector, mStreamSocket);
  } else {
    // ... but keep it open.
    mListenSocket->Listen(mStreamSocket);
  }
}

static bool gRecordingInProgress = false;
void OnScreenrecordFinish(int streamFd, int res) {
  // Dispatch the job back to main thread for notifying client.
  nsCOMPtr<nsIRunnable> screenrecordFinishCB = NS_NewRunnableFunction(
      "mozilla::ipc::GfxDebugger::ScreenrecordFinish",
      [streamFd, res]() -> void {
        Parcel reply;
        reply.writeInt32(res);
        GD_LOGD("screenrecord result: %d", res);
        send(streamFd, reply.data(), reply.dataSize(), 0);
        gRecordingInProgress = false;
      });
  NS_DispatchToMainThread(screenrecordFinishCB);
}

void GfxDebugger::ReceiveSocketData(int aIndex,
                                    UniquePtr<UnixSocketBuffer>& aBuffer) {
  Parcel parcel;
  UnixSocketBuffer* usb = aBuffer.get();
  while (usb->GetSize()) {
    GD_LOGD("read %d bytes from socket buff", usb->GetSize());
    const uint8_t* data = usb->GetData();
    parcel.setData(data, usb->GetSize());

    parcel.readUint32();  // parcel size
    uint32_t cmd = parcel.readUint32();
    switch (cmd) {
      case GD_CMD_GRALLOC: {
        uint32_t op = parcel.readUint32();
        aBuffer->Consume(usb->GetSize());

        switch (op) {
          case GRALLOC_OP_LIST: {
            std::vector<int64_t> gb_indices;
            SharedBufferManagerParent::ListGrallocBuffers(gb_indices);

            Parcel reply;
            reply.writeUint32(gb_indices.size());
            for (uint64_t i = 0; i < gb_indices.size(); i++) {
              reply.writeInt64(gb_indices[i]);
            }
            send(mConnector->mStreamFd, reply.data(), reply.dataSize(), 0);
            break;
          }

          case GRALLOC_OP_DUMP: {
            uint32_t index = parcel.readUint32();
            GD_LOGD("dumping gralloc[%d]:", index);
            aBuffer->Consume(usb->GetSize());
            char filename[PATH_MAX];
            SharedBufferManagerParent::DumpGrallocBuffer(index, filename);

            Parcel reply;
            reply.writeCString(filename);
            GD_LOGD("filename=%s, strlen=%d, writed %d bytes", filename,
                    strlen(filename), reply.dataSize());
            send(mConnector->mStreamFd, reply.data(), reply.dataSize(), 0);
            break;
          }
        }
      }  // case GD_CMD_GRALLOC
      break;

      case GD_CMD_SCREENCAP: {
        uint32_t op = parcel.readUint32();
        aBuffer->Consume(usb->GetSize());

        switch (op) {
          case SCREENCAP_OP_CAPTURE: {
            uint32_t displayId = parcel.readUint32();
            bool pngEncode = parcel.readBool();
            const char* filePath = parcel.readCString();
            GD_LOGD("screencap[%s]:", filePath);
            int res = GonkScreenShot::capture(displayId, pngEncode, filePath);

            Parcel reply;
            reply.writeInt32(res);
            GD_LOGD("screencap result: %d", res);
            send(mConnector->mStreamFd, reply.data(), reply.dataSize(), 0);
            break;
          }
        }
      }  // case GD_CMD_SCREENCAP
      break;

      case GD_CMD_SCREENRECORD: {
        uint32_t op = parcel.readUint32();
        aBuffer->Consume(usb->GetSize());

        switch (op) {
          case SCREENRECORD_OP_CAPTURE: {
            if (gRecordingInProgress) {
              OnScreenrecordFinish(mConnector->mStreamFd, EBUSY);
              break;
            }
            uint32_t displayId = parcel.readUint32();
            uint32_t outputFormat = parcel.readUint32();
            uint32_t timeLimitSec = parcel.readUint32();
            const char* filePath = parcel.readCString();
            GD_LOGD("screenrecord[%s]:", filePath);
            // Dispatch the job to non main/IO thread to prevent B2G jobs
            // and composition jobs being blocked.
            RefPtr<GonkScreenRecord> runnable = new GonkScreenRecord(
                displayId, outputFormat, timeLimitSec, filePath,
                &OnScreenrecordFinish, mConnector->mStreamFd);

            nsCOMPtr<nsIEventTarget> target =
                do_GetService(NS_STREAMTRANSPORTSERVICE_CONTRACTID);
            MOZ_ASSERT(target);

            target->Dispatch(runnable.forget(), NS_DISPATCH_NORMAL);
            gRecordingInProgress = true;
            break;
          }
        }
      }  // case GD_CMD_SCREENRECORD
      break;

      default:
        GD_LOGE("Unknown command: %d", cmd);
        break;
    }
  }
}

void GfxDebugger::OnConnectSuccess(int aIndex) {
  struct stat fstat;
  stat(GD_SOCKET_NAME, &fstat);
  uint32_t old_mode = fstat.st_mode & 511;

  int ret = chmod(GD_SOCKET_NAME,
                  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
  if (ret == -1) {
    ALOGE("Failed on chmod: %s, err=%s", GD_SOCKET_NAME, strerror(errno));
  }

  stat(GD_SOCKET_NAME, &fstat);
  uint32_t new_mode = fstat.st_mode & 511;

  GD_LOGD(
      "Connector::CreateListenSocket- changing socket mode from "
      "%04o to %04o(octal)",
      old_mode, new_mode);
}

void GfxDebugger::OnConnectError(int aIndex) { mStreamSocket->Close(); }

void GfxDebugger::OnDisconnect(int aIndex) {
  switch (aIndex) {
    case LISTEN_SOCKET:
      // Listen socket disconnected; start a new one.
      mListenSocket = nullptr;
      Listen();
      break;
    case STREAM_SOCKET:
      // Stream socket disconnected; start listening again.
      Listen();
      break;
  }
}

}  // namespace ipc
}  // namespace mozilla
