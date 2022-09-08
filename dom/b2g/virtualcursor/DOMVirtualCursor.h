/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMVirtualCursor_h
#define mozilla_dom_DOMVirtualCursor_h

#include "nsCOMPtr.h"
#include "nsISupportsImpl.h"
#include "nsWrapperCache.h"
#include "mozilla/dom/DOMVirtualCursorBinding.h"
#include "nsPIDOMWindow.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

class DOMVirtualCursor final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(DOMVirtualCursor)

  static already_AddRefed<DOMVirtualCursor> Create(nsIGlobalObject* aGlobal);

  // webidl
  void Enable(ErrorResult& aRv);
  void Disable(ErrorResult& aRv);
  bool Enabled() const;
  void StartPanning();
  void StopPanning();
  bool IsPanning() const;

  // binding methods
  nsIGlobalObject* GetParentObject() const { return mGlobal; }
  virtual JSObject* WrapObject(JSContext* aContext,
                               JS::Handle<JSObject*> aGivenProto) override;

  static bool HasPermission(JSContext* aCx, JSObject* aObject);

 private:
  explicit DOMVirtualCursor(nsIGlobalObject* aGlobal);
  ~DOMVirtualCursor();
  bool HasPermission(nsPIDOMWindowInner* aWindow) const;

  nsCOMPtr<nsIGlobalObject> mGlobal;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_DOMVirtualCursor_h
