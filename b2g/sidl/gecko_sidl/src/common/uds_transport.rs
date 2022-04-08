/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// A client side UDS transport.
use crate::adopt_current_thread;
use crate::common::core::{BaseMessage, BaseMessageKind};
use crate::common::frame::{Error as FrameError, Frame};
use crate::common::sidl_task::{SidlRunnable, SidlTask};
use crate::common::traits::Shared;
use bincode::Options;
use log::{debug, error, info};
use moz_task::ThreadPtrHandle;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCString};
use parking_lot::Mutex;
use serde::{Deserialize, Serialize};
use std::any::Any;
use std::collections::HashMap;
use std::ffi::CString;
use std::net::Shutdown;
use std::ops::DerefMut;
use std::os::unix::net::UnixStream;
use std::result::Result as StdResult;
use std::sync::Arc;
use std::thread;
use thiserror::Error as ThisError;
use xpcom::interfaces::nsISidlConnectionObserver;

#[derive(ThisError, Debug)]
pub enum UdsError {
    #[error("Framing error")]
    Frame(#[from] FrameError),
    #[error("No stream available")]
    NoStreamAvailable,
    #[error("Generic Error: {0}")]
    Generic(String),
}

impl From<&str> for UdsError {
    fn from(val: &str) -> Self {
        Self::Generic(val.into())
    }
}

impl From<String> for UdsError {
    fn from(val: String) -> Self {
        Self::Generic(val)
    }
}

pub type UdsResult<T> = StdResult<T, UdsError>;

// A response dispatcher.
pub trait ResponseReceiver: Send {
    fn on_message(&mut self, message: BaseMessage);
}

// Trait that needs to be implemented by session objects, that can receive
// events and raw requests/responses.
// These methods may be called on a different thread than the caller one.
pub trait SessionObject: Send {
    // Requests are messages initiated by the service to the client, used
    // when calling methods on callbacks.
    // If this returns a BaseMessage, it will be sent by the transport layer.
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage>;
    // Events are messages for all events on this object.
    fn on_event(&mut self, event: Vec<u8>);
    // Returns the (service, object) tuple.
    fn get_ids(&self) -> (u32, u32);
}

pub trait XpcomSessionObject: SessionObject {
    // Get the XPCom that can be tied to this session object.
    fn as_xpcom(&self) -> &dyn Any;
}

pub type SharedSessionObject = Arc<Mutex<dyn SessionObject>>;

struct SessionData {
    // The map from request IDs to messages for the client.
    pending_responses: HashMap<u64, Box<dyn ResponseReceiver>>,
    // The current request id used when sending requests.
    request_id: u64,
    // Session objects. The key is (service, object)
    session_objects: HashMap<(u32, u32), SharedSessionObject>,
    // The connection to the Unix Domain Socket.
    stream: Option<UnixStream>,
}

impl SessionData {
    fn new(stream: Option<UnixStream>) -> Self {
        Self {
            pending_responses: HashMap::default(),
            request_id: 0,
            session_objects: HashMap::default(),
            stream,
        }
    }

    fn replace_stream(&mut self, stream: UnixStream) {
        self.stream = Some(stream);
    }
}

struct ConnectionChangeTask {
    disconnected: bool,
    observer: ThreadPtrHandle<nsISidlConnectionObserver>,
}

impl SidlTask for ConnectionChangeTask {
    fn run(&self) {
        info!(
            "Running ConnectionChangeTask disconnected=`{}`",
            self.disconnected
        );

        if let Some(obs) = self.observer.get() {
            let result = unsafe {
                if self.disconnected {
                    obs.Disconnected()
                } else {
                    obs.Reconnected()
                }
            };
            if result != NS_OK {
                error!(
                    "Error notifying disconnected={} : {}",
                    self.disconnected, result
                );
            } else {
                debug!("Notified observer, disconnected={}", self.disconnected);
            }
        } else {
            error!("Failed to get nsISidlConnectionObserver object.");
        }
    }
}

#[derive(Default)]
struct ConnectionObservers {
    observers: HashMap<usize, ThreadPtrHandle<nsISidlConnectionObserver>>,
    current_id: usize,
}

impl ConnectionObservers {
    fn add(&mut self, observer: ThreadPtrHandle<nsISidlConnectionObserver>) -> usize {
        self.current_id += 1;
        self.observers.insert(self.current_id, observer);
        self.current_id
    }

    fn remove(&mut self, id: usize) {
        self.observers.remove(&id);
    }

    fn next_id(&self) -> usize {
        self.current_id + 1
    }

    fn len(&self) -> usize {
        self.observers.len()
    }

    fn for_each<T>(&self, closure: T)
    where
        T: Fn(&ThreadPtrHandle<nsISidlConnectionObserver>),
    {
        for observer in self.observers.values() {
            closure(observer)
        }
    }
}

// Helper function to get a char pref with a default value.
// Similar to https://searchfox.org/mozilla-central/rev/23b89f8c85c3898cc95c96c90afd6c304aad0158/toolkit/components/glean/src/init/mod.rs#267
// but using 'GetBranch()' instead of 'GetDefaultBranch()' to pick up prefs from user prefs files.
fn fallible_get_char_pref(name: &str, default_value: &str) -> Result<nsCString, nsresult> {
    if let Some(pref_service) = xpcom::services::get_PrefService() {
        let branch = xpcom::getter_addrefs(|p| {
            // Safe because:
            //  * `null` is explicitly allowed per documentation
            //  * `p` is a valid outparam guaranteed by `getter_addrefs`
            unsafe { pref_service.GetBranch(std::ptr::null(), p) }
        })?;
        let pref_name = CString::new(name).map_err(|_| NS_ERROR_FAILURE)?;
        let mut pref_value = nsCString::new();
        // Safe because:
        //  * `branch` is non-null (otherwise `getter_addrefs` would've been `Err`
        //  * `pref_name` exists so a pointer to it is valid for the life of the function
        //  * `channel` exists so a pointer to it is valid, and it can be written to
        unsafe {
            if (*branch)
                .GetCharPref(
                    pref_name.as_ptr(),
                    pref_value.deref_mut() as *mut nsACString,
                )
                .to_result()
                .is_err()
            {
                pref_value = default_value.into();
            }
        }
        Ok(nsCString::from(pref_value))
    } else {
        Ok(nsCString::from(default_value))
    }
}

fn get_char_pref(name: &str, default_value: &str) -> String {
    fallible_get_char_pref(name, default_value)
        .unwrap_or_else(|_| nsCString::from(default_value))
        .to_string()
}

#[derive(Clone)]
pub struct UdsTransport {
    // The book keeping data for this session.
    session_data: Shared<SessionData>,
    // The current id that will be set for connection observers.
    // The observers of disconnection and reconnection events.
    connection_observers: Shared<ConnectionObservers>,
}

impl UdsTransport {
    pub fn is_ready(&self) -> bool {
        self.session_data.lock().stream.is_some()
    }

    fn notify_observers(&self, disconnected: bool) {
        let observers = self.connection_observers.lock();
        info!(
            "Will notify connection status change to {} observers",
            observers.len()
        );
        observers.for_each(move |observer| {
            let _ = SidlRunnable::new(
                "ApiDaemonNotifyObservers",
                Box::new(ConnectionChangeTask {
                    disconnected,
                    observer: observer.clone(),
                }),
            )
            .and_then(|r| SidlRunnable::dispatch(r, observer.owning_thread()));
        });
    }

    pub fn next_connection_observer_id(&self) -> usize {
        self.connection_observers.lock().next_id()
    }

    pub fn add_connection_observer(
        &mut self,
        observer: ThreadPtrHandle<nsISidlConnectionObserver>,
    ) -> usize {
        let mut observers = self.connection_observers.lock();
        let res = observers.add(observer.clone());
        info!("Connection observer added, total is {}", observers.len());
        res
    }

    pub fn remove_connection_observer(&mut self, observer_id: usize) {
        let mut observers = self.connection_observers.lock();
        observers.remove(observer_id);
        info!("Connection observer removed, total is {}", observers.len());
    }

    pub fn open() -> Self {
        #[cfg(target_os = "android")]
        let path = get_char_pref("b2g.api-daemon.uds-socket", "/dev/socket/api-daemon");
        #[cfg(not(target_os = "android"))]
        let path = get_char_pref("b2g.api-daemon.uds-socket", "/tmp/api-daemon-socket");

        let (transport, mut recv_stream) = match UnixStream::connect(&path) {
            Ok(stream) => {
                let reader = stream.try_clone().expect("Failed to clone UDS stream");
                (
                    Self {
                        session_data: Shared::adopt(SessionData::new(Some(stream))),
                        connection_observers: Shared::default(),
                    },
                    Some(reader),
                )
            }
            Err(err) => {
                error!("Failed to connect to {} : {}", path, err);
                (
                    Self {
                        session_data: Shared::adopt(SessionData::new(None)),
                        connection_observers: Shared::default(),
                    },
                    None,
                )
            }
        };

        let mut transport_inner = transport.clone();

        // Start the reading thread.
        thread::spawn(move || {
            // Needed to make this thread known from Gecko, since we can dispatch Tasks
            // from there.
            let _thread = adopt_current_thread();

            loop {
                // Try to reconnect every 5 seconds.
                if recv_stream.is_none() {
                    loop {
                        info!("Waiting 1 seconds to reconnect to {} ...", path);
                        thread::sleep(std::time::Duration::from_secs(1));
                        if let Ok(stream) = UnixStream::connect(&path) {
                            // Update the receiving stream.
                            recv_stream =
                                Some(stream.try_clone().expect("Failed to clone UDS stream"));
                            info!("Reconnected successfully");
                            // Update the session data with the new stream.
                            let mut data = transport_inner.session_data.lock();
                            data.replace_stream(stream);

                            transport_inner.notify_observers(false);
                            break;
                        }
                    }
                }

                if let Some(reader) = &mut recv_stream {
                    // TODO: check if we need a BufReader for performance reasons.
                    match Frame::deserialize_from(reader) {
                        Ok(data) => {
                            transport_inner.dispatch(data);
                        }
                        Err(crate::common::frame::Error::Io(err)) => {
                            // That can happen if the server side is closed under us, eg. if
                            // the daemon restarts.
                            // In that case we need to mark this transport as "zombie" and try
                            // to reconnect it.
                            let mut session = transport_inner.session_data.lock();
                            error!(
                                "Unexpected daemon deconnection, will try to reconnect: {}",
                                err
                            );
                            error!(
                                "There were {} pending requests.",
                                session.pending_responses.len()
                            );

                            // TODO: manage replay of in flight requests/responses. For now we just drop them.
                            session.pending_responses.clear();
                            // Clear our session objects, since they can't be linked to the daemon side anymore.
                            session.session_objects.clear();
                            session.stream = None;

                            transport_inner.notify_observers(true);
                            recv_stream = None;
                        }
                        Err(err) => {
                            // Can be a bincode error, considered not fatal for now.
                            error!("Unexpected error: {}", err);
                        }
                    }
                }
            }
        });

        transport
    }

    pub fn dispatch(&mut self, message: BaseMessage) {
        match message.kind {
            BaseMessageKind::Response(id) => {
                let maybe_receiver = {
                    let mut session_data = self.session_data.lock();
                    if let Some(receiver) = session_data.pending_responses.remove(&id) {
                        Some(receiver)
                    } else {
                        error!("No receiver object available for request #{}", id);
                        None
                    }
                };

                if let Some(mut receiver) = maybe_receiver {
                    receiver.on_message(message);
                }
            }
            BaseMessageKind::Event => {
                // Send the message to the correct handler.
                debug!(
                    "Received event for service #{} object #{}",
                    message.service, message.object,
                );
                let session_data = self.session_data.lock();
                match session_data
                    .session_objects
                    .get(&(message.service, message.object))
                {
                    Some(handler) => handler.lock().on_event(message.content),
                    None => error!(
                        "No event handler for service #{} object #{}",
                        message.service, message.object
                    ),
                }
            }
            BaseMessageKind::Request(id) => {
                // Look for the session object that should receive this request.
                let key = (message.service, message.object);
                let mut session_data = self.session_data.lock();
                if let Some(target) = session_data.session_objects.get(&key) {
                    let message = target.lock().on_request(message, id);
                    if let Some(message) = message {
                        // Call directly Frame::serialize_to so that we can reuse the session_data
                        // lock we already hold.
                        if let Some(stream) = &mut session_data.stream {
                            let _ = Frame::serialize_to(&message, stream);
                        } else {
                            error!("Trying to send data but no stream is available.");
                        }
                    }
                } else {
                    error!(
                        "No session object found for service #{} object #{}",
                        message.service, message.object
                    );
                }
            }
            BaseMessageKind::PermissionError(error) => {
                error!("Unexpected Permission Error: {:?}", error);
            }
        }
    }

    // Returns a base message matching this request payload.
    pub fn send_request<S: Serialize>(
        &mut self,
        service: u32,
        object: u32,
        payload: &S,
        response_task: Box<dyn ResponseReceiver>,
    ) -> UdsResult<()> {
        let request_id = {
            let mut session_data = self.session_data.lock();
            session_data.request_id += 1;
            session_data.request_id
        };

        // Wrap the payload in a base message and send it.
        let message = BaseMessage {
            service,
            object,
            kind: BaseMessageKind::Request(request_id),
            content: crate::common::get_bincode().serialize(payload).unwrap(),
        };

        // Register the sender in the requests HashMap.
        let mut session_data = self.session_data.lock();
        session_data
            .pending_responses
            .insert(request_id, response_task);

        // Send the message.
        if let Some(stream) = &mut session_data.stream {
            Frame::serialize_to(&message, stream).map_err(|e| e.into())
        } else {
            Err(UdsError::NoStreamAvailable)
        }
    }

    // Sends a BaseMessage fully build.
    pub fn send_message(&mut self, message: &BaseMessage) -> UdsResult<()> {
        let mut session_data = self.session_data.lock();
        if let Some(stream) = &mut session_data.stream {
            Frame::serialize_to(&message, stream).map_err(|e| e.into())
        } else {
            Err(UdsError::NoStreamAvailable)
        }
    }

    fn close(&mut self) {
        info!("Closing UdsTransport");
        let mut session_data = self.session_data.lock();
        session_data.pending_responses.clear();
        session_data.session_objects.clear();

        if let Some(stream) = &session_data.stream {
            let _ = stream.shutdown(Shutdown::Both);
        }

        session_data.stream = None;
    }

    pub fn register_session_object(&mut self, object: SharedSessionObject) {
        let ids = object.lock().get_ids();
        self.session_data
            .lock()
            .session_objects
            .insert(ids, object.clone());
    }

    pub fn unregister_session_object(&mut self, object: SharedSessionObject) {
        let ids = object.lock().get_ids();
        self.session_data.lock().session_objects.remove(&ids);
    }
}

impl Drop for UdsTransport {
    fn drop(&mut self) {
        let ref_count = self.session_data.ref_count();
        info!(
            "Dropping UdsTransport session_data ref count is {}, connection observer ref count is {}",
            ref_count, self.connection_observers.ref_count()
        );
        if ref_count == 0 {
            self.close();
        }
    }
}

pub fn from_base_message<'a, T: Deserialize<'a>>(message: &'a BaseMessage) -> UdsResult<T> {
    crate::common::deserialize_bincode(&message.content)
        .map_err(|err| UdsError::Generic(format!("Failed to deserialize base message: {}", err)))
}
