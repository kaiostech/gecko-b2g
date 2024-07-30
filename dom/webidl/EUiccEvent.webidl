/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

enum AddProfileStatus {
  "none",
  "error",
  "Download-progress",
  "installation-progress",
  "installation-complete",
  "get-user-consent"
};
enum AddProfileFailureCause {
  "none",
  "generic-error",
  "sim-error",
  "network-error",
  "memory-error"
};

[Func="B2G::HasEUiccSupport", Exposed=Window]
interface EUiccEvent : Event
{
  constructor(DOMString type, optional EUiccEventInit eventInitDict={});
  /**
    * STATUS_NONE                  = 0,
    * STATUS_ERROR                 = 1,
    * STATUS_DOWNLOAD_PROGRESS     = 2,
    * STATUS_INSTALLATION_PROGRESS = 3,
    * STATUS_INSTALLATION_COMPLETE = 4,
    * STATUS_GET_USER_CONSENT      = 5
    */
  readonly attribute AddProfileStatus status;

  /**
    * FAILURE_CAUSE_NONE    = 0,
    * FAILURE_CAUSE_GENERIC = 1,
    * FAILURE_CAUSE_SIM     = 2,
    * FAILURE_CAUSE_NETWORK = 3,
    * FAILURE_CAUSE_MEMORY  = 4
    */
  readonly attribute AddProfileFailureCause cause;

  readonly attribute unsigned long progress;

  /**
    * PROFILE_TYPE_DISABLE_NOT_ALLOWED = 1,
    * PROFILE_TYPE_DELETE_NOT_ALLOWED  = 2,
    * PROFILE_TYPE_DELETE_ON_DISABLE   = 4
    */
  readonly attribute ProfilePolicyMask policyMask;

  readonly attribute boolean  userConsentRequired;
};

dictionary EUiccEventInit : EventInit
{
    /**
    * STATUS_NONE                  = 0,
    * STATUS_ERROR                 = 1,
    * STATUS_DOWNLOAD_PROGRESS     = 2,
    * STATUS_INSTALLATION_PROGRESS = 3,
    * STATUS_INSTALLATION_COMPLETE = 4,
    * STATUS_GET_USER_CONSENT      = 5
    */
  AddProfileStatus status ="none";

  /**
    * FAILURE_CAUSE_NONE    = 0,
    * FAILURE_CAUSE_GENERIC = 1,
    * FAILURE_CAUSE_SIM     = 2,
    * FAILURE_CAUSE_NETWORK = 3,
    * FAILURE_CAUSE_MEMORY  = 4
    */
  AddProfileFailureCause cause = "none";

  unsigned long progress = 0;

  /**
    * PROFILE_TYPE_DISABLE_NOT_ALLOWED = 1,
    * PROFILE_TYPE_DELETE_NOT_ALLOWED  = 2,
    * PROFILE_TYPE_DELETE_ON_DISABLE   = 4
    */
  ProfilePolicyMask policyMask = "disable-not-allowed";

  boolean  userConsentRequired = false;
};
