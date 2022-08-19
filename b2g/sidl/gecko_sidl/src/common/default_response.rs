/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use log::debug;
use moz_task::ThreadPtrHolder;
use nserror::{nsresult, NS_OK};
use xpcom::interfaces::nsISidlDefaultResponse;
use xpcom::RefPtr;

// XPCOM implementation of nsISidlDefaultResponse, used when we need a dummy implementation.
#[xpcom(implement(nsISidlDefaultResponse), atomic)]
pub struct SidlDefaultResponseXpcom {}

impl SidlDefaultResponseXpcom {
    pub fn new() -> RefPtr<Self> {
        debug!("SidlDefaultResponseXpcom::new");

        Self::allocate(InitSidlDefaultResponseXpcom {})
    }

    xpcom_method!(resolve => Resolve());
    fn resolve(&self) -> Result<(), nsresult> {
        debug!("SidlDefaultResponseXpcom::resolve");
        Ok(())
    }

    xpcom_method!(reject => Reject());
    fn reject(&self) -> Result<(), nsresult> {
        debug!("SidlDefaultResponseXpcom::reject");
        Ok(())
    }

    pub fn as_callback() -> RefPtr<ThreadPtrHolder<nsISidlDefaultResponse>> {
        let response = Self::new();
        unsafe {
            response.AddRef();
        }
        let callback = response.coerce::<nsISidlDefaultResponse>();
        ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback)).unwrap()
    }
}
