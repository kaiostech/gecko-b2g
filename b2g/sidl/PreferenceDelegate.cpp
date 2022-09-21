/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PreferenceDelegate.h"
#include "mozilla/Preferences.h"
#include "mozilla/ModuleUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Services.h"
#include "nsServiceManagerUtils.h"
#include "nsXULAppAPI.h"
#include "nsQueryObject.h"
#include "nsIObserverService.h"

using namespace mozilla;

namespace mozilla {

// The singleton PreferenceDelegate service, to be used on the main thread.
static StaticRefPtr<PreferenceDelegateService> gPreferenceDelegateService;

NS_IMPL_ISUPPORTS(PreferenceDelegateService, nsIPreferenceDelegate)

PreferenceDelegateService::PreferenceDelegateService() {}

PreferenceDelegateService::~PreferenceDelegateService() {}

/* static */
already_AddRefed<PreferenceDelegateService>
PreferenceDelegateService::ConstructPreferenceDelegate() {
  if (!XRE_IsParentProcess()) {
    return nullptr;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!gPreferenceDelegateService) {
    gPreferenceDelegateService = new PreferenceDelegateService();
    ClearOnShutdown(&gPreferenceDelegateService);
  }

  RefPtr<PreferenceDelegateService> service = gPreferenceDelegateService.get();
  return service.forget();
}

void PreferenceDelegateService::NotifyObservers(const nsAString& aKey) {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return;
  }
  nsString key(aKey);
  obs->NotifyObservers(nullptr, "preference-delegate-notify", key.get());
}

NS_IMETHODIMP
PreferenceDelegateService::GetInt(const nsAString& key, int* value) {
  nsAutoCString nKey;
  LossyCopyUTF16toASCII(key, nKey);

  NotifyObservers(key);
  nsresult rv = Preferences::GetInt(nKey.get(), value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
PreferenceDelegateService::GetChar(const nsAString& key, nsAString& value) {
  nsAutoCString nKey;
  nsAutoString nValue;
  LossyCopyUTF16toASCII(key, nKey);

  NotifyObservers(key);
  nsresult rv = Preferences::GetString(nKey.get(), nValue);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  if (!nValue.IsEmpty()) {
    value.Assign(nValue);
  }

  return NS_OK;
}

NS_IMETHODIMP
PreferenceDelegateService::GetBool(const nsAString& key, bool* value) {
  nsAutoCString nKey;
  LossyCopyUTF16toASCII(key, nKey);

  NotifyObservers(key);
  nsresult rv = Preferences::GetBool(nKey.get(), value);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
PreferenceDelegateService::SetInt(const nsAString& key, const int value) {
  nsAutoCString nKey;
  LossyCopyUTF16toASCII(key, nKey);

  return Preferences::SetInt(nKey.get(), value);
}

NS_IMETHODIMP
PreferenceDelegateService::SetChar(const nsAString& key,
                                   const nsAString& value) {
  nsAutoCString nKey;
  nsAutoCString nValue;
  LossyCopyUTF16toASCII(key, nKey);
  LossyCopyUTF16toASCII(value, nValue);

  return Preferences::SetString(nKey.get(), value);
}

NS_IMETHODIMP
PreferenceDelegateService::SetBool(const nsAString& key, const bool value) {
  nsAutoCString nKey;
  LossyCopyUTF16toASCII(key, nKey);

  return Preferences::SetBool(nKey.get(), value);
}

}  // namespace mozilla
