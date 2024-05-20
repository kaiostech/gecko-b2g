/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMVirtualCursor.h"
#include "VirtualCursorService.h"
#include "mozilla/dom/DOMVirtualCursorBinding.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/Event.h"
#include "mozilla/Preferences.h"
#include "nsContentPermissionHelper.h"
#include "nsIGlobalObject.h"

#ifdef MOZ_WIDGET_GONK
#  include <cutils/properties.h>
#else
#  include "prenv.h"
#endif

mozilla::LazyLogModule gVirtualCursorLog("virtualcursor");

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(DOMVirtualCursor, mGlobal)

NS_IMPL_CYCLE_COLLECTING_ADDREF(DOMVirtualCursor)
NS_IMPL_CYCLE_COLLECTING_RELEASE(DOMVirtualCursor)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(DOMVirtualCursor)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

DOMVirtualCursor::DOMVirtualCursor(nsIGlobalObject* aGlobal)
    : mGlobal(aGlobal) {
  MOZ_ASSERT(mGlobal);
}

DOMVirtualCursor::~DOMVirtualCursor() {
  NS_ASSERT_OWNINGTHREAD(DOMVirtualCursor);
}

// namespace anonymous
namespace {
VirtualCursorProxy* GetCursorProxy(nsIGlobalObject* aGlobal) {
  if (!aGlobal) {
    return nullptr;
  }

  nsPIDOMWindowInner* inner = aGlobal->GetAsInnerWindow();
  if (!inner) {
    return nullptr;
  }

  nsPIDOMWindowOuter* outer = inner->GetOuterWindow();
  if (!outer) {
    return nullptr;
  }

  RefPtr<VirtualCursorProxy> cursorProxy =
      VirtualCursorService::GetOrCreateCursor(outer);
  if (!cursorProxy) {
    return nullptr;
  }

  return cursorProxy;
}
}  // namespace

/* static */
already_AddRefed<DOMVirtualCursor> DOMVirtualCursor::Create(
    nsIGlobalObject* aGlobal) {
  RefPtr<DOMVirtualCursor> cursor = new DOMVirtualCursor(aGlobal);
  return cursor.forget();
}

void DOMVirtualCursor::Enable(ErrorResult& aRv) {
  nsPIDOMWindowInner* inner = mGlobal->GetAsInnerWindow();
  MOZ_ASSERT(inner);
  if (!HasPermission(inner)) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("DOMVirtualCursor::SetEnabled no permission"));
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsPIDOMWindowOuter* outer = inner->GetOuterWindow();
  if (!outer) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<VirtualCursorProxy> cursorProxy =
      VirtualCursorService::GetOrCreateCursor(outer);
  if (!cursorProxy) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  cursorProxy->RequestEnable();
}

void DOMVirtualCursor::Disable(ErrorResult& aRv) {
  nsPIDOMWindowInner* inner = mGlobal->GetAsInnerWindow();
  MOZ_ASSERT(inner);
  if (!HasPermission(inner)) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("DOMVirtualCursor::SetEnabled no permission"));
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  nsPIDOMWindowOuter* outer = inner->GetOuterWindow();
  if (!outer) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  RefPtr<VirtualCursorProxy> cursorProxy =
      VirtualCursorService::GetOrCreateCursor(outer);
  if (!cursorProxy) {
    aRv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  cursorProxy->RequestDisable();
}

bool DOMVirtualCursor::Enabled() const {
  nsPIDOMWindowInner* inner = mGlobal->GetAsInnerWindow();
  MOZ_ASSERT(inner);

  nsPIDOMWindowOuter* outer = inner->GetOuterWindow();
  if (!outer) {
    return false;
  }

  RefPtr<VirtualCursorProxy> cursorProxy =
      VirtualCursorService::GetOrCreateCursor(outer);
  if (!cursorProxy) {
    return false;
  }
  bool enabled = false;
  cursorProxy->IsEnabled(&enabled);
  return enabled;
}

void DOMVirtualCursor::StartPanning() {
  RefPtr<VirtualCursorService> service = VirtualCursorService::GetService();
  service->StartPanning();
}

void DOMVirtualCursor::StopPanning() {
  RefPtr<VirtualCursorService> service = VirtualCursorService::GetService();
  service->StopPanning();
}

bool DOMVirtualCursor::IsPanning() const {
  RefPtr<VirtualCursorService> service = VirtualCursorService::GetService();
  return service->IsPanning();
}

void DOMVirtualCursor::EnterSelectionMode() {
  auto* cursorProxy = GetCursorProxy(mGlobal);
  if (!cursorProxy) {
    return;
  }
  cursorProxy->SetSelectionMode(CursorSimulator::SelectionMode::Active);
}

void DOMVirtualCursor::ExitSelectionMode() {
  auto* cursorProxy = GetCursorProxy(mGlobal);
  if (!cursorProxy) {
    return;
  }
  cursorProxy->SetSelectionMode(CursorSimulator::SelectionMode::None);
}

void DOMVirtualCursor::StartSelection() {
  auto* cursorProxy = GetCursorProxy(mGlobal);
  if (!cursorProxy) {
    return;
  }
  cursorProxy->SetSelectionMode(CursorSimulator::SelectionMode::Start);
}

void DOMVirtualCursor::StopSelection() {
  auto* cursorProxy = GetCursorProxy(mGlobal);
  if (!cursorProxy) {
    return;
  }
  cursorProxy->SetSelectionMode(CursorSimulator::SelectionMode::Stop);
}

JSObject* DOMVirtualCursor::WrapObject(JSContext* aContext,
                                       JS::Handle<JSObject*> aGivenProto) {
  return DOMVirtualCursor_Binding::Wrap(aContext, this, aGivenProto);
}

bool DOMVirtualCursor::HasPermission(nsPIDOMWindowInner* aWindow) const {
  RefPtr<Document> document = aWindow->GetExtantDoc();
  NS_ENSURE_TRUE(document, false);
  nsCOMPtr<nsIPrincipal> principal = document->NodePrincipal();
  nsAutoCString origin;
  Unused << principal->GetOrigin(origin);
  if (nsContentUtils::IsCallerChrome()) {
    MOZ_LOG(gVirtualCursorLog, LogLevel::Debug,
            ("DOMVirtualCursor::HasPermission grant chrome caller, origin=%s",
             origin.get()));
    return true;
  }
  if (principal->IsSystemPrincipal()) {
    MOZ_LOG(
        gVirtualCursorLog, LogLevel::Debug,
        ("DOMVirtualCursor::HasPermission grant system principal, origin=%s",
         origin.get()));
    return true;
  }
  MOZ_LOG(
      gVirtualCursorLog, LogLevel::Debug,
      ("DOMVirtualCursor::HasPermission check with origin %s", origin.get()));
  nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
  NS_ENSURE_TRUE(permissionManager, false);
  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  nsresult rv = permissionManager->TestPermissionFromPrincipal(
      principal, "virtualcursor"_ns, &permission);
  return NS_SUCCEEDED(rv) && permission == nsIPermissionManager::ALLOW_ACTION;
}

// static
bool DOMVirtualCursor::HasPermission(JSContext* aContext, JSObject* aGlobal) {
  if (nsContentUtils::IsSystemCaller(aContext)) {
    return true;
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aGlobal);
  NS_ENSURE_TRUE(principal, false);

  nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
  NS_ENSURE_TRUE(permissionManager, false);

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  permissionManager->TestExactPermissionFromPrincipal(
      principal, "virtualcursor"_ns, &permission);

  if (permission == nsIPermissionManager::ALLOW_ACTION ||
      permission == nsIPermissionManager::PROMPT_ACTION) {
    return true;
  }

  return false;
}

}  // namespace dom
}  // namespace mozilla
