/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::services::bridge::messages::SimContactInfo;
use log::debug;
use nserror::{nsresult, NS_ERROR_NOT_IMPLEMENTED, NS_OK};
use nsstring::*;
use xpcom::RefPtr;

// XPCOM implementation of nsISimContactInfo, used to dispatch SimContactInfo to events and observers.
#[xpcom(implement(nsISimContactInfo), atomic)]
pub struct SimContactInfoXpcom {
    id: nsString,
    tel: nsString,
    email: nsString,
    name: nsString,
    category: nsString,
}

impl SimContactInfoXpcom {
    pub fn new(contact: &SimContactInfo) -> RefPtr<Self> {
        debug!("SimContactInfoXpcom::new");

        Self::allocate(InitSimContactInfoXpcom {
            id: nsString::from(&contact.id),
            tel: nsString::from(&contact.tel),
            email: nsString::from(&contact.email),
            name: nsString::from(&contact.name),
            category: nsString::from(&contact.category),
        })
    }

    xpcom_method!(set_id => SetId(aId: *const nsAString));
    fn set_id(&self, _id: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(set_tel => SetTel(aTel: *const nsAString));
    fn set_tel(&self, _tel: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(set_email => SetEmail(aEmail: *const nsAString));
    fn set_email(&self, _email: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(set_name => SetName(aName: *const nsAString));
    fn set_name(&self, _name: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(set_category => SetCategory(aCategory: *const nsAString));
    fn set_category(&self, _category: *const nsAString) -> Result<(), nsresult> {
        Err(NS_ERROR_NOT_IMPLEMENTED)
    }

    xpcom_method!(get_id => GetId(id: *mut nsAString));
    fn get_id(&self, id: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*id).assign(&self.id);
        }
        Ok(())
    }

    xpcom_method!(get_tel => GetTel(tel: *mut nsAString));
    fn get_tel(&self, tel: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*tel).assign(&self.tel);
        }
        Ok(())
    }

    xpcom_method!(get_email => GetEmail(email: *mut nsAString));
    fn get_email(&self, email: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*email).assign(&self.email);
        }
        Ok(())
    }

    xpcom_method!(get_name => GetName(name: *mut nsAString));
    fn get_name(&self, name: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*name).assign(&self.name);
        }
        Ok(())
    }

    xpcom_method!(get_category => GetCategory(category: *mut nsAString));
    fn get_category(&self, category: *mut nsAString) -> Result<(), nsresult> {
        unsafe {
            (*category).assign(&self.category);
        }
        Ok(())
    }
}
