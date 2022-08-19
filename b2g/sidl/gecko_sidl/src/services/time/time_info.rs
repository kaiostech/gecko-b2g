/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//use crate::common::sidl_task::*;
use crate::services::time::messages::TimeInfo;
use log::debug;
use nserror::{nsresult, NS_OK};
use nsstring::*;
use xpcom::RefPtr;

// XPCOM implementation of nsITimeInfo, used to dispatch TimeInfo to observers.
#[xpcom(implement(nsITimeInfo), atomic)]
pub struct TimeInfoXpcom {
    reason: i16,
    timezone: nsString,
    delta: i64,
}

impl TimeInfoXpcom {
    pub fn new(time_info: &TimeInfo) -> RefPtr<Self> {
        debug!("TimeInfoXpcom::new");

        Self::allocate(InitTimeInfoXpcom {
            reason: time_info.reason as i16,
            timezone: nsString::from(&time_info.timezone),
            delta: time_info.delta,
        })
    }

    xpcom_method!(get_reason => GetReason(reaon: *mut i16));
    fn get_reason(&self, reason: *mut i16) -> Result<(), nsresult> {
        unsafe {
            *reason = self.reason;
        }
        Ok(())
    }

    xpcom_method!(get_timezone => GetTimezone(timezone: *mut nsAString));
    fn get_timezone(&self, timezone: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*timezone).assign(&self.timezone);
        }
        Ok(())
    }

    xpcom_method!(get_delta => GetDelta(delta: *mut i64));
    fn get_delta(&self, delta: *mut i64) -> Result<(), nsresult> {
        unsafe {
            *delta = self.delta;
        }
        Ok(())
    }
}
