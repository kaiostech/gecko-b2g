/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_mobilemessage_MobileMessageCursorCallback_h
#define mozilla_dom_mobilemessage_MobileMessageCursorCallback_h

#include "mozilla/Attributes.h"
#include "nsIMobileMessageCursorCallback.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCOMPtr.h"

namespace mozilla {
namespace dom {

class MobileMessageIterable;
class MobileMessageManager;

namespace mobilemessage {

class MobileMessageCursorCallback final
    : public nsIMobileMessageCursorCallback {
  friend class mozilla::dom::MobileMessageManager;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIMOBILEMESSAGECURSORCALLBACK

  NS_DECL_CYCLE_COLLECTION_CLASS(MobileMessageCursorCallback)

  MobileMessageCursorCallback() {}

 private:
  // final suppresses -Werror,-Wdelete-non-virtual-dtor
  ~MobileMessageCursorCallback() {}

  RefPtr<MobileMessageIterable> mMessageIterable;
};

}  // namespace mobilemessage
}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_mobilemessage_MobileMessageCursorCallback_h
