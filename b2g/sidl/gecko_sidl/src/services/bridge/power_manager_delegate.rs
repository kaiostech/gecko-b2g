/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper for the nsIPowerManagerDelegate interface.

use super::messages::*;
use super::service::ObjectIdGenerator;
use crate::common::client_object::*;
use crate::common::core::{BaseMessage, BaseMessageKind};
use crate::common::sidl_task::{SidlRunnable, SidlTask};
use crate::common::traits::{Shared, TrackerId};
use crate::common::uds_transport::{
    from_base_message, SessionObject, UdsTransport, XpcomSessionObject,
};
use bincode::Options;
use log::{debug, error};
use moz_task::{ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_NOT_AVAILABLE, NS_OK};
use nsstring::*;
use std::any::Any;
use std::collections::HashMap;
use std::ptr;
use xpcom::{
    getter_addrefs,
    interfaces::{nsIPowerManagerDelegate, nsIPowerManagerService, nsIWakeLock},
    RefPtr,
};

pub struct WakeLockDelegate {
    xpcom: ThreadPtrHandle<nsIWakeLock>,
    service_id: TrackerId,
    object_id: TrackerId,
    transport: UdsTransport,
    topic: String,
    wakelocks: Shared<HashMap<u32, Option<ClientObject<WakeLockDelegate>>>>,
}

impl WakeLockDelegate {
    pub fn new(
        xpcom: ThreadPtrHandle<nsIWakeLock>,
        service_id: TrackerId,
        object_id: TrackerId,
        transport: &UdsTransport,
        topic: String,
        wakelocks: Shared<HashMap<u32, Option<ClientObject<WakeLockDelegate>>>>,
    ) -> Self {
        Self {
            xpcom,
            service_id,
            object_id,
            transport: transport.clone(),
            topic,
            wakelocks,
        }
    }

    fn post_task(&mut self, command: WakeLockCommand, request_id: u64) {
        let task = WakeLockTask {
            xpcom: self.xpcom.clone(),
            command,
            transport: self.transport.clone(),
            service_id: self.service_id,
            object_id: self.object_id,
            request_id,
            topic: self.topic.clone(),
            wakelocks: self.wakelocks.clone(),
        };
        let _ = SidlRunnable::new("WakeLockTask", Box::new(task))
            .and_then(|r| SidlRunnable::dispatch(r, self.xpcom.owning_thread()));
    }
}

impl SessionObject for WakeLockDelegate {
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage> {
        debug!(
            "WakeLockDelegate on_request request id = {} service id {} object id {}",
            request_id, request.service, request.object
        );
        // Unpack the request.
        match from_base_message(&request) {
            Ok(GeckoBridgeToClient::WakelockGetTopic) => {
                debug!("WakeLockDelegate WakelockGetTopic");
                self.post_task(WakeLockCommand::WakelockGetTopic, request_id);
            }
            Ok(GeckoBridgeToClient::WakelockUnlock) => {
                debug!("WakeLockDelegate WakelockUnlock");
                self.post_task(WakeLockCommand::WakelockUnlock, request_id);
            }
            _ => {
                error!(
                    "WakeLockDelegate::on_request unexpected message: {:?}",
                    request.content
                );
            }
        }
        None
    }

    fn on_event(&mut self, _event: Vec<u8>) {}

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, self.object_id)
    }
}

impl XpcomSessionObject for WakeLockDelegate {
    fn as_xpcom(&self) -> &dyn Any {
        &self.xpcom
    }
}

// Commands supported by the wakelock delegate.
#[derive(Clone)]
enum WakeLockCommand {
    WakelockGetTopic,
    WakelockUnlock,
}

// A Task to dispatch commands to the delegate.
#[derive(Clone)]
struct WakeLockTask {
    xpcom: ThreadPtrHandle<nsIWakeLock>,
    command: WakeLockCommand,
    transport: UdsTransport,
    service_id: TrackerId,
    object_id: TrackerId,
    request_id: u64,
    topic: String,
    wakelocks: Shared<HashMap<u32, Option<ClientObject<WakeLockDelegate>>>>,
}

impl WakeLockTask {
    fn reply(&self, payload: GeckoBridgeFromClient, obj_id: TrackerId) {
        let message = BaseMessage {
            service: self.service_id,
            object: obj_id,
            kind: BaseMessageKind::Response(self.request_id),
            content: crate::common::get_bincode().serialize(&payload).unwrap(),
        };
        let mut t = self.transport.clone();
        let _ = t.send_message(&message);
    }
}

impl SidlTask for WakeLockTask {
    fn run(&self) {
        // Call the method on the initial thread.
        debug!("WakeLockTask::run");
        match &self.command {
            WakeLockCommand::WakelockGetTopic => {
                debug!("WakeLockTask:: Get topic {}", self.topic);
                let payload = GeckoBridgeFromClient::WakelockGetTopicSuccess(self.topic.clone());
                self.reply(payload, self.object_id);
            }
            WakeLockCommand::WakelockUnlock => {
                debug!("WakeLockTask:: unlock");
                let payload;
                if let Some(object) = self.xpcom.get() {
                    unsafe {
                        object.Unlock();
                    }
                    payload = GeckoBridgeFromClient::WakelockUnlockSuccess;
                } else {
                    payload = GeckoBridgeFromClient::WakelockUnlockError;
                }
                self.reply(payload, self.object_id);
                self.wakelocks.lock().remove(&self.object_id);
            }
        }
    }
}

pub struct PowerManagerDelegate {
    xpcom: ThreadPtrHandle<nsIPowerManagerDelegate>,
    service_id: TrackerId,
    object_id: TrackerId,
    object_id_generator: Shared<ObjectIdGenerator>,
    wakelocks: Shared<HashMap<u32, Option<ClientObject<WakeLockDelegate>>>>,
    transport: UdsTransport,
}

impl PowerManagerDelegate {
    pub fn new(
        xpcom: ThreadPtrHandle<nsIPowerManagerDelegate>,
        service_id: TrackerId,
        object_id: TrackerId,
        object_id_generator: Shared<ObjectIdGenerator>,
        wakelocks: Shared<HashMap<u32, Option<ClientObject<WakeLockDelegate>>>>,
        transport: &UdsTransport,
    ) -> Self {
        Self {
            xpcom,
            service_id,
            object_id,
            object_id_generator,
            wakelocks,
            transport: transport.clone(),
        }
    }

    fn post_task(&mut self, command: PowerManagerCommand, request_id: u64) {
        let task = PowerManagerDelegateTask {
            xpcom: self.xpcom.clone(),
            command,
            transport: self.transport.clone(),
            service_id: self.service_id,
            object_id: self.object_id,
            object_id_generator: self.object_id_generator.clone(),
            wakelocks: self.wakelocks.clone(),
            request_id,
        };
        let _ = SidlRunnable::new("PowerManagerDelegate", Box::new(task))
            .and_then(|r| SidlRunnable::dispatch(r, self.xpcom.owning_thread()));
    }
}

impl SessionObject for PowerManagerDelegate {
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage> {
        debug!("PowerManagerDelegate on_request {}", request_id);
        // Unpack the request.
        match from_base_message(&request) {
            Ok(GeckoBridgeToClient::PowerManagerDelegateSetScreenEnabled(
                value,
                is_external_screen,
            )) => {
                debug!("PowerManagerDelegate set_screen_enabled {}", value);
                self.post_task(
                    PowerManagerCommand::SetScreenEnabled(value, is_external_screen),
                    request_id,
                );
            }
            Ok(GeckoBridgeToClient::PowerManagerDelegateRequestWakelock(topic)) => {
                debug!("PowerManagerDelegate Request Wakelock {}", topic);
                self.post_task(PowerManagerCommand::RequestWakelock(topic), request_id);
            }
            _ => {
                error!(
                    "PowerManagerDelegate::on_request unexpected message: {:?}",
                    request.content
                );
            }
        }
        None
    }

    fn on_event(&mut self, _event: Vec<u8>) {}

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, self.object_id)
    }
}

impl XpcomSessionObject for PowerManagerDelegate {
    fn as_xpcom(&self) -> &dyn Any {
        &self.xpcom
    }
}

impl Drop for PowerManagerDelegate {
    fn drop(&mut self) {
        self.wakelocks.lock().clear();
    }
}

// Commands supported by the power manager delegate.
#[derive(Clone)]
enum PowerManagerCommand {
    SetScreenEnabled(bool, bool),
    RequestWakelock(String),
}

// A Task to dispatch commands to the delegate.
#[derive(Clone)]
struct PowerManagerDelegateTask {
    xpcom: ThreadPtrHandle<nsIPowerManagerDelegate>,
    command: PowerManagerCommand,
    transport: UdsTransport,
    service_id: TrackerId,
    object_id: TrackerId,
    object_id_generator: Shared<ObjectIdGenerator>,
    wakelocks: Shared<HashMap<u32, Option<ClientObject<WakeLockDelegate>>>>,
    request_id: u64,
}

impl PowerManagerDelegateTask {
    fn reply(&self, payload: GeckoBridgeFromClient, obj_id: TrackerId) {
        let message = BaseMessage {
            service: self.service_id,
            object: obj_id,
            kind: BaseMessageKind::Response(self.request_id),
            content: crate::common::get_bincode().serialize(&payload).unwrap(),
        };
        let mut t = self.transport.clone();
        let _ = t.send_message(&message);
    }

    fn new_wakelock(&self, topic: &String) -> Result<RefPtr<nsIWakeLock>, nsresult> {
        if let Some(power_manager_service) = xpcom::get_service::<nsIPowerManagerService>(cstr!(
            "@mozilla.org/power/powermanagerservice;1"
        )) {
            let _topic = nsString::from(topic);
            getter_addrefs(|p| unsafe {
                debug!("PowerManagerDelegateTask:: create wakelock successfully");
                power_manager_service.NewWakeLock(&*(_topic) as &nsAString, ptr::null(), p)
            })
        } else {
            error!("PowerManagerDelegateTask:: failed to get powerManagerService XPCOM");
            Err(NS_ERROR_NOT_AVAILABLE)
        }
    }
}

impl SidlTask for PowerManagerDelegateTask {
    fn run(&self) {
        // Call the method on the initial thread.
        debug!("PowerManagerDelegateTask::run");
        if let Some(object) = self.xpcom.get() {
            match &self.command {
                PowerManagerCommand::SetScreenEnabled(value, is_external_screen) => {
                    let status = unsafe { object.SetScreenEnabled(*value, *is_external_screen) };
                    let payload = match status {
                        NS_OK => GeckoBridgeFromClient::PowerManagerDelegateSetScreenEnabledSuccess,
                        _ => GeckoBridgeFromClient::PowerManagerDelegateSetScreenEnabledError,
                    };
                    self.reply(payload, self.object_id);
                }
                PowerManagerCommand::RequestWakelock(topic) => {
                    debug!("PowerManagerDelegateTask:: request wakelock {}", topic);
                    let wakelock_object_id = self.object_id_generator.lock().next_id();
                    // Default value to please the compiler.
                    let mut payload =
                        GeckoBridgeFromClient::PowerManagerDelegateRequestWakelockError;
                    if let Ok(wakelock_ptr) = self.new_wakelock(topic) {
                        if let Ok(holder) = ThreadPtrHolder::new(cstr!("nsIWakeLock"), wakelock_ptr)
                        {
                            let wakelock_session = WakeLockDelegate::new(
                                holder,
                                self.service_id,
                                wakelock_object_id,
                                &self.transport.clone(),
                                topic.to_string(),
                                self.wakelocks.clone(),
                            );
                            let client = Some(ClientObject::new::<WakeLockDelegate>(
                                wakelock_session,
                                &mut self.transport.clone(),
                            ));
                            self.wakelocks.lock().insert(wakelock_object_id, client);
                            payload =
                                GeckoBridgeFromClient::PowerManagerDelegateRequestWakelockSuccess(
                                    wakelock_object_id.into(),
                                );
                        } else {
                            error!("Failed to get ThreadPtrHolder for nsIWakeLock object");
                        }
                    }
                    self.reply(payload, self.object_id);
                }
            }
        }
    }
}
