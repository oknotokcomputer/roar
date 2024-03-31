use argh::FromArgs;
use base::syslog::{LogArgs, LogConfig};
use libcrosvm::crosvm::sys::config::SharedDirKind;

use cros_async::Executor;

use libcrosvm::crosvm::cmdline::RunCommand;
use libcrosvm::crosvm::config::Config;

use libcrosvm::crosvm::sys::linux::{run_kvm, setup_vm_components};

use libcrosvm::sys::init_log;

use std::path::Path;

use std::collections::BTreeMap;

#[cfg(feature = "scudo")]
#[global_allocator]
static ALLOCATOR: scudo::GlobalScudoAllocator = scudo::GlobalScudoAllocator;

fn main() {
    let _guard = sentry::init(("https://743ef83df5b81f38da5b4122ed6a7a8f@o4506496905773056.ingest.us.sentry.io/4506955882233856", sentry::ClientOptions {
        release: sentry::release_name!(),
        ..Default::default()
      }));

    let _e = Executor::new().unwrap();

    let cmd: RunCommand = RunCommand::from_args(
        &["run"],
        &[
            // crosvm run --rwdisk /home/sevki/src/okLinux/okLinuxFS --initrd /home/sevki/src/okLinux/initrd.img-6.1.0-9-amd64 -p "root=/dev/vda1" /home/sevki/src/okLinux/build/boot/vmlinux-6.6.21
            // initrd
            "--initrd",
            "/workspaces/roar/okLinux.initramfs.x86_64.cpio",
            // "--rwdisk",
            // "./okLinuxFS",
            // "-p",
            "/workspaces/roar/okLinux.bzImage",
            // "-p", "rootfstype=ext4 root=/dev/vda1",
            // "-p", "rw",
            // "-p", "init=/bin/init",
            // "./build/boot/vmlinux-6.6.21",
        ],
    )
    .unwrap();

    let _serializer = serde_json::Serializer::pretty(std::io::stdout());

    let log_config = LogConfig {
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

    let path: Box<Path> = Box::from(Path::new("/"));

    // Define the shared directory and tag
    let shared_dir = libcrosvm::crosvm::config::SharedDir {
        tag: "shared".to_string(),
        src: "/tmp".into(),
        kind: SharedDirKind::P9,
        p9_cfg: p9::Config {
            root: path,
            msize: 0,
            uid_map: BTreeMap::new(),
            gid_map: BTreeMap::new(),
            ascii_casefold: false,
        },

        ..Default::default()
    };

    // Assuming Config has a field for shared directories
    cfg.shared_dirs.push(shared_dir);
    cfg.jail_config = None;

    //  pretty print the json
    // serde_json::to_writer_pretty(std::io::stdout(), &json!(cfg)).unwrap();
    let _ = init_log(log_config, &cfg);
    let components = setup_vm_components(&cfg).unwrap();

    let exit = run_kvm(Some(Path::new("/dev/kvm")), cfg, components);

    println!("exit: {:?}", exit);
}
