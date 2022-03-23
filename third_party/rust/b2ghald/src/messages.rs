use serde::{Deserialize, Serialize};

#[derive(Serialize, Deserialize, Debug)]
pub enum Request {
    SetBrightness((u8, u8)), // screen id, level.
    GetBrightness(u8),       // screen id.
    PowerOff,
    Reboot,
    EnableScreen(u8),              // screen id.
    DisableScreen(u8),             // screen id.
    IsFlashlightSupported(String), // The path to the flashlight, eg. /sys/class/leds/white:torch
    EnableFlashlight(String),      // The path to the flashlight
    DisableFlashlight(String),     // The path to the flashlight
    FlashlightState(String),       // The path to the flashlight
}

#[derive(Serialize, Deserialize, Clone, Debug)]
pub enum Response {
    SetBrightnessSuccess,
    SetBrightnessError,
    GetBrightnessSuccess((u8, u8)), // screen id, level.
    GetBrightnessError,
    GenericSuccess,
    GenericError,
    FlashlightSupported(bool),
    FlashlightState(bool),
}

#[derive(Serialize, Deserialize)]
pub struct ToDaemon(u64, Request);
impl ToDaemon {
    pub fn new(id: u64, req: Request) -> Self {
        Self(id, req)
    }

    pub fn request(&self) -> &Request {
        &self.1
    }

    pub fn id(&self) -> u64 {
        self.0
    }
}

#[derive(Serialize, Deserialize)]
pub struct FromDaemon(u64, Response);
impl FromDaemon {
    pub fn new(id: u64, resp: Response) -> Self {
        Self(id, resp)
    }

    pub fn response(&self) -> &Response {
        &self.1
    }

    pub fn id(&self) -> u64 {
        self.0
    }
}
