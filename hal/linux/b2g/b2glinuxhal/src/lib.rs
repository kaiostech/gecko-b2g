/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate xpcom;

use b2ghald::client::SimpleClient;
use log::{debug, error};
use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::{nsACString, nsCString};
use parking_lot::Mutex;
use std::ffi::CString;
use std::ops::DerefMut;
use xpcom::{interfaces::nsIB2gLinuxHal, RefPtr};

// Helper functions to get a char pref with a default value.
fn fallible_get_char_pref(name: &str, default_value: &str) -> Result<nsCString, nsresult> {
    if let Some(pref_service) = xpcom::services::get_PrefService() {
        let branch = xpcom::getter_addrefs(|p| {
            // Safe because:
            //  * `null` is explicitly allowed per documentation
            //  * `p` is a valid outparam guaranteed by `getter_addrefs`
            unsafe { pref_service.GetBranch(std::ptr::null(), p) }
        })?;
        let pref_name = CString::new(name).map_err(|_| NS_ERROR_FAILURE)?;
        let mut pref_value = nsCString::new();
        // Safe because:
        //  * `branch` is non-null (otherwise `getter_addrefs` would've been `Err`
        //  * `pref_name` exists so a pointer to it is valid for the life of the function
        //  * `channel` exists so a pointer to it is valid, and it can be written to
        unsafe {
            if (*branch)
                .GetCharPref(
                    pref_name.as_ptr(),
                    pref_value.deref_mut() as *mut nsACString,
                )
                .to_result()
                .is_err()
            {
                pref_value = default_value.into();
            }
        }
        Ok(nsCString::from(pref_value))
    } else {
        Ok(nsCString::from(default_value))
    }
}

fn get_char_pref(name: &str, default_value: &str) -> nsCString {
    match fallible_get_char_pref(name, default_value) {
        Ok(value) => value,
        Err(_) => nsCString::from(default_value),
    }
}

// Defaulting to the PinephonePro path.
static DEFAULT_FLASHLIGHT_PATH: &str = "/sys/class/leds/white:flash/";

#[derive(xpcom)]
#[xpimplements(nsIB2gLinuxHal)]
#[refcnt = "atomic"]
struct InitB2gLinuxHal {
    flashlight_path: nsCString,
    hal: Option<Mutex<SimpleClient>>,
}

impl B2gLinuxHal {
    fn new() -> RefPtr<Self> {
        let flashlight_path = get_char_pref("hal.linux.flashlight.path", DEFAULT_FLASHLIGHT_PATH);
        debug!("InitB2gLinuxHal::new(), path={}", flashlight_path);

        let hal = SimpleClient::new().map(Mutex::new);

        Self::allocate(InitB2gLinuxHal {
            flashlight_path,
            hal,
        })
    }

    xpcom_method!(enable_flashlight => EnableFlashlight());
    fn enable_flashlight(&self) -> Result<(), nsresult> {
        if self.flashlight_path.is_empty() {
            debug!("No flashlight configured!");
            return Err(NS_ERROR_INVALID_ARG);
        }

        if let Some(mutex) = &self.hal {
            let mut hal = mutex.lock();
            hal.enable_flashlight(&self.flashlight_path.to_string());
        } else {
            error!("Not connected to b2ghald!");
            return Err(NS_ERROR_FAILURE);
        }

        Ok(())
    }

    xpcom_method!(disable_flashlight => DisableFlashlight());
    fn disable_flashlight(&self) -> Result<(), nsresult> {
        if self.flashlight_path.is_empty() {
            debug!("No flashlight configured!");
            return Err(NS_ERROR_INVALID_ARG);
        }

        if let Some(mutex) = &self.hal {
            let mut hal = mutex.lock();
            hal.disable_flashlight(&self.flashlight_path.to_string());
        } else {
            error!("Not connected to b2ghald!");
            return Err(NS_ERROR_FAILURE);
        }

        Ok(())
    }

    xpcom_method!(is_flashlight_supported => IsFlashlighSupported(supported: *mut bool));
    fn is_flashlight_supported(&self, supported: *mut bool) -> Result<(), nsresult> {
        let mut result = false;
        if !self.flashlight_path.is_empty() {
            if let Some(mutex) = &self.hal {
                let mut hal = mutex.lock();
                result = hal.is_flashlight_supported(&self.flashlight_path.to_string());
            } else {
                error!("Not connected to b2ghald!");
                return Err(NS_ERROR_FAILURE);
            }
        }

        unsafe {
            *supported = result;
        }
        Ok(())
    }

    xpcom_method!(flashlight_state => FlashlightState(state: *mut bool));
    fn flashlight_state(&self, state: *mut bool) -> Result<(), nsresult> {
        let mut result = false;
        if !self.flashlight_path.is_empty() {
            if let Some(mutex) = &self.hal {
                let mut hal = mutex.lock();
                result = hal.flashlight_state(&self.flashlight_path.to_string());
            } else {
                error!("Not connected to b2ghald!");
                return Err(NS_ERROR_FAILURE);
            }
        }

        unsafe {
            *state = result;
        }
        Ok(())
    }
}

#[no_mangle]
pub unsafe extern "C" fn b2glinuxhal_construct(result: &mut *const nsIB2gLinuxHal) {
    let inst = B2gLinuxHal::new();
    *result = inst.coerce::<nsIB2gLinuxHal>();
    std::mem::forget(inst);
}
