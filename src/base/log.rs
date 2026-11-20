use std::ffi::CString;
use std::os::raw::c_char;

/// Installs a log handler for Rust that `log_log`.
#[unsafe(no_mangle)]
pub extern "C" fn rust_log_use_log_log() {
    log::set_logger(&LoggerLogLog).expect("rust_log_use_log_log must only be called once");
    log::set_max_level(log::LevelFilter::Trace);
}

struct LoggerLogLog;

const LEVEL_ERROR: c_char = 0;
const LEVEL_WARN: c_char = 1;
const LEVEL_INFO: c_char = 2;
const LEVEL_DEBUG: c_char = 3;
const LEVEL_TRACE: c_char = 4;

fn log_log_level(level: log::Level) -> c_char {
    use log::Level::*;
    match level {
        Error => LEVEL_ERROR,
        Warn => LEVEL_WARN,
        Info => LEVEL_INFO,
        Debug => LEVEL_DEBUG,
        Trace => LEVEL_TRACE,
    }
}

extern "C" {
    fn log_log(level: c_char, sys: *const c_char, fmt: *const c_char, ...);
}

impl log::Log for LoggerLogLog {
    fn enabled(&self, _: &log::Metadata) -> bool {
        true
    }
    fn log(&self, record: &log::Record) {
        let target = CString::new(record.target()).unwrap();
        let message = CString::new(record.args().to_string()).unwrap();
        unsafe {
            log_log(
                log_log_level(record.level()),
                target.as_ptr(),
                b"%s\0".as_ptr() as *const _,
                message.as_ptr(),
            );
        }
    }
    fn flush(&self) {
        unimplemented!("use GlobalFinish() on the logger instance instead");
    }
}
