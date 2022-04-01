/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaFormatReaderProxy_h_
#define MediaFormatReaderProxy_h_

#include "MediaFormatReader.h"
#include "MediaOffloadPlayer.h"
#include "mozilla/Variant.h"

#define PROXY_MEMBER_FUNCTION(aMemberFunc)                      \
  template <typename... Args>                                   \
  decltype(auto) aMemberFunc(Args&&... args) {                  \
    return mTarget.match([&](auto& aTarget) -> decltype(auto) { \
      return aTarget->aMemberFunc(std::forward<Args>(args)...); \
    });                                                         \
  }

namespace mozilla {

DDLoggedTypeDeclName(MediaFormatReaderProxy);

class MediaFormatReaderProxy
    : public DecoderDoctorLifeLogger<MediaFormatReaderProxy> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaFormatReaderProxy)

 public:
  MediaFormatReaderProxy(MediaFormatReader* aReader);
  MediaFormatReaderProxy(MediaOffloadPlayer* aPlayer);

  PROXY_MEMBER_FUNCTION(RequestDebugInfo)
  PROXY_MEMBER_FUNCTION(OwnerThread)
  PROXY_MEMBER_FUNCTION(OnEncrypted)
  PROXY_MEMBER_FUNCTION(OnWaitingForKey)
  PROXY_MEMBER_FUNCTION(OnDecodeWarning)
  PROXY_MEMBER_FUNCTION(OnStoreDecoderBenchmark)

  // Hand-craft these functions instead of using generic template, because
  // MediaDecoder invokes them by function pointers.
  void NotifyDataArrived();
  RefPtr<SetCDMPromise> SetCDMProxy(CDMProxy* aProxy);
  void UpdateCompositor(already_AddRefed<layers::KnowsCompositor> aCompositor);

 private:
  ~MediaFormatReaderProxy();
  Variant<RefPtr<MediaFormatReader>, RefPtr<MediaOffloadPlayer>> mTarget;
};

}  // namespace mozilla

#undef PROXY_MEMBER_FUNCTION

#endif
