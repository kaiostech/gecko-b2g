/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[Pref="dom.telephony.enabled",
 Exposed=Window]
interface TelephonyCallGroup : EventTarget {
  readonly attribute CallsList calls;

  [NewObject, Throws]
  Promise<undefined> add(TelephonyCall call);

  [NewObject, Throws]
  Promise<undefined> add(TelephonyCall call, TelephonyCall secondCall);

  [NewObject, Throws]
  Promise<undefined> remove(TelephonyCall call);

  [NewObject]
  Promise<undefined> hangUp();

  [NewObject, Throws]
  Promise<undefined> hold();

  [NewObject, Throws]
  Promise<undefined> resume();

  readonly attribute TelephonyCallGroupState state;

  attribute EventHandler onstatechange;
  attribute EventHandler onconnected;
  attribute EventHandler onheld;
  attribute EventHandler oncallschanged;
  attribute EventHandler onerror;
};

enum TelephonyCallGroupState {
  "",
  "connected",
  "held",
};
