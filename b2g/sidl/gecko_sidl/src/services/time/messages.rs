/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is generated. Do not edit.
// @generated

#![allow(clippy::large_enum_variant)]

#[allow(unused_imports)]
use crate::common::{Blob, JsonValue, ObjectRef, SystemTime, Url};
use serde::{Deserialize, Serialize};

pub static SERVICE_FINGERPRINT: &str =
    "d76f2b662bafaf981ee817c61e66698be414446732db362e245ace81c399928";

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug, Eq, Hash)]
pub enum CallbackReason {
    None,            // #0
    TimeChanged,     // #1
    TimezoneChanged, // #2
}
impl Copy for CallbackReason {}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct TimeInfo {
    pub reason: CallbackReason,
    pub timezone: String,
    pub delta: i64,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum TimeServiceFromClient {
    TimeAddObserver(CallbackReason, ObjectRef),    // 0
    TimeGet,                                       // 1
    TimeGetElapsedRealTime,                        // 2
    TimeRemoveObserver(CallbackReason, ObjectRef), // 3
    TimeSet(SystemTime),                           // 4
    TimeSetTimezone(String),                       // 5
    TimeObserverCallbackSuccess,                   // 6
    TimeObserverCallbackError,                     // 7
}

#[derive(Debug, Deserialize)]
pub enum TimeServiceToClient {
    TimeAddObserverSuccess,             // 0
    TimeAddObserverError,               // 1
    TimeGetSuccess(SystemTime),         // 2
    TimeGetError,                       // 3
    TimeGetElapsedRealTimeSuccess(i64), // 4
    TimeGetElapsedRealTimeError,        // 5
    TimeRemoveObserverSuccess,          // 6
    TimeRemoveObserverError,            // 7
    TimeSetSuccess,                     // 8
    TimeSetError,                       // 9
    TimeSetTimezoneSuccess,             // 10
    TimeSetTimezoneError,               // 11
    TimeTimeChangedEvent,               // 12
    TimeTimezoneChangedEvent,           // 13
    TimeObserverCallback(TimeInfo),     // 14
}
