/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of the nsISettings xpcom interface

use super::setting_error::*;
use super::setting_info::*;
use crate::common::client_object::*;
use crate::common::core::BaseMessage;
use crate::common::default_response::*;
use crate::common::event_manager::*;
use crate::common::sidl_task::*;
use crate::common::traits::{Shared, TrackerId};
use crate::common::uds_transport::*;
use crate::geterror_as_isettingerror;
use crate::services::core::service::*;
use crate::services::settings::messages::*;
use crate::services::settings::observer::*;
use crate::settinginfo_as_isettinginfo;
use log::{debug, error};
use moz_task::{ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::*;
use parking_lot::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        nsISettingError, nsISettingInfo, nsISettingsGetBatchResponse, nsISettingsGetResponse,
        nsISettingsManager, nsISettingsObserver, nsISidlConnectionObserver, nsISidlDefaultResponse,
        nsISidlEventListener,
    },
    RefPtr,
};

type GetSuccessType = SettingInfo;
type GetBatchSuccessType = Vec<SettingInfo>;

macro_rules! sidl_callback_for_resolve_settings_array {
    ($interface:ty, $success:ty, $error: ty, $errtrans:tt) => {
        impl SidlCallback for $interface {
            type Success = $success;
            type Error = $error;

            fn resolve(&self, value: &Self::Success) {
                let mut list: thin_vec::ThinVec<Option<RefPtr<nsISettingInfo>>> = ThinVec::new();
                for settinginfo in value {
                    let settinginfo_xpcom = SettingInfoXpcom::new(&settinginfo);
                    unsafe {
                        settinginfo_xpcom.AddRef();
                        let xpcom_coerce = settinginfo_xpcom.coerce::<nsISettingInfo>();
                        list.push(Some(RefPtr::new(xpcom_coerce)));
                    }
                }
                unsafe {
                    self.Resolve(&list);
                }
            }

            fn reject(&self, value: &Self::Error) {
                unsafe {
                    self.Reject($errtrans!(value));
                }
            }
        }
    };
}
sidl_callback_for!(nsISidlDefaultResponse);
sidl_callback_for!(
    nsISettingsGetResponse,
    GetSuccessType,
    GetError,
    settinginfo_as_isettinginfo,
    geterror_as_isettingerror
);
sidl_callback_for_resolve_settings_array!(
    nsISettingsGetBatchResponse,
    GetBatchSuccessType,
    GetError,
    geterror_as_isettingerror
);

// Task types
type ClearTask = SidlCallTask<(), (), nsISidlDefaultResponse>;
type SetTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    Vec<SettingInfo>, /* settings */
);
type GetTask = (
    SidlCallTask<GetSuccessType, GetError, nsISettingsGetResponse>,
    String, /* name */
);
type GetBatchTask = (
    SidlCallTask<GetBatchSuccessType, GetError, nsISettingsGetBatchResponse>,
    Vec<String>, /* names */
);
type AddObserverTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    (String, ThreadPtrHandle<nsISettingsObserver>, usize), /* (name, observer, key) */
);
type RemoveObserverTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    (String, usize), /* (name, key) */
);

// The tasks that can be dispatched to the calling thread.
enum SettingsTask {
    Clear(ClearTask),
    Set(SetTask),
    Get(GetTask),
    GetBatch(GetBatchTask),
    AddObserver(AddObserverTask),
    RemoveObserver(RemoveObserverTask),
}

struct SettingsManagerImpl {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The service ID of this instance.
    service_id: TrackerId,
    // The core service we use to manage event listeners.
    core_service: Arc<Mutex<CoreService>>,
    // Helper struct to send tasks.
    sender: TaskSender,
    // Events management.
    event_manager: Shared<EventManager>,
    // The registered observers. (object_id, key, object, name)
    observers: Vec<(TrackerId, usize, ClientObject<ObserverWrapper>, String)>,
    // The next usable object_id when we create an observer.
    current_object_id: TrackerId,
}

impl SessionObject for SettingsManagerImpl {
    fn on_request(&mut self, _request: BaseMessage, _id: u64) -> Option<BaseMessage> {
        debug!("SettingsManagerImpl::on_request");
        None
    }

    fn on_event(&mut self, event_data: Vec<u8>) {
        debug!("SettingsManagerImpl::on_event");
        // TODO: Extract that to some helper.
        match crate::common::deserialize_bincode::<SettingsManagerToClient>(&event_data) {
            Ok(SettingsManagerToClient::SettingsFactoryChangeEvent(event_info)) => {
                let event_manager = self.event_manager.lock();
                let listeners = event_manager
                    .handlers
                    .get(&(nsISettingsManager::CHANGE_EVENT as _));
                if let Some(listeners) = listeners {
                    debug!(
                        "Received settings change for {}, will dispatch to {} listeners.",
                        event_info.name,
                        listeners.len()
                    );
                    for listener in listeners {
                        // We are not on the calling thread, so dispatch a task.
                        let mut task = SidlEventTask::new(listener.0.clone());
                        task.set_value(event_info.clone());
                        let _ = SidlRunnable::new("SidlEvent", Box::new(task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, task.thread()));
                    }
                } else {
                    debug!(
                        "Received settings change for {}, no listeners.",
                        event_info.name
                    );
                };
            }
            Ok(other) => error!("Unexpected setting event: {:?}", other),
            Err(err) => error!("Failed to deserialize settings event: {}", err),
        }
    }

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, 0)
    }
}

impl ServiceClientImpl<SettingsTask> for SettingsManagerImpl {
    fn new(mut transport: UdsTransport, service_id: TrackerId) -> Self {
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));
        transport.register_session_object(core_service.clone());
        let sender = TaskSender::new(transport.clone(), service_id);

        let event_manager = Shared::adopt(EventManager::new(
            core_service.clone(),
            service_id,
            0, /* object id */
        ));

        Self {
            transport,
            service_id,
            core_service,
            sender,
            event_manager,
            observers: vec![],
            // current_object_id starts at 1 since 0 is reserved for the service object itself.
            current_object_id: 1,
        }
    }

    fn run_task(&mut self, task: SettingsTask) -> Result<(), nsresult> {
        match task {
            SettingsTask::Clear(task) => self.clear(task),
            SettingsTask::Set(task) => self.set(task),
            SettingsTask::Get(task) => self.get(task),
            SettingsTask::GetBatch(task) => self.get_batch(task),
            SettingsTask::AddObserver(task) => self.add_observer(task),
            SettingsTask::RemoveObserver(task) => self.remove_observer(task),
        }
    }

    fn dispatch_queue(
        &mut self,
        task_queue: &Shared<Vec<SettingsTask>>,
        pending_listeners: &PendingListeners,
    ) {
        let mut task_queue = task_queue.lock();
        debug!("Running the {} pending tasks", task_queue.len());

        // Manage event listeners.
        {
            let mut event_manager = self.event_manager.lock();
            for (event, handler, key) in pending_listeners.lock().drain(..) {
                event_manager.add_event_listener(event, handler, key);
            }
        }

        // drain the queue.
        for task in task_queue.drain(..) {
            let _ = self.run_task(task);
        }
    }
}

impl SettingsManagerImpl {
    fn get_tasks_to_reconnect(&mut self) -> Vec<SettingsTask> {
        let mut tasks = vec![];

        // Re-create the tasks to add the observers.
        for (_object_id, key, client_object, name) in &mut self.observers {
            client_object.dont_release();

            if let Some(delegate) = client_object.maybe_xpcom::<nsISettingsObserver>() {
                let task = (
                    SidlCallTask::new(SidlDefaultResponseXpcom::as_callback()),
                    (name.clone(), delegate, *key),
                );
                tasks.push(SettingsTask::AddObserver(task));
            }
        }
        self.observers.clear();

        tasks
    }

    fn clear(&mut self, task: ClearTask) -> Result<(), nsresult> {
        debug!("SettingsManager::clear");

        let request = SettingsManagerFromClient::SettingsFactoryClear;
        self.sender.send_task(&request, ClearTaskReceiver { task });
        Ok(())
    }

    fn set(&mut self, task: SetTask) -> Result<(), nsresult> {
        debug!("SettingsManager::set");
        let (task, settings) = task;
        let request = SettingsManagerFromClient::SettingsFactorySet(settings);
        self.sender.send_task(&request, SetTaskReceiver { task });
        Ok(())
    }

    fn get(&mut self, task: GetTask) -> Result<(), nsresult> {
        debug!("SettingsManager::get");

        let (task, name) = task;
        let request = SettingsManagerFromClient::SettingsFactoryGet(name);
        self.sender.send_task(&request, GetTaskReceiver { task });
        Ok(())
    }

    fn get_batch(&mut self, task: GetBatchTask) -> Result<(), nsresult> {
        debug!("SettingsManager::get_batch");

        let (task, names) = task;
        let request = SettingsManagerFromClient::SettingsFactoryGetBatch(names);
        self.sender
            .send_task(&request, GetBatchTaskReceiver { task });
        Ok(())
    }

    fn add_observer(&mut self, task: AddObserverTask) -> Result<(), nsresult> {
        debug!("SettingsManager::add_observer");
        let object_id = self.next_object_id();

        let (task, (name, observer, key)) = task;
        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper = ObserverWrapper::new(observer, self.service_id, object_id);
        let proxy = ClientObject::new::<ObserverWrapper>(wrapper, &mut self.transport);

        self.observers.push((object_id, key, proxy, name.clone()));

        let request = SettingsManagerFromClient::SettingsFactoryAddObserver(name, object_id.into());
        self.sender
            .send_task(&request, AddObserverTaskReceiver { task });

        Ok(())
    }

    fn remove_observer(&mut self, task: RemoveObserverTask) -> Result<(), nsresult> {
        debug!("SettingsManager::remove_observer");

        let (task, (name, key)) = task;
        let object_id = self.get_observer_id(key);

        if let Some(pos) = self
            .observers
            .iter()
            .position(|item| item.1 == key && item.3 == name)
        {
            self.observers.remove(pos);
        }

        if let Some(object_id) = object_id {
            let request =
                SettingsManagerFromClient::SettingsFactoryRemoveObserver(name, object_id.into());
            self.sender
                .send_task(&request, RemoveObserverTaskReceiver { task });
        }
        Ok(())
    }

    // Returns the object id for a given observer.
    fn get_observer_id(&self, key: usize) -> Option<TrackerId> {
        debug!("SettingsManager::get_observer_id for {}", key);
        self.observers
            .iter()
            .find(|item| item.1 == key)
            .map(|item| item.0)
    }

    fn next_object_id(&mut self) -> TrackerId {
        let res = self.current_object_id;
        self.current_object_id += 1;
        res
    }

    impl_event_listener!();
}

impl Drop for SettingsManagerImpl {
    fn drop(&mut self) {
        self.transport
            .unregister_session_object(self.core_service.clone());
    }
}

task_receiver!(
    ClearTaskReceiver,
    nsISidlDefaultResponse,
    SettingsManagerToClient,
    SettingsFactoryClearSuccess,
    SettingsFactoryClearError
);

task_receiver!(
    SetTaskReceiver,
    nsISidlDefaultResponse,
    SettingsManagerToClient,
    SettingsFactorySetSuccess,
    SettingsFactorySetError
);

task_receiver_success_error!(
    GetTaskReceiver,
    nsISettingsGetResponse,
    SettingsManagerToClient,
    SettingsFactoryGetSuccess,
    SettingsFactoryGetError,
    GetSuccessType,
    GetError
);

task_receiver_success_error!(
    GetBatchTaskReceiver,
    nsISettingsGetBatchResponse,
    SettingsManagerToClient,
    SettingsFactoryGetBatchSuccess,
    SettingsFactoryGetError,
    GetBatchSuccessType,
    GetError
);

task_receiver!(
    AddObserverTaskReceiver,
    nsISidlDefaultResponse,
    SettingsManagerToClient,
    SettingsFactoryAddObserverSuccess,
    SettingsFactoryAddObserverError
);

task_receiver!(
    RemoveObserverTaskReceiver,
    nsISidlDefaultResponse,
    SettingsManagerToClient,
    SettingsFactoryRemoveObserverSuccess,
    SettingsFactoryRemoveObserverError
);

#[xpcom(implement(nsISettingsManager, nsISidlConnectionObserver), atomic)]
struct SettingsManagerXpcom {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The core service we use to get an instance of the SettingsManager service.
    core_service: Arc<Mutex<CoreService>>,
    // The implementation registered as a session object with the transport.
    inner: Shared<Option<Arc<Mutex<SettingsManagerImpl>>>>,
    // The list of pending tasks queued while we are waiting on the service id
    // to be available.
    pending_tasks: Shared<Vec<SettingsTask>>,
    // The list of pending event listeners to add.
    pending_listeners: PendingListeners,
    // Flag to know if are already fetching the service id.
    getting_service: AtomicBool,
    // Id of ourselves as a connection observer.
    connection_observer_id: usize,
}

impl SettingsManagerXpcom {
    fn new() -> RefPtr<Self> {
        debug!("SettingsManagerXpcom::new");
        let mut transport = UdsTransport::open();
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));

        let instance = Self::allocate(InitSettingsManagerXpcom {
            transport: transport.clone(),
            core_service,
            pending_tasks: Shared::adopt(vec![]),
            pending_listeners: Shared::adopt(vec![]),
            inner: Shared::adopt(None),
            getting_service: AtomicBool::new(false),
            connection_observer_id: transport.next_connection_observer_id(),
        });

        let obs = instance.coerce::<nsISidlConnectionObserver>();
        match ThreadPtrHolder::new(cstr!("nsISidlConnectionObserver"), RefPtr::new(obs)) {
            Ok(obs) => {
                transport.add_connection_observer(obs);
            }
            Err(err) => error!("Failed to create connection observer: {}", err),
        }

        instance
    }

    // nsISidlConnectionObserver implementation.
    implement_connection_observer!("SettingsManagerXpcom");

    // nsISettingsManager implementation.

    xpcom_method!(clear => Clear(callback: *const nsISidlDefaultResponse));
    fn clear(&self, callback: &nsISidlDefaultResponse) -> Result<(), nsresult> {
        debug!("SettingsManagerXpcom::clear");

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = SidlCallTask::new(callback);

        self.run_or_queue_task(Some(SettingsTask::Clear(task)));
        Ok(())
    }

    xpcom_method!(set => Set(settings: *const ThinVec<Option<RefPtr<nsISettingInfo>>>, callback: *const nsISidlDefaultResponse));
    fn set(
        &self,
        settings: &ThinVec<Option<RefPtr<nsISettingInfo>>>,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("SettingsManagerXpcom::set");

        // Validate parameters.
        let settings_info: Vec<SettingInfo> = settings
            .iter()
            .filter_map(|item| {
                if let Some(item) = item {
                    let mut name = nsString::new();
                    let mut value = nsString::new();
                    unsafe {
                        if item.GetName(&mut *name) != NS_OK {
                            return None;
                        }
                        if item.GetValue(&mut *value) != NS_OK {
                            return None;
                        };
                    }

                    let json: serde_json::Value = match serde_json::from_str(&value.to_string()) {
                        Ok(json) => json,
                        Err(err) => {
                            error!("Invalid json `{}` : {}", value, err);
                            return None;
                        }
                    };
                    Some(SettingInfo {
                        name: name.to_string(),
                        value: json.into(),
                    })
                } else {
                    None
                }
            })
            .collect();

        // If we failed to convert at least one element of the array, bail out.
        if settings_info.len() != settings.len() {
            return Err(NS_ERROR_INVALID_ARG);
        }

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), settings_info);

        self.run_or_queue_task(Some(SettingsTask::Set(task)));
        Ok(())
    }

    xpcom_method!(get => Get(name: *const nsAString, callback: *const nsISettingsGetResponse));
    fn get(&self, name: &nsAString, callback: &nsISettingsGetResponse) -> Result<(), nsresult> {
        debug!("SettingsManagerXpcom::get");

        let callback =
            ThreadPtrHolder::new(cstr!("nsISettingsGetResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), name.to_string());

        self.run_or_queue_task(Some(SettingsTask::Get(task)));
        Ok(())
    }

    xpcom_method!(get_batch => GetBatch(names: *const ThinVec<nsString>, callback: *const nsISettingsGetBatchResponse));
    fn get_batch(
        &self,
        names: &ThinVec<nsString>,
        callback: &nsISettingsGetBatchResponse,
    ) -> Result<(), nsresult> {
        debug!("SettingsManagerXpcom::get_batch");

        let names: Vec<String> = names
            .iter()
            .filter_map(|item| Some(item.to_string()))
            .collect();

        let callback =
            ThreadPtrHolder::new(cstr!("nsISettingsGetBatchResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), names);

        self.run_or_queue_task(Some(SettingsTask::GetBatch(task)));
        Ok(())
    }

    xpcom_method!(add_observer => AddObserver(name: *const nsAString, observer: *const nsISettingsObserver, callback: *const nsISidlDefaultResponse));
    fn add_observer(
        &self,
        name: &nsAString,
        observer: &nsISettingsObserver,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("SettingsManagerXpcom::add_observer {}", name);

        let key: usize = unsafe { std::mem::transmute(observer) };
        let observer = ThreadPtrHolder::new(cstr!("nsISettingsObserver"), RefPtr::new(observer))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (
            SidlCallTask::new(callback),
            (name.to_string(), observer, key),
        );

        self.run_or_queue_task(Some(SettingsTask::AddObserver(task)));
        Ok(())
    }

    xpcom_method!(remove_observer => RemoveObserver(name: *const nsAString, observer: *const nsISettingsObserver, callback: *const nsISidlDefaultResponse));
    fn remove_observer(
        &self,
        name: &nsAString,
        observer: &nsISettingsObserver,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("SettingsManagerXpcom::remove_observer {}", name);

        let key: usize = unsafe { std::mem::transmute(observer) };

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), (name.to_string(), key));

        self.run_or_queue_task(Some(SettingsTask::RemoveObserver(task)));
        Ok(())
    }

    task_runner!(SettingsTask, "SettingsManager", SERVICE_FINGERPRINT);
    xpcom_sidl_event_target!();
}

impl Drop for SettingsManagerXpcom {
    fn drop(&mut self) {
        if let Some(inner) = self.inner.lock().as_ref() {
            self.transport.unregister_session_object(inner.clone());
        }

        // Remove ourselves as a connection observer.
        self.transport
            .remove_connection_observer(self.connection_observer_id);
    }
}

#[no_mangle]
pub unsafe extern "C" fn settings_manager_construct(result: &mut *const nsISettingsManager) {
    let inst = SettingsManagerXpcom::new();
    *result = inst.coerce::<nsISettingsManager>();
    std::mem::forget(inst);
}
