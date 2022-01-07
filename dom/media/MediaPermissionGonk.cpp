/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaManager.h"
#include "MediaPermissionGonk.h"

#include "Document.h"
#include "nsArray.h"
#include "nsCOMPtr.h"
#include "nsIContentPermissionPrompt.h"
#include "nsIStringEnumerator.h"
#include "nsJSUtils.h"
#include "nsQueryObject.h"
#include "nsPIDOMWindow.h"
#include "nsTArray.h"
#include "GetUserMediaRequest.h"
#include "mozilla/dom/CreateOfferRequestBinding.h"
#include "mozilla/dom/PBrowserChild.h"
#include "mozilla/dom/MediaStreamTrackBinding.h"
#include "mozilla/dom/MediaStreamError.h"
#include "nsISupportsPrimitives.h"
#include "nsComponentManagerUtils.h"
#include "nsGlobalWindowInner.h"
#include "nsServiceManagerUtils.h"
#include "nsArrayUtils.h"
#include "nsContentPermissionHelper.h"
#include "mozilla/dom/PermissionMessageUtils.h"

#define AUDIO_PERMISSION_NAME "audio-capture"
#define VIDEO_PERMISSION_NAME "video-capture"

constexpr auto kAUDIO_PERMISSION_NAME = "audio-capture"_ns;
constexpr auto kVIDEO_PERMISSION_NAME = "video-capture"_ns;

namespace mozilla {

/* static */
StaticRefPtr<MediaPermissionManager> MediaPermissionManager::sSingleton;

static void CreateDeviceNameList(nsTArray<nsCOMPtr<nsIMediaDevice>>& aDevices,
                                 nsTArray<nsString>& aDeviceNameList) {
  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    nsCOMPtr<nsIMediaDevice> device = aDevices[i];
    nsString name = static_cast<LocalMediaDevice*>(device.get())->mName;
    aDeviceNameList.AppendElement(name);
  }
}

static already_AddRefed<nsIMediaDevice> FindDeviceByName(
    nsTArray<nsCOMPtr<nsIMediaDevice>>& aDevices,
    const nsAString& aDeviceName) {
  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    nsCOMPtr<nsIMediaDevice> device = aDevices[i];
    nsString deviceName = static_cast<LocalMediaDevice*>(device.get())->mName;
    if (deviceName.Equals(aDeviceName)) {
      return device.forget();
    }
  }

  return nullptr;
}

// Helper function for notifying permission granted
static nsresult AllowGetUserMediaRequest(
    const nsAString& aCallID, nsTArray<nsCOMPtr<nsIMediaDevice>>& aDevices) {
  nsresult rv;
  nsCOMPtr<nsIMutableArray> array = nsArray::Create();

  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    rv = array->AppendElement(aDevices.ElementAt(i));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  return obs->NotifyObservers(array, "getUserMedia:response:allow",
                              aCallID.BeginReading());
}

// Helper function for notifying permision denial or error
static nsresult DenyGetUserMediaRequest(const nsAString& aCallID,
                                        const nsAString& aErrorMsg) {
  nsresult rv;
  nsCOMPtr<nsISupportsString> supportsString =
      do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = supportsString->SetData(aErrorMsg);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  NS_ENSURE_TRUE(obs, NS_ERROR_FAILURE);

  return obs->NotifyObservers(supportsString, "getUserMedia:response:deny",
                              aCallID.BeginReading());
}

namespace {

/**
 * MediaPermissionRequest will send a prompt ipdl request to b2g process
 * according to its owned type.
 */
class MediaPermissionRequest : public dom::ContentPermissionRequestBase {
 public:
  MediaPermissionRequest(nsPIDOMWindowInner* aWindow,
                         dom::GetUserMediaRequest* aRequest,
                         nsTArray<RefPtr<nsIMediaDevice>>& aDevices);

  NS_IMETHOD GetTypes(nsIArray** aTypes) override;
  NS_IMETHOD Cancel() override;
  NS_IMETHOD Allow(JS::HandleValue choices) override;

 protected:
  virtual ~MediaPermissionRequest() {}

 private:
  nsresult DoAllow(const nsString& audioDevice, const nsString& videoDevice);

  bool mAudio;  // Request for audio permission
  bool mVideo;  // Request for video permission
  RefPtr<dom::GetUserMediaRequest> mRequest;
  nsTArray<nsCOMPtr<nsIMediaDevice>> mAudioDevices;  // candidate audio devices
  nsTArray<nsCOMPtr<nsIMediaDevice>> mVideoDevices;  // candidate video devices
};

MediaPermissionRequest::MediaPermissionRequest(
    nsPIDOMWindowInner* aWindow, dom::GetUserMediaRequest* aRequest,
    nsTArray<RefPtr<nsIMediaDevice>>& aDevices)
    : dom::ContentPermissionRequestBase(aWindow->GetDoc()->NodePrincipal(),
                                        aWindow, ""_ns, "getusermedia"_ns),
      mRequest(aRequest) {
  dom::MediaStreamConstraints constraints;
  mRequest->GetConstraints(constraints);

  mAudio = !constraints.mAudio.IsBoolean() || constraints.mAudio.GetAsBoolean();
  mVideo = !constraints.mVideo.IsBoolean() || constraints.mVideo.GetAsBoolean();

  for (uint32_t i = 0; i < aDevices.Length(); ++i) {
    nsCOMPtr<nsIMediaDevice> device(aDevices[i]);
    nsAutoString deviceType;
    device->GetType(deviceType);
    if (mAudio && deviceType.EqualsLiteral("audioinput")) {
      mAudioDevices.AppendElement(device);
    }
    if (mVideo && deviceType.EqualsLiteral("videoinput")) {
      mVideoDevices.AppendElement(device);
    }
  }
}

// nsIContentPermissionRequest methods
NS_IMETHODIMP
MediaPermissionRequest::GetTypes(nsIArray** aTypes) {
  nsCOMPtr<nsIMutableArray> types = do_CreateInstance(NS_ARRAY_CONTRACTID);
  // XXX append device list
  if (mAudio) {
    nsTArray<nsString> audioDeviceNames;
    CreateDeviceNameList(mAudioDevices, audioDeviceNames);
    nsCOMPtr<nsISupports> AudioType = new dom::ContentPermissionType(
        kAUDIO_PERMISSION_NAME, audioDeviceNames);
    types->AppendElement(AudioType);
  }
  if (mVideo) {
    nsTArray<nsString> videoDeviceNames;
    CreateDeviceNameList(mVideoDevices, videoDeviceNames);
    nsCOMPtr<nsISupports> VideoType = new dom::ContentPermissionType(
        kVIDEO_PERMISSION_NAME, videoDeviceNames);
    types->AppendElement(VideoType);
  }
  NS_IF_ADDREF(*aTypes = types);

  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::Cancel() {
  nsString callID;
  mRequest->GetCallID(callID);
  DenyGetUserMediaRequest(callID, u"SecurityError"_ns);
  return NS_OK;
}

NS_IMETHODIMP
MediaPermissionRequest::Allow(JS::HandleValue aChoices) {
  // check if JS object
  if (!aChoices.isObject()) {
    MOZ_ASSERT(false, "Not a correct format of PermissionChoice");
    return NS_ERROR_INVALID_ARG;
  }
  // iterate through audio-capture and video-capture
  dom::AutoJSAPI jsapi;
  if (!jsapi.Init(&aChoices.toObject())) {
    return NS_ERROR_UNEXPECTED;
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JSObject*> obj(cx, &aChoices.toObject());
  JS::Rooted<JS::Value> v(cx);

  // get selected audio device name
  nsString audioDevice;
  if (mAudio) {
    if (!JS_GetProperty(cx, obj, AUDIO_PERMISSION_NAME, &v) || !v.isString()) {
      return NS_ERROR_FAILURE;
    }
    nsAutoJSString deviceName;
    if (!deviceName.init(cx, v)) {
      MOZ_ASSERT(false, "Couldn't initialize string from aChoices");
      return NS_ERROR_FAILURE;
    }
    audioDevice = deviceName;
  }

  // get selected video device name
  nsString videoDevice;
  if (mVideo) {
    if (!JS_GetProperty(cx, obj, VIDEO_PERMISSION_NAME, &v) || !v.isString()) {
      return NS_ERROR_FAILURE;
    }
    nsAutoJSString deviceName;
    if (!deviceName.init(cx, v)) {
      MOZ_ASSERT(false, "Couldn't initialize string from aChoices");
      return NS_ERROR_FAILURE;
    }
    videoDevice = deviceName;
  }

  return DoAllow(audioDevice, videoDevice);
}

nsresult MediaPermissionRequest::DoAllow(const nsString& audioDevice,
                                         const nsString& videoDevice) {
  nsTArray<nsCOMPtr<nsIMediaDevice>> selectedDevices;
  if (mAudio) {
    nsCOMPtr<nsIMediaDevice> device =
        FindDeviceByName(mAudioDevices, audioDevice);
    if (device) {
      selectedDevices.AppendElement(device);
    }
  }

  if (mVideo) {
    nsCOMPtr<nsIMediaDevice> device =
        FindDeviceByName(mVideoDevices, videoDevice);
    if (device) {
      selectedDevices.AppendElement(device);
    }
  }

  nsString callID;
  mRequest->GetCallID(callID);
  return AllowGetUserMediaRequest(callID, selectedDevices);
}

}  // namespace

// MediaPermissionManager
NS_IMPL_ISUPPORTS(MediaPermissionManager, nsIObserver)

void MediaPermissionManager::EnsureSingleton() {
  if (!sSingleton) {
    sSingleton = new MediaPermissionManager();
  }
}

MediaPermissionManager::MediaPermissionManager() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->AddObserver(this, "getUserMedia:request", false);
    obs->AddObserver(this, "PeerConnection:request", false);
    obs->AddObserver(this, "xpcom-shutdown", false);
  }
}

MediaPermissionManager::~MediaPermissionManager() { Deinit(); }

nsresult MediaPermissionManager::Deinit() {
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    obs->RemoveObserver(this, "getUserMedia:request");
    obs->RemoveObserver(this, "PeerConnection:request");
    obs->RemoveObserver(this, "xpcom-shutdown");
  }
  return NS_OK;
}

// nsIObserver method
NS_IMETHODIMP
MediaPermissionManager::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  nsresult rv;
  if (!strcmp(aTopic, "getUserMedia:request")) {
    RefPtr<dom::GetUserMediaRequest> req =
        static_cast<dom::GetUserMediaRequest*>(aSubject);
    rv = HandleRequest(req);

    if (NS_FAILED(rv)) {
      nsString callID;
      req->GetCallID(callID);
      DenyGetUserMediaRequest(callID, u"unable to enumerate media device"_ns);
    }
  } else if (!strcmp(aTopic, "PeerConnection:request")) {
    RefPtr<dom::CreateOfferRequest> req =
        static_cast<dom::CreateOfferRequest*>(aSubject);
    nsString callID;
    ErrorResult err;
    req->GetCallID(callID, err);
    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    rv = obs->NotifyObservers(nullptr, "PeerConnection:response:allow",
                              callID.BeginReading());
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    rv = Deinit();
  } else {
    // not reachable
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

// Handle GetUserMediaRequest, query available media device first.
nsresult MediaPermissionManager::HandleRequest(
    RefPtr<dom::GetUserMediaRequest>& aRequest) {
  nsString callID;
  aRequest->GetCallID(callID);
  uint64_t innerWindowID = aRequest->InnerWindowID();

  nsCOMPtr<nsPIDOMWindowInner> innerWindow =
      nsGlobalWindowInner::GetInnerWindowWithId(innerWindowID);
  if (!innerWindow) {
    MOZ_ASSERT(false, "No inner window");
    return NS_ERROR_FAILURE;
  }

  nsTArray<RefPtr<nsIMediaDevice>> devices;
  aRequest->GetDevices(devices);

  // Trigger permission prompt UI
  auto req = MakeRefPtr<MediaPermissionRequest>(innerWindow, aRequest, devices);
  return dom::nsContentPermissionUtils::AskPermission(req, innerWindow);
}

}  // namespace mozilla
