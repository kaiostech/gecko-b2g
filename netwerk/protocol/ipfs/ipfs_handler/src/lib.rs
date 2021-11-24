/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use]
extern crate xpcom;

use cid::{multibase, Cid};
use log::debug;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_OK};
use nsstring::{nsACString, nsCString};
use std::convert::TryFrom;
use std::ffi::CString;
use std::ops::DerefMut;
use std::ptr;
use xpcom::{
    getter_addrefs,
    interfaces::{nsIChannel, nsILoadInfo, nsIProtocolHandler, nsIURI},
    RefPtr,
};

#[derive(Debug, PartialEq)]
enum Protocol {
    Ipfs,
    Ipns,
}

impl Protocol {
    fn as_nscstring(&self) -> nsCString {
        match &self {
            Protocol::Ipfs => nsCString::from("ipfs"),
            Protocol::Ipns => nsCString::from("ipns"),
        }
    }
}

// Default gateway domain.
// Use the 'ipfs.gateway' preference to change it.
static DEFAULT_GATEWAY_DOMAIN: &str = "dweb.link";

// Helper functions to get a char pref with a default value.
fn fallible_get_char_pref(name: &str, default_value: &str) -> Result<nsCString, nsresult> {
    if let Some(pref_service) = xpcom::services::get_PrefService() {
        let branch = xpcom::getter_addrefs(|p| {
            // Safe because:
            //  * `null` is explicitly allowed per documentation
            //  * `p` is a valid outparam guaranteed by `getter_addrefs`
            unsafe { pref_service.GetDefaultBranch(std::ptr::null(), p) }
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

fn get_char_pref(name: &str, default_value: &str) -> nsCString {
    match fallible_get_char_pref(name, default_value) {
        Ok(value) => value,
        Err(_) => nsCString::from(default_value),
    }
}

#[derive(xpcom)]
#[xpimplements(nsIProtocolHandler)]
#[refcnt = "atomic"]
struct InitIpfsHandler {
    // The handler protocol.
    protocol: Protocol,
    gateway_domain: nsCString,
}

impl IpfsHandler {
    fn new(protocol: Protocol) -> RefPtr<Self> {
        debug!("IpfsHandler::new {:?}", protocol);

        Self::allocate(InitIpfsHandler {
            gateway_domain: get_char_pref("ips.gateway", DEFAULT_GATEWAY_DOMAIN),
            protocol,
        })
    }

    xpcom_method!(get_default_port => GetDefaultPort(aDefaultPort: *mut i32));
    fn get_default_port(&self, default_port: *mut i32) -> Result<(), nsresult> {
        unsafe {
            (*default_port) = -1;
        }
        Ok(())
    }

    xpcom_method!(allow_port => AllowPort(port: i32, scheme: *const libc::c_char, _retval: *mut bool));
    fn allow_port(
        &self,
        _port: i32,
        _scheme: *const libc::c_char,
        retval: *mut bool,
    ) -> Result<(), nsresult> {
        unsafe {
            (*retval) = false;
        }
        Ok(())
    }

    xpcom_method!(get_protocol_flags => GetProtocolFlags(aProtocolFlags: *mut u32));
    fn get_protocol_flags(&self, protocol_flags: *mut u32) -> Result<(), nsresult> {
        let flags = nsIProtocolHandler::URI_NOAUTH
            | nsIProtocolHandler::URI_LOADABLE_BY_ANYONE
            | nsIProtocolHandler::URI_IS_POTENTIALLY_TRUSTWORTHY
            | nsIProtocolHandler::URI_FETCHABLE_BY_ANYONE
            | nsIProtocolHandler::URI_LOADABLE_BY_EXTENSIONS;

        unsafe {
            (*protocol_flags) = flags as u32;
        }

        Ok(())
    }

    xpcom_method!(get_scheme => GetScheme(aScheme: *mut nsACString));
    fn get_scheme(&self, scheme: *mut nsACString) -> Result<(), nsresult> {
        unsafe {
            (*scheme).assign(&self.protocol.as_nscstring());
        }

        Ok(())
    }

    xpcom_method!(new_channel => NewChannel(aURI: *const nsIURI, aLoadinfo: *const nsILoadInfo, retval: *mut *const nsIChannel));
    fn new_channel(
        &self,
        uri: *const nsIURI,
        load_info: *const nsILoadInfo,
        retval: *mut *const nsIChannel,
    ) -> Result<(), nsresult> {
        debug!("new_channel {:?}", {
            let mut spec = nsCString::new();
            unsafe {
                (*uri).GetSpec(&mut *spec);
            }
            spec
        });

        let mut host = nsCString::new();
        let mut path_query = nsCString::new();
        let scheme = self.protocol.as_nscstring();

        unsafe {
            (*uri).GetHost(&mut *host);
            (*uri).GetPathQueryRef(&mut *path_query);
        }

        // Mapping to gateway url:
        // ipfs://bafybeiemxf5abjwjbikoz4mc3a3dla6ual3jsgpdr4cjr3oz3evfyavhwq/wiki/Vincent_van_Gogh.html ->
        // https://bafybeiemxf5abjwjbikoz4mc3a3dla6ual3jsgpdr4cjr3oz3evfyavhwq.ipfs.dweb.link/wiki/Vincent_van_Gogh.html
        //
        // We don't use the `https://dweb.link/ipfs?uri=...` form because this causes an HTTP redirects and
        // this doesn't preserve the ipfs:// or ipns:// origin of the loaded document.

        let safe_host = if self.protocol == Protocol::Ipfs {
            // Try to convert 'host' into a CIDv1
            let cid = Cid::try_from(host.to_utf8().as_ref()).map_err(|_| NS_ERROR_FAILURE)?;
            // Same as Cid::to_string_v1() which is unfortunately private.
            multibase::encode(multibase::Base::Base32Lower, cid.to_bytes().as_slice())
        } else {
            // For ipns://, convert '.' to '-'
            host.to_utf8().replace('.', "-")
        };

        let gateway_url = format!(
            "https://{}.{}.{}{}",
            safe_host, scheme, self.gateway_domain, path_query
        );

        let io_service = xpcom::services::get_IOService().ok_or(NS_ERROR_FAILURE)?;
        unsafe {
            let final_uri: RefPtr<nsIURI> = getter_addrefs(|p| {
                io_service.NewURI(&*nsCString::from(gateway_url), ptr::null(), ptr::null(), p)
            })?;

            io_service.NewChannelFromURIWithLoadInfo(&*final_uri, load_info, retval);
            (*retval)
                .as_ref()
                .ok_or(NS_ERROR_FAILURE)?
                .SetOriginalURI(uri);
        }

        Ok(())
    }
}

#[no_mangle]
pub unsafe extern "C" fn ipfs_construct(result: &mut *const nsIProtocolHandler) {
    let inst = IpfsHandler::new(Protocol::Ipfs);
    *result = inst.coerce::<nsIProtocolHandler>();
    std::mem::forget(inst);
}

#[no_mangle]
pub unsafe extern "C" fn ipns_construct(result: &mut *const nsIProtocolHandler) {
    let inst = IpfsHandler::new(Protocol::Ipns);
    *result = inst.coerce::<nsIProtocolHandler>();
    std::mem::forget(inst);
}
