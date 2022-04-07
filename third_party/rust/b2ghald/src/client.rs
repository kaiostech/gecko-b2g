/// Helper structs to build b2ghald clients.
use crate::messages::*;
use bincode::Options;
use log::{debug, error};
use std::collections::HashMap;
use std::io;
use std::io::{Error, ErrorKind};
use std::os::unix::net::UnixStream;
use std::sync::mpsc::{channel, Sender};

pub enum HalError {
    StreamError,
    NoListener,
}

pub struct HalClient {
    stream: UnixStream,
    req_id: u64,
    listeners: HashMap<u64, Sender<Response>>,
}

impl HalClient {
    pub fn connect(path: &str) -> Result<Self, io::Error> {
        match UnixStream::connect(path) {
            Ok(stream) => Ok(Self {
                stream,
                req_id: 0,
                listeners: HashMap::new(),
            }),
            Err(err) => {
                error!("Failed to connect to b2ghald at {}: {}", path, err);
                Err(err)
            }
        }
    }

    pub fn send(&mut self, request: Request, sender: Sender<Response>) -> Result<(), io::Error> {
        let id = self.req_id;
        self.req_id += 1;
        let message = ToDaemon::new(id, request);
        self.listeners.insert(id, sender);

        let config = bincode::DefaultOptions::new().with_native_endian();

        config
            .serialize_into(&self.stream, &message)
            .map_err(|_| Error::new(ErrorKind::Other, "bincode error"))?;

        Ok(())
    }

    // Blocks to get the next message, and dispatch it to the receiver.
    pub fn get_next_message(&mut self) -> Result<(), HalError> {
        let config = bincode::DefaultOptions::new().with_native_endian();
        if let Ok(message) = config.deserialize_from::<_, FromDaemon>(&self.stream) {
            if let Some(listener) = self.listeners.remove(&message.id()) {
                let _ = listener.send((*message.response()).clone());
            } else {
                error!("No listener registered for message #{}", message.id());
                return Err(HalError::NoListener);
            }
        } else {
            error!("Failed to deserialize messages.");
            return Err(HalError::StreamError);
        }

        Ok(())
    }
}

// A simple, blocking client.
pub struct SimpleClient {
    client: HalClient,
}

impl SimpleClient {
    pub fn new() -> Option<Self> {
        match HalClient::connect(crate::SOCKET_PATH) {
            Ok(client) => Some(Self { client }),
            Err(_) => None,
        }
    }

    pub fn set_screen_brightness(&mut self, screen_id: u8, value: u8) {
        let (sender, receiver) = channel();
        let _ = self
            .client
            .send(Request::SetBrightness((screen_id, value)), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn get_screen_brightness(&mut self, screen_id: u8) -> u8 {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::GetBrightness(screen_id), sender);
        if self.client.get_next_message().is_ok() {
            match receiver.recv() {
                Ok(Response::GetBrightnessSuccess(value)) => value.1,
                Ok(_) | Err(_) => 0,
            }
        } else {
            0
        }
    }

    pub fn enable_screen(&mut self, screen_id: u8) {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::EnableScreen(screen_id), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn disable_screen(&mut self, screen_id: u8) {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::DisableScreen(screen_id), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn reboot(&mut self) {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::Reboot, sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn poweroff(&mut self) {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::PowerOff, sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn enable_flashlight(&mut self, path: &str) {
        debug!("enable_flashlight {}", path);
        let (sender, receiver) = channel();
        let _ = self
            .client
            .send(Request::EnableFlashlight(path.to_owned()), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn disable_flashlight(&mut self, path: &str) {
        let (sender, receiver) = channel();
        let _ = self
            .client
            .send(Request::DisableFlashlight(path.to_owned()), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn is_flashlight_supported(&mut self, path: &str) -> bool {
        let (sender, receiver) = channel();
        let _ = self
            .client
            .send(Request::IsFlashlightSupported(path.to_owned()), sender);
        if self.client.get_next_message().is_ok() {
            match receiver.recv() {
                Ok(Response::FlashlightSupported(value)) => value,
                Ok(_) | Err(_) => false,
            }
        } else {
            false
        }
    }

    pub fn flashlight_state(&mut self, path: &str) -> bool {
        let (sender, receiver) = channel();
        let _ = self
            .client
            .send(Request::FlashlightState(path.to_owned()), sender);
        if self.client.get_next_message().is_ok() {
            match receiver.recv() {
                Ok(Response::FlashlightState(value)) => value,
                Ok(_) | Err(_) => false,
            }
        } else {
            false
        }
    }

    pub fn set_timezone(&mut self, tz: &str) {
        let (sender, receiver) = channel();
        let _ = self
            .client
            .send(Request::SetTimezone(tz.to_owned()), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn get_timezone(&mut self) -> Option<String> {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::GetTimezone, sender);
        if self.client.get_next_message().is_ok() {
            match receiver.recv() {
                Ok(Response::GetTimezone(value)) => Some(value),
                Ok(_) | Err(_) => None,
            }
        } else {
            None
        }
    }

    pub fn get_uptime(&mut self) -> i64 {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::GetUptime, sender);
        if self.client.get_next_message().is_ok() {
            match receiver.recv() {
                Ok(Response::GetUptime(value)) => value,
                Ok(_) | Err(_) => 0,
            }
        } else {
            0
        }
    }

    pub fn set_system_time(&mut self, ms: i64) {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::SetSystemClock(ms), sender);
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }

    pub fn get_system_time(&mut self) -> i64 {
        let (sender, receiver) = channel();
        let _ = self.client.send(Request::GetSystemClock, sender);
        if self.client.get_next_message().is_ok() {
            match receiver.recv() {
                Ok(Response::GetSystemClock(value)) => value,
                Ok(_) | Err(_) => 0,
            }
        } else {
            0
        }
    }

    pub fn control_service(&mut self, command: &str, service: &str) {
        let (sender, receiver) = channel();
        let _ = self.client.send(
            Request::ControlService(command.to_owned(), service.to_owned()),
            sender,
        );
        if self.client.get_next_message().is_ok() {
            let _ = receiver.recv();
        }
    }
}
