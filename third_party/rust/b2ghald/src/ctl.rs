use b2ghald::client::SimpleClient;
use b2ghald::humantime::FormattedDuration;
use clap::{Parser, Subcommand};

#[derive(Parser)]
#[clap(author, version, about, long_about = None)]
struct Cli {
    #[clap(subcommand)]
    command: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    /// Turn on the main screen
    EnableScreen,
    /// Turn off the main screen
    DisableScreen,
    /// Get or set the screen brightness
    Brightness {
        /// If set, change the screen brightness. Valid value is 0-100
        level: Option<u8>,
    },
    // Enables a device flashlight
    EnableFlashlight {
        // The path to the flashlight device, eg. /sys/class/leds/white:torch
        path: String,
    },
    // Disables a device flashlight
    DisableFlashlight {
        // The path to the flashlight device, eg. /sys/class/leds/white:torch
        path: String,
    },
    /// Reboots the device.
    Reboot {},
    /// Powers off the device.
    PowerOff {},
    /// Get or set the timezone.
    Timezone {
        /// If set, change the timezone.
        tz: Option<String>,
    },
    /// Get the device uptime
    Uptime,
    /// Restart a systemctl service
    RestartService {
        /// The service name
        service: String,
    },
    /// Stop a systemctl service
    StopService {
        /// The service name
        service: String,
    },
    /// Start a systemctl service
    StartService {
        /// The service name
        service: String,
    },
    /// Set the device time.
    SetTime {
        /// Milliseconds since epoch.
        ms: i64,
    },
}

fn check_flashlight(client: &mut SimpleClient, path: &str) -> bool {
    if client.is_flashlight_supported(path) {
        true
    } else {
        println!("No flashlight detected at {}", path);
        false
    }
}

fn main() {
    env_logger::init();

    let mut client = SimpleClient::new().expect("Failed to connect to b2ghald");

    let cli = Cli::parse();

    match &cli.command {
        Command::Brightness { level } => {
            if let Some(value) = level {
                client.set_screen_brightness(0, *value);
            } else {
                println!(
                    "Current brightness of default screen: {}",
                    client.get_screen_brightness(0)
                );
            }
        }
        Command::EnableScreen {} => client.enable_screen(0),
        Command::DisableScreen {} => client.disable_screen(0),
        Command::PowerOff {} => client.poweroff(),
        Command::Reboot {} => client.reboot(),
        Command::EnableFlashlight { path } => {
            if check_flashlight(&mut client, path) {
                client.enable_flashlight(path);
            }
        }
        Command::DisableFlashlight { path } => {
            if check_flashlight(&mut client, path) {
                client.disable_flashlight(path);
            }
        }
        Command::Timezone { tz } => {
            if let Some(value) = tz {
                client.set_timezone(value);
            } else {
                println!(
                    "Current timezone: {}",
                    client.get_timezone().unwrap_or_else(|| "<not set>".into())
                );
            }
        }
        Command::Uptime => {
            let uptime = client.get_uptime();
            println!(
                "Current uptime: {} ({}ms)",
                FormattedDuration::from_millis(uptime),
                client.get_uptime()
            );
        }
        Command::RestartService { service } => {
            client.control_service("restart", &service);
        }
        Command::StartService { service } => {
            client.control_service("start", &service);
        }
        Command::StopService { service } => {
            client.control_service("stop", &service);
        }
        Command::SetTime { ms } => {
            client.set_system_time(*ms);
        }
    }
}
