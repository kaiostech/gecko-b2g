/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum ProfileState
{
    "unknown",
    "active",
    "inactive"
};
enum IconType
{
  "unknown",
  "JPEG",
  "PNG"
};
enum ProfileClassType {
  "test",
  "provisioning",
  "operational"
};
enum ProfilePolicyMask {
  "unknown",
  "disable-not-allowed",
  "delete-not_allowed",
  "unknown1",
  "delete-on-disable"
};

[Func="B2G::HasEUiccSupport", Exposed=Window]
interface EUiccProfileInfo {
  [Throws]
  constructor(DOMString profileName,
              DOMString nickName,
              DOMString spName,
              IconType iconClass,
              ProfileClassType profileClass,
              ProfilePolicyMask profilePolicy,
              ProfileState state,
              DOMString iccid,
              DOMString icon);
  readonly attribute  DOMString profileName;
  readonly attribute  DOMString nickName;
  readonly attribute  DOMString spName;
  readonly attribute  IconType iconClass;
  readonly attribute  ProfileClassType profileClass;
  readonly attribute  ProfilePolicyMask profilePolicy;
  readonly attribute  ProfileState state;
  readonly attribute  DOMString iccid;
  readonly attribute  DOMString icon;
};
