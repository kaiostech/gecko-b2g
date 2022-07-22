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
    "d2e7699bc8cdc73421e28699cfb91dc2b149c8a64823efc2416f1382826dc0";

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum GetErrorReason {
    UnknownError,       // #0
    NonExistingSetting, // #1
}
impl Copy for GetErrorReason {}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct GetError {
    pub name: String,
    pub reason: GetErrorReason,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SettingInfo {
    pub name: String,
    pub value: JsonValue,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum SettingsManagerFromClient {
    SettingsFactoryAddObserver(String, ObjectRef),    // 0
    SettingsFactoryClear,                             // 1
    SettingsFactoryGet(String),                       // 2
    SettingsFactoryGetBatch(Vec<String>),             // 3
    SettingsFactoryRemoveObserver(String, ObjectRef), // 4
    SettingsFactorySet(Vec<SettingInfo>),             // 5
    SettingObserverCallbackSuccess,                   // 6
    SettingObserverCallbackError,                     // 7
}

#[derive(Debug, Deserialize)]
pub enum SettingsManagerToClient {
    SettingsFactoryAddObserverSuccess,                // 0
    SettingsFactoryAddObserverError,                  // 1
    SettingsFactoryClearSuccess,                      // 2
    SettingsFactoryClearError,                        // 3
    SettingsFactoryGetSuccess(SettingInfo),           // 4
    SettingsFactoryGetError(GetError),                // 5
    SettingsFactoryGetBatchSuccess(Vec<SettingInfo>), // 6
    SettingsFactoryGetBatchError,                     // 7
    SettingsFactoryRemoveObserverSuccess,             // 8
    SettingsFactoryRemoveObserverError,               // 9
    SettingsFactorySetSuccess,                        // 10
    SettingsFactorySetError,                          // 11
    SettingsFactoryChangeEvent(SettingInfo),          // 12
    SettingObserverCallback(SettingInfo),             // 13
}
