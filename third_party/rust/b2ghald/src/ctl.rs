use b2ghald::client::SimpleClient;
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
    }
}
