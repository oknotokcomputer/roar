// Copyright 2022 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cfg_if::cfg_if! {
    if #[cfg(any(target_os = "android", target_os = "linux"))] {
        pub mod linux;
        use linux as platform;
        pub use crate::crosvm::sys::linux::{run_config, ExitState};
    } else if #[cfg(windows)] {
        pub(crate) mod windows;
        use windows as platform;
        pub(crate) use windows::ExitState;
        pub(crate) use windows::run_config;
    } else {
        compile_error!("Unsupported platform");
    }
}

pub use platform::main::cleanup;
pub use platform::main::error_to_exit_code;
pub use platform::main::get_library_watcher;
pub use platform::main::init_log;
pub use platform::main::run_command;
#[cfg(feature = "sandbox")]
pub use platform::main::sandbox_lower_token;
pub use platform::main::start_device;
#[cfg(not(feature = "crash-report"))]
pub use platform::set_panic_hook;
