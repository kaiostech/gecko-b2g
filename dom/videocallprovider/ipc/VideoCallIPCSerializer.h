/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_videocallprovider_videocallprovideripcserializer_h__
#define mozilla_dom_videocallprovider_videocallprovideripcserializer_h__

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/DOMVideoCallCameraCapabilities.h"
#include "mozilla/dom/DOMVideoCallProfile.h"
#include "mozilla/dom/VideoCallProviderBinding.h"
#include "nsIVideoCallProvider.h"
#include "nsIVideoCallCallback.h"

using mozilla::dom::DOMVideoCallCameraCapabilities;
using mozilla::dom::DOMVideoCallProfile;
using mozilla::dom::VideoCallQuality;
using mozilla::dom::VideoCallState;

typedef nsIVideoCallProfile* nsVideoCallProfile;
typedef nsIVideoCallCameraCapabilities* nsVideoCallCameraCapabilities;

namespace IPC {

/**
 * nsIVideoCallProfile Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIVideoCallProfile*> {
  typedef nsIVideoCallProfile* paramType;

  // Function to serialize a DOMVideoCallProfile.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are don.
    if (isNull) {
      return;
    }

    uint16_t quality;
    uint16_t state;

    aParam->GetQuality(&quality);
    aParam->GetState(&state);

    WriteParam(aWriter, quality);
    WriteParam(aWriter, state);
    aParam->Release();
  }

  // Function to de-serialize a DOMVideoCallProfile.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transferred.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
    }

    uint16_t quality;
    uint16_t state;

    if (!(ReadParam(aReader, &quality) && ReadParam(aReader, &state))) {
      return false;
    }

    *aResult = new DOMVideoCallProfile(static_cast<VideoCallQuality>(quality),
                                       static_cast<VideoCallState>(state));
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

/**
 * nsIVideoCallCameraCapabilities Serialize/De-serialize.
 */
template <>
struct ParamTraits<nsIVideoCallCameraCapabilities*> {
  typedef nsIVideoCallCameraCapabilities* paramType;

  // Function to serialize a DOMVideoCallCameraCapabilities.
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are don.
    if (isNull) {
      return;
    }

    uint16_t width;
    uint16_t height;
    bool zoomSupported;
    float maxZoom;

    aParam->GetWidth(&width);
    aParam->GetHeight(&height);
    aParam->GetZoomSupported(&zoomSupported);
    aParam->GetMaxZoom(&maxZoom);

    WriteParam(aWriter, width);
    WriteParam(aWriter, height);
    WriteParam(aWriter, zoomSupported);
    WriteParam(aWriter, maxZoom);
    aParam->Release();
  }

  // Function to de-serialize a DOMVideoCallCameraCapabilities.
  static bool Read(MessageReader* aReader, paramType* aResult) {
    // Check if is the null pointer we have transferred.
    bool isNull;
    if (!ReadParam(aReader, &isNull)) {
      return false;
    }

    if (isNull) {
      *aResult = nullptr;
    }

    uint16_t width;
    uint16_t height;
    bool zoomSupported;
    float maxZoom;

    if (!(ReadParam(aReader, &width) && ReadParam(aReader, &height) &&
          ReadParam(aReader, &zoomSupported) && ReadParam(aReader, &maxZoom))) {
      return false;
    }

    *aResult = new DOMVideoCallCameraCapabilities(width, height, zoomSupported,
                                                  maxZoom);
    // We release this ref after receiver finishes processing.
    NS_ADDREF(*aResult);

    return true;
  }
};

}  // namespace IPC

#endif  // mozilla_dom_videocallprovider_videocallprovideripcserializer_h__
