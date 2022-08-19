/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// Helpers to create and dispatch Gecko tasks representing SIDL call results and event.
use crate::common::traits::{Shared, TrackerId};
use crate::common::uds_transport::{ResponseReceiver, SessionObject, UdsResult, UdsTransport};
use crate::services::core::service::CoreGetServiceReceiverDelegate;
use log::{debug, error};
use moz_task::{DispatchOptions, ThreadPtrHandle};
use nserror::{nsresult, NS_OK};
use parking_lot::Mutex;
use serde::Serialize;
use std::sync::Arc;
use xpcom::{
    interfaces::{nsIEventTarget, nsISidlEventListener, nsIThread},
    RefPtr, XpCom,
};

/// A task represents an operation that asynchronously executes on a target
/// thread.
pub trait SidlTask {
    fn run(&self);
}

/// The struct responsible for dispatching a Task by calling its run() method
/// on the target thread.
///
/// This is a simplified version of moz_task::TaskRunnable that doesn't dispatch
/// back to the calling thread since it's the Rust i/o thread that is not an xpcom
/// event target.
#[xpcom(implement(nsIRunnable), atomic)]
pub struct SidlRunnable {
    pub name: &'static str,
    task: Box<dyn SidlTask + Send + Sync>,
}

impl SidlRunnable {
    pub fn new(
        name: &'static str,
        task: Box<dyn SidlTask + Send + Sync>,
    ) -> Result<RefPtr<SidlRunnable>, nsresult> {
        Ok(SidlRunnable::allocate(InitSidlRunnable { name, task }))
    }

    /// Dispatches this task runnable to an event target with the default
    /// options.
    ///
    /// Note that this is an associated function, not a method, because it takes
    /// an owned reference to the runnable, and must be called like
    /// `SidlRunnable::dispatch(runnable, target)` and *not*
    /// `runnable.dispatch(target)`.
    ///
    /// This function leaks the runnable if dispatch fails.
    #[inline]
    pub fn dispatch(this: RefPtr<Self>, target: &nsIEventTarget) -> Result<(), nsresult> {
        debug!("Dispatching {}", this.name);
        unsafe { target.DispatchFromScript(this.coerce(), DispatchOptions::default().flags()) }
            .to_result()
    }

    xpcom_method!(run => Run());
    fn run(&self) -> Result<(), nsresult> {
        self.task.run();
        Ok(())
    }
}

// SIDL call results always have a success and an error case.
// This struct makes it possible to create tasks matching
// the SIDL return types and the XPIDL interface that is
// used to represent the callback.
// Tasks are hold by the UDS transport and when needing to be dispatched,
// are dispatched on the calling (often main) thread.
pub trait SidlCallback {
    type Success;
    type Error;

    fn resolve(&self, value: &Self::Success);
    fn reject(&self, value: &Self::Error);
}
pub struct SidlCallTask<S, E, I>
where
    I: XpCom + 'static + SidlCallback<Success = S, Error = E>,
{
    callback: ThreadPtrHandle<I>,
    value: Option<Result<S, E>>,
}

impl<S, E, I> Clone for SidlCallTask<S, E, I>
where
    I: XpCom + 'static + SidlCallback<Success = S, Error = E>,
    S: Clone,
    E: Clone,
{
    fn clone(&self) -> Self {
        Self {
            callback: self.callback.clone(),
            value: self.value.clone(),
        }
    }
}

impl<S, E, I> SidlCallTask<S, E, I>
where
    I: XpCom + 'static + SidlCallback<Success = S, Error = E>,
{
    pub fn new(callback: ThreadPtrHandle<I>) -> Self {
        Self {
            callback,
            value: None,
        }
    }

    pub fn thread(&self) -> &nsIThread {
        self.callback.owning_thread()
    }

    pub fn set_value(&mut self, value: Result<S, E>) {
        self.value = Some(value);
    }
}

impl<S, E, I> SidlTask for SidlCallTask<S, E, I>
where
    S: Clone,
    E: Clone,
    I: XpCom + 'static + SidlCallback<Success = S, Error = E>,
{
    fn run(&self) {
        // Call back the pseudo-promise on the initial thread.
        debug!("SidlCallTask::run");
        if let Some(callback) = self.callback.get() {
            match &self.value {
                Some(Ok(success)) => callback.resolve(success),
                Some(Err(error)) => callback.reject(error),
                None => error!("No value available for this task."),
            }
        } else {
            error!("Failed to get callback");
        }
    }
}

// Helper macros to convert arguments.
#[allow(unused_macros)]
macro_rules! deref {
    ($input:expr) => {
        $input.deref()
    };
}

#[allow(unused_macros)]
macro_rules! into {
    ($input:expr) => {
        $input.into()
    };
}

// Helper macro to forward the implementation of the SidlCallback to the
// XPCOM interfaces.
macro_rules! sidl_callback_for {
    // Full case, with success and error types.
    ($interface:ty, $success:ty, $error: ty, $succtrans:tt, $errtrans:tt) => {
        impl SidlCallback for $interface {
            type Success = $success;
            type Error = $error;

            fn resolve(&self, value: &Self::Success) {
                unsafe {
                    self.Resolve($succtrans!(value));
                }
            }

            fn reject(&self, value: &Self::Error) {
                unsafe {
                    self.Reject($errtrans!(value));
                }
            }
        }
    };

    // Common case with only a success type.
    ($interface:ty, $success:ty, $succtrans:tt) => {
        impl SidlCallback for $interface {
            type Success = $success;
            type Error = ();

            fn resolve(&self, value: &Self::Success) {
                #[allow(unused_imports)]
                use std::ops::Deref;
                unsafe {
                    self.Resolve($succtrans!(value));
                }
            }

            fn reject(&self, _value: &Self::Error) {
                unsafe {
                    self.Reject();
                }
            }
        }
    };

    // Common case with no return type.
    ($interface:ty) => {
        impl SidlCallback for $interface {
            type Success = ();
            type Error = ();

            fn resolve(&self, _value: &Self::Success) {
                unsafe {
                    self.Resolve();
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

// Helper macros to write the ResponseReciever for a task, based on the success and error
// types.

// Common case with no return types.
macro_rules! task_receiver {
    ($name:ident, $interface:ty, $base:ident, $succname:ident, $errname:ident) => {
        struct $name {
            task: SidlCallTask<(), (), $interface>,
        }

        impl ResponseReceiver for $name {
            fn on_message(&mut self, message: BaseMessage) {
                debug!("TaskReceiver::on_message");
                match from_base_message(&message) {
                    Ok($base::$succname) => {
                        debug!("{}", stringify!($succname));
                        self.task.set_value(Ok(()));
                        let _ = SidlRunnable::new(stringify!($name), Box::new(self.task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, self.task.thread()));
                    }
                    Ok($base::$errname) => {
                        debug!("{}", stringify!($errname));
                        self.task.set_value(Err(()));
                        let _ = SidlRunnable::new(stringify!($name), Box::new(self.task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, self.task.thread()));
                    }
                    other => {
                        error!("Unexpected {} message: {:?}", stringify!($name), other);
                    }
                }
            }
        }
    };
}

// Common case with only a success type.
#[allow(unused_macros)]
macro_rules! task_receiver_success {
    ($name:ident, $interface:ty, $base:ident, $succname:ident, $errname:ident, $succtype:ty) => {
        struct $name {
            task: SidlCallTask<$succtype, (), $interface>,
        }

        impl ResponseReceiver for $name {
            fn on_message(&mut self, message: BaseMessage) {
                debug!("TaskReceiver::on_message");
                match from_base_message(&message) {
                    Ok($base::$succname(value)) => {
                        debug!("{}", stringify!($succname));
                        self.task.set_value(Ok(value.into()));
                        let _ = SidlRunnable::new(stringify!($name), Box::new(self.task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, self.task.thread()));
                    }
                    Ok($base::$errname) => {
                        debug!("{}", stringify!($errname));
                        self.task.set_value(Err(()));
                        let _ = SidlRunnable::new(stringify!($name), Box::new(self.task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, self.task.thread()));
                    }
                    other => {
                        error!("Unexpected {} message: {:?}", stringify!($name), other);
                    }
                }
            }
        }
    };
}

// Common case with a success and an error type.
#[allow(unused_macros)]
macro_rules! task_receiver_success_error {
    ($name:ident, $interface:ty, $base:ident, $succname:ident, $errname:ident, $succtype:ty, $errtype:ty) => {
        struct $name {
            task: SidlCallTask<$succtype, $errtype, $interface>,
        }

        impl ResponseReceiver for $name {
            fn on_message(&mut self, message: BaseMessage) {
                debug!("TaskReceiver::on_message");
                match from_base_message(&message) {
                    Ok($base::$succname(value)) => {
                        debug!("{}", stringify!($succname));
                        self.task.set_value(Ok(value.into()));
                        let _ = SidlRunnable::new(stringify!($name), Box::new(self.task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, self.task.thread()));
                    }
                    Ok($base::$errname(value)) => {
                        debug!("{}", stringify!($errname));
                        self.task.set_value(Err(value.into()));
                        let _ = SidlRunnable::new(stringify!($name), Box::new(self.task.clone()))
                            .and_then(|r| SidlRunnable::dispatch(r, self.task.thread()));
                    }
                    other => {
                        error!("Unexpected {} message: {:?}", stringify!($name), other);
                    }
                }
            }
        }
    };
}

// Struct providing the get_service() delegate for a service.

// Vec<(event_type, handler, key)>
pub type PendingListeners = Shared<Vec<(i32, ThreadPtrHandle<nsISidlEventListener>, usize)>>;

pub trait ServiceClientImpl<T> {
    fn new(transport: UdsTransport, service_id: TrackerId) -> Self;
    fn dispatch_queue(&mut self, task_queue: &Shared<Vec<T>>, pending_listeners: &PendingListeners);
    fn run_task(&mut self, task: T) -> Result<(), nsresult>;
}

pub struct GetServiceDelegate<I, T> {
    service_impl: Shared<Option<Arc<Mutex<I>>>>,
    pending_tasks: Shared<Vec<T>>,
    pending_listeners: PendingListeners,
    transport: UdsTransport,
}

impl<I, T> GetServiceDelegate<I, T> {
    pub fn new(
        service_impl: Shared<Option<Arc<Mutex<I>>>>,
        pending_tasks: Shared<Vec<T>>,
        pending_listeners: PendingListeners,
        transport: UdsTransport,
    ) -> Self {
        Self {
            service_impl,
            pending_tasks,
            pending_listeners,
            transport,
        }
    }
}

impl<I, T> CoreGetServiceReceiverDelegate for GetServiceDelegate<I, T>
where
    I: ServiceClientImpl<T> + SessionObject + Send + 'static,
    T: Send,
{
    fn on_success(&mut self, service_id: TrackerId) {
        debug!("Service is ready: #{}", service_id);

        // Now that the service id is available, create the implementation struct and register it
        // with the transport session.
        let implem = I::new(self.transport.clone(), service_id);
        let shared = Arc::new(Mutex::new(implem));
        self.transport.register_session_object(shared.clone());
        *self.service_impl.lock() = Some(shared.clone());
        shared
            .lock()
            .dispatch_queue(&self.pending_tasks, &self.pending_listeners);
    }
    fn on_error(&mut self) {
        error!("Failed to get service. :(");
    }
}

#[derive(Clone)]
pub struct TaskSender {
    service_id: TrackerId,
    transport: UdsTransport,
}

impl TaskSender {
    pub fn new(transport: UdsTransport, service_id: TrackerId) -> Self {
        Self {
            service_id,
            transport,
        }
    }

    // Pre-process a response.
    fn send_request<S: Serialize>(
        &mut self,
        payload: &S,
        response_task: Box<dyn ResponseReceiver>,
    ) -> UdsResult<()> {
        self.transport.send_request(
            self.service_id,
            0, /* service object */
            payload,
            response_task,
        )
    }

    pub fn send_task<S: Serialize, R: ResponseReceiver + 'static>(
        &mut self,
        payload: &S,
        receiver: R,
    ) {
        // Send  the task wrapped in a receiver.
        let _ = self.send_request(payload, Box::new(receiver));
    }
}

// Event handling is done similarly to callbacks, since dispatching an event
// is similar to resolving a callback.

// Helper macro to forward the implementation of the SidlEvent to the
// XPCOM interfaces.
macro_rules! sidl_event_for {
    // Common case with a payload type.
    ($interface:ty, $success:ty, $succtrans:tt) => {
        impl SidlEvent for $interface {
            type Success = $success;

            fn handle_event(&self, value: &Self::Success) {
                unsafe {
                    self.HandleEvent($succtrans!(value));
                }
            }
        }
    };

    // Case with no event type.
    ($interface:ty) => {
        impl SidlEvent for $interface {
            type Success = ();

            fn handleEvent(&self, _value: &Self::Success) {
                unsafe {
                    self.HandleEvent(());
                }
            }
        }
    };
}

pub trait SidlEvent {
    type Success;

    fn handle_event(&self, value: &Self::Success);
}

pub struct SidlEventTask<S, I>
where
    I: XpCom + 'static + SidlEvent<Success = S>,
{
    handler: ThreadPtrHandle<I>,
    value: Option<S>,
}

impl<S, I> Clone for SidlEventTask<S, I>
where
    I: XpCom + 'static + SidlEvent<Success = S>,
    S: Clone,
{
    fn clone(&self) -> Self {
        Self {
            handler: self.handler.clone(),
            value: self.value.clone(),
        }
    }
}

impl<S, I> SidlEventTask<S, I>
where
    I: XpCom + 'static + SidlEvent<Success = S>,
{
    pub fn new(handler: ThreadPtrHandle<I>) -> Self {
        Self {
            handler,
            value: None,
        }
    }

    pub fn thread(&self) -> &nsIThread {
        self.handler.owning_thread()
    }

    pub fn set_value(&mut self, value: S) {
        self.value = Some(value);
    }
}

impl<S, I> SidlTask for SidlEventTask<S, I>
where
    S: Clone,
    I: XpCom + 'static + SidlEvent<Success = S>,
{
    fn run(&self) {
        // Call back the handler on the initial thread.
        debug!("SidlEventTask::run");
        if let Some(handler) = self.handler.get() {
            match &self.value {
                Some(value) => handler.handle_event(value),
                None => error!("No value available for this event task."),
            }
        } else {
            error!("Failed to get event handler");
        }
    }
}

// Extracts commononly used boilerplate.

macro_rules! task_runner {
    ($tasks:ty, $service_name:expr, $fingerprint:expr) => {
        // Runs the given task right away or queue it and kick off the service getter.
        fn run_or_queue_task(&self, task: Option<$tasks>) {
            if let Some(inner) = self.inner.lock().as_ref() {
                self.getting_service.store(false, Ordering::Relaxed);
                // Run the task right away.
                if let Some(task) = task {
                    let _ = inner.lock().run_task(task);
                }
                return;
            }

            // Queue the task since we can't run it yet.
            if let Some(task) = task {
                let mut queue = self.pending_tasks.lock();
                queue.push(task);
                debug!("New task added to the queue, length is now {}", queue.len());
            }

            // Return early when we already started the get_service request but haven't received the response yet
            // to prevent fetching of multiple instances.
            if self.getting_service.load(Ordering::Relaxed) == true {
                return;
            }

            let receiver = CoreGetServiceReceiver::new(Box::new(GetServiceDelegate::new(
                self.inner.clone(),
                self.pending_tasks.clone(),
                self.pending_listeners.clone(),
                self.transport.clone(),
            )));
            let mut lock = self.core_service.lock();
            if let Err(err) = lock.get_service($service_name, $fingerprint, Box::new(receiver)) {
                error!("Failed to get {} service: {}", $service_name, err);
            } else {
                self.getting_service.store(true, Ordering::Relaxed);
            }
        }
    };
}
