use nix::sys::time::TimeSpec;
/// Timezone & clock functions.
use nix::time::{clock_gettime, clock_settime, ClockId};
use std::process::Command;
use std::time::Duration;

pub enum TimezoneError {
    SetError,
    GetError,
}

pub enum SystemClockError {
    SetError,
    GetError,
}

pub struct Timezone {}

impl Timezone {
    pub fn set(tz: &str) -> Result<(), TimezoneError> {
        if let Ok(status) = Command::new("timedatectl")
            .arg("set-timezone")
            .arg(tz)
            .status()
        {
            if status.success() {
                return Ok(());
            } else {
                return Err(TimezoneError::SetError);
            }
        }
        Err(TimezoneError::SetError)
    }

    pub fn get() -> Result<String, TimezoneError> {
        if let Ok(output) = Command::new("timedatectl").arg("show").output() {
            let s = String::from_utf8_lossy(&output.stdout);
            for line in s.split('\n') {
                let parts: Vec<&str> = line.split('=').collect();
                if parts.len() == 2 && parts[0] == "Timezone" {
                    return Ok(parts[1].to_owned());
                }
            }
        }

        Err(TimezoneError::GetError)
    }
}

pub struct SystemClock {}

fn get_clock_id_ms(clock: ClockId) -> i64 {
    match clock_gettime(clock) {
        Ok(time) => time.tv_nsec() / 1_000_000 + time.tv_sec() * 1000,
        Err(_) => 0,
    }
}

impl SystemClock {
    pub fn set_time(ms: i64) -> Result<(), SystemClockError> {
        let time_spec = TimeSpec::from_duration(Duration::from_millis(ms as _));
        clock_settime(ClockId::CLOCK_REALTIME, time_spec).map_err(|_| SystemClockError::SetError)
    }

    pub fn get_time() -> i64 {
        get_clock_id_ms(ClockId::CLOCK_REALTIME)
    }

    pub fn get_uptime() -> i64 {
        get_clock_id_ms(ClockId::CLOCK_BOOTTIME)
    }
}
