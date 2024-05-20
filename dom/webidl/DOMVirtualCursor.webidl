/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Window, Pref="dom.virtualcursor.enabled"]
interface DOMVirtualCursor {
  /**
   * Enable virtual cursor on this window
   * Throws an exception if no permissions.
   */
  [Throws]
  undefined enable();

  /**
   * Disable virtual cursor on this window
   * Throws an exception if no permissions.
   */
  [Throws]
  undefined disable();
  readonly attribute boolean enabled;

  [ChromeOnly]
  undefined startPanning();
  [ChromeOnly]
  undefined stopPanning();
  readonly attribute boolean isPanning;

  [ChromeOnly]
  undefined enterSelectionMode();
  [ChromeOnly]
  undefined exitSelectionMode();
  [ChromeOnly]
  undefined startSelection();
  [ChromeOnly]
  undefined stopSelection();
};
