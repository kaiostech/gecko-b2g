/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef mozilla_dom_KeyboardEventGenerator_h_
#define mozilla_dom_KeyboardEventGenerator_h_

#include "mozilla/ErrorResult.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class KeyboardEvent;

class KeyboardEventGenerator final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(KeyboardEventGenerator)

  static already_AddRefed<KeyboardEventGenerator> Constructor(
      const GlobalObject& aGlobal, ErrorResult& aRv);

  explicit KeyboardEventGenerator(nsPIDOMWindowInner* aWindow);

  nsPIDOMWindowOuter* GetParentObject() const {
    return mWindow->GetOuterWindow();
  }

  // virtual JSObject* WrapObject(JSContext* aCx) override;
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;
  MOZ_CAN_RUN_SCRIPT_BOUNDARY void Generate(KeyboardEvent& aEvent,
                                            ErrorResult& aRv);

 protected:
  ~KeyboardEventGenerator();

 private:
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_KeyboardEventGenerator_h_
