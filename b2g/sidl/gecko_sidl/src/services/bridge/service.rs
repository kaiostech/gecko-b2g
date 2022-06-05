/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Client for the Gecko Bridge service.

use super::apps_service_delegate::*;
use super::messages::*;
use super::mobile_manager_delegate::*;
use super::network_manager_delegate::*;
use super::power_manager_delegate::*;
use super::preference_delegate::*;
use crate::common::client_object::*;
use crate::common::core::BaseMessage;
use crate::common::default_response::*;
use crate::common::sidl_task::*;
use crate::common::traits::{Shared, TrackerId};
use crate::common::uds_transport::*;
use crate::services::core::service::*;
use log::{debug, error};
use moz_task::{ThreadPtrHandle, ThreadPtrHolder};
use nserror::{nsresult, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::*;
use parking_lot::Mutex;
use std::collections::HashMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;
use thin_vec::ThinVec;
use xpcom::{
    interfaces::{
        nsIAppsServiceDelegate, nsIGeckoBridge, nsIMobileManagerDelegate,
        nsINetworkManagerDelegate, nsIPowerManagerDelegate, nsIPreferenceDelegate,
        nsISidlConnectionObserver, nsISidlDefaultResponse, nsISidlEventListener, nsISimContactInfo,
    },
    RefPtr,
};

#[derive(Default)]
pub struct ObjectIdGenerator {
    current_id: TrackerId,
}

impl ObjectIdGenerator {
    pub fn next_id(&mut self) -> TrackerId {
        self.current_id += 1;
        self.current_id
    }
}

type SetAppsServiceDelegateTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    ThreadPtrHandle<nsIAppsServiceDelegate>,
);

type SetMobileManagerDelegateTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    ThreadPtrHandle<nsIMobileManagerDelegate>,
);

type SetPowerManagerDelegateTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    ThreadPtrHandle<nsIPowerManagerDelegate>,
);

type SetPreferenceDelegateTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    ThreadPtrHandle<nsIPreferenceDelegate>,
);

type SetNetworkManagerDelegateTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    ThreadPtrHandle<nsINetworkManagerDelegate>,
);

type OnCharPrefChangedTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    (String, String),
);

type OnIntPrefChangedTask = (SidlCallTask<(), (), nsISidlDefaultResponse>, (String, i32));

type OnBoolPrefChangedTask = (SidlCallTask<(), (), nsISidlDefaultResponse>, (String, bool));

type OnRegisterTokenTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    (String, String, Vec<String>),
);

type ImportSimContactsTask = (
    SidlCallTask<(), (), nsISidlDefaultResponse>,
    Vec<SimContactInfo>, /* contacts */
);

// The tasks that can be dispatched to the calling thread.
enum GeckoBridgeTask {
    SetAppsServiceDelegate(SetAppsServiceDelegateTask),
    SetMobileManagerDelegate(SetMobileManagerDelegateTask),
    SetNetworkManagerDelegate(SetNetworkManagerDelegateTask),
    SetPowerManagerDelegate(SetPowerManagerDelegateTask),
    SetPreferenceDelegate(SetPreferenceDelegateTask),
    CharPrefChanged(OnCharPrefChangedTask),
    IntPrefChanged(OnIntPrefChangedTask),
    BoolPrefChanged(OnBoolPrefChangedTask),
    RegisterToken(OnRegisterTokenTask),
    ImportSimContacts(ImportSimContactsTask),
}

struct GeckoBridgeImpl {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The service ID of this instance.
    service_id: TrackerId,
    // Helper struct to send tasks.
    sender: TaskSender,
    // A factory to yield unique object ids for this service.
    object_id_generator: Shared<ObjectIdGenerator>,
    // The apps_service delegate.
    apps_service_delegate: Option<ClientObject<AppsServiceDelegate>>,
    // The power manager delegate.
    power_manager_delegate: Option<ClientObject<PowerManagerDelegate>>,
    // The preference delegate.
    preference_delegate: Option<ClientObject<PreferenceDelegate>>,
    // The mobile manager delegate.
    mobile_manager_delegate: Option<ClientObject<MobileManagerDelegate>>,
    // The network manager delegate.
    network_manager_delegate: Option<ClientObject<NetworkManagerDelegate>>,
}

impl SessionObject for GeckoBridgeImpl {
    fn on_request(&mut self, _request: BaseMessage, _id: u64) -> Option<BaseMessage> {
        debug!("GeckoBridgeImpl::on_request");
        None
    }

    fn on_event(&mut self, _event_data: Vec<u8>) {
        debug!("GeckoBridgeImpl::on_event");
    }

    fn get_ids(&self) -> (u32, u32) {
        (self.service_id, 0)
    }
}

impl ServiceClientImpl<GeckoBridgeTask> for GeckoBridgeImpl {
    fn new(transport: UdsTransport, service_id: TrackerId) -> Self {
        let sender = TaskSender::new(transport.clone(), service_id);

        Self {
            transport,
            service_id,
            sender,
            object_id_generator: Shared::adopt(ObjectIdGenerator { current_id: 1 }),
            power_manager_delegate: None,
            apps_service_delegate: None,
            preference_delegate: None,
            mobile_manager_delegate: None,
            network_manager_delegate: None,
        }
    }

    fn run_task(&mut self, task: GeckoBridgeTask) -> Result<(), nsresult> {
        match task {
            GeckoBridgeTask::SetAppsServiceDelegate(task) => self.set_apps_service_delegate(task),
            GeckoBridgeTask::SetMobileManagerDelegate(task) => {
                self.set_mobile_manager_delegate(task)
            }
            GeckoBridgeTask::SetNetworkManagerDelegate(task) => {
                self.set_network_manager_delegate(task)
            }
            GeckoBridgeTask::SetPowerManagerDelegate(task) => self.set_power_manager_delegate(task),
            GeckoBridgeTask::SetPreferenceDelegate(task) => self.set_preference_delegate(task),
            GeckoBridgeTask::CharPrefChanged(task) => self.char_pref_changed(task),
            GeckoBridgeTask::IntPrefChanged(task) => self.int_pref_changed(task),
            GeckoBridgeTask::BoolPrefChanged(task) => self.bool_pref_changed(task),
            GeckoBridgeTask::RegisterToken(task) => self.register_token(task),
            GeckoBridgeTask::ImportSimContacts(task) => self.import_sim_contacts(task),
        }
    }

    fn dispatch_queue(
        &mut self,
        task_queue: &Shared<Vec<GeckoBridgeTask>>,
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

impl GeckoBridgeImpl {
    fn set_apps_service_delegate(
        &mut self,
        task: SetAppsServiceDelegateTask,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridge::set_apps_service_delegate");
        let object_id = self.next_object_id();
        let (task, delegate) = task;

        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper =
            AppsServiceDelegate::new(delegate, self.service_id, object_id, &self.transport);
        self.apps_service_delegate = Some(ClientObject::new::<AppsServiceDelegate>(
            wrapper,
            &mut self.transport,
        ));

        let request = GeckoBridgeFromClient::GeckoFeaturesSetAppsServiceDelegate(object_id.into());
        self.sender
            .send_task(&request, SetAppsServiceDelegateTaskReceiver { task });
        Ok(())
    }

    fn set_mobile_manager_delegate(
        &mut self,
        task: SetMobileManagerDelegateTask,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridge::set_mobile_manager_delegate");
        let object_id = self.next_object_id();

        let (task, delegate) = task;

        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper =
            MobileManagerDelegate::new(delegate, self.service_id, object_id, &self.transport);
        self.mobile_manager_delegate = Some(ClientObject::new::<MobileManagerDelegate>(
            wrapper,
            &mut self.transport,
        ));

        let request =
            GeckoBridgeFromClient::GeckoFeaturesSetMobileManagerDelegate(object_id.into());
        self.sender
            .send_task(&request, SetMobileManagerDelegateTaskReceiver { task });

        Ok(())
    }

    fn get_tasks_to_reconnect(&mut self) -> Vec<GeckoBridgeTask> {
        debug!("GeckoBridge::get_tasks_to_reconnect");
        let mut tasks = vec![];

        // Helper macro to generate tasks and reset delegates.
        macro_rules! delegate_task {
            ($delegate:tt, $interface:ty, $enum:tt) => {
                // If we have a delegate, recreate the task to add it.
                // For that we need to get the origin xpcom delegate handle, and create
                // a fake callback since there is no point calling the original one again.
                if let Some(client_object) = &mut self.$delegate {
                    // Since the session was dead, we can't send a release request anymore.
                    client_object.dont_release();
                    if let Some(delegate) = client_object.maybe_xpcom::<$interface>() {
                        let task = (
                            SidlCallTask::new(SidlDefaultResponseXpcom::as_callback()),
                            delegate,
                        );
                        tasks.push(GeckoBridgeTask::$enum(task));
                    } else {
                        // This should never happen as all delegates are backed by an xpcom component.
                        panic!("No xpcom for {} delegate!", stringify!($delegate));
                    }
                }
                // Make sure we drop any existing power manager delegate.
                self.$delegate = None;
            };
        }

        delegate_task!(
            apps_service_delegate,
            nsIAppsServiceDelegate,
            SetAppsServiceDelegate
        );
        delegate_task!(
            preference_delegate,
            nsIPreferenceDelegate,
            SetPreferenceDelegate
        );
        delegate_task!(
            mobile_manager_delegate,
            nsIMobileManagerDelegate,
            SetMobileManagerDelegate
        );
        delegate_task!(
            network_manager_delegate,
            nsINetworkManagerDelegate,
            SetNetworkManagerDelegate
        );
        delegate_task!(
            power_manager_delegate,
            nsIPowerManagerDelegate,
            SetPowerManagerDelegate
        );

        tasks
    }

    // Empty implementation needed by the nsISidlConnectionObserver implementation
    fn get_event_listeners_to_reconnect(
        &mut self,
    ) -> Vec<(i32, RefPtr<ThreadPtrHolder<nsISidlEventListener>>, usize)> {
        vec![]
    }

    fn set_network_manager_delegate(
        &mut self,
        task: SetNetworkManagerDelegateTask,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridge::set_network_manager_delegate");
        let object_id = self.next_object_id();

        let (task, delegate) = task;

        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper =
            NetworkManagerDelegate::new(delegate, self.service_id, object_id, &self.transport);
        self.network_manager_delegate = Some(ClientObject::new::<NetworkManagerDelegate>(
            wrapper,
            &mut self.transport,
        ));

        let request =
            GeckoBridgeFromClient::GeckoFeaturesSetNetworkManagerDelegate(object_id.into());
        self.sender
            .send_task(&request, SetNetworkManagerDelegateTaskReceiver { task });

        Ok(())
    }

    fn set_power_manager_delegate(
        &mut self,
        task: SetPowerManagerDelegateTask,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridge::set_power_manager_delegate");
        let object_id = self.next_object_id();
        let (task, delegate) = task;

        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper = PowerManagerDelegate::new(
            delegate,
            self.service_id,
            object_id,
            self.object_id_generator.clone(),
            Shared::adopt(HashMap::default()),
            &self.transport,
        );
        self.power_manager_delegate = Some(ClientObject::new::<PowerManagerDelegate>(
            wrapper,
            &mut self.transport,
        ));

        let request = GeckoBridgeFromClient::GeckoFeaturesSetPowerManagerDelegate(object_id.into());
        self.sender
            .send_task(&request, SetPowerManagerDelegateTaskReceiver { task });

        Ok(())
    }

    fn set_preference_delegate(&mut self, task: SetPreferenceDelegateTask) -> Result<(), nsresult> {
        debug!("GeckoBridge::set_preference_delegate");
        let object_id = self.next_object_id();

        let (task, delegate) = task;
        // Create a lightweight xpcom wrapper + session proxy that manages object release for us.
        let wrapper =
            PreferenceDelegate::new(delegate, self.service_id, object_id, &self.transport);
        self.preference_delegate = Some(ClientObject::new::<PreferenceDelegate>(
            wrapper,
            &mut self.transport,
        ));

        let request = GeckoBridgeFromClient::GeckoFeaturesSetPreferenceDelegate(object_id.into());
        self.sender
            .send_task(&request, SetPreferenceDelegateTaskReceiver { task });

        Ok(())
    }

    fn char_pref_changed(&mut self, task: OnCharPrefChangedTask) -> Result<(), nsresult> {
        debug!("GeckoBridge::char_pref_changed");

        let (task, (name, value)) = task;
        let request = GeckoBridgeFromClient::GeckoFeaturesCharPrefChanged(name, value);
        self.sender
            .send_task(&request, OnCharPrefChangedTaskReceiver { task });
        Ok(())
    }

    fn int_pref_changed(&mut self, task: OnIntPrefChangedTask) -> Result<(), nsresult> {
        debug!("GeckoBridge::int_pref_changed");

        let (task, (name, value)) = task;
        let request = GeckoBridgeFromClient::GeckoFeaturesIntPrefChanged(name, value.into());
        self.sender
            .send_task(&request, OnIntPrefChangedTaskReceiver { task });
        Ok(())
    }

    fn bool_pref_changed(&mut self, task: OnBoolPrefChangedTask) -> Result<(), nsresult> {
        debug!("GeckoBridge::bool_pref_changed");

        let (task, (name, value)) = task;
        let request = GeckoBridgeFromClient::GeckoFeaturesBoolPrefChanged(name, value);
        self.sender
            .send_task(&request, OnBoolPrefChangedTaskReceiver { task });
        Ok(())
    }

    fn register_token(&mut self, task: OnRegisterTokenTask) -> Result<(), nsresult> {
        debug!("GeckoBridge::register_token");

        let (task, (token, url, permissions)) = task;
        let request =
            GeckoBridgeFromClient::GeckoFeaturesRegisterToken(token, url, Some(permissions));
        self.sender
            .send_task(&request, OnRegisterTokenTaskReceiver { task });
        Ok(())
    }

    fn import_sim_contacts(&mut self, task: ImportSimContactsTask) -> Result<(), nsresult> {
        debug!("GeckoBridge::import_sim_contacts");

        let (task, contacts) = task;
        let request = GeckoBridgeFromClient::GeckoFeaturesImportSimContacts(Some(contacts));
        self.sender
            .send_task(&request, ImportSimContactsTaskReceiver { task });
        Ok(())
    }

    fn next_object_id(&mut self) -> TrackerId {
        let res = self.object_id_generator.lock().next_id();
        res
    }
}

task_receiver!(
    SetAppsServiceDelegateTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesSetAppsServiceDelegateSuccess,
    GeckoFeaturesSetAppsServiceDelegateError
);

task_receiver!(
    SetMobileManagerDelegateTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesSetMobileManagerDelegateSuccess,
    GeckoFeaturesSetMobileManagerDelegateError
);

task_receiver!(
    SetNetworkManagerDelegateTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesSetNetworkManagerDelegateSuccess,
    GeckoFeaturesSetNetworkManagerDelegateError
);

task_receiver!(
    SetPowerManagerDelegateTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesSetPowerManagerDelegateSuccess,
    GeckoFeaturesSetPowerManagerDelegateError
);

task_receiver!(
    SetPreferenceDelegateTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesSetPreferenceDelegateSuccess,
    GeckoFeaturesSetPreferenceDelegateError
);

task_receiver!(
    OnCharPrefChangedTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesCharPrefChangedSuccess,
    GeckoFeaturesCharPrefChangedError
);

task_receiver!(
    OnIntPrefChangedTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesIntPrefChangedSuccess,
    GeckoFeaturesIntPrefChangedError
);

task_receiver!(
    OnBoolPrefChangedTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesBoolPrefChangedSuccess,
    GeckoFeaturesBoolPrefChangedError
);

task_receiver!(
    OnRegisterTokenTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesRegisterTokenSuccess,
    GeckoFeaturesRegisterTokenError
);

task_receiver!(
    ImportSimContactsTaskReceiver,
    nsISidlDefaultResponse,
    GeckoBridgeToClient,
    GeckoFeaturesImportSimContactsSuccess,
    GeckoFeaturesImportSimContactsError
);

#[derive(xpcom)]
#[xpimplements(nsIGeckoBridge)]
#[xpimplements(nsISidlConnectionObserver)]
#[refcnt = "atomic"]
struct InitGeckoBridgeXpcom {
    // The underlying UDS transport we are connected to.
    transport: UdsTransport,
    // The core service we use to get an instance of the GeckoBridge service.
    core_service: Arc<Mutex<CoreService>>,
    // The implementation registered as a session object with the transport.
    inner: Shared<Option<Arc<Mutex<GeckoBridgeImpl>>>>,
    // The list of pending tasks queued while we are waiting on the service id
    // to be available.
    pending_tasks: Shared<Vec<GeckoBridgeTask>>,
    // The list of pending event listeners to add.
    pending_listeners: PendingListeners,
    // Flag to know if are already fetching the service id.
    getting_service: AtomicBool,
    // Id of ourselves as a connection observer.
    connection_observer_id: usize,
}

impl GeckoBridgeXpcom {
    fn new() -> RefPtr<Self> {
        debug!("GeckoBridgeXpcom::new");
        let mut transport = UdsTransport::open();
        let core_service = Arc::new(Mutex::new(CoreService::new(&transport)));

        let instance = Self::allocate(InitGeckoBridgeXpcom {
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
    implement_connection_observer!("GeckoBridgeXpcom");

    xpcom_method!(set_apps_service_delegate => SetAppsServiceDelegate(delegate: *const nsIAppsServiceDelegate, callback: *const nsISidlDefaultResponse));
    fn set_apps_service_delegate(
        &self,
        delegate: &nsIAppsServiceDelegate,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::set_apps_service_delegate");

        let delegate =
            ThreadPtrHolder::new(cstr!("nsIAppsServiceDelegate"), RefPtr::new(delegate))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), delegate);

        self.run_or_queue_task(Some(GeckoBridgeTask::SetAppsServiceDelegate(task)));
        Ok(())
    }

    xpcom_method!(set_mobile_manager_delegate => SetMobileManagerDelegate(delegate: *const nsIMobileManagerDelegate, callback: *const nsISidlDefaultResponse));
    fn set_mobile_manager_delegate(
        &self,
        delegate: &nsIMobileManagerDelegate,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::set_mobile_manager_delegate");

        let delegate =
            ThreadPtrHolder::new(cstr!("nsIMobileManagerDelegate"), RefPtr::new(delegate))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), delegate);

        self.run_or_queue_task(Some(GeckoBridgeTask::SetMobileManagerDelegate(task)));
        Ok(())
    }

    xpcom_method!(set_network_manager_delegate => SetNetworkManagerDelegate(delegate: *const nsINetworkManagerDelegate, callback: *const nsISidlDefaultResponse));
    fn set_network_manager_delegate(
        &self,
        delegate: &nsINetworkManagerDelegate,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::set_network_manager_delegate");

        let delegate =
            ThreadPtrHolder::new(cstr!("nsINetworkManagerDelegate"), RefPtr::new(delegate))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), delegate);

        self.run_or_queue_task(Some(GeckoBridgeTask::SetNetworkManagerDelegate(task)));
        Ok(())
    }

    xpcom_method!(set_power_manager_delegate => SetPowerManagerDelegate(delegate: *const nsIPowerManagerDelegate, callback: *const nsISidlDefaultResponse));
    fn set_power_manager_delegate(
        &self,
        delegate: &nsIPowerManagerDelegate,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::set_power_manager_delegate");

        let delegate =
            ThreadPtrHolder::new(cstr!("nsIPowerManagerDelegate"), RefPtr::new(delegate))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), delegate);

        self.run_or_queue_task(Some(GeckoBridgeTask::SetPowerManagerDelegate(task)));
        Ok(())
    }

    xpcom_method!(set_preference_delegate => SetPreferenceDelegate(delegate: *const nsIPreferenceDelegate, callback: *const nsISidlDefaultResponse));
    fn set_preference_delegate(
        &self,
        delegate: &nsIPreferenceDelegate,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::set_preference_delegate");

        let delegate = ThreadPtrHolder::new(cstr!("nsIPreferenceDelegate"), RefPtr::new(delegate))?;

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), delegate);

        self.run_or_queue_task(Some(GeckoBridgeTask::SetPreferenceDelegate(task)));
        Ok(())
    }

    xpcom_method!(char_pref_changed => CharPrefChanged(name: *const nsACString, value: *const nsACString, callback: *const nsISidlDefaultResponse));
    fn char_pref_changed(
        &self,
        name: &nsACString,
        value: &nsACString,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::char_pref_changed");

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (
            SidlCallTask::new(callback),
            (name.to_string(), value.to_string()),
        );

        self.run_or_queue_task(Some(GeckoBridgeTask::CharPrefChanged(task)));
        Ok(())
    }

    xpcom_method!(int_pref_changed => IntPrefChanged(name: *const nsACString, value: i32, callback: *const nsISidlDefaultResponse));
    fn int_pref_changed(
        &self,
        name: &nsACString,
        value: i32,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::int_pref_changed");

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), (name.to_string(), value));

        self.run_or_queue_task(Some(GeckoBridgeTask::IntPrefChanged(task)));
        Ok(())
    }

    xpcom_method!(bool_pref_changed => BoolPrefChanged(name: *const nsACString, value: bool, callback: *const nsISidlDefaultResponse));
    fn bool_pref_changed(
        &self,
        name: &nsACString,
        value: bool,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::bool_pref_changed");

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), (name.to_string(), value));

        self.run_or_queue_task(Some(GeckoBridgeTask::BoolPrefChanged(task)));
        Ok(())
    }

    xpcom_method!(register_token => RegisterToken(token: *const nsAString, url: *const nsAString, settings: *const ThinVec<nsString>, callback: *const nsISidlDefaultResponse));
    fn register_token(
        &self,
        token: &nsAString,
        url: &nsAString,
        xpermissions: &ThinVec<nsString>,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        let permissions = xpermissions
            .into_iter()
            .map(|item| item.to_string())
            .collect();
        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (
            SidlCallTask::new(callback),
            (token.to_string(), url.to_string(), permissions),
        );

        self.run_or_queue_task(Some(GeckoBridgeTask::RegisterToken(task)));
        Ok(())
    }

    xpcom_method!(import_sim_contacts => ImportSimContacts(contacts: *const ThinVec<RefPtr<nsISimContactInfo>>, callback: *const nsISidlDefaultResponse));
    fn import_sim_contacts(
        &self,
        contacts: &ThinVec<RefPtr<nsISimContactInfo>>,
        callback: &nsISidlDefaultResponse,
    ) -> Result<(), nsresult> {
        debug!("GeckoBridgeXpcom::import_sim_contacts");

        // Validate parameters.
        let contacts_info: Vec<SimContactInfo> = contacts
            .iter()
            .filter_map(|item| {
                let mut id = nsString::new();
                let mut tel = nsString::new();
                let mut email = nsString::new();
                let mut name = nsString::new();
                let mut category = nsString::new();

                unsafe {
                    if item.GetId(&mut *id) != NS_OK {
                        return None;
                    }
                    if item.GetTel(&mut *tel) != NS_OK {
                        return None;
                    }
                    if item.GetEmail(&mut *email) != NS_OK {
                        return None;
                    }
                    if item.GetName(&mut *name) != NS_OK {
                        return None;
                    }
                    if item.GetCategory(&mut *category) != NS_OK {
                        return None;
                    }
                }

                Some(SimContactInfo {
                    id: id.to_string(),
                    tel: tel.to_string(),
                    email: email.to_string(),
                    name: name.to_string(),
                    category: category.to_string(),
                })
            })
            .collect();

        // If we failed to convert at least one element of the array, bail out.
        if contacts_info.len() != contacts.len() {
            return Err(NS_ERROR_INVALID_ARG);
        }

        let callback =
            ThreadPtrHolder::new(cstr!("nsISidlDefaultResponse"), RefPtr::new(callback))?;
        let task = (SidlCallTask::new(callback), contacts_info);

        self.run_or_queue_task(Some(GeckoBridgeTask::ImportSimContacts(task)));
        Ok(())
    }

    task_runner!(GeckoBridgeTask, "GeckoBridge", SERVICE_FINGERPRINT);
}

impl Drop for GeckoBridgeXpcom {
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
pub unsafe extern "C" fn gecko_bridge_construct(result: &mut *const nsIGeckoBridge) {
    let inst = GeckoBridgeXpcom::new();
    *result = inst.coerce::<nsIGeckoBridge>();
    std::mem::forget(inst);
}
