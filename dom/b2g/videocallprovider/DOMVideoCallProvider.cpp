/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMSurfaceControl.h"
#include "IDOMSurfaceControlCallback.h"
#include "mozilla/dom/DOMVideoCallProvider.h"
#include "mozilla/dom/DOMVideoCallProfile.h"
#include "mozilla/dom/DOMVideoCallCameraCapabilities.h"
#include "nsISupportsImpl.h"
#include "nsPIDOMWindow.h"
#include "mozilla/dom/VideoCallCameraCapabilitiesChangeEvent.h"
#include "mozilla/dom/VideoCallPeerDimensionsEvent.h"
#include "mozilla/dom/VideoCallQualityEvent.h"
#include "mozilla/dom/VideoCallSessionChangeEvent.h"
#include "mozilla/dom/VideoCallSessionModifyRequestEvent.h"
#include "mozilla/dom/VideoCallSessionModifyResponseEvent.h"
#include "nsIVideoCallProvider.h"

#include <android/log.h>
#undef LOG
#define LOG(args...) \
  __android_log_print(ANDROID_LOG_INFO, "DOMVideoCallProvider", ##args)

#define FEED_TEST_DATA_TO_PRODUCER
#ifdef FEED_TEST_DATA_TO_PRODUCER
#  include <cutils/properties.h>
#endif

using mozilla::ErrorResult;

namespace mozilla {
namespace dom {

const int16_t TYPE_DISPLAY = 0;
const int16_t TYPE_PREVIEW = 1;

const int16_t ROTATE_ANGLE = 90;

class SurfaceControlBack : public IDOMSurfaceControlCallback {
 public:
  NS_INLINE_DECL_REFCOUNTING(SurfaceControlBack)

  SurfaceControlBack(RefPtr<DOMVideoCallProvider> aProvider, int16_t aType,
                     const SurfaceConfiguration& aConfig);

  virtual void OnProducerCreated(
      android::sp<android::IGraphicBufferProducer> aProducer) override;
  virtual void OnProducerDestroyed() override;
  void Shutdown();
  android::sp<android::IGraphicBufferProducer> getProducer();

 protected:
  ~SurfaceControlBack() {
    LOG("%s", __FUNCTION__);
    Shutdown();
  }

 private:
  android::sp<android::IGraphicBufferProducer> mProducer;
  RefPtr<DOMVideoCallProvider> mProvider;
  int16_t mType;
  uint16_t mWidth;
  uint16_t mHeight;

  void SetProducer(android::sp<android::IGraphicBufferProducer> aProducer);
};

SurfaceControlBack::SurfaceControlBack(RefPtr<DOMVideoCallProvider> aProvider,
                                       int16_t aType,
                                       const SurfaceConfiguration& aConfig)
    : mProducer(nullptr),
      mProvider(aProvider),
      mType(aType),
      mWidth(aConfig.mPreviewSize.mWidth),
      mHeight(aConfig.mPreviewSize.mHeight) {
  LOG("%s", __FUNCTION__);
}

void SurfaceControlBack::OnProducerCreated(
    android::sp<android::IGraphicBufferProducer> aProducer) {
  LOG("OnProducerCreated: %p", aProducer.get());
  SetProducer(aProducer);
}

void SurfaceControlBack::OnProducerDestroyed() {
  LOG("OnProducerDestroyed");
  SetProducer(nullptr);
}

void SurfaceControlBack::SetProducer(
    android::sp<android::IGraphicBufferProducer> aProducer) {
  LOG("%s, mType: %d", __FUNCTION__, mType);
  mProducer = aProducer;
  mProvider->SetSurface(mType, aProducer, mWidth, mHeight);
}

void SurfaceControlBack::Shutdown() {}

android::sp<android::IGraphicBufferProducer> SurfaceControlBack::getProducer() {
  return mProducer;
}

class DOMVideoCallProvider::Listener final : public nsIVideoCallCallback {
  DOMVideoCallProvider* mVideoCallProvider;

 public:
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIVIDEOCALLCALLBACK(mVideoCallProvider)

  explicit Listener(DOMVideoCallProvider* aVideoCallProvider)
      : mVideoCallProvider(aVideoCallProvider) {
    MOZ_ASSERT(mVideoCallProvider);
  }

  void Disconnect() {
    MOZ_ASSERT(mVideoCallProvider);
    mVideoCallProvider = nullptr;
  }

 private:
  ~Listener() { MOZ_ASSERT(!mVideoCallProvider); }
};

NS_IMPL_ISUPPORTS(DOMVideoCallProvider::Listener, nsIVideoCallCallback)

// WebAPI VideoCallProvider.webidl

NS_IMPL_ADDREF_INHERITED(DOMVideoCallProvider, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(DOMVideoCallProvider, DOMEventTargetHelper)

NS_IMPL_CYCLE_COLLECTION_CLASS(DOMVideoCallProvider)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(DOMVideoCallProvider,
                                                  DOMEventTargetHelper)
  // Don't traverse mListener because it doesn't keep any reference to
  // DOMVideoCallProvider but a raw pointer instead. Neither does
  // mVideoCallProvider because it's an xpcom service owned object and is only
  // released at shutting down.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mProvider)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(DOMVideoCallProvider,
                                                DOMEventTargetHelper)
  tmp->Shutdown();
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mProvider)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(DOMVideoCallProvider)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

DOMVideoCallProvider::DOMVideoCallProvider(nsPIDOMWindowInner* aWindow,
                                           nsIVideoCallProvider* aProvider)
    : DOMEventTargetHelper(aWindow),
      mOrientation(0),
      mZoom(0),
      mMaxZoom(0.0),
      mZoomSupported(false),
      mProvider(aProvider),
      mDisplayControl(nullptr),
      mPreviewControl(nullptr),
      mDisplayCallback(nullptr),
      mPreviewCallback(nullptr) {
  MOZ_ASSERT(mProvider);
  LOG("constructor");
  mListener = new Listener(this);
  mProvider->RegisterCallback(mListener);
}

DOMVideoCallProvider::~DOMVideoCallProvider() {
  LOG("deconstructor");
  Shutdown();
}

void DOMVideoCallProvider::DisconnectFromOwner() {
  LOG("DisconnectFromOwner");
  DOMEventTargetHelper::DisconnectFromOwner();
  // Event listeners can't be handled anymore, so we can shutdown
  // the DOMVideoCallProvider.
  Shutdown();
}

void DOMVideoCallProvider::Shutdown() {
  LOG("Shutdown");

  if (mDisplayCallback) {
    mDisplayCallback->Shutdown();
    mDisplayCallback = nullptr;
  }

  if (mPreviewCallback) {
    mPreviewCallback->Shutdown();
    mPreviewCallback = nullptr;
  }

  if (mDisplayControl) {
    mDisplayControl = nullptr;
  }

  if (mPreviewControl) {
    mPreviewControl = nullptr;
  }

  if (mProvider) {
#ifdef FEED_TEST_DATA_TO_PRODUCER
    char prop[128];
    if ((property_get("vt.loopback", prop, NULL) != 0) &&
        (strcmp(prop, "1") == 0)) {
      android::sp<android::IGraphicBufferProducer> nullProducer;
      mProvider->SetPreviewSurface(nullProducer, 0, 0);
      mProvider->SetDisplaySurface(nullProducer, 0, 0);
    }
#endif
    if (mListener) {
      if (mProvider) {
        mProvider->UnregisterCallback(mListener);
      }

      mListener->Disconnect();
      mListener = nullptr;
    }

    if (mProvider) {
      mProvider = nullptr;
    }

    LOG("null pointer mProvider");
  }
}

JSObject* DOMVideoCallProvider::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return VideoCallProviderBinding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<Promise> DOMVideoCallProvider::SetCamera(
    const Optional<nsAString>& aCamera, ErrorResult& aRv) {
  LOG("SetCamera: %s",
      (aCamera.WasPassed() ? NS_ConvertUTF16toUTF8(aCamera.Value()).get()
                           : "null"));
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  int16_t cameraId = 0;  // back (or forward-facing) camera by default

  if (!aCamera.WasPassed()) {
    cameraId = -1;
  } else {
    if (aCamera.Value().EqualsLiteral("front")) {
      cameraId = 1;
    }

    mCamera = aCamera.Value();
  }

  mProvider->SetCamera(cameraId);

  return promise.forget();
}

already_AddRefed<Promise> DOMVideoCallProvider::GetPreviewStream(
    const SurfaceConfiguration& aOptions, ErrorResult& aRv) {
  LOG("GetPreviewStream");

  return GetStream(TYPE_PREVIEW, aOptions, aRv);
}

already_AddRefed<Promise> DOMVideoCallProvider::GetDisplayStream(
    const SurfaceConfiguration& aOptions, ErrorResult& aRv) {
  LOG("GetDisplayStream");
  return GetStream(TYPE_DISPLAY, aOptions, aRv);
}

already_AddRefed<Promise> DOMVideoCallProvider::GetStream(
    const int16_t aType, const SurfaceConfiguration& aOptions,
    ErrorResult& aRv) {
  LOG("GetStream, type: %d", aType);
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.StealNSResult());
    return nullptr;
  }

  if (!IsValidSurfaceSize(aOptions.mPreviewSize.mWidth,
                          aOptions.mPreviewSize.mHeight)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<DOMVideoCallProvider> provider = this;
  RefPtr<SurfaceControlBack> callback =
      new SurfaceControlBack(provider, aType, aOptions);
  RefPtr<nsDOMSurfaceControl> control =
      new nsDOMSurfaceControl(aOptions, promise, GetOwner(), callback);

  if (aType == TYPE_DISPLAY) {
    mDisplayCallback = callback;
    mDisplayControl = control;
  } else {
    mPreviewCallback = callback;
    mPreviewControl = control;
  }

  return promise.forget();
}

bool DOMVideoCallProvider::ValidOrientation(uint16_t aOrientation) {
  return (aOrientation % ROTATE_ANGLE) == 0;
}

bool DOMVideoCallProvider::ValidZoom(uint16_t aZoom) {
  if (!mZoomSupported) {
    return false;
  }

  if (aZoom > mMaxZoom) {
    return false;
  }

  return true;
}

bool DOMVideoCallProvider::IsValidSurfaceSize(const uint16_t aWidth,
                                              const uint16_t aHeight) {
  // width/height must have values, otherwise UI may display improperly.
  if (aWidth <= 0 || aHeight <= 0) {
    return false;
  } else {
    return true;
  }
}

already_AddRefed<Promise> DOMVideoCallProvider::SetOrientation(
    uint16_t aOrientation, ErrorResult& aRv) {
  LOG("SetOrientation");

  if (!ValidOrientation(aOrientation)) {
    LOG("Invalid orientation %d", aOrientation);
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.StealNSResult());
    return nullptr;
  }

  mProvider->SetDeviceOrientation(aOrientation);
  mOrientation = aOrientation;

  return promise.forget();
}

already_AddRefed<Promise> DOMVideoCallProvider::SetZoom(float aZoom,
                                                        ErrorResult& aRv) {
  LOG("SetZoom: %f", aZoom);

  if (!ValidZoom(aZoom)) {
    LOG("Invalid zoom: %f", aZoom);
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.StealNSResult());
    return nullptr;
  }

  mProvider->SetZoom(aZoom);
  mZoom = aZoom;

  return promise.forget();
}

already_AddRefed<Promise> DOMVideoCallProvider::SendSessionModifyRequest(
    const VideoCallProfile& aFrom, const VideoCallProfile& aTo,
    ErrorResult& aRv) {
  LOG("SendSessionModifyRequest, from {quality: %d, state: %d} to {quality: "
      "%d, state: %d}",
      static_cast<int16_t>(aFrom.mQuality.Value()),
      static_cast<int16_t>(aFrom.mState.Value()),
      static_cast<int16_t>(aTo.mQuality.Value()),
      static_cast<int16_t>(aTo.mState.Value()));
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.StealNSResult());
    return nullptr;
  }

  RefPtr<DOMVideoCallProfile> from =
      new DOMVideoCallProfile(aFrom.mQuality.Value(), aFrom.mState.Value());
  RefPtr<DOMVideoCallProfile> to =
      new DOMVideoCallProfile(aTo.mQuality.Value(), aTo.mState.Value());

  mProvider->SendSessionModifyRequest(from, to);

  return promise.forget();
}

already_AddRefed<Promise> DOMVideoCallProvider::SendSessionModifyResponse(
    const VideoCallProfile& aResponse, ErrorResult& aRv) {
  LOG("SendSessionModifyResponse response {quality: %d, state: %d}",
      static_cast<int16_t>(aResponse.mQuality.Value()),
      static_cast<int16_t>(aResponse.mState.Value()));
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.StealNSResult());
    return nullptr;
  }

  RefPtr<DOMVideoCallProfile> response = new DOMVideoCallProfile(
      aResponse.mQuality.Value(), aResponse.mState.Value());

  mProvider->SendSessionModifyResponse(response);

  return promise.forget();
}

already_AddRefed<Promise> DOMVideoCallProvider::RequestCameraCapabilities(
    ErrorResult& aRv) {
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(GetOwner());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  RefPtr<Promise> promise = Promise::Create(global, aRv);
  if (aRv.Failed()) {
    aRv.Throw(aRv.StealNSResult());
    return nullptr;
  }

  mProvider->RequestCameraCapabilities();

  return promise.forget();
}

// Class API

nsresult DOMVideoCallProvider::DispatchSessionModifyRequestEvent(
    const nsAString& aType, nsIVideoCallProfile* aRequest) {
  LOG("%s", __FUNCTION__);
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    LOG("%s return due to fail to init jsapi", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsRequest(cx);
  nsresult rv = ConvertToJsValue(aRequest, &jsRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  VideoCallSessionModifyRequestEventInit init;
  init.mRequest = jsRequest;
  RefPtr<VideoCallSessionModifyRequestEvent> event =
      VideoCallSessionModifyRequestEvent::Constructor(this, aType, init);

  uint16_t quality;
  uint16_t state;
  aRequest->GetQuality(&quality);
  aRequest->GetState(&state);

  LOG("DispatchSessionModifyRequestEvent request {quality: %d, state: %d}",
      quality, state);

  return DispatchTrustedEvent(event);
}

nsresult DOMVideoCallProvider::DispatchSessionModifyResponseEvent(
    const nsAString& aType, const uint16_t aStatus,
    DOMVideoCallProfile* aRequest, DOMVideoCallProfile* aResponse) {
  LOG("DispatchSessionModifyResponseEvent: %d", aStatus);
  uint16_t requestQuality = 0;
  uint16_t requestState = 0;
  if (aRequest) {
    aRequest->GetQuality(&requestQuality);
    aRequest->GetState(&requestState);
  }

  uint16_t responseQuality = 0;
  uint16_t responseState = 0;
  if (aResponse) {
    aResponse->GetQuality(&responseQuality);
    aResponse->GetState(&responseState);
  }

  // TODO: Remove me, debug info.
  LOG("DispatchSessionModifyResponseEvent, status: %d, request {quality: %d, "
      "state: %d}, response {quality: %d, state: %d}",
      aStatus, requestQuality, requestState, responseQuality, responseState);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    LOG("%s, return due to fail to initiate jsapi", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> jsRequest(cx);
  nsresult rv = ConvertToJsValue(aRequest, &jsRequest);
  NS_ENSURE_SUCCESS(rv, rv);

  JS::Rooted<JS::Value> jsResponse(cx);
  rv = ConvertToJsValue(aResponse, &jsResponse);
  NS_ENSURE_SUCCESS(rv, rv);

  VideoCallSessionModifyResponseEventInit init;
  init.mStatus = static_cast<VideoCallSessionStatus>(aStatus);
  init.mRequest = jsRequest;
  init.mResponse = jsResponse;
  RefPtr<VideoCallSessionModifyResponseEvent> event =
      VideoCallSessionModifyResponseEvent::Constructor(this, aType, init);

  return DispatchTrustedEvent(event);
}

nsresult DOMVideoCallProvider::ConvertToJsValue(
    nsIVideoCallProfile* aProfile, JS::Rooted<JS::Value>* jsResult) {
  LOG("%s", __FUNCTION__);
  uint16_t quality = 0;
  uint16_t state = 0;
  if (aProfile) {
    aProfile->GetQuality(&quality);
    aProfile->GetState(&state);
  }

  VideoCallProfile requestParams;
  VideoCallQuality& resultQuality = requestParams.mQuality.Construct();
  VideoCallState& resultState = requestParams.mState.Construct();
  resultQuality = static_cast<VideoCallQuality>(quality);
  resultState = static_cast<VideoCallState>(state);

  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(GetOwner()))) {
    LOG("%s, return, fail to init jsapi", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSContext* cx = jsapi.cx();
  if (!ToJSValue(cx, requestParams, jsResult)) {
    JS_ClearPendingException(cx);
    LOG("%s, return, fail to tojsvalue", __FUNCTION__);
    return NS_ERROR_TYPE_ERR;
  }

  return NS_OK;
}

nsresult DOMVideoCallProvider::DispatCallSessionEvent(const nsAString& aType,
                                                      const int16_t aEvent) {
  LOG("DispatCallSessionEvent aEvent: %d", aEvent);
  VideoCallSessionChangeEventInit init;
  init.mType = static_cast<VideoCallSessionChangeType>(aEvent);
  RefPtr<VideoCallSessionChangeEvent> event =
      VideoCallSessionChangeEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

nsresult DOMVideoCallProvider::DispatchChangePeerDimensionsEvent(
    const nsAString& aType, const uint16_t aWidth, const uint16_t aHeight) {
  VideoCallPeerDimensionsEventInit init;
  init.mWidth = aWidth;
  init.mHeight = aHeight;
  RefPtr<VideoCallPeerDimensionsEvent> event =
      VideoCallPeerDimensionsEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

nsresult DOMVideoCallProvider::DispatchCameraCapabilitiesEvent(
    const nsAString& aType, nsIVideoCallCameraCapabilities* capabilities) {
  VideoCallCameraCapabilitiesChangeEventInit init;
  capabilities->GetWidth(&init.mWidth);
  capabilities->GetHeight(&init.mHeight);
  capabilities->GetMaxZoom(&init.mMaxZoom);
  capabilities->GetZoomSupported(&init.mZoomSupported);

  mMaxZoom = init.mMaxZoom;
  mZoomSupported = init.mZoomSupported;

  RefPtr<VideoCallCameraCapabilitiesChangeEvent> event =
      VideoCallCameraCapabilitiesChangeEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

nsresult DOMVideoCallProvider::DispatchVideoQualityChangeEvent(
    const nsAString& aType, const uint16_t aQuality) {
  VideoCallQualityEventInit init;
  init.mQuality = static_cast<VideoCallQuality>(aQuality);
  RefPtr<VideoCallQualityEvent> event =
      VideoCallQualityEvent::Constructor(this, aType, init);
  return DispatchTrustedEvent(event);
}

void DOMVideoCallProvider::SetSurface(
    const int16_t aType,
    android::sp<android::IGraphicBufferProducer>& aProducer,
    const uint16_t aWidth, const uint16_t aHeight) {
  LOG("SetDisplaySurface, type: %d, producer: %p", aType, aProducer.get());
  if (!mProvider) {
    return;
  }

  if (aType == TYPE_DISPLAY) {
    mProvider->SetDisplaySurface(aProducer, aWidth, aHeight);
  } else {
    mProvider->SetPreviewSurface(aProducer, aWidth, aHeight);
  }
}

void DOMVideoCallProvider::SetDataSourceSize(const int16_t aType,
                                             const uint16_t aWidth,
                                             const uint16_t aHeight) {
  LOG("%s, type: %d, width: %d, height: %d", __FUNCTION__, aType, aWidth,
      aHeight);
  RefPtr<nsDOMSurfaceControl> control =
      aType == TYPE_DISPLAY ? mDisplayControl : mPreviewControl;
  if (!control) {
    return;
  }

  control->SetDataSourceSize(aWidth, aHeight);
}

// nsIVideoCallCallback

NS_IMETHODIMP
DOMVideoCallProvider::OnReceiveSessionModifyRequest(
    nsIVideoCallProfile* request) {
  LOG("%s", __FUNCTION__);
  DispatchSessionModifyRequestEvent(u"sessionmodifyrequest"_ns, request);
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallProvider::OnReceiveSessionModifyResponse(
    uint16_t status, nsIVideoCallProfile* request,
    nsIVideoCallProfile* response) {
  LOG("%s, status: %d", __FUNCTION__, status);

  DispatchSessionModifyResponseEvent(
      u"sessionmodifyresponse"_ns, status,
      (request ? static_cast<DOMVideoCallProfile*>(request) : nullptr),
      (response ? static_cast<DOMVideoCallProfile*>(response) : nullptr));
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallProvider::OnHandleCallSessionEvent(int16_t event) {
  DispatCallSessionEvent(u"callsessionevent"_ns, event);
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallProvider::OnChangePeerDimensions(uint16_t aWidth,
                                             uint16_t aHeight) {
  LOG("%s, width: %d, height: %d", __FUNCTION__, aWidth, aHeight);
  DispatchChangePeerDimensionsEvent(u"changepeerdimensions"_ns, aWidth,
                                    aHeight);

  if (!mDisplayCallback) {
    return NS_OK;
  }

  android::sp<android::IGraphicBufferProducer> producer =
      mDisplayCallback->getProducer();
  if (producer != nullptr) {
    LOG("%s setDisplaySurface with new width: %u, height: %u", __FUNCTION__,
        aWidth, aHeight);
    SetSurface(TYPE_DISPLAY, producer, aWidth, aWidth);
  }

  SetDataSourceSize(TYPE_DISPLAY, aWidth, aHeight);
  return NS_OK;
}
NS_IMETHODIMP
DOMVideoCallProvider::OnChangeCameraCapabilities(
    nsIVideoCallCameraCapabilities* capabilities) {
  LOG("%s", __FUNCTION__);
  DispatchCameraCapabilitiesEvent(u"changecameracapabilities"_ns, capabilities);
  if (!mPreviewCallback) {
    return NS_OK;
  }

  uint16_t width;
  uint16_t height;
  capabilities->GetWidth(&width);
  capabilities->GetHeight(&height);
  LOG("%s width: %u, height: %u", __FUNCTION__, width, height);

  android::sp<android::IGraphicBufferProducer> producer =
      mPreviewCallback->getProducer();
  if (producer != nullptr) {
    LOG("%s setPreviewSurface with new width: %u, height: %u", __FUNCTION__,
        width, height);
    SetSurface(TYPE_PREVIEW, producer, width, height);
  }

  // Bug-16836. To swap width/height as a temporary solution.
  SetDataSourceSize(TYPE_PREVIEW, height, width);
  return NS_OK;
}

NS_IMETHODIMP
DOMVideoCallProvider::OnChangeVideoQuality(uint16_t quality) {
  DispatchVideoQualityChangeEvent(u"changevideoquality"_ns, quality);
  return NS_OK;
}

}  // namespace dom
}  // namespace mozilla
