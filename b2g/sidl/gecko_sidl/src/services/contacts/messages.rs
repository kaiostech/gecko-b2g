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
    "a7ab5919f6ff24f594d94ce1e98aae136fb19fbc4c11af93c51b76bb8453d";

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum ChangeReason {
    Create, // #0
    Update, // #1
    Remove, // #2
}
impl Copy for ChangeReason {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum FilterByOption {
    Name,       // #0
    GivenName,  // #1
    FamilyName, // #2
    Tel,        // #3
    Email,      // #4
    Category,   // #5
}
impl Copy for FilterByOption {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum FilterOption {
    Equals,     // #0
    Contains,   // #1
    Match,      // #2
    StartsWith, // #3
    FuzzyMatch, // #4
}
impl Copy for FilterOption {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum Order {
    Ascending,  // #0
    Descending, // #1
}
impl Copy for Order {}

#[derive(Clone, PartialEq, Deserialize, Serialize, Debug)]
pub enum SortOption {
    GivenName,  // #0
    FamilyName, // #1
    Name,       // #2
}
impl Copy for SortOption {}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct Address {
    pub atype: Option<String>,
    pub street_address: Option<String>,
    pub locality: Option<String>,
    pub region: Option<String>,
    pub postal_code: Option<String>,
    pub country_name: Option<String>,
    pub pref: Option<bool>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct AlphabetSearchOptions {
    pub filter_value: String,
    pub batch_size: i64,
    pub filter_limit: Option<i64>,
    pub only_main_data: Option<bool>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct BlockedNumberChangeEvent {
    pub reason: ChangeReason,
    pub number: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct BlockedNumberFindOptions {
    pub filter_value: String,
    pub filter_option: FilterOption,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactField {
    pub atype: Option<String>,
    pub value: String,
    pub pref: Option<bool>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactFindSortOptions {
    pub sort_by: Option<SortOption>,
    pub sort_order: Option<Order>,
    pub sort_language: Option<String>,
    pub filter_value: String,
    pub filter_option: FilterOption,
    pub filter_by: Vec<FilterByOption>,
    pub only_main_data: Option<bool>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactInfo {
    pub id: Option<String>,
    pub published: Option<SystemTime>,
    pub updated: Option<SystemTime>,
    pub bday: Option<SystemTime>,
    pub anniversary: Option<SystemTime>,
    pub sex: Option<String>,
    pub gender_identity: Option<String>,
    pub ringtone: Option<String>,
    pub photo_type: Option<String>,
    pub photo_blob: Option<Vec<u8>>,
    pub addresses: Option<Vec<Address>>,
    pub email: Option<Vec<ContactField>>,
    pub url: Option<Vec<ContactField>>,
    pub name: Option<String>,
    pub tel: Option<Vec<ContactTelField>>,
    pub honorific_prefix: Option<Vec<String>>,
    pub given_name: Option<String>,
    pub phonetic_given_name: Option<String>,
    pub additional_name: Option<Vec<String>>,
    pub family_name: Option<String>,
    pub phonetic_family_name: Option<String>,
    pub honorific_suffix: Option<Vec<String>>,
    pub nickname: Option<Vec<String>>,
    pub category: Option<Vec<String>>,
    pub org: Option<Vec<String>>,
    pub job_title: Option<Vec<String>>,
    pub note: Option<Vec<String>>,
    pub groups: Option<Vec<String>>,
    pub ice_position: i64,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactSortOptions {
    pub sort_by: SortOption,
    pub sort_order: Order,
    pub sort_language: Option<String>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactTelField {
    pub atype: Option<String>,
    pub value: String,
    pub pref: Option<bool>,
    pub carrier: Option<String>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct ContactsChangeEvent {
    pub reason: ChangeReason,
    pub contacts: Option<Vec<ContactInfo>>,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct GroupChangeEvent {
    pub reason: ChangeReason,
    pub group: GroupInfo,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct GroupInfo {
    pub id: String,
    pub name: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct IceInfo {
    pub position: i64,
    pub contact_id: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SimContactInfo {
    pub id: String,
    pub tel: String,
    pub email: String,
    pub name: String,
    pub category: String,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SimContactLoadedEvent {
    pub remove_count: i64,
    pub update_count: i64,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SpeedDialChangeEvent {
    pub reason: ChangeReason,
    pub speeddial: SpeedDialInfo,
}

#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct SpeedDialInfo {
    pub dial_key: String,
    pub tel: String,
    pub contact_id: String,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum ContactsManagerFromClient {
    ContactCursorNext,                                            // 0
    ContactsFactoryAdd(Vec<ContactInfo>),                         // 1
    ContactsFactoryAddBlockedNumber(String),                      // 2
    ContactsFactoryAddGroup(String),                              // 3
    ContactsFactoryAddSpeedDial(String, String, String),          // 4
    ContactsFactoryAlphabetSearch(AlphabetSearchOptions),         // 5
    ContactsFactoryClearContacts,                                 // 6
    ContactsFactoryFind(ContactFindSortOptions, i64),             // 7
    ContactsFactoryFindBlockedNumbers(BlockedNumberFindOptions),  // 8
    ContactsFactoryGet(String, bool),                             // 9
    ContactsFactoryGetAll(ContactSortOptions, i64, bool),         // 10
    ContactsFactoryGetAllBlockedNumbers,                          // 11
    ContactsFactoryGetAllGroups,                                  // 12
    ContactsFactoryGetAllIce,                                     // 13
    ContactsFactoryGetContactidsFromGroup(String),                // 14
    ContactsFactoryGetCount,                                      // 15
    ContactsFactoryGetSpeedDials,                                 // 16
    ContactsFactoryImportVcf(String),                             // 17
    ContactsFactoryMatches(FilterByOption, FilterOption, String), // 18
    ContactsFactoryRemove(Vec<String>),                           // 19
    ContactsFactoryRemoveBlockedNumber(String),                   // 20
    ContactsFactoryRemoveGroup(String),                           // 21
    ContactsFactoryRemoveIce(String),                             // 22
    ContactsFactoryRemoveSpeedDial(String),                       // 23
    ContactsFactorySetIce(String, i64),                           // 24
    ContactsFactoryUpdate(Vec<ContactInfo>),                      // 25
    ContactsFactoryUpdateGroup(String, String),                   // 26
    ContactsFactoryUpdateSpeedDial(String, String, String),       // 27
}

#[derive(Debug, Deserialize)]
pub enum ContactsManagerToClient {
    ContactCursorNextSuccess(Vec<ContactInfo>), // 0
    ContactCursorNextError,                     // 1
    ContactsFactoryAddSuccess,                  // 2
    ContactsFactoryAddError,                    // 3
    ContactsFactoryAddBlockedNumberSuccess,     // 4
    ContactsFactoryAddBlockedNumberError,       // 5
    ContactsFactoryAddGroupSuccess,             // 6
    ContactsFactoryAddGroupError,               // 7
    ContactsFactoryAddSpeedDialSuccess,         // 8
    ContactsFactoryAddSpeedDialError,           // 9
    ContactsFactoryAlphabetSearchSuccess(u32),  // 10
    ContactsFactoryAlphabetSearchError,         // 11
    ContactsFactoryClearContactsSuccess,        // 12
    ContactsFactoryClearContactsError,          // 13
    ContactsFactoryFindSuccess(u32),            // 14
    ContactsFactoryFindError,                   // 15
    ContactsFactoryFindBlockedNumbersSuccess(Option<Vec<String>>), // 16
    ContactsFactoryFindBlockedNumbersError,     // 17
    ContactsFactoryGetSuccess(ContactInfo),     // 18
    ContactsFactoryGetError,                    // 19
    ContactsFactoryGetAllSuccess(u32),          // 20
    ContactsFactoryGetAllError,                 // 21
    ContactsFactoryGetAllBlockedNumbersSuccess(Option<Vec<String>>), // 22
    ContactsFactoryGetAllBlockedNumbersError,   // 23
    ContactsFactoryGetAllGroupsSuccess(Option<Vec<GroupInfo>>), // 24
    ContactsFactoryGetAllGroupsError,           // 25
    ContactsFactoryGetAllIceSuccess(Option<Vec<IceInfo>>), // 26
    ContactsFactoryGetAllIceError,              // 27
    ContactsFactoryGetContactidsFromGroupSuccess(Option<Vec<String>>), // 28
    ContactsFactoryGetContactidsFromGroupError, // 29
    ContactsFactoryGetCountSuccess(i64),        // 30
    ContactsFactoryGetCountError,               // 31
    ContactsFactoryGetSpeedDialsSuccess(Option<Vec<SpeedDialInfo>>), // 32
    ContactsFactoryGetSpeedDialsError,          // 33
    ContactsFactoryImportVcfSuccess(i64),       // 34
    ContactsFactoryImportVcfError,              // 35
    ContactsFactoryMatchesSuccess(bool),        // 36
    ContactsFactoryMatchesError,                // 37
    ContactsFactoryRemoveSuccess,               // 38
    ContactsFactoryRemoveError,                 // 39
    ContactsFactoryRemoveBlockedNumberSuccess,  // 40
    ContactsFactoryRemoveBlockedNumberError,    // 41
    ContactsFactoryRemoveGroupSuccess,          // 42
    ContactsFactoryRemoveGroupError,            // 43
    ContactsFactoryRemoveIceSuccess,            // 44
    ContactsFactoryRemoveIceError,              // 45
    ContactsFactoryRemoveSpeedDialSuccess,      // 46
    ContactsFactoryRemoveSpeedDialError,        // 47
    ContactsFactorySetIceSuccess,               // 48
    ContactsFactorySetIceError,                 // 49
    ContactsFactoryUpdateSuccess,               // 50
    ContactsFactoryUpdateError,                 // 51
    ContactsFactoryUpdateGroupSuccess,          // 52
    ContactsFactoryUpdateGroupError,            // 53
    ContactsFactoryUpdateSpeedDialSuccess,      // 54
    ContactsFactoryUpdateSpeedDialError,        // 55
    ContactsFactoryBlockednumberChangeEvent(BlockedNumberChangeEvent), // 56
    ContactsFactoryContactsChangeEvent(ContactsChangeEvent), // 57
    ContactsFactoryGroupChangeEvent(GroupChangeEvent), // 58
    ContactsFactorySimContactLoadedEvent(SimContactLoadedEvent), // 59
    ContactsFactorySpeeddialChangeEvent(SpeedDialChangeEvent), // 60
}
