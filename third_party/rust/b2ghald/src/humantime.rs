/// Extract of the humantine crate time formatting.

use std::time::Duration;
use std::fmt;

pub struct FormattedDuration(Duration);

impl FormattedDuration {
    pub fn from_millis(ms: i64) -> Self {
        FormattedDuration(Duration::from_millis(ms as _))
    }
}

fn item_plural(f: &mut fmt::Formatter, started: &mut bool,
    name: &str, value: u64)
    -> fmt::Result
{
    if value > 0 {
        if *started {
            f.write_str(" ")?;
        }
        write!(f, "{}{}", value, name)?;
        if value > 1 {
            f.write_str("s")?;
        }
        *started = true;
    }
    Ok(())
}
fn item(f: &mut fmt::Formatter, started: &mut bool, name: &str, value: u32)
    -> fmt::Result
{
    if value > 0 {
        if *started {
            f.write_str(" ")?;
        }
        write!(f, "{}{}", value, name)?;
        *started = true;
    }
    Ok(())
}

impl fmt::Display for FormattedDuration {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let secs = self.0.as_secs();
        let nanos = self.0.subsec_nanos();

        if secs == 0 && nanos == 0 {
            f.write_str("0s")?;
            return Ok(());
        }

        let years = secs / 31_557_600; // 365.25d
        let ydays = secs % 31_557_600;
        let months = ydays / 2_630_016; // 30.44d
        let mdays = ydays % 2_630_016;
        let days = mdays / 86400;
        let day_secs = mdays % 86400;
        let hours = day_secs / 3600;
        let minutes = day_secs % 3600 / 60;
        let seconds = day_secs % 60;

        let millis = nanos / 1_000_000;
        let micros = nanos / 1000 % 1000;
        let nanosec = nanos % 1000;

        let ref mut started = false;
        item_plural(f, started, "year", years)?;
        item_plural(f, started, "month", months)?;
        item_plural(f, started, "day", days)?;
        item(f, started, "h", hours as u32)?;
        item(f, started, "m", minutes as u32)?;
        item(f, started, "s", seconds as u32)?;
        item(f, started, "ms", millis)?;
        item(f, started, "us", micros)?;
        item(f, started, "ns", nanosec)?;
        Ok(())
    }
}
