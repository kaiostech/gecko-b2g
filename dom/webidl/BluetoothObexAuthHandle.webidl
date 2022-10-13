/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Window]
interface BluetoothObexAuthHandle
{
  /**
   * Reply password for obexpasswordreq. The promise will be rejected if the
   * operation fails.
   */
  [NewObject]
  Promise<undefined> setPassword(DOMString aPassword);

  // Reject the OBEX authentication request. The promise will be rejected if
  // operation fails.
  [NewObject]
  Promise<undefined> reject();
};
