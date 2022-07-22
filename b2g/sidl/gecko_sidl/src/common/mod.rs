/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub mod client_object;
#[macro_use]
pub mod connection_observer;
pub mod core;
pub mod daemon_manager;
pub mod default_response;
#[macro_use]
pub mod event_manager;
pub mod frame;
#[macro_use]
pub mod sidl_task;
pub mod traits;
pub mod uds_transport;

use bincode::Options;
use nsstring::nsString;
use serde::de::Visitor;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use std::fmt;
use std::ops::Deref;
use std::time::UNIX_EPOCH;
use traits::TrackerId;

pub fn get_bincode() -> impl Options {
    bincode::options().with_big_endian().with_varint_encoding()
}

pub fn deserialize_bincode<'a, T>(input: &'a [u8]) -> Result<T, bincode::Error>
where
    T: Deserialize<'a>,
{
    get_bincode().deserialize(input)
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq, Serialize, Deserialize)]
pub struct ObjectRef(TrackerId);
impl From<TrackerId> for ObjectRef {
    fn from(val: TrackerId) -> Self {
        Self(val)
    }
}
impl From<ObjectRef> for TrackerId {
    fn from(val: ObjectRef) -> Self {
        val.0
    }
}

// A wrapper around a JsonValue to help with the encoding/decoding of JSON strings.
#[derive(Clone, Debug)]
pub struct JsonValue(serde_json::Value);

impl<'de> Deserialize<'de> for JsonValue {
    fn deserialize<D>(deserializer: D) -> Result<JsonValue, D::Error>
    where
        D: Deserializer<'de>,
    {
        use serde::de::Error;
        use std::str::FromStr;

        struct JsonVisitor;
        impl<'de> Visitor<'de> for JsonVisitor {
            type Value = String;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "a JSON string")
            }

            fn visit_str<E>(self, s: &str) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(s.to_owned())
            }
        }

        let json_str = deserializer.deserialize_str(JsonVisitor)?;
        let value = serde_json::Value::from_str(&json_str)
            .map_err(|err| D::Error::custom(format!("{}", err)))?;

        Ok(JsonValue(value))
    }
}

impl Serialize for JsonValue {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_str(&self.0.to_string())
    }
}

impl Deref for JsonValue {
    type Target = serde_json::Value;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<serde_json::Value> for JsonValue {
    fn from(v: serde_json::Value) -> Self {
        JsonValue(v)
    }
}

impl From<JsonValue> for String {
    fn from(v: JsonValue) -> Self {
        v.to_string()
    }
}

impl From<JsonValue> for nsString {
    fn from(v: JsonValue) -> Self {
        let s = &v.to_string();
        s.into()
    }
}

// The generated code from sidl project includes SystemTime. Copy from sidl project.
//A wrapper around a std::time::SystemTime to provide serde support as i64 milliseconds since epoch.
#[derive(Clone, Debug)]
pub struct SystemTime(pub std::time::SystemTime);

impl<'de> Deserialize<'de> for SystemTime {
    fn deserialize<D>(deserializer: D) -> Result<SystemTime, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct TimeVisitor;
        impl<'de> Visitor<'de> for TimeVisitor {
            type Value = i64;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                write!(formatter, "i64: time in ms since eopch")
            }

            fn visit_i64<E>(self, val: i64) -> Result<Self::Value, E>
            where
                E: serde::de::Error,
            {
                Ok(val)
            }
        }

        let milliseconds = deserializer.deserialize_i64(TimeVisitor)?;
        let system_time = if milliseconds >= 0 {
            UNIX_EPOCH
                .checked_add(std::time::Duration::from_millis(milliseconds as _))
                .unwrap_or(UNIX_EPOCH)
        } else {
            UNIX_EPOCH
                .checked_sub(std::time::Duration::from_millis(-milliseconds as _))
                .unwrap_or(UNIX_EPOCH)
        };
        Ok(SystemTime(system_time))
    }
}

impl Serialize for SystemTime {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match self.0.duration_since(UNIX_EPOCH) {
            Ok(from_epoch) => serializer.serialize_i64(from_epoch.as_millis() as _),
            // In the error case, we get the number of milliseconds as the error duration.
            Err(err) => serializer.serialize_i64(-(err.duration().as_millis() as i64)),
        }
    }
}

impl Deref for SystemTime {
    type Target = std::time::SystemTime;

    fn deref(&self) -> &Self::Target {
        &self.0
    }
}

impl From<std::time::SystemTime> for SystemTime {
    fn from(v: std::time::SystemTime) -> Self {
        SystemTime(v)
    }
}

// The type used to represent Blobs on the Rust side.
// Copied from api-daemon/common/src/lib.rs
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct Blob {
    mime_type: String,
    data: Vec<u8>,
}

impl Blob {
    pub fn len(&self) -> usize {
        self.data.len()
    }

    pub fn is_empty(&self) -> bool {
        self.data.is_empty()
    }

    pub fn mime_type(&self) -> String {
        self.mime_type.clone()
    }

    pub fn data(&self) -> &Vec<u8> {
        &self.data
    }

    pub fn set_mime_type(&mut self, mime_type: &str) {
        self.mime_type = mime_type.into();
    }

    pub fn set_data(&mut self, data: &[u8]) {
        self.data = data.to_vec();
    }
}

// Re-export url::Url to make it available to generated code.
pub use url::Url;
