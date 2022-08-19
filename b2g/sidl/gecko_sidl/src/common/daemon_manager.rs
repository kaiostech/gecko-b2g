/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of nsIDaemonManager, that let Gecko observer the api-daemon status.

use super::uds_transport::UdsTransport;
use log::debug;
use moz_task::ThreadPtrHolder;
use nserror::{nsresult, NS_OK};
use std::cell::Cell;
use xpcom::interfaces::{nsIDaemonManager, nsISidlConnectionObserver};
use xpcom::RefPtr;

#[xpcom(implement(nsIDaemonManager), atomic)]
pub struct DaemonManagerXpcom {
    transport: UdsTransport,
    observer: Cell<Option<usize>>,
}

impl DaemonManagerXpcom {
    pub fn new() -> RefPtr<Self> {
        debug!("DaemonManagerXpcom::new");

        let transport = UdsTransport::open();

        Self::allocate(InitDaemonManagerXpcom {
            transport,
            observer: Cell::default(),
        })
    }

    // nsIDaemonManager implementation.

    xpcom_method!(set_observer => SetObserver(observer: *const nsISidlConnectionObserver));
    fn set_observer(&self, observer: &nsISidlConnectionObserver) -> Result<(), nsresult> {
        let mut transport = self.transport.clone();
        self.observer
            .set(Some(transport.next_connection_observer_id()));

        let obs = ThreadPtrHolder::new(cstr!("nsISidlConnectionObserver"), RefPtr::new(observer))?;
        transport.add_connection_observer(obs);

        Ok(())
    }

    xpcom_method!(is_ready => IsReady(_retval: *mut bool));
    fn is_ready(&self, retval: *mut bool) -> Result<(), nsresult> {
        unsafe {
            *retval = self.transport.is_ready();
        }
        Ok(())
    }
}

impl Drop for DaemonManagerXpcom {
    fn drop(&mut self) {
        if let Some(id) = self.observer.get() {
            let mut transport = self.transport.clone();
            transport.remove_connection_observer(id);
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn daemon_manager_construct(result: &mut *const nsIDaemonManager) {
    let inst = DaemonManagerXpcom::new();
    *result = inst.coerce::<nsIDaemonManager>();
    std::mem::forget(inst);
}
