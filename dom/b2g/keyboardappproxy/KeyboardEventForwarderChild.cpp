/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardEventForwarderChild.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/KeyboardEvent.h"
#include "mozilla/dom/BrowserChild.h"
#include <string.h>
#include "mozilla/dom/CustomEvent.h"
extern mozilla::LazyLogModule gKeyboardAppProxyLog;

namespace mozilla {
namespace dom {

KeyboardEventForwarderChild::KeyboardEventForwarderChild() {
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("KeyboardEventForwarderChild::Constructor[%p]", this));
}

KeyboardEventForwarderChild::~KeyboardEventForwarderChild() {
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("KeyboardEventForwarderChild::Destructor[%p]", this));
}

IPCResult KeyboardEventForwarderChild::RecvKey(const KeyRequest& aKey) {
  bool defaultPrevented = false;
  do {
    BrowserChild* browserChild = static_cast<BrowserChild*>(Manager());
    if (!browserChild) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardEventForwarderChild::RecvKey no BrowserChild"));
      break;
    }
    PresShell* presShell = browserChild->GetTopLevelPresShell();
    if (!presShell) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardEventForwarderChild::RecvKey no PresShell"));
      break;
    }
    Document* doc = presShell->GetDocument();
    if (!doc) {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardEventForwarderChild::RecvKey no doc"));
      break;
    }
    EventMessage message = eVoidEvent;
    if (!strcmp("keydown", aKey.eventType().get())) {
      message = eKeyDown;
    } else if (!strcmp("keypress", aKey.eventType().get())) {
      message = eKeyPress;
    } else if (!strcmp("keyup", aKey.eventType().get())) {
      message = eKeyUp;
    } else {
      MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
              ("KeyboardEventForwarderChild::RecvKey bad key %s",
               aKey.eventType().get()));
      break;
    }
    UniquePtr<WidgetKeyboardEvent> keyEvent(
        new WidgetKeyboardEvent(true, message, nullptr));
    keyEvent->mCharCode = aKey.charCode();
    keyEvent->mKeyCode = aKey.keyCode();
    keyEvent->mKeyNameIndex =
        WidgetKeyboardEvent::GetKeyNameIndex(NS_ConvertUTF8toUTF16(aKey.key()));
    if (keyEvent->mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
      keyEvent->mKeyValue = NS_ConvertUTF8toUTF16(aKey.key());
    }
    RefPtr<KeyboardEvent> domEvent =
        new KeyboardEvent(doc, presShell->GetPresContext(), keyEvent.get());
    doc->DispatchEvent(*domEvent);
    defaultPrevented = domEvent->DefaultPrevented();
  } while (0);

  Unused << SendResponse(
      KeyResponse(aKey.eventType(), defaultPrevented, aKey.generation()));
  return IPC_OK();
}

IPCResult KeyboardEventForwarderChild::RecvTextChanged(const nsCString& aText) {
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("KeyboardEventForwarderChild::RecvTextChanged [%s]", aText.get()));
  BrowserChild* browserChild = static_cast<BrowserChild*>(Manager());
  if (!browserChild) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("KeyboardEventForwarderChild::RecvKey no BrowserChild"));
    return IPC_OK();
  }
  PresShell* presShell = browserChild->GetTopLevelPresShell();
  if (!presShell) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("KeyboardEventForwarderChild::RecvKey no PresShell"));
    return IPC_OK();
  }
  Document* doc = presShell->GetDocument();
  if (!doc) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("KeyboardEventForwarderChild::RecvKey no doc"));
    return IPC_OK();
  }
  RefPtr<CustomEvent> event = NS_NewDOMCustomEvent(doc, nullptr, nullptr);
  AutoJSAPI jsapi;
  bool success = jsapi.Init(event->GetParentObject());
  if (!success) {
    return IPC_OK();
  }
  JSContext* cx = jsapi.cx();
  JS::Rooted<JS::Value> detail(cx);
  if (!ToJSValue(cx, aText, &detail)) {
    return IPC_OK();
  }
  event->InitCustomEvent(cx, u"inputmethod-contextchange"_ns, false, false,
                         detail);
  event->SetTrusted(true);
  doc->DispatchEvent(*event);
  return IPC_OK();
}

IPCResult KeyboardEventForwarderChild::RecvSelectionChanged(uint32_t aStartOffset, uint32_t aEndOffset) {
  MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
          ("KeyboardEventForwarderChild::RecvSelectionChanged [%u, %u]", aStartOffset, aEndOffset));
  BrowserChild* browserChild = static_cast<BrowserChild*>(Manager());
  if (!browserChild) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("KeyboardEventForwarderChild::RecvSelectionChanged no BrowserChild"));
    return IPC_OK();
  }
  PresShell* presShell = browserChild->GetTopLevelPresShell();
  if (!presShell) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("KeyboardEventForwarderChild::RecvSelectionChanged no PresShell"));
    return IPC_OK();
  }
  Document* doc = presShell->GetDocument();
  if (!doc) {
    MOZ_LOG(gKeyboardAppProxyLog, LogLevel::Debug,
            ("KeyboardEventForwarderChild::RecvSelectionChanged no doc"));
    return IPC_OK();
  }
  RefPtr<CustomEvent> event = NS_NewDOMCustomEvent(doc, nullptr, nullptr);
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(event->GetParentObject()))) {
    return IPC_OK();
  }

  JSContext* cx = jsapi.cx();
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj) {
    return IPC_OK();
  }

  JS::RootedValue startOffset(cx, JS::NumberValue(aStartOffset));
  JS::RootedValue endOffset(cx, JS::NumberValue(aEndOffset));
  JS_SetProperty(cx, obj, "selectionStart", startOffset);
  JS_SetProperty(cx, obj, "selectionEnd", endOffset);
  JS::RootedValue detail(cx);
  if (!ToJSValue(cx, obj, &detail)) {
    return IPC_OK();
  }

  event->InitCustomEvent(cx, u"selectionchange"_ns, false, false, detail);
  event->SetTrusted(true);
  doc->DispatchEvent(*event);
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
