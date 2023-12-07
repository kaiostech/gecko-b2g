/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * https://w3c.github.io/screen-wake-lock/
 */

[SecureContext, Exposed=(Window), Pref="dom.wakelock.enabled", Func="B2G::HasWakeLockSupport"]
interface WakeLock {
  readonly attribute DOMString topic;

  /**
   * Release the wake lock.
   * @throw NS_ERROR_DOM_INVALID_STATE_ERR if already unlocked.
   */
  [Throws]
  undefined unlock();

  [Throws]
  Promise<WakeLockSentinel> request(optional WakeLockType type = "screen");
};

enum WakeLockType { "screen" };
