/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::services::settings::messages::*;
use log::debug;
use nserror::{nsresult, NS_ERROR_NOT_IMPLEMENTED, NS_OK};
use nsstring::*;
use xpcom::RefPtr;

// XPCOM implementation of nsISettingError, used when rejecting a Get() call.
#[xpcom(implement(nsISettingError), atomic)]
pub struct SettingErrorXpcom {
    name: nsString,
    value: u16,
}

impl SettingErrorXpcom {
    pub fn new(error: &GetError) -> RefPtr<Self> {
        debug!("SettingErrorXpcom::new");

        Self::allocate(InitSettingErrorXpcom {
            name: nsString::from(&error.name),
            value: match error.reason {
                GetErrorReason::UnknownError => 0,
                GetErrorReason::NonExistingSetting => 1,
            },
        })
    }

    xpcom_method!(set_name => SetName(aName: *const nsAString));
    fn set_name(&self, _name: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(set_reason => SetReason(aValue: u16));
    fn set_reason(&self, _value: u16) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(get_name => GetName(name: *mut nsAString));
    fn get_name(&self, name: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*name).assign(&self.name);
        }
        Ok(())
    }

    xpcom_method!(get_reason => GetReason(value: *mut u16));
    fn get_reason(&self, value: *mut u16) -> Result<(), nsresult> {
        unsafe {
            (*value) = self.value;
        }
        Ok(())
    }
}

// Turns a SettingInfo into an xpcom nsISettingsError.
#[macro_export]
macro_rules! geterror_as_isettingerror {
    ($obj:ident) => {
        {
            let xpcom = SettingErrorXpcom::new(&$obj);
            // TODO: Check if instead of AddRef we can use mem::forget() like in settings_manager_construct()
            xpcom.AddRef();
            xpcom.coerce::<nsISettingError>()
        }

    }
}
