use anyhow::Context;
use arch::VmComponents;
use argh::{FromArgs, SubCommands};
use base::syslog::{self, LogArgs, LogConfig};
use cros_async::sys::ExecutorKindSys;
use cros_async::{Executor, ExecutorKind};
use devices::virtio::block::DiskOption;
use devices::virtio::vhost::user::{device, run_vsock_device, VsockOptions};
use devices::virtio::P9;
use jail::JailConfig;
use libcrosvm::crosvm::cmdline::RunCommand;
use libcrosvm::crosvm::config::{self, Config};
use libcrosvm::crosvm::sys::cmdline::{Commands, DeviceSubcommand};
use libcrosvm::crosvm::sys::config::SharedDir;
use libcrosvm::crosvm::sys::linux::device_helpers::create_9p_device;
use libcrosvm::crosvm::sys::linux::{run_kvm, setup_vm_components};
use libcrosvm::crosvm::sys::HypervisorKind;
use libcrosvm::run_vm;
use libcrosvm::sys::{self, start_device};
use libcrosvm::sys::{init_log, run_config};
use serde::Serialize;
use std::collections::BTreeMap;
use std::path::{Path, PathBuf};

#[cfg(feature = "scudo")]
#[global_allocator]
static ALLOCATOR: scudo::GlobalScudoAllocator = scudo::GlobalScudoAllocator;

fn main() {
    let _guard = sentry::init(("https://743ef83df5b81f38da5b4122ed6a7a8f@o4506496905773056.ingest.us.sentry.io/4506955882233856", sentry::ClientOptions {
        release: sentry::release_name!(),
        ..Default::default()
      }));

    let e = Executor::new().unwrap();

    let mut cmd: RunCommand = RunCommand::from_args(
        &["run"],
        &[
            // crosvm run --rwdisk /home/sevki/src/okLinux/okLinuxFS --initrd /home/sevki/src/okLinux/initrd.img-6.1.0-9-amd64 -p "root=/dev/vda1" /home/sevki/src/okLinux/build/boot/vmlinux-6.6.21
            "okLinux.bzImage",
            "-p",
            "root=/dev/vda1,ro",
            // initrd
            "--initrd",
            "initrd.img",
            // "--rwdisk",
            // "./okLinuxFS",
            // "-p",
            
            // "./build/boot/vmlinux-6.6.21",
        ],
    )
    .unwrap();

    let mut serializer = serde_json::Serializer::pretty(std::io::stdout());

    let mut log_config = LogConfig {
        log_args: LogArgs {
            filter: "debug".to_string(),
            proc_name: "bldy".to_string(),
            syslog: true,
            ..Default::default()
        },

        ..Default::default()
    };

    let mut cfg = match TryInto::<Config>::try_into(cmd) {
        Ok(cfg) => cfg,
        Err(e) => {
            eprintln!("Failed to parse command line arguments: {}", e);
            std::process::exit(1);
        }
    };

    cfg.jail_config = None;

    let _ = init_log(log_config, &cfg);
    let components = setup_vm_components(&cfg).unwrap();

    let exit = run_kvm(Some(Path::new("/dev/kvm")), cfg, components);


    println!("exit: {:?}", exit);
}
