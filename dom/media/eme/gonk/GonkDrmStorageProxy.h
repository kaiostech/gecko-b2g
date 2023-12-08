/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmStorageProxy_H
#define GonkDrmStorageProxy_H

#include "nsCOMPtr.h"
#include "nsIGonkDrmStorage.h"
#include "nsString.h"

namespace mozilla {

class GonkDrmStorageProxy final {
 public:
  // clang-format off
  typedef std::function<void(bool /* aSuccess */)> GenericCallback;
  typedef std::function<void(bool /* aSuccess */,
                             const nsACString& /* aMimeType */,
                             const nsACString& /* aSessionType */,
                             const nsACString& /* aKeySetId */)>
      GetCallback;
  // clang-format on

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(GonkDrmStorageProxy)

  GonkDrmStorageProxy(const nsAString& aOrigin, const nsAString& aKeySystem);

  bool Init();

  void Shutdown();

  // The following storage access API can be called on any Gecko thread. The
  // callback will be dispatched to the same thread or called recursively. Note
  // that EME session ID should be unique globally.

  void Add(const nsACString& aEmeSessionId, const nsACString& aMimeType,
           const nsACString& aSessionType, const nsACString& aKeySetId,
           const GenericCallback& aCallback);

  void Get(const nsACString& aEmeSessionId, const GetCallback& aCallback);

  void Remove(const nsACString& aEmeSessionId,
              const GenericCallback& aCallback);

  void Clear(const GenericCallback& aCallback);

 private:
  ~GonkDrmStorageProxy() = default;

  const nsString mOrigin;
  const nsString mKeySystem;
  nsCOMPtr<nsIGonkDrmStorage> mStorage;
};

}  // namespace mozilla

#endif
