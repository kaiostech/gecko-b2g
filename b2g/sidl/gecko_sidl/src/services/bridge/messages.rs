/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is generated. Do not edit.
// @generated

#[allow(unused_imports)]
use crate::common::{JsonValue, ObjectRef, SystemTime};
use serde::{Deserialize, Serialize};

pub static SERVICE_FINGERPRINT: &str =
    "7d8a63753d27db79a379c39e4dc9785bd04e8f6a8fe2fec676c7d4965e8414bf";

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
    AppsServiceDelegateOnUninstallSuccess,        // 20
    AppsServiceDelegateOnUninstallError,          // 21
    AppsServiceDelegateOnUpdateSuccess,           // 22
    AppsServiceDelegateOnUpdateError,             // 23
    MobileManagerDelegateGetCardInfoSuccess(String), // 24
    MobileManagerDelegateGetCardInfoError,        // 25
    MobileManagerDelegateGetMncMccSuccess(NetworkOperator), // 26
    MobileManagerDelegateGetMncMccError,          // 27
    NetworkManagerDelegateGetNetworkInfoSuccess(NetworkInfo), // 28
    NetworkManagerDelegateGetNetworkInfoError,    // 29
    PowerManagerDelegateRequestWakelockSuccess(ObjectRef), // 30
    PowerManagerDelegateRequestWakelockError,     // 31
    PowerManagerDelegateSetScreenEnabledSuccess,  // 32
    PowerManagerDelegateSetScreenEnabledError,    // 33
    PreferenceDelegateGetBoolSuccess(bool),       // 34
    PreferenceDelegateGetBoolError,               // 35
    PreferenceDelegateGetCharSuccess(String),     // 36
    PreferenceDelegateGetCharError,               // 37
    PreferenceDelegateGetIntSuccess(i64),         // 38
    PreferenceDelegateGetIntError,                // 39
    PreferenceDelegateSetBoolSuccess,             // 40
    PreferenceDelegateSetBoolError,               // 41
    PreferenceDelegateSetCharSuccess,             // 42
    PreferenceDelegateSetCharError,               // 43
    PreferenceDelegateSetIntSuccess,              // 44
    PreferenceDelegateSetIntError,                // 45
    WakelockGetTopicSuccess(String),              // 46
    WakelockGetTopicError,                        // 47
    WakelockUnlockSuccess,                        // 48
    WakelockUnlockError,                          // 49
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
    AppsServiceDelegateOnUninstall(String),                // 25
    AppsServiceDelegateOnUpdate(String, JsonValue),        // 26
    MobileManagerDelegateGetCardInfo(i64, CardInfoType),   // 27
    MobileManagerDelegateGetMncMcc(i64, bool),             // 28
    NetworkManagerDelegateGetNetworkInfo,                  // 29
    PowerManagerDelegateRequestWakelock(String),           // 30
    PowerManagerDelegateSetScreenEnabled(bool, bool),      // 31
    PreferenceDelegateGetBool(String),                     // 32
    PreferenceDelegateGetChar(String),                     // 33
    PreferenceDelegateGetInt(String),                      // 34
    PreferenceDelegateSetBool(String, bool),               // 35
    PreferenceDelegateSetChar(String, String),             // 36
    PreferenceDelegateSetInt(String, i64),                 // 37
    WakelockGetTopic,                                      // 38
    WakelockUnlock,                                        // 39
}
