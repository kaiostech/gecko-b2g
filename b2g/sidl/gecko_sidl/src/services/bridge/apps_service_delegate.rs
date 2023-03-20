/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Wrapper for the nsIAppsServiceDelegate interface.

use super::messages::*;
use crate::common::core::{BaseMessage, BaseMessageKind};
use crate::common::sidl_task::{SidlRunnable, SidlTask};
use crate::common::traits::TrackerId;
use crate::common::uds_transport::{
    from_base_message, SessionObject, UdsTransport, XpcomSessionObject,
};
use bincode::Options;
use log::{debug, error};
use moz_task::ThreadPtrHandle;
use nserror::{nsresult, NS_OK};
use nsstring::*;
use std::any::Any;
use xpcom::interfaces::{nsIAppsServiceDelegate, nsISidlDefaultResponse};
use xpcom::RefPtr;

pub struct AppsServiceDelegate {
    xpcom: ThreadPtrHandle<nsIAppsServiceDelegate>,
    service_id: TrackerId,
    object_id: TrackerId,
    transport: UdsTransport,
}

impl AppsServiceDelegate {
    pub fn new(
        xpcom: ThreadPtrHandle<nsIAppsServiceDelegate>,
        service_id: TrackerId,
        object_id: TrackerId,
        transport: &UdsTransport,
    ) -> Self {
        Self {
            xpcom,
            service_id,
            object_id,
            transport: transport.clone(),
        }
    }

    fn post_task(&mut self, command: AppsServiceCommand, request_id: u64) {
        let task = AppsServiceDelegateTask {
            xpcom: self.xpcom.clone(),
            command,
            transport: self.transport.clone(),
            service_id: self.service_id,
            object_id: self.object_id,
            request_id,
        };
        let _ = SidlRunnable::new("AppsServiceDelegate", Box::new(task))
            .and_then(|r| SidlRunnable::dispatch(r, self.xpcom.owning_thread()));
    }
}

impl SessionObject for AppsServiceDelegate {
    fn on_request(&mut self, request: BaseMessage, request_id: u64) -> Option<BaseMessage> {
        debug!("AppsServiceDelegate on_request id: {}", request_id);
        // Unpack the request.
        match from_base_message(&request) {
            Ok(GeckoBridgeToClient::AppsServiceDelegateGetUa) => {
                self.post_task(AppsServiceCommand::GetUa(), request_id);
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnBoot(manifest_url, value)) => {
                self.post_task(
                    AppsServiceCommand::OnBoot(manifest_url, value.into()),
                    request_id,
                );
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnBootDone) => {
                self.post_task(AppsServiceCommand::OnBootDone(), request_id);
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnClear(
                manifest_url,
                clear_type,
                value,
            )) => {
                self.post_task(
                    AppsServiceCommand::OnClear(manifest_url, clear_type, value.into()),
                    request_id,
                );
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnInstall(manifest_url, value)) => {
                self.post_task(
                    AppsServiceCommand::OnInstall(manifest_url, value.into()),
                    request_id,
                );
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnUpdate(manifest_url, value)) => {
                self.post_task(
                    AppsServiceCommand::OnUpdate(manifest_url, value.into()),
                    request_id,
                );
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnUninstall(manifest_url)) => {
                self.post_task(AppsServiceCommand::OnUninstall(manifest_url), request_id);
            }
            Ok(GeckoBridgeToClient::AppsServiceDelegateOnLaunch(manifest_url)) => {
                self.post_task(AppsServiceCommand::OnLaunch(manifest_url), request_id);
            }
            _ => {
                error!(
                    "AppsServiceDelegate::on_request unexpected message: {:?}",
                    request.content
                );
            }
        }
        None
    }

    fn on_event(&mut self, _event: Vec<u8>) {
        debug!("AppsServiceDelegate on_request event");
    }

    fn get_ids(&self) -> (u32, u32) {
        debug!(
            "AppsServiceDelegate get_ids, {}, {}",
            self.service_id, self.object_id
        );
        (self.service_id, self.object_id)
    }
}

impl XpcomSessionObject for AppsServiceDelegate {
    fn as_xpcom(&self) -> &dyn Any {
        &self.xpcom
    }
}

// Commands supported by the power manager delegate.
#[derive(Clone)]
enum AppsServiceCommand {
    OnBoot(
        String, // manifest_url
        String, // b2g_features
    ),
    OnBootDone(),
    OnClear(
        String, // manifest_url
        String, // clear_type
        String, // b2g_features
    ),
    OnInstall(
        String, // manifest_url
        String, // b2g_features
    ),
    OnUpdate(
        String, // manifest_url
        String, // b2g_features
    ),
    OnUninstall(
        String, // manifest_url
    ),
    OnLaunch(
        String, // manifest_url
    ),
    GetUa(),
}

// Implementation of nsISidlDefaultResponse for delegate results.
#[xpcom(implement(nsISidlDefaultResponse), atomic)]
pub struct AppResponseXpcom {
    task: AppsServiceDelegateTask,
    success: GeckoBridgeFromClient,
    error: GeckoBridgeFromClient,
}

impl AppResponseXpcom {
    fn new(
        task: AppsServiceDelegateTask,
        success: GeckoBridgeFromClient,
        error: GeckoBridgeFromClient,
    ) -> RefPtr<Self> {
        debug!("AppResponseXpcom::new");

        Self::allocate(InitAppResponseXpcom {
            task,
            success,
            error,
        })
    }

    xpcom_method!(resolve => Resolve());
    fn resolve(&self) -> Result<(), nsresult> {
        debug!("AppResponseXpcom::resolve");
        self.task.reply(&self.success);
        Ok(())
    }

    xpcom_method!(reject => Reject());
    fn reject(&self) -> Result<(), nsresult> {
        debug!("AppResponseXpcom::reject");
        self.task.reply(&self.error);

        Ok(())
    }
}

// A Task to dispatch commands to the delegate.
#[derive(Clone)]
struct AppsServiceDelegateTask {
    xpcom: ThreadPtrHandle<nsIAppsServiceDelegate>,
    command: AppsServiceCommand,
    service_id: TrackerId,
    object_id: TrackerId,
    transport: UdsTransport,
    request_id: u64,
}

impl AppsServiceDelegateTask {
    fn reply(&self, payload: &GeckoBridgeFromClient) {
        let message = BaseMessage {
            service: self.service_id,
            object: self.object_id,
            kind: BaseMessageKind::Response(self.request_id),
            content: crate::common::get_bincode().serialize(&payload).unwrap(),
        };
        let mut t = self.transport.clone();
        let _ = t.send_message(&message);
    }

    fn apps_response(
        &self,
        success: GeckoBridgeFromClient,
        error: GeckoBridgeFromClient,
    ) -> RefPtr<AppResponseXpcom> {
        AppResponseXpcom::new(self.clone(), success, error)
    }
}

impl SidlTask for AppsServiceDelegateTask {
    fn run(&self) {
        // Call the method on the initial thread.
        debug!("AppsServiceDelegateTask::run");
        if let Some(object) = self.xpcom.get() {
            let payload = match &self.command {
                AppsServiceCommand::OnBoot(manifest_url, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let value = nsString::from(value);
                    debug!(
                        "AppsServiceDelegateTask OnBoot manifest_url: {}",
                        manifest_url
                    );
                    debug!("AppsServiceDelegateTask OnBoot value: {:?}", value);
                    unsafe {
                        object.OnBoot(&*manifest_url as &nsAString, &*value as &nsAString);
                    }
                    GeckoBridgeFromClient::AppsServiceDelegateOnBootSuccess
                }
                AppsServiceCommand::OnBootDone() => {
                    debug!("AppsServiceDelegateTask OnBootDone");
                    unsafe {
                        object.OnBootDone();
                    }
                    GeckoBridgeFromClient::AppsServiceDelegateOnBootDoneSuccess
                }
                AppsServiceCommand::OnClear(manifest_url, clear_type, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let clear_type = nsString::from(clear_type);
                    let value = nsString::from(value);
                    let callback = self.apps_response(
                        GeckoBridgeFromClient::AppsServiceDelegateOnClearSuccess,
                        GeckoBridgeFromClient::AppsServiceDelegateOnClearError,
                    );
                    unsafe {
                        object.OnClear(
                            &*manifest_url as &nsAString,
                            &*clear_type as &nsAString,
                            &*value as &nsAString,
                            callback.coerce::<nsISidlDefaultResponse>(),
                        );
                    }
                    return;
                }
                AppsServiceCommand::OnInstall(manifest_url, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let value = nsString::from(value);
                    let callback = self.apps_response(
                        GeckoBridgeFromClient::AppsServiceDelegateOnInstallSuccess,
                        GeckoBridgeFromClient::AppsServiceDelegateOnInstallError,
                    );
                    unsafe {
                        object.OnInstall(
                            &*manifest_url as &nsAString,
                            &*value as &nsAString,
                            callback.coerce::<nsISidlDefaultResponse>(),
                        );
                    }
                    return;
                }
                AppsServiceCommand::OnUpdate(manifest_url, value) => {
                    let manifest_url = nsString::from(manifest_url);
                    let value = nsString::from(value);
                    let callback = self.apps_response(
                        GeckoBridgeFromClient::AppsServiceDelegateOnUpdateSuccess,
                        GeckoBridgeFromClient::AppsServiceDelegateOnUpdateError,
                    );
                    unsafe {
                        object.OnUpdate(
                            &*manifest_url as &nsAString,
                            &*value as &nsAString,
                            callback.coerce::<nsISidlDefaultResponse>(),
                        );
                    }
                    return;
                }
                AppsServiceCommand::OnUninstall(manifest_url) => {
                    let url = nsString::from(manifest_url);
                    let callback = self.apps_response(
                        GeckoBridgeFromClient::AppsServiceDelegateOnUninstallSuccess,
                        GeckoBridgeFromClient::AppsServiceDelegateOnUninstallError,
                    );
                    unsafe {
                        object.OnUninstall(
                            &*url as &nsAString,
                            callback.coerce::<nsISidlDefaultResponse>(),
                        );
                    }
                    return;
                }
                AppsServiceCommand::OnLaunch(manifest_url) => {
                    let url = nsString::from(manifest_url);
                    unsafe {
                        object.OnLaunch(&*url as &nsAString);
                    }
                    GeckoBridgeFromClient::AppsServiceDelegateOnLaunchSuccess
                }
                AppsServiceCommand::GetUa() => {
                    let mut ua = nsString::new();
                    let status = unsafe { object.GetUa(&mut *ua) };
                    if status == nserror::NS_OK {
                        GeckoBridgeFromClient::AppsServiceDelegateGetUaSuccess(ua.to_string())
                    } else {
                        GeckoBridgeFromClient::AppsServiceDelegateGetUaError
                    }
                }
            };
            self.reply(&payload);
        }
    }
}
