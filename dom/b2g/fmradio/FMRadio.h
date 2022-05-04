/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_FMRadio_h
#define mozilla_dom_FMRadio_h

#include "AudioChannelAgent.h"
#include "FMRadioCommon.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "nsWeakReference.h"

BEGIN_FMRADIO_NAMESPACE

class Promise;

class FMRadio final : public DOMEventTargetHelper,
                      public FMRadioEventObserver,
                      public nsSupportsWeakReference,
                      public nsIAudioChannelAgentCallback {
  friend class FMRadioRequest;

 public:
  FMRadio();

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIAUDIOCHANNELAGENTCALLBACK

  void Init(nsIGlobalObject* aGlobal);
  void Shutdown();

  /* FMRadioEventObserver */
  virtual void Notify(const FMRadioEventType& aType) override;

  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  static bool Enabled();

  bool RdsEnabled();

  bool AntennaAvailable() const;

  Nullable<double> GetFrequency() const;

  double FrequencyUpperBound() const;

  double FrequencyLowerBound() const;

  double ChannelWidth() const;

  uint32_t RdsGroupMask() const;

  void SetRdsGroupMask(uint32_t aRdsGroupMask);

  Nullable<unsigned short> GetPi() const;

  Nullable<uint8_t> GetPty() const;

  void GetPs(DOMString& aPsname) const;

  void GetRt(DOMString& aRadiotext) const;

  void GetRdsgroup(JSContext* cx, JS::MutableHandle<JSObject*> retval);

  already_AddRefed<Promise> Enable(double aFrequency);

  already_AddRefed<Promise> Disable();

  already_AddRefed<Promise> SetFrequency(double aFrequency);

  already_AddRefed<Promise> SeekUp();

  already_AddRefed<Promise> SeekDown();

  already_AddRefed<Promise> CancelSeek();

  already_AddRefed<Promise> EnableRDS();

  already_AddRefed<Promise> DisableRDS();

  IMPL_EVENT_HANDLER(enabled);
  IMPL_EVENT_HANDLER(disabled);
  IMPL_EVENT_HANDLER(rdsenabled);
  IMPL_EVENT_HANDLER(rdsdisabled);
  IMPL_EVENT_HANDLER(frequencychange);
  IMPL_EVENT_HANDLER(pichange);
  IMPL_EVENT_HANDLER(ptychange);
  IMPL_EVENT_HANDLER(pschange);
  IMPL_EVENT_HANDLER(rtchange);
  IMPL_EVENT_HANDLER(newrdsgroup);

 private:
  ~FMRadio();

  void EnableAudioChannelAgent();
  void DisableAudioChannelAgent();
  bool IsPlayingThroughAudioChannel();

  uint32_t mRdsGroupMask;
  double mCachedFrequency;
  bool mSuspendedByAudioChannel;
  bool mAudioChannelAgentEnabled;
  bool mHasInternalAntenna;
  bool mIsShutdown;

  class MediaControlKeyListener;

  RefPtr<AudioChannelAgent> mAudioChannelAgent;
  RefPtr<MediaControlKeyListener> mMediaControlKeyListener;
};

END_FMRADIO_NAMESPACE

#endif  // mozilla_dom_FMRadio_h
