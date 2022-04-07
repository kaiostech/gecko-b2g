use b2ghald::backlight::Backlight;
use b2ghald::messages::*;
use b2ghald::time::{SystemClock, Timezone};
use bincode::Options;
use log::{debug, error, info};
use nix::sys::reboot::{reboot, RebootMode};
use nix::sys::stat::{umask, Mode};
use std::io::{Error, ErrorKind};
use std::os::unix::net::{UnixListener, UnixStream};
use std::thread;

fn flash_helper(path: &str, enabled: bool) -> Response {
    match Backlight::from_path(path) {
        Ok(flash) => {
            if flash
                .internal_set_brightness(0, if enabled { 100 } else { 0 })
                .is_ok()
            {
                return Response::GenericSuccess;
            } else {
                return Response::GenericError;
            }
        }
        Err(err) => {
            error!("Failed to create device for {} : {:?}", path, err);
        }
    }
    Response::GenericError
}

fn control_service(command: &str, service: &str) -> Result<(), Error> {
    match std::process::Command::new("systemctl")
        .arg(command)
        .arg(service)
        .status()
    {
        Ok(exit) => {
            if exit.code() == Some(0) {
                Ok(())
            } else {
                Err(Error::new(ErrorKind::Other, "systemctl error"))
            }
        }
        Err(err) => Err(err),
    }
}

// Manages a session with a client.
fn handle_client(stream: UnixStream) -> Result<(), Error> {
    let config = bincode::DefaultOptions::new().with_native_endian();

    let backlight = Backlight::find("/sys/class/backlight");
    if let Ok(ref device) = backlight {
        info!("Backlight available at {:?}", device);
    }

    loop {
        match config.deserialize_from::<_, ToDaemon>(&stream) {
            Ok(message) => {
                let sock_copy = stream.try_clone().expect("Couldn't clone socket");

                macro_rules! send {
                    ($payload:expr) => {
                        let response = FromDaemon::new(message.id(), $payload);
                        config
                            .serialize_into(sock_copy, &response)
                            .map_err(|_| Error::new(ErrorKind::Other, "bincode error"))?
                    };
                }

                match message.request() {
                    Request::GetBrightness(screen_id) => {
                        info!("GetBrightness #{}", message.id());
                        let payload = match backlight {
                            Ok(ref device) => Response::GetBrightnessSuccess((
                                *screen_id,
                                device.get_brightness(*screen_id),
                            )),
                            Err(_) => Response::GetBrightnessError,
                        };
                        send!(payload);
                    }
                    Request::SetBrightness((screen_id, level)) => {
                        info!(
                            "SetBrightness {} on screen {} #{}",
                            level,
                            screen_id,
                            message.id()
                        );
                        let payload = match backlight {
                            Ok(ref device) => {
                                device.set_brightness(*screen_id, *level);
                                Response::SetBrightnessSuccess
                            }
                            Err(_) => Response::SetBrightnessError,
                        };
                        send!(payload);
                    }
                    Request::PowerOff => {
                        let _ = reboot(RebootMode::RB_POWER_OFF);
                        send!(Response::GenericSuccess);
                    }
                    Request::Reboot => {
                        let _ = reboot(RebootMode::RB_AUTOBOOT);
                        send!(Response::GenericSuccess);
                    }
                    Request::EnableScreen(value) => {
                        let payload = match backlight {
                            Ok(ref device) => {
                                device.enable_screen(*value);
                                Response::GenericSuccess
                            }
                            Err(_) => Response::GenericError,
                        };
                        send!(payload);
                    }
                    Request::DisableScreen(value) => {
                        let payload = match backlight {
                            Ok(ref device) => {
                                device.disable_screen(*value);
                                Response::GenericSuccess
                            }
                            Err(_) => Response::GenericError,
                        };
                        send!(payload);
                    }
                    Request::EnableFlashlight(path) => {
                        debug!("EnableFlashlight {}", path);
                        send!(flash_helper(path, true));
                    }
                    Request::DisableFlashlight(path) => {
                        send!(flash_helper(path, false));
                    }
                    Request::IsFlashlightSupported(path) => {
                        let supported = Backlight::from_path(path).is_ok();
                        send!(Response::FlashlightSupported(supported));
                    }
                    Request::FlashlightState(path) => {
                        let payload = if let Ok(device) = Backlight::from_path(path) {
                            Response::FlashlightState(device.get_brightness(0) != 0)
                        } else {
                            Response::GenericError
                        };
                        send!(payload);
                    }
                    Request::SetTimezone(tz) => {
                        let payload = if Timezone::set(tz).is_ok() {
                            Response::GenericSuccess
                        } else {
                            Response::GenericError
                        };
                        send!(payload);
                    }
                    Request::GetTimezone => {
                        let payload = match Timezone::get() {
                            Ok(tz) => Response::GetTimezone(tz),
                            Err(_) => Response::GenericError,
                        };
                        send!(payload);
                    }
                    Request::SetSystemClock(ms) => {
                        let payload = if SystemClock::set_time(*ms).is_ok() {
                            Response::GenericSuccess
                        } else {
                            Response::GenericError
                        };
                        send!(payload);
                    }
                    Request::GetSystemClock => {
                        send!(Response::GetSystemClock(SystemClock::get_time()));
                    }
                    Request::GetUptime => {
                        send!(Response::GetUptime(SystemClock::get_uptime()));
                    }
                    Request::ControlService(command, service) => {
                        let payload = if control_service(command, service).is_ok() {
                            Response::GenericSuccess
                        } else {
                            Response::GenericError
                        };
                        send!(payload);
                    }
                }
            }
            Err(err) => {
                match *err {
                    bincode::ErrorKind::Io(io_err) => {
                        info!("Client connection was closed: {}", io_err)
                    }
                    err => error!("Decoding error: {}", err),
                }
                break;
            }
        }
    }
    Ok(())
}

fn main() -> std::io::Result<()> {
    env_logger::init();

    // Unix sockets inherits the permissions of the parent directory,
    // so we create it as drwxrwxrwx
    let mask = umask(Mode::empty());
    std::fs::DirBuilder::new()
        .recursive(true)
        .create("/tmp/b2g")?;

    let _ = std::fs::remove_file(b2ghald::SOCKET_PATH);
    let listener = UnixListener::bind(b2ghald::SOCKET_PATH)?;

    // Reset the umask for other operations in this process.
    umask(mask);

    // accept connections and process them, spawning a new thread for each one
    for stream in listener.incoming() {
        match stream {
            Ok(stream) => {
                thread::spawn(|| handle_client(stream));
            }
            Err(err) => {
                error!("Failed to get incoming client: {}", err);
                break;
            }
        }
    }
    Ok(())
}
