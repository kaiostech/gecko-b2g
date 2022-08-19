/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Implementation of the nsIContacts xpcom interface
use crate::common::core::BaseMessage;
use crate::common::sidl_task::*;
use crate::common::traits::{Shared, TrackerId};
use crate::common::uds_transport::*;
use crate::services::core::service::*;

use crate::services::contacts::messages::*;
use log::{debug, error};
use moz_task::ThreadPtrHolder;
use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::*;
use parking_lot::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        nsIBlockedNumberFindOptions, nsIContactsManager, nsIFindBlockedNumbersResponse,
        nsIMatchesResponse, nsISidlConnectionObserver, nsISidlEventListener,
    },
    RefPtr,
};

type FindBlockedNumbersSuccessType = Option<Vec<String>>;

macro_rules! sidl_callback_for_resolve_string_array {
    ($interface:ty, $success:ty) => {
        impl SidlCallback for $interface {
            type Success = $success;
            type Error = ();
            fn resolve(&self, value: &Self::Success) {
                let mut list: thin_vec::ThinVec<nsString> = ThinVec::new();
                if let Some(array) = value {
                    for item in array {
                        list.push(nsString::from(item));
                    }
                }

                unsafe {
                    self.Resolve(&list);
                }
            }

            fn reject(&self, _value: &Self::Error) {
                unsafe {
                    self.Reject();
                }
            }
        }
    };
}

macro_rules! deref_bool {
    ($obj:ident) => {
        *$obj
    };
}

sidl_callback_for!(nsIMatchesResponse, bool, deref_bool);

sidl_callback_for_resolve_string_array!(
    nsIFindBlockedNumbersResponse,
    FindBlockedNumbersSuccessType
);

type MatchesTask = (
    SidlCallTask<bool, (), nsIMatchesResponse>,
    FilterByOption,
    FilterOption,
    String,
);

type FindBlockedNumbersTask = (
    SidlCallTask<FindBlockedNumbersSuccessType, (), nsIFindBlockedNumbersResponse>,
    BlockedNumberFindOptions,
);

task_receiver_success!(
    MatchesTaskReceiver,
    nsIMatchesResponse,
    ContactsManagerToClient,
    ContactsFactoryMatchesSuccess,
    ContactsFactoryMatchesError,
    bool
);

task_receiver_success!(
    FindBlockedNumbersTaskReceiver,
    nsIFindBlockedNumbersResponse,
    ContactsManagerToClient,
    ContactsFactoryFindBlockedNumbersSuccess,
    ContactsFactoryFindBlockedNumbersError,
    FindBlockedNumbersSuccessType
);

// The tasks that can be dispatched to the calling thread.
enum ContactsTask {
    Matches(MatchesTask),
    FindBlockedNumbers(FindBlockedNumbersTask),
}

struct ContactsManagerImpl {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The service ID of this instance.
    service_id: TrackerId,
    // The core service we use to manage event listeners.
    core_service: Arc<Mutex<CoreService>>,
    // Helper struct to send tasks.
    sender: TaskSender,
}

impl SessionObject for ContactsManagerImpl {
    fn on_request(&mut self, _request: BaseMessage, _id: u64) -> Option<BaseMessage> {
        debug!("ContactsManagerImpl::on_request");
        None
    }

    fn on_event(&mut self, _event_data: Vec<u8>) {
        debug!("ContactsManagerImpl::on_event");
    }

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, 0)
    }
}

impl ServiceClientImpl<ContactsTask> for ContactsManagerImpl {
    fn new(mut transport: UdsTransport, service_id: TrackerId) -> Self {
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));
        transport.register_session_object(core_service.clone());
        let sender = TaskSender::new(transport.clone(), service_id);

        Self {
            transport,
            service_id,
            core_service,
            sender,
        }
    }

    fn run_task(&mut self, task: ContactsTask) -> Result<(), nsresult> {
        match task {
            ContactsTask::Matches(task) => self.matches(task),
            ContactsTask::FindBlockedNumbers(task) => self.find_blocked_numbers(task),
        }
    }

    fn dispatch_queue(
        &mut self,
        task_queue: &Shared<Vec<ContactsTask>>,
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

impl ContactsManagerImpl {
    fn get_tasks_to_reconnect(&mut self) -> Vec<ContactsTask> {
        vec![]
    }
    // Empty implementation needed by the nsISidlConnectionObserver implementation
    fn get_event_listeners_to_reconnect(
        &mut self,
    ) -> Vec<(i32, RefPtr<ThreadPtrHolder<nsISidlEventListener>>, usize)> {
        vec![]
    }

    fn matches(&mut self, task: MatchesTask) -> Result<(), nsresult> {
        debug!("ContactsManager::matches");

        let (task, filter_by_option, filter_option, value) = task;
        let request = ContactsManagerFromClient::ContactsFactoryMatches(
            filter_by_option,
            filter_option,
            value,
        );
        self.sender
            .send_task(&request, MatchesTaskReceiver { task });
        Ok(())
    }

    fn find_blocked_numbers(&mut self, task: FindBlockedNumbersTask) -> Result<(), nsresult> {
        debug!("ContactsManager::find_blocked_numbers");

        let (task, option) = task;
        let request = ContactsManagerFromClient::ContactsFactoryFindBlockedNumbers(option);
        self.sender
            .send_task(&request, FindBlockedNumbersTaskReceiver { task });
        Ok(())
    }
}

impl Drop for ContactsManagerImpl {
    fn drop(&mut self) {
        self.transport
            .unregister_session_object(self.core_service.clone());
    }
}

#[xpcom(implement(nsIContactsManager, nsISidlConnectionObserver), atomic)]
struct ContactsManagerXpcom {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The core service we use to get an instance of the ContactsManager service.
    core_service: Arc<Mutex<CoreService>>,
    // The implementation registered as a session object with the transport.
    inner: Shared<Option<Arc<Mutex<ContactsManagerImpl>>>>,
    // The list of pending tasks queued while we are waiting on the service id
    // to be available.
    pending_tasks: Shared<Vec<ContactsTask>>,
    // The list of pending event listeners to add.
    pending_listeners: PendingListeners,
    // Flag to know if are already fetching the service id.
    getting_service: AtomicBool,
    // Id of ourselves as a connection observer.
    connection_observer_id: usize,
}

impl ContactsManagerXpcom {
    fn new() -> RefPtr<Self> {
        debug!("ContactsManagerXpcom::new");
        let mut transport = UdsTransport::open();
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));

        let instance = Self::allocate(InitContactsManagerXpcom {
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
    implement_connection_observer!("ContactsManagerXpcom");

    xpcom_method!(matches => Matches(filter_by_option: u16, filter_option: u16, value: *const nsAString, callback: *const nsIMatchesResponse));
    fn matches(
        &self,
        filter_by_option: u16,
        filter_option: u16,
        value: &nsAString,
        callback: &nsIMatchesResponse,
    ) -> Result<(), nsresult> {
        let filter_by = match filter_by_option {
            0 => FilterByOption::Name,
            1 => FilterByOption::GivenName,
            2 => FilterByOption::FamilyName,
            3 => FilterByOption::Tel,
            4 => FilterByOption::Email,
            5 => FilterByOption::Category,
            _ => return Err(NS_ERROR_INVALID_ARG),
        };

        let filter = match filter_option {
            0 => FilterOption::Equals,
            1 => FilterOption::Contains,
            2 => FilterOption::Match,
            3 => FilterOption::StartsWith,
            4 => FilterOption::FuzzyMatch,
            _ => return Err(NS_ERROR_INVALID_ARG),
        };

        let callback = ThreadPtrHolder::new(cstr!("nsIMatchesResponse"), RefPtr::new(callback))?;
        let task = (
            SidlCallTask::new(callback),
            filter_by,
            filter,
            value.to_string(),
        );

        self.run_or_queue_task(Some(ContactsTask::Matches(task)));
        Ok(())
    }

    xpcom_method!(find_blocked_numbers => FindBlockedNumbers(option: *const nsIBlockedNumberFindOptions, callback: *const nsIFindBlockedNumbersResponse));
    fn find_blocked_numbers(
        &self,
        options: &nsIBlockedNumberFindOptions,
        callback: &nsIFindBlockedNumbersResponse,
    ) -> Result<(), nsresult> {
        let mut value = nsString::new();
        let mut option: u16 = 0;
        unsafe {
            if options.GetFilterValue(&mut *value) != NS_OK {
                return Err(NS_ERROR_INVALID_ARG);
            }
            if options.GetFilterOption(&mut option) != NS_OK {
                return Err(NS_ERROR_INVALID_ARG);
            };
        }

        let find_options = BlockedNumberFindOptions {
            filter_value: value.to_string(),
            filter_option: match option {
                0 => FilterOption::Equals,
                1 => FilterOption::Contains,
                2 => FilterOption::Match,
                3 => FilterOption::StartsWith,
                4 => FilterOption::FuzzyMatch,
                _ => FilterOption::Equals,
            },
        };

        let callback = ThreadPtrHolder::new(
            cstr!("nsIFindBlockedNumbersResponse"),
            RefPtr::new(callback),
        )?;
        let task = (SidlCallTask::new(callback), (find_options));

        self.run_or_queue_task(Some(ContactsTask::FindBlockedNumbers(task)));
        Ok(())
    }

    task_runner!(ContactsTask, "ContactsManager", SERVICE_FINGERPRINT);
}

impl Drop for ContactsManagerXpcom {
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
pub unsafe extern "C" fn contacts_manager_construct(result: &mut *const nsIContactsManager) {
    let inst = ContactsManagerXpcom::new();
    *result = inst.coerce::<nsIContactsManager>();
    std::mem::forget(inst);
}
