/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MobileMessageCursorCallback.h"
#include "MmsMessage.h"
#include "MmsMessageInternal.h"
#include "MobileMessageIterable.h"
#include "MobileMessageThread.h"
#include "MobileMessageThreadInternal.h"
#include "mozilla/dom/ScriptSettings.h"
#include "SmsMessage.h"
#include "SmsMessageInternal.h"
#include "nsIMobileMessageCallback.h"
#include "nsServiceManagerUtils.h"  // for do_GetService

namespace mozilla {
namespace dom {

namespace mobilemessage {

NS_IMPL_CYCLE_COLLECTION(MobileMessageCursorCallback, mMessageIterable)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileMessageCursorCallback)
  NS_INTERFACE_MAP_ENTRY(nsIMobileMessageCursorCallback)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileMessageCursorCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileMessageCursorCallback)

// nsIMobileMessageCursorCallback

NS_IMETHODIMP
MobileMessageCursorCallback::NotifyCursorError(int32_t aError) {
  MOZ_ASSERT(mMessageIterable);

  RefPtr<MobileMessageIterable> messageIterable = mMessageIterable.forget();
  switch (aError) {
    case nsIMobileMessageCallback::NO_SIGNAL_ERROR:
      messageIterable->FireError(u"NoSignalError"_ns);
      break;
    case nsIMobileMessageCallback::NOT_FOUND_ERROR:
      messageIterable->FireError(u"NotFoundError"_ns);
      break;
    case nsIMobileMessageCallback::UNKNOWN_ERROR:
      messageIterable->FireError(u"UnknownError"_ns);
      break;
    case nsIMobileMessageCallback::INTERNAL_ERROR:
      messageIterable->FireError(u"InternalError"_ns);
      break;
    default:  // SUCCESS_NO_ERROR is handled above.
      MOZ_CRASH("Should never get here!");
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCursorCallback::NotifyCursorResult(nsISupports** aResults,
                                                uint32_t aSize) {
  MOZ_ASSERT(mMessageIterable);
  // We should only be notified with valid results. Or, either
  // |NotifyCursorDone()| or |NotifyCursorError()| should be called instead.
  MOZ_ASSERT(aResults && *aResults && aSize);
  nsTArray<nsCOMPtr<nsISupports>>& pending = mMessageIterable->mPendingResults;

  // Push pending results in reversed order.
  pending.SetCapacity(pending.Length() + aSize);
  while (aSize) {
    --aSize;
    pending.AppendElement(aResults[aSize]);
  }

  mMessageIterable->FireSuccess();

  return NS_OK;
}

NS_IMETHODIMP
MobileMessageCursorCallback::NotifyCursorDone() {
  MOZ_ASSERT(mMessageIterable);

  RefPtr<MobileMessageIterable> messageIterable = mMessageIterable.forget();
  messageIterable->FireDone();

  return NS_OK;
}

}  // namespace mobilemessage
}  // namespace dom
}  // namespace mozilla
