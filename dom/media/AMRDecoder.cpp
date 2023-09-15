/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AMRDecoder.h"
#include "MediaContainerType.h"
#include "PDMFactory.h"

namespace mozilla {

/* static */
bool AMRDecoder::IsEnabled() {
  RefPtr<PDMFactory> platform = new PDMFactory();
  return !platform->SupportsMimeType("audio/3gpp"_ns).isEmpty();
}

/* static */
bool AMRDecoder::IsSupportedType(const MediaContainerType& aContainerType) {
  if (aContainerType.Type() == MEDIAMIMETYPE("audio/amr")) {
    return IsEnabled();
  }

  return false;
}

/* static */
nsTArray<UniquePtr<TrackInfo>> AMRDecoder::GetTracksInfo(
    const MediaContainerType& aType) {
  nsTArray<UniquePtr<TrackInfo>> tracks;
  if (!IsSupportedType(aType)) {
    return tracks;
  }

  tracks.AppendElement(
      CreateTrackInfoWithMIMETypeAndContainerTypeExtraParameters(
          "audio/3gpp"_ns, aType));

  return tracks;
}

}  // namespace mozilla
