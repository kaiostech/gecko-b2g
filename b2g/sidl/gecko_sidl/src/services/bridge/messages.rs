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
    "114b7863857548a5f1ee3a609f27fddc3cc75e832385662cd315856c6829c4";

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum CardInfoType {
    Imei,   // #0
    Imsi,   // #1
    Msisdn, // #2
}
impl Copy for CardInfoType {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum NetworkState {
    NetworkStateUnknown,       // #0
    NetworkStateConnecting,    // #1
    NetworkStateConnected,     // #2
    NetworkStateDisconnecting, // #3
    NetworkStateDisconnected,  // #4
    NetworkStateEnabled,       // #5
    NetworkStateDisabled,      // #6
    NetworkStateSuspended,     // #7
}
impl Copy for NetworkState {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum NetworkType {
    NetworkTypeUnknown,     // #0
    NetworkTypeWifi,        // #1
    NetworkTypeMobile,      // #2
    NetworkTypeMobileMms,   // #3
    NetworkTypeMobileSupl,  // #4
    NetworkTypeWifiP2p,     // #5
    NetworkTypeMobileIms,   // #6
    NetworkTypeMobileDun,   // #7
    NetworkTypeMobileFota,  // #8
    NetworkTypeEthernet,    // #9
    NetworkTypeMobileHipri, // #10
    NetworkTypeMobileCbs,   // #11
    NetworkTypeMobileIa,    // #12
    NetworkTypeMobileEcc,   // #13
    NetworkTypeMobileXcap,  // #14
}
impl Copy for NetworkType {}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct NetworkInfo {
    pub network_state: NetworkState,
    pub network_type: NetworkType,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct NetworkOperator {
    pub mnc: String,
    pub mcc: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SimContactInfo {
    pub id: String,
    pub tel: String,
    pub email: String,
    pub name: String,
    pub category: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum GeckoBridgeFromClient {
    GeckoFeaturesBoolPrefChanged(String, bool),   // 0
    GeckoFeaturesCharPrefChanged(String, String), // 1
    GeckoFeaturesImportSimContacts(Option<Vec<SimContactInfo>>), // 2
    GeckoFeaturesIntPrefChanged(String, i64),     // 3
    GeckoFeaturesRegisterToken(String, String, Option<Vec<String>>), // 4
    GeckoFeaturesSetAppsServiceDelegate(ObjectRef), // 5
    GeckoFeaturesSetMobileManagerDelegate(ObjectRef), // 6
    GeckoFeaturesSetNetworkManagerDelegate(ObjectRef), // 7
    GeckoFeaturesSetPowerManagerDelegate(ObjectRef), // 8
    GeckoFeaturesSetPreferenceDelegate(ObjectRef), // 9
    AppsServiceDelegateGetUaSuccess(String),      // 10
    AppsServiceDelegateGetUaError,                // 11
    AppsServiceDelegateOnBootSuccess,             // 12
    AppsServiceDelegateOnBootError,               // 13
    AppsServiceDelegateOnBootDoneSuccess,         // 14
    AppsServiceDelegateOnBootDoneError,           // 15
    AppsServiceDelegateOnClearSuccess,            // 16
    AppsServiceDelegateOnClearError,              // 17
    AppsServiceDelegateOnInstallSuccess,          // 18
    AppsServiceDelegateOnInstallError,            // 19
    AppsServiceDelegateOnLaunchSuccess,           // 20
    AppsServiceDelegateOnLaunchError,             // 21
    AppsServiceDelegateOnUninstallSuccess,        // 22
    AppsServiceDelegateOnUninstallError,          // 23
    AppsServiceDelegateOnUpdateSuccess,           // 24
    AppsServiceDelegateOnUpdateError,             // 25
    MobileManagerDelegateGetCardInfoSuccess(String), // 26
    MobileManagerDelegateGetCardInfoError,        // 27
    MobileManagerDelegateGetMncMccSuccess(NetworkOperator), // 28
    MobileManagerDelegateGetMncMccError,          // 29
    NetworkManagerDelegateGetNetworkInfoSuccess(NetworkInfo), // 30
    NetworkManagerDelegateGetNetworkInfoError,    // 31
    PowerManagerDelegateRequestWakelockSuccess(ObjectRef), // 32
    PowerManagerDelegateRequestWakelockError,     // 33
    PowerManagerDelegateSetDisplayBrightnessSuccess, // 34
    PowerManagerDelegateSetDisplayBrightnessError, // 35
    PowerManagerDelegateSetScreenEnabledSuccess,  // 36
    PowerManagerDelegateSetScreenEnabledError,    // 37
    PreferenceDelegateGetBoolSuccess(bool),       // 38
    PreferenceDelegateGetBoolError,               // 39
    PreferenceDelegateGetCharSuccess(String),     // 40
    PreferenceDelegateGetCharError,               // 41
    PreferenceDelegateGetIntSuccess(i64),         // 42
    PreferenceDelegateGetIntError,                // 43
    PreferenceDelegateSetBoolSuccess,             // 44
    PreferenceDelegateSetBoolError,               // 45
    PreferenceDelegateSetCharSuccess,             // 46
    PreferenceDelegateSetCharError,               // 47
    PreferenceDelegateSetIntSuccess,              // 48
    PreferenceDelegateSetIntError,                // 49
    WakelockGetTopicSuccess(String),              // 50
    WakelockGetTopicError,                        // 51
    WakelockUnlockSuccess,                        // 52
    WakelockUnlockError,                          // 53
}

#[derive(Debug, Deserialize)]
pub enum GeckoBridgeToClient {
    GeckoFeaturesBoolPrefChangedSuccess,                   // 0
    GeckoFeaturesBoolPrefChangedError,                     // 1
    GeckoFeaturesCharPrefChangedSuccess,                   // 2
    GeckoFeaturesCharPrefChangedError,                     // 3
    GeckoFeaturesImportSimContactsSuccess,                 // 4
    GeckoFeaturesImportSimContactsError,                   // 5
    GeckoFeaturesIntPrefChangedSuccess,                    // 6
    GeckoFeaturesIntPrefChangedError,                      // 7
    GeckoFeaturesRegisterTokenSuccess,                     // 8
    GeckoFeaturesRegisterTokenError,                       // 9
    GeckoFeaturesSetAppsServiceDelegateSuccess,            // 10
    GeckoFeaturesSetAppsServiceDelegateError,              // 11
    GeckoFeaturesSetMobileManagerDelegateSuccess,          // 12
    GeckoFeaturesSetMobileManagerDelegateError,            // 13
    GeckoFeaturesSetNetworkManagerDelegateSuccess,         // 14
    GeckoFeaturesSetNetworkManagerDelegateError,           // 15
    GeckoFeaturesSetPowerManagerDelegateSuccess,           // 16
    GeckoFeaturesSetPowerManagerDelegateError,             // 17
    GeckoFeaturesSetPreferenceDelegateSuccess,             // 18
    GeckoFeaturesSetPreferenceDelegateError,               // 19
    AppsServiceDelegateGetUa,                              // 20
    AppsServiceDelegateOnBoot(String, JsonValue),          // 21
    AppsServiceDelegateOnBootDone,                         // 22
    AppsServiceDelegateOnClear(String, String, JsonValue), // 23
    AppsServiceDelegateOnInstall(String, JsonValue),       // 24
    AppsServiceDelegateOnLaunch(String),                   // 25
    AppsServiceDelegateOnUninstall(String),                // 26
    AppsServiceDelegateOnUpdate(String, JsonValue),        // 27
    MobileManagerDelegateGetCardInfo(i64, CardInfoType),   // 28
    MobileManagerDelegateGetMncMcc(i64, bool),             // 29
    NetworkManagerDelegateGetNetworkInfo,                  // 30
    PowerManagerDelegateRequestWakelock(String),           // 31
    PowerManagerDelegateSetDisplayBrightness(i64, f64),    // 32
    PowerManagerDelegateSetScreenEnabled(bool, bool),      // 33
    PreferenceDelegateGetBool(String),                     // 34
    PreferenceDelegateGetChar(String),                     // 35
    PreferenceDelegateGetInt(String),                      // 36
    PreferenceDelegateSetBool(String, bool),               // 37
    PreferenceDelegateSetChar(String, String),             // 38
    PreferenceDelegateSetInt(String, i64),                 // 39
    WakelockGetTopic,                                      // 40
    WakelockUnlock,                                        // 41
}
