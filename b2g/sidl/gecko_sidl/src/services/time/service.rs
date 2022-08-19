/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of the nsITime xpcom interface
//
use crate::common::client_object::*;
use crate::common::core::BaseMessage;
use crate::common::default_response::*;
use crate::common::sidl_task::*;
use crate::common::traits::{Shared, TrackerId};
use crate::common::uds_transport::*;
use crate::common::SystemTime;
use crate::services::core::service::*;
use crate::services::time::messages::*;
use crate::services::time::observer::*;
use log::{debug, error};
use moz_task::{ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::*;
use parking_lot::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use xpcom::{
    interfaces::{
        nsISidlConnectionObserver, nsISidlDefaultResponse, nsISidlEventListener, nsITime,
        nsITimeGetElapsedRealTime, nsITimeObserver,
    },
    RefPtr,
};

#[macro_export]
macro_rules! i64_to_u64 {
    ($obj:ident) => {
        *($obj) as u64
    };
}

sidl_callback_for!(nsITimeGetElapsedRealTime, i64, i64_to_u64);

type AddObserverTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    (i16, ThreadPtrHandle<nsITimeObserver>, usize), /* (reason, observer, key) */
);

type RemoveObserverTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    (i16, usize), /* (reason, key) */
);

type SetTimezoneTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    String, /* timezone */
);

type SetTimeTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    SystemTime, /* time */
);

type GetElapsedRealTimeTask = SidlCallTask<i64, (), nsITimeGetElapsedRealTime>;

// The tasks that can be dispatched to the calling thread.
enum TimeTask {
    AddObserver(AddObserverTask),
    RemoveObserver(RemoveObserverTask),
    SetTimezone(SetTimezoneTask),
    SetTime(SetTimeTask),
    GetElapsedRealTime(GetElapsedRealTimeTask),
}

struct TimeImpl {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The service ID of this instance.
    service_id: TrackerId,
    // The core service we use to manage event listeners.
    core_service: Arc<Mutex<CoreService>>,
    // Helper struct to send tasks.
    sender: TaskSender,
    // The registered observers. (object_id, key, object, CallbackReason)
    observers: Vec<(
        TrackerId,
        usize,
        ClientObject<ObserverWrapper>,
        CallbackReason,
    )>,
    // The next usable object_id when we create an observer.
    current_object_id: TrackerId,
}

impl SessionObject for TimeImpl {
    fn on_request(&mut self, _request: BaseMessage, _id: u64) -> Option<BaseMessage> {
        debug!("SessionObject::TimeImpl::on_request");
        None
    }

    fn on_event(&mut self, _event_data: Vec<u8>) {
        debug!("SessionObject::TimeImpl::on_event");
    }

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, 0)
    }
}

impl ServiceClientImpl<TimeTask> for TimeImpl {
    fn new(mut transport: UdsTransport, service_id: TrackerId) -> Self {
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));
        transport.register_session_object(core_service.clone());
        let sender = TaskSender::new(transport.clone(), service_id);

        Self {
            transport,
            service_id,
            core_service,
            sender,
            observers: vec![],
            // current_object_id starts at 1 since 0 is reserved for the service object itself.
            current_object_id: 1,
        }
    }

    fn run_task(&mut self, task: TimeTask) -> Result<(), nsresult> {
        match task {
            TimeTask::SetTimezone(task) => self.set_timezone(task),
            TimeTask::SetTime(task) => self.set_time(task),
            TimeTask::AddObserver(task) => self.add_observer(task),
            TimeTask::RemoveObserver(task) => self.remove_observer(task),
            TimeTask::GetElapsedRealTime(task) => self.get_elapse_realtime(task),
        }
    }

    fn dispatch_queue(
        &mut self,
        task_queue: &Shared<Vec<TimeTask>>,
        _pending_listeners: &PendingListeners,
    ) {
        let mut task_queue = task_queue.lock();
        debug!("Running the {} pending tasks", task_queue.len());

        // drain the queue.
        for task in task_queue.drain(..) {
            let _ = self.run_task(task);
        }
    }
}

impl CallbackReason {
    pub fn from_i16(value: i16) -> CallbackReason {
        match value {
            1 => CallbackReason::TimeChanged,
            2 => CallbackReason::TimezoneChanged,
            _ => CallbackReason::None,
        }
    }
}

impl TimeImpl {
    // Empty implementation needed by the nsISidlConnectionObserver implementation
    fn get_event_listeners_to_reconnect(
        &mut self,
    ) -> Vec<(i32, RefPtr<ThreadPtrHolder<nsISidlEventListener>>, usize)> {
        vec![]
    }

    fn get_tasks_to_reconnect(&mut self) -> Vec<TimeTask> {
        let mut tasks = vec![];

        // Re-create the tasks to add the observers.
        for (_object_id, key, client_object, reason) in &mut self.observers {
            client_object.dont_release();

            if let Some(delegate) = client_object.maybe_xpcom::<nsITimeObserver>() {
                let task = (
                    SidlCallTask::new(SidlDefaultResponseXpcom::as_callback()),
                    (*reason as i16, delegate, *key),
                );
                tasks.push(TimeTask::AddObserver(task));
            }
        }
        self.observers.clear();

        tasks
    }

    fn add_observer(&mut self, task: AddObserverTask) -> Result<(), nsresult> {
        debug!("TimeImpl::add_observer");
        let object_id = self.next_object_id();

        let (task, (r, observer, key)) = task;
        let reason = CallbackReason::from_i16(r);
        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper = ObserverWrapper::new(observer, self.service_id, object_id);
        let proxy = ClientObject::new::<ObserverWrapper>(wrapper, &mut self.transport);

        self.observers.push((object_id, key, proxy, reason));

        let request = TimeServiceFromClient::TimeAddObserver(reason, object_id.into());
        self.sender
            .send_task(&request, AddObserverTaskReceiver { task });

        Ok(())
    }

    fn remove_observer(&mut self, task: RemoveObserverTask) -> Result<(), nsresult> {
        debug!("TimeImpl::remove_observer");
        let (task, (r, key)) = task;
        let object_id = self.get_observer_id(key);
        let reason: CallbackReason = CallbackReason::from_i16(r);

        if let Some(pos) = self
            .observers
            .iter()
            .position(|item| item.1 == key && item.3 == reason)
        {
            self.observers.remove(pos);
        }

        if let Some(object_id) = object_id {
            let request = TimeServiceFromClient::TimeRemoveObserver(reason, object_id.into());
            self.sender
                .send_task(&request, RemoveObserverTaskReceiver { task });
        }

        Ok(())
    }

    fn set_timezone(&mut self, task: SetTimezoneTask) -> Result<(), nsresult> {
        debug!("TimeImpl::set_timezone");

        let (task, timezone) = task;
        let request = TimeServiceFromClient::TimeSetTimezone(timezone);
        self.sender
            .send_task(&request, SetTimezoneTaskReceiver { task });
        Ok(())
    }

    fn set_time(&mut self, task: SetTimeTask) -> Result<(), nsresult> {
        debug!("TimeImpl::set_time");

        let (task, time) = task;
        let request = TimeServiceFromClient::TimeSet(time);
        self.sender
            .send_task(&request, SetTimeTaskReceiver { task });
        Ok(())
    }

    fn get_elapse_realtime(&mut self, task: GetElapsedRealTimeTask) -> Result<(), nsresult> {
        debug!("TimeImpl::get_elapse_realtime");

        let request = TimeServiceFromClient::TimeGetElapsedRealTime;
        self.sender
            .send_task(&request, GetElapsedRealTimeTaskReceiver { task });
        Ok(())
    }

    // Returns the object id for a given observer.
    fn get_observer_id(&self, key: usize) -> Option<TrackerId> {
        debug!("TimeImpl::get_observer_id for {}", key);
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
}

impl Drop for TimeImpl {
    fn drop(&mut self) {
        debug!("Drop TimeImpl");
        self.transport
            .unregister_session_object(self.core_service.clone());
    }
}

task_receiver!(
    SetTimezoneTaskReceiver,
    nsISidlDefaultResponse,
    TimeServiceToClient,
    TimeSetTimezoneSuccess,
    TimeSetTimezoneError
);

task_receiver!(
    SetTimeTaskReceiver,
    nsISidlDefaultResponse,
    TimeServiceToClient,
    TimeSetSuccess,
    TimeSetError
);

task_receiver!(
    AddObserverTaskReceiver,
    nsISidlDefaultResponse,
    TimeServiceToClient,
    TimeAddObserverSuccess,
    TimeAddObserverError
);

task_receiver!(
    RemoveObserverTaskReceiver,
    nsISidlDefaultResponse,
    TimeServiceToClient,
    TimeRemoveObserverSuccess,
    TimeRemoveObserverError
);

task_receiver_success!(
    GetElapsedRealTimeTaskReceiver,
    nsITimeGetElapsedRealTime,
    TimeServiceToClient,
    TimeGetElapsedRealTimeSuccess,
    TimeGetElapsedRealTimeError,
    i64
);

#[xpcom(implement(nsITime, nsISidlConnectionObserver), atomic)]
struct TimeXpcom {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The core service we use to get an instance of the Time service.
    core_service: Arc<Mutex<CoreService>>,
    // The implementation registered as a session object with the transport.
    inner: Shared<Option<Arc<Mutex<TimeImpl>>>>,
    // The list of pending tasks queued while we are waiting on the service id
    // to be available.
    pending_tasks: Shared<Vec<TimeTask>>,
    // The list of pending event listeners to add.
    pending_listeners: PendingListeners,
    // Flag to know if are already fetching the service id.
    getting_service: AtomicBool,
    // Id of ourselves as a connection observer.
    connection_observer_id: usize,
}

impl TimeXpcom {
    fn new() -> RefPtr<Self> {
        debug!("TimeXpcom::new");
        let mut transport = UdsTransport::open();
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));

        let instance = Self::allocate(InitTimeXpcom {
            transport: transport.clone(),
            core_service,
            pending_tasks: Shared::adopt(vec![]),
            pending_listeners: Shared::adopt(vec![]),
            inner: Shared::adopt(None),
            getting_service: AtomicBool::new(false),
            connection_observer_id: transport.next_connection_observer_id(),
        });

        let obs = instance.coerce::<nsISidlConnectionObserver>();
        match ThreadPtrHolder::new(
            cstr!("TimeXpcom::nsISidlConnectionObserver"),
            RefPtr::new(obs),
        ) {
            Ok(obs) => {
                transport.add_connection_observer(obs);
            }
            Err(err) => error!("Failed to create connection observer: {}", err),
        }

        instance
    }

    // nsISidlConnectionObserver implementation.
    implement_connection_observer!("TimeXpcom");

    // nsITime implementation.

    xpcom_method!(add_observer => AddObserver(reason: i16, observer: *const nsITimeObserver, callback: *const nsISidlDefaultResponse));
    fn add_observer(
        &self,
        reason: i16,
        observer: &nsITimeObserver,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("TimeImpl::add_observer reason {}", reason);

        let key: usize = unsafe { std::mem::transmute(observer) };
        let observer = ThreadPtrHolder::new(cstr!("nsITimeObserver"), RefPtr::new(observer))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), (reason, observer, key));

        self.run_or_queue_task(Some(TimeTask::AddObserver(task)));
        Ok(())
    }

    xpcom_method!(remove_observer => RemoveObserver(reason: i16, observer: *const nsITimeObserver, callback: *const nsISidlDefaultResponse));
    fn remove_observer(
        &self,
        reason: i16,
        observer: &nsITimeObserver,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("TimeImpl::remove_observer reason {}", reason);

        let key: usize = unsafe { std::mem::transmute(observer) };

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), (reason, key));

        self.run_or_queue_task(Some(TimeTask::RemoveObserver(task)));
        Ok(())
    }

    xpcom_method!(set_timezone => SetTimezone(timezone: *const nsAString, callback: *const nsISidlDefaultResponse));
    fn set_timezone(
        &self,
        timezone: &nsAString,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("Time::set_timezone {}", timezone);

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), timezone.to_string());

        self.run_or_queue_task(Some(TimeTask::SetTimezone(task)));
        Ok(())
    }

    xpcom_method!(set_time => SetTime(time: u64, callback: *const nsISidlDefaultResponse));
    fn set_time(&self, time: u64, callback: &nsISidlDefaultResponse) -> Result<(), nsresult> {
        debug!("Time::set_time {}", time);

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let t = std::time::UNIX_EPOCH;
        let duration = t
            .checked_add(std::time::Duration::from_millis(time))
            .ok_or::<Result<(), nsresult>>(Err(NS_ERROR_INVALID_ARG))?;

        let task = (SidlCallTask::new(callback), SystemTime(duration));

        self.run_or_queue_task(Some(TimeTask::SetTime(task)));
        Ok(())
    }

    xpcom_method!(get_elapse_realtime => GetElapsedRealTime(callback: *const nsITimeGetElapsedRealTime));
    fn get_elapse_realtime(&self, callback: &nsITimeGetElapsedRealTime) -> Result<(), nsresult> {
        debug!("Time::get_elapsetime");

        let callback =
            ThreadPtrHolder::new(cstr!("nsITimeGetElapsedRealTime"), RefPtr::new(callback))?;
        let task = SidlCallTask::new(callback);

        self.run_or_queue_task(Some(TimeTask::GetElapsedRealTime(task)));
        Ok(())
    }

    task_runner!(TimeTask, "TimeService", SERVICE_FINGERPRINT);
}

#[no_mangle]
pub unsafe extern "C" fn time_construct(result: &mut *const nsITime) {
    let inst = TimeXpcom::new();
    *result = inst.coerce::<nsITime>();
    std::mem::forget(inst);
}
