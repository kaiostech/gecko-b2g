/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaDecoderStateMachineProxy_h__
#define MediaDecoderStateMachineProxy_h__

#include "MediaDecoderStateMachine.h"
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

DDLoggedTypeDeclName(MediaDecoderStateMachineProxy);

class MediaDecoderStateMachineProxy
    : public DecoderDoctorLifeLogger<MediaDecoderStateMachineProxy> {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MediaDecoderStateMachineProxy)

 public:
  MediaDecoderStateMachineProxy(MediaDecoderStateMachine* aStateMachine);
  MediaDecoderStateMachineProxy(MediaOffloadPlayer* aPlayer);

  PROXY_MEMBER_FUNCTION(Init)
  PROXY_MEMBER_FUNCTION(OwnerThread)
  PROXY_MEMBER_FUNCTION(RequestDebugInfo)
  PROXY_MEMBER_FUNCTION(InvokeSeek)
  PROXY_MEMBER_FUNCTION(DispatchSetPlaybackRate)
  PROXY_MEMBER_FUNCTION(BeginShutdown)
  PROXY_MEMBER_FUNCTION(DispatchSetFragmentEndTime)
  PROXY_MEMBER_FUNCTION(DispatchCanPlayThrough)
  PROXY_MEMBER_FUNCTION(DispatchIsLiveStream)
  PROXY_MEMBER_FUNCTION(TimedMetadataEvent)
  PROXY_MEMBER_FUNCTION(OnMediaNotSeekable)
  PROXY_MEMBER_FUNCTION(MetadataLoadedEvent)
  PROXY_MEMBER_FUNCTION(FirstFrameLoadedEvent)
  PROXY_MEMBER_FUNCTION(OnPlaybackEvent)
  PROXY_MEMBER_FUNCTION(OnPlaybackErrorEvent)
  PROXY_MEMBER_FUNCTION(OnDecoderDoctorEvent)
  PROXY_MEMBER_FUNCTION(OnTrackInfoUpdatedEvent)
  PROXY_MEMBER_FUNCTION(OnNextFrameStatus)
  PROXY_MEMBER_FUNCTION(OnTrackInfoUpdated)
  PROXY_MEMBER_FUNCTION(OnSecondaryVideoContainerInstalled)
  PROXY_MEMBER_FUNCTION(SizeOfVideoQueue)
  PROXY_MEMBER_FUNCTION(SizeOfAudioQueue)
  PROXY_MEMBER_FUNCTION(SetVideoDecodeMode)
  PROXY_MEMBER_FUNCTION(InvokeSetSink)
  PROXY_MEMBER_FUNCTION(InvokeSuspendMediaSink)
  PROXY_MEMBER_FUNCTION(InvokeResumeMediaSink)
  PROXY_MEMBER_FUNCTION(CanonicalBuffered)
  PROXY_MEMBER_FUNCTION(CanonicalDuration)
  PROXY_MEMBER_FUNCTION(CanonicalCurrentPosition)
  PROXY_MEMBER_FUNCTION(CanonicalIsAudioDataAudible)
  PROXY_MEMBER_FUNCTION(SetCDMProxy)

 private:
  ~MediaDecoderStateMachineProxy();
  Variant<RefPtr<MediaDecoderStateMachine>, RefPtr<MediaOffloadPlayer>> mTarget;
};

}  // namespace mozilla

#undef PROXY_MEMBER_FUNCTION

#endif
