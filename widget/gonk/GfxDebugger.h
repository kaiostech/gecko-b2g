/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et ft=cpp: tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_DEBUGGER_H
#define GFX_DEBUGGER_H

#include <sys/socket.h>

#include "GfxDebugger_defs.h"
#include "mozilla/ipc/ListenSocket.h"
#include "mozilla/ipc/ListenSocketConsumer.h"
#include "mozilla/ipc/StreamSocket.h"
#include "mozilla/ipc/StreamSocketConsumer.h"
#include "mozilla/ipc/UnixSocketConnector.h"
//#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/layers/SharedBufferManagerParent.h"

namespace mozilla {
namespace ipc {

class GfxDebuggerConnector final : public UnixSocketConnector {
 public:
  friend class GfxDebugger;

  GfxDebuggerConnector(const nsACString& aSocketName,
                       const char** const aAllowedUsers);

  nsresult ConvertAddressToString(const struct sockaddr& aAddress,
                                  socklen_t aAddressLength,
                                  nsACString& aAddressString);

  nsresult CreateListenSocket(struct sockaddr* aAddress,
                              socklen_t* aAddressLength, int& aListenFd);

  nsresult AcceptStreamSocket(int aListenFd, struct sockaddr* aAddress,
                              socklen_t* aAddressLen, int& aStreamFd);

  nsresult CreateStreamSocket(struct sockaddr* aAddress,
                              socklen_t* aAddressLength, int& aStreamFd);

  nsresult Duplicate(UnixSocketConnector*& aConnector);

 private:
  nsCString mSocketName;

  const char** const mAllowedUsers;

  int mStreamFd;

  ~GfxDebuggerConnector() {}

  nsresult CreateSocket(int& aFd) const;

  nsresult SetSocketFlags(int aFd) const;

  nsresult CheckPermission(int aFd) const;

  nsresult CreateAddress(struct sockaddr& aAddress,
                         socklen_t& aAddressLength) const;
};

class GfxDebugger final : public StreamSocketConsumer,
                          public ListenSocketConsumer {
 public:
  GfxDebugger();

  static GfxDebugger* GetInstance();

  void Shutdown();

  void ReceiveSocketData(int aIndex, UniquePtr<UnixSocketBuffer>& aBuffer);

  void OnConnectSuccess(int aIndex);

  void OnConnectError(int aIndex);

  void OnDisconnect(int aIndex);

  // void SetLayerManager(mozilla::layers::LayerManagerComposite* lmc) {
  //   mLayerManager = lmc;
  // }

  void SetCompositorBridge(mozilla::layers::CompositorBridgeParent* cbp) {
    mCompositorBridge = cbp;
  }

  void SetSharedBufferManagerParent(
      mozilla::layers::SharedBufferManagerParent* sbmp) {
    mSharedBufferManagerParent = sbmp;
  }

 protected:
 private:
  enum SocketType { LISTEN_SOCKET, STREAM_SOCKET };
  ~GfxDebugger(){};

  nsCString mSocketName;

  GfxDebuggerConnector* mConnector;

  RefPtr<ListenSocket> mListenSocket;

  RefPtr<StreamSocket> mStreamSocket;

  void Listen();

  bool mShutdown;

  //mozilla::layers::LayerManagerComposite* mLayerManager;

  mozilla::layers::CompositorBridgeParent* mCompositorBridge;

  mozilla::layers::SharedBufferManagerParent* mSharedBufferManagerParent;
};

}  // namespace ipc
}  // namespace mozilla

#endif
