/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <binder/IServiceManager.h>
#include <binder/ActivityManager.h>
#include <android/log.h>

#include "GonkActivityManagerService.h"

#undef ALOGV
#if 0
#  define ALOGV(args...) \
    __android_log_print(ANDROID_LOG_DEBUG, "GonkActivityManagerService", ##args)
#else  // Turn off log.
#  define ALOGV(args...)
#endif

namespace android {
// NOLINTNEXTLINE(google-default-arguments)
status_t BnActivityManager::onTransact(uint32_t code, const Parcel& data,
                                       Parcel* reply, uint32_t flags) {
  switch (code) {
    case OPEN_CONTENT_URI_TRANSACTION: {
      ALOGV("%s(), line:%d", __func__, __LINE__);
      CHECK_INTERFACE(IActivityManager, data, reply);
      String16 stringUri = data.readString16();
      int res = openContentUri(stringUri);
      reply->writeNoException();
      reply->writeInt32(res);
      return NO_ERROR;
    } break;

    case REGISTER_UID_OBSERVER_TRANSACTION: {
      ALOGV("%s(), line:%d", __func__, __LINE__);
      CHECK_INTERFACE(IActivityManager, data, reply);
      sp<IUidObserver> observer =
          interface_cast<IUidObserver>(data.readStrongBinder());
      int32_t event = data.readInt32();
      int32_t cutpoint = data.readInt32();
      String16 callingPackage = data.readString16();
      registerUidObserver(observer, event, cutpoint, callingPackage);
      reply->writeNoException();
      return NO_ERROR;
    } break;

    case UNREGISTER_UID_OBSERVER_TRANSACTION: {
      ALOGV("%s(), line:%d", __func__, __LINE__);
      CHECK_INTERFACE(IActivityManager, data, reply);
      sp<IUidObserver> observer =
          interface_cast<IUidObserver>(data.readStrongBinder());
      unregisterUidObserver(observer);
      reply->writeNoException();
      return NO_ERROR;
    } break;

    case IS_UID_ACTIVE_TRANSACTION: {
      ALOGV("%s(), line:%d", __func__, __LINE__);
      CHECK_INTERFACE(IActivityManager, data, reply);
      int32_t uid = data.readInt32();
      String16 callingPackage = data.readString16();
      bool res = isUidActive(uid, callingPackage);
      reply->writeNoException();
      reply->writeInt32(res ? 1 : 0);
      return NO_ERROR;
    } break;

    case GET_UID_PROCESS_STATE_TRANSACTION: {
      ALOGV("%s(), line:%d", __func__, __LINE__);
      CHECK_INTERFACE(IActivityManager, data, reply);
      int32_t uid = data.readInt32();
      String16 callingPackage = data.readString16();
      int32_t state = getUidProcessState(uid, callingPackage);
      reply->writeNoException();
      reply->writeInt32(state);
      return NO_ERROR;
    } break;

    default:
      return BBinder::onTransact(code, data, reply, flags);
  }
}
};  // namespace android

namespace mozilla {

int GonkActivityManagerService::openContentUri(
    const android::String16& stringUri) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return -1;
}

#if ANDROID_VERSION < 33
void GonkActivityManagerService::registerUidObserver(
    const android::sp<android::IUidObserver>& observer, const int32_t event,
    const int32_t cutpoint, const android::String16& callingPackage) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
}

void GonkActivityManagerService::unregisterUidObserver(
    const android::sp<android::IUidObserver>& observer) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
}
#else
android::status_t GonkActivityManagerService::registerUidObserver(
    const android::sp<android::IUidObserver>& observer, const int32_t event,
    const int32_t cutpoint, const android::String16& callingPackage) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::unregisterUidObserver(
    const android::sp<android::IUidObserver>& observer) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::checkPermission(
  const android::String16& permission, pid_t pid,
  uid_t uid, int32_t* outResult) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}
#endif

#if ANDROID_VERSION >= 34
android::status_t GonkActivityManagerService::registerUidObserverForUids(
    const android::sp<android::IUidObserver>& observer, const int32_t event,
    const int32_t cutpoint, const android::String16& callingPackage,
    const int32_t uids[], size_t nUids,
    /*out*/ android::sp<android::IBinder>& observerToken) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::addUidToObserver(
    const android::sp<android::IBinder>& observerToken,
    const android::String16& callingPackage, int32_t uid) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::removeUidFromObserver(
    const android::sp<android::IBinder>& observerToken,
    const android::String16& callingPackage, int32_t uid) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::logFgsApiBegin(int32_t apiType,
                                                             int32_t appUid,
                                                             int32_t appPid) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::logFgsApiEnd(int32_t apiType,
                                                           int32_t appUid,
                                                           int32_t appPid) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}

android::status_t GonkActivityManagerService::logFgsApiStateChanged(
    int32_t apiType, int32_t state, int32_t appUid, int32_t appPid) {
  // Gonk doesn't use this interface currently.
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return 0;
}
#endif

bool GonkActivityManagerService::isUidActive(
    const uid_t uid, const android::String16& callingPackage) {
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return true;
}

int32_t GonkActivityManagerService::getUidProcessState(
    const uid_t uid, const android::String16& callingPackage) {
  ALOGV("%s(), line:%d", __func__, __LINE__);
  return android::ActivityManager::PROCESS_STATE_TOP;
}

static GonkActivityManagerService* gGonkActivityManagerService = NULL;

/* static */
void GonkActivityManagerService::instantiate() {
  ALOGV("%s(), line:%d", __func__, __LINE__);
  android::defaultServiceManager()->addService(
      android::String16(getServiceName()), GetInstance());
}

/* static */
GonkActivityManagerService* GonkActivityManagerService::GetInstance() {
  ALOGV("%s(), line:%d", __func__, __LINE__);
  if (!gGonkActivityManagerService) {
    gGonkActivityManagerService = new GonkActivityManagerService();
  }
  return gGonkActivityManagerService;
}
};  // namespace mozilla
