/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain at http://mozilla.org/MPL/2.0/. */

[JSImplementation="@mozilla.org/permissions-manager;1",
 Pref="dom.permissions.manager.enabled",
 Exposed=Window]
interface PermissionsManager
{
  Promise<DOMString> get(DOMString permission, DOMString origin);

  undefined set(DOMString permission, DOMString value, DOMString origin);

  Promise<boolean> isExplicit(DOMString permission, DOMString origin);

  undefined remove(DOMString permission, DOMString origin);
};
