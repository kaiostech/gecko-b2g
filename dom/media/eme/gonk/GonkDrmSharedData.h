/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GonkDrmSharedData_H
#define GonkDrmSharedData_H

#include <utils/RefBase.h>
#include <utils/Mutex.h>
#include <utils/Vector.h>

#include <map>
#include <set>
#include <vector>

namespace android {

class GonkDrmSharedData : public RefBase {
 public:
  GonkDrmSharedData() = default;

  void SetCryptoSessionId(const Vector<uint8_t>& aSessionId);

  Vector<uint8_t> GetCryptoSessionId();

  void AddKey(const Vector<uint8_t>& aSessionId, const Vector<uint8_t>& aKeyId);

  Vector<uint8_t> FindSessionByKey(const Vector<uint8_t>& aKeyId);

  void RemoveSession(const Vector<uint8_t>& aSessionId);

 private:
  ~GonkDrmSharedData() = default;

  Mutex mLock;
  Vector<uint8_t> mCryptoSessionId;

  typedef std::set<std::vector<uint8_t>> KeyIdSet;
  std::map<std::vector<uint8_t>, KeyIdSet> mSessions;
};

}  // namespace android

#endif
