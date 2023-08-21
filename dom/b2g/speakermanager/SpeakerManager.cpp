/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SpeakerManager.h"

#include "AudioChannelService.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/Event.h"
#include "mozilla/Services.h"
#include "nsIDocShell.h"
#include "nsIDOMEventListener.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "SpeakerManagerService.h"

namespace mozilla {
namespace dom {

NS_IMPL_QUERY_INTERFACE_INHERITED(SpeakerManager, DOMEventTargetHelper,
                                  nsIDOMEventListener)
NS_IMPL_ADDREF_INHERITED(SpeakerManager, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(SpeakerManager, DOMEventTargetHelper)

SpeakerManager::SpeakerManager(SpeakerPolicy aPolicy)
    : mForcespeaker(false),
      mVisible(false),
      mAudioChannelActive(false),
      mPolicy(aPolicy) {}

void SpeakerManager::Shutdown() {
  SpeakerManagerService* service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);

  service->ForceSpeaker(false, false, false, WindowID());
  service->UnRegisterSpeakerManager(this);
  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);

  target->RemoveSystemEventListener(u"visibilitychange"_ns, this,
                                    /* useCapture = */ true);
}

bool SpeakerManager::Speakerforced() {
  SpeakerManagerService* service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);
  return service->GetSpeakerStatus();
}

bool SpeakerManager::Forcespeaker() { return mForcespeaker; }

void SpeakerManager::SetForcespeaker(bool aEnable) {
  if (mForcespeaker != aEnable) {
    MOZ_LOG(SpeakerManagerService::GetSpeakerManagerLog(), LogLevel::Debug,
            ("SpeakerManager, SetForcespeaker, enable %d", aEnable));
    mForcespeaker = aEnable;
    UpdateStatus();
  }
}

void SpeakerManager::DispatchSimpleEvent(const nsAString& aStr) {
  MOZ_ASSERT(NS_IsMainThread(), "Not running on main thread");
  if (NS_FAILED(CheckCurrentGlobalCorrectness())) {
    return;
  }

  RefPtr<Event> event = NS_NewDOMEvent(this, nullptr, nullptr);
  event->InitEvent(aStr, false, false);
  event->SetTrusted(true);

  ErrorResult rv;
  DispatchEvent(*event, rv);
  if (rv.Failed()) {
    NS_ERROR("Failed to dispatch the event!!!");
    return;
  }
}

nsresult SpeakerManager::Init(nsPIDOMWindowInner* aWindow) {
  nsIGlobalObject* global = aWindow ? aWindow->AsGlobal() : nullptr;
  BindToOwner(global);

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE(docshell, NS_ERROR_FAILURE);
  mVisible = docshell->GetBrowsingContext()->IsActive();

  nsCOMPtr<EventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE(target, NS_ERROR_FAILURE);

  target->AddSystemEventListener(u"visibilitychange"_ns, this,
                                 /* useCapture = */ true,
                                 /* wantsUntrusted = */ false);

  mWindow = AudioChannelService::GetTopAppWindow(aWindow->GetOuterWindow());

  MOZ_LOG(SpeakerManagerService::GetSpeakerManagerLog(), LogLevel::Debug,
          ("SpeakerManager, Init, window ID %lu, policy %hhu", WindowID(),
           mPolicy));

  SpeakerManagerService* service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);
  nsresult rv = service->RegisterSpeakerManager(this);
  NS_WARN_IF(NS_FAILED(rv));

  // APP may want to keep its forceSpeaker setting same as current global
  // forceSpeaker state, so sync |mForcespeaker| with the global state by
  // default. This can prevent the global state from being disabled and enabled
  // again if APP wants to enable forceSpeaker, and the global state is already
  // on.
  mForcespeaker = Speakerforced();
  UpdateStatus();
  return NS_OK;
}

nsPIDOMWindowInner* SpeakerManager::GetParentObject() const {
  return GetOwner();
}

/* static */
already_AddRefed<SpeakerManager> SpeakerManager::Constructor(
    const GlobalObject& aGlobal, const Optional<SpeakerPolicy>& aPolicy,
    ErrorResult& aRv) {
  nsCOMPtr<nsIScriptGlobalObject> sgo =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!sgo) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (!ownerWindow) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  if (!HasPermission(ownerWindow.get())) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // If APP did not specify a policy, set default value as
  // Foreground_or_playing.
  SpeakerPolicy policy = SpeakerPolicy::Foreground_or_playing;
  if (aPolicy.WasPassed()) {
    policy = aPolicy.Value();
  }

  RefPtr<SpeakerManager> object = new SpeakerManager(policy);
  nsresult rv = object->Init(ownerWindow);
  if (NS_FAILED(rv)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  return object.forget();
}

JSObject* SpeakerManager::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  return MozSpeakerManager_Binding::Wrap(aCx, this, aGivenProto);
}

NS_IMETHODIMP
SpeakerManager::HandleEvent(Event* aEvent) {
  nsAutoString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("visibilitychange")) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE(docshell, NS_ERROR_FAILURE);

  bool visible = docshell->GetBrowsingContext()->IsActive();
  if (mVisible != visible) {
    MOZ_LOG(SpeakerManagerService::GetSpeakerManagerLog(), LogLevel::Debug,
            ("SpeakerManager, HandleEvent, visible %d", visible));
    mVisible = visible;
    UpdateStatus();
  }
  return NS_OK;
}

void SpeakerManager::SetAudioChannelActive(bool isActive) {
  if (mAudioChannelActive != isActive) {
    MOZ_LOG(SpeakerManagerService::GetSpeakerManagerLog(), LogLevel::Debug,
            ("SpeakerManager, SetAudioChannelActive, active %d", isActive));
    mAudioChannelActive = isActive;
    UpdateStatus();
  }
}

void SpeakerManager::UpdateStatus() {
  bool visible = mVisible;
  switch (mPolicy) {
    case SpeakerPolicy::Foreground_or_playing:
      break;
    case SpeakerPolicy::Playing:
      // Expect SpeakerManagerService to respect our setting only when
      // audio channel is active, so let our visibility be always false.
      visible = false;
      break;
    default:
      // SpeakerPolicy::Query. Don't send status to SpeakerManagerService.
      return;
  }

  SpeakerManagerService* service =
      SpeakerManagerService::GetOrCreateSpeakerManagerService();
  MOZ_ASSERT(service);
  service->ForceSpeaker(mForcespeaker, visible, mAudioChannelActive,
                        WindowID());
}

/* static */
bool SpeakerManager::HasPermission(nsPIDOMWindowInner* aWindow) {
  RefPtr<Document> doc = aWindow->GetExtantDoc();
  NS_ENSURE_TRUE(doc, false);

  nsCOMPtr<nsIPrincipal> principal = doc->NodePrincipal();
  NS_ENSURE_TRUE(principal, false);

  nsCOMPtr<nsIPermissionManager> permissionManager =
      services::GetPermissionManager();
  NS_ENSURE_TRUE(permissionManager, false);

  uint32_t permission = nsIPermissionManager::UNKNOWN_ACTION;
  permissionManager->TestExactPermissionFromPrincipal(
      principal, "speaker-control"_ns, &permission);

  return permission == nsIPermissionManager::ALLOW_ACTION;
}

}  // namespace dom
}  // namespace mozilla
