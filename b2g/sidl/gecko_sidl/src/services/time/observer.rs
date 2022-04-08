/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::time_info::TimeInfoXpcom;
use crate::common::core::{BaseMessage, BaseMessageKind};
use crate::common::sidl_task::{SidlRunnable, SidlTask};
use crate::common::traits::TrackerId;
use crate::common::uds_transport::*;
use crate::services::time::messages::*;
use bincode::Options;
use log::error;
use moz_task::ThreadPtrHandle;
use std::any::Any;
use xpcom::interfaces::nsITimeObserver;

// A Task to dispatch time/timezone changes to the observer.
#[derive(Clone)]
struct ObserverTask {
    xpcom: ThreadPtrHandle<nsITimeObserver>,
    time_info: TimeInfo,
}

impl SidlTask for ObserverTask {
    fn run(&self) {
        // Call the method on the initial thread.
        if let Some(object) = self.xpcom.get() {
            let timeinfo_xpcom = TimeInfoXpcom::new(&self.time_info);
            unsafe {
                object.Notify(timeinfo_xpcom.coerce());
            }
        }
    }
}

pub struct ObserverWrapper {
    xpcom: ThreadPtrHandle<nsITimeObserver>,
    service_id: TrackerId,
    object_id: TrackerId,
}

impl ObserverWrapper {
    pub fn new(
        xpcom: ThreadPtrHandle<nsITimeObserver>,
        service_id: TrackerId,
        object_id: TrackerId,
    ) -> Self {
        Self {
            xpcom,
            service_id,
            object_id,
        }
    }
}

impl SessionObject for ObserverWrapper {
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage> {
        // Unpack the request.
        if let Ok(TimeServiceToClient::TimeObserverCallback(time_info)) =
            from_base_message(&request)
        {
            // Dispatch the setting change to the xpcom observer.
            let task = ObserverTask {
                xpcom: self.xpcom.clone(),
                time_info,
            };
            let _ = SidlRunnable::new("ObserverWrapper", Box::new(task))
                .and_then(|r| SidlRunnable::dispatch(r, self.xpcom.owning_thread()));
            // Unconditionally Return a success response for now.
            let payload = TimeServiceFromClient::TimeObserverCallbackSuccess;

            // Wrap the payload in a base message and send it.
            let message = BaseMessage {
                service: self.service_id,
                object: self.object_id,
                kind: BaseMessageKind::Response(request_id),
                content: crate::common::get_bincode().serialize(&payload).unwrap(),
            };
            Some(message)
        } else {
            error!(
                "ObserverWrapper::on_request unexpected message: {:?}",
                request.content
            );
            None
        }
    }

    fn on_event(&mut self, _event: Vec<u8>) {}

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, self.object_id)
    }
}

impl XpcomSessionObject for ObserverWrapper {
    fn as_xpcom(&self) -> &dyn Any {
        &self.xpcom
    }
}
