/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::messages::*;
use log::debug;
use nserror::{nsresult, NS_ERROR_NOT_IMPLEMENTED, NS_OK};
use nsstring::*;
use xpcom::RefPtr;

// XPCOM implementation of nsIBlockedNumberFindOptions, used to dispatch BlockedNumberFindOptions to events and observers.
#[xpcom(implement(nsIBlockedNumberFindOptions), atomic)]
pub struct BlockedNumberFindOptionsXpcom {
    filter_value: nsString,
    filter_option: u16,
}

impl BlockedNumberFindOptionsXpcom {
    pub fn new(options: &BlockedNumberFindOptions) -> RefPtr<Self> {
        debug!("BlockedNumberFindOptionsXpcom::new");

        Self::allocate(InitBlockedNumberFindOptionsXpcom {
            filter_value: nsString::from(&options.filter_value),
            filter_option: match options.filter_option {
                FilterOption::Equals => 0,
                FilterOption::Contains => 1,
                FilterOption::Match => 2,
                FilterOption::StartsWith => 3,
                FilterOption::FuzzyMatch => 4,
            },
        })
    }
    xpcom_method!(set_filter_value => SetFilterValue(aFilterValue: *const nsAString));
    fn set_filter_value(&self, _filter_value: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }
    xpcom_method!(set_filter_option => SetFilterOption(aFilterOption: u16));
    fn set_filter_option(&self, _filter_option: u16) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }
    xpcom_method!(get_filter_value => GetFilterValue(filterValue: *mut nsAString));
    fn get_filter_value(&self, filter_value: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*filter_value).assign(&self.filter_value);
        }
        Ok(())
    }
    xpcom_method!(get_filter_option => GetFilterOption(filterOption: *mut u16));
    fn get_filter_option(&self, filter_option: *mut u16) -> Result<(), nsresult> {
        unsafe {
            (*filter_option) = self.filter_option;
        }
        Ok(())
    }
}
