// Copyright 2017 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![deny(missing_docs)]
#![allow(dead_code)]

use std::path::Path;
use std::str;

use anyhow::bail;
use anyhow::Context;
use anyhow::Result;
#[cfg(feature = "seccomp_trace")]
use base::debug;
use base::getegid;
use base::geteuid;
#[cfg(feature = "seccomp_trace")]
use base::warn;
use libc::c_ulong;
use minijail::Minijail;
#[cfg(not(feature = "seccomp_trace"))]
use once_cell::sync::Lazy;
#[cfg(feature = "seccomp_trace")]
use static_assertions::assert_eq_size;
#[cfg(feature = "seccomp_trace")]
use zerocopy::AsBytes;

use crate::config::JailConfig;

#[cfg(not(feature = "seccomp_trace"))]
static EMBEDDED_BPFS: Lazy<std::collections::HashMap<&str, Vec<u8>>> =
    Lazy::new(|| include!(concat!(env!("OUT_DIR"), "/bpf_includes.in")));

/// Most devices don't need to open many fds.
pub const MAX_OPEN_FILES_DEFAULT: u64 = 1024;
/// The max open files for gpu processes.
const MAX_OPEN_FILES_FOR_GPU: u64 = 32768;
/// The max open files for jail warden, matching FD_RAW_FAILURE.
pub const MAX_OPEN_FILES_FOR_JAIL_WARDEN: u64 = 65536;

/// The user in the jail to run as.
pub enum RunAsUser {
    /// Do not specify the user
    Unspecified,
    /// Runs as the same user in the jail as the current user.
    CurrentUser,
    /// Runs as the root user in the jail.
    Root,
    /// Runs as the specified uid and gid.
    /// This requires `SandboxConfig::ugid_map` to be set.
    Specified(u32, u32),
}

/// Config for the sandbox to be created by [Minijail].
pub struct SandboxConfig<'a> {
    /// Whether or not to drop all capabilities in the sandbox.
    pub limit_caps: bool,
    log_failures: bool,
    seccomp_policy_dir: Option<&'a Path>,
    seccomp_policy_name: &'a str,
    /// The pair of `uid_map` and `gid_map`.
    pub ugid_map: Option<(&'a str, &'a str)>,
    /// The remount mode instead of default MS_PRIVATE.
    pub remount_mode: Option<c_ulong>,
    /// Whether to use empty net namespace. Enabled by default.
    pub namespace_net: bool,
    /// Whether or not to configure the jail to support bind-mounts.
    ///
    /// Note that most device processes deny `open(2)` and `openat(2)` by seccomp policy and just
    /// returns `ENOENT`. Passing opened file descriptors is recommended over opening files in the
    /// sandbox.
    pub bind_mounts: bool,
    /// Specify the user in the jail to run as.
    pub run_as: RunAsUser,
}

impl<'a> SandboxConfig<'a> {
    /// Creates [SandboxConfig].
    pub fn new(jail_config: &'a JailConfig, policy: &'a str) -> Self {
        Self {
            limit_caps: true,
            log_failures: jail_config.seccomp_log_failures,
            seccomp_policy_dir: jail_config.seccomp_policy_dir.as_ref().map(Path::new),
            seccomp_policy_name: policy,
            ugid_map: None,
            remount_mode: None,
            namespace_net: true,
            bind_mounts: false,
            run_as: RunAsUser::Unspecified,
        }
    }
}

/// Wrapper that cleans up a [Minijail] when it is dropped
pub struct ScopedMinijail(pub Minijail);

impl Drop for ScopedMinijail {
    fn drop(&mut self) {
        let _ = self.0.kill();
    }
}

/// Creates a [Minijail] instance which just changes the root using pivot_root(2) path and
/// `max_open_files` using `RLIMIT_NOFILE`.
///
/// If `root` path is "/", the minijail don't change the root.
///
/// # Arguments
///
/// * `root` - The root path to be changed to by minijail.
/// * `max_open_files` - The maximum number of file descriptors to allow a jailed process to open.
pub fn create_base_minijail(root: &Path, max_open_files: u64) -> Result<Minijail> {
    // Validate new root directory. Path::is_dir() also checks the existence.
    if !root.is_dir() {
        bail!("{:?} is not a directory, cannot create jail", root);
    }
    // chroot accepts absolute path only.
    if !root.is_absolute() {
        bail!("{:?} is not absolute path", root);
    }

    // All child jails run in a new user namespace without any users mapped, they run as nobody
    // unless otherwise configured.
    let mut jail = Minijail::new().context("failed to jail device")?;

    // Only pivot_root if we are not re-using the current root directory.
    if root != Path::new("/") {
        // It's safe to call `namespace_vfs` multiple times.
        jail.namespace_vfs();
        jail.enter_pivot_root(root)
            .context("failed to pivot root device")?;
    }

    jail.set_rlimit(libc::RLIMIT_NOFILE as i32, max_open_files, max_open_files)
        .context("error setting max open files")?;

    Ok(jail)
}

/// Creates a [Minijail] instance which creates a sandbox.
///
/// # Arguments
///
/// * `root` - The root path to be changed to by minijail.
/// * `max_open_files` - The maximum number of file descriptors to allow a jailed process to open.
/// * `config` - The [SandboxConfig] to control details of the sandbox.
pub fn create_sandbox_minijail(
    root: &Path,
    max_open_files: u64,
    config: &SandboxConfig,
) -> Result<Minijail> {
    let mut jail = create_base_minijail(root, max_open_files)?;

    jail.namespace_pids();
    jail.namespace_user();
    jail.namespace_user_disable_setgroups();
    if config.limit_caps {
        // Don't need any capabilities.
        jail.use_caps(0);
    }
    match config.run_as {
        RunAsUser::Unspecified => {
            if config.bind_mounts && config.ugid_map.is_none() {
                // Minijail requires to set user/group map to mount extra directories.
                add_current_user_to_jail(&mut jail)?;
            }
        }
        RunAsUser::CurrentUser => {
            add_current_user_to_jail(&mut jail)?;
        }
        RunAsUser::Root => {
            // Add the current user as root in the jail.
            let crosvm_uid = geteuid();
            let crosvm_gid = getegid();
            jail.uidmap(&format!("0 {} 1", crosvm_uid))
                .context("error setting UID map")?;
            jail.gidmap(&format!("0 {} 1", crosvm_gid))
                .context("error setting GID map")?;
        }
        RunAsUser::Specified(uid, gid) => {
            if uid != 0 {
                jail.change_uid(uid)
            }
            if gid != 0 {
                jail.change_gid(gid)
            }
        }
    }
    if config.bind_mounts {
        // Create a tmpfs in the device's root directory so that we can bind mount files.
        // The size=67108864 is size=64*1024*1024 or size=64MB.
        // TODO(b/267581374): Use appropriate size for tmpfs.
        jail.mount_with_data(
            Path::new("none"),
            Path::new("/"),
            "tmpfs",
            (libc::MS_NOSUID | libc::MS_NODEV | libc::MS_NOEXEC) as usize,
            "size=67108864",
        )?;
    }
    if let Some((uid_map, gid_map)) = config.ugid_map {
        jail.uidmap(uid_map).context("error setting UID map")?;
        jail.gidmap(gid_map).context("error setting GID map")?;
    }
    // Run in a new mount namespace.
    jail.namespace_vfs();

    if config.namespace_net {
        // Run in an empty network namespace.
        jail.namespace_net();
    }

    // Don't allow the device to gain new privileges.
    jail.no_new_privs();

    #[cfg(feature = "seccomp_trace")]
    {
        #[repr(C)]
        #[derive(AsBytes)]
        struct sock_filter {
            /* Filter block */
            code: u16, /* Actual filter code */
            jt: u8,    /* Jump true */
            jf: u8,    /* Jump false */
            k: u32,    /* Generic multiuse field */
        }

        // BPF constant is defined in https://elixir.bootlin.com/linux/latest/source/include/uapi/linux/bpf_common.h
        // BPF parser/assembler is defined in https://elixir.bootlin.com/linux/v4.9/source/tools/net/bpf_exp.y
        const SECCOMP_RET_TRACE: u32 = 0x7ff00000;
        const SECCOMP_RET_LOG: u32 = 0x7ffc0000;
        const BPF_RET: u16 = 0x06;
        const BPF_K: u16 = 0x00;

        // return SECCOMP_RET_LOG for all syscalls
        const FILTER_RET_LOG_BLOCK: sock_filter = sock_filter {
            code: BPF_RET | BPF_K,
            jt: 0,
            jf: 0,
            k: SECCOMP_RET_LOG,
        };

        warn!("The running crosvm is compiled with seccomp_trace feature, and is striclty used for debugging purpose only. DO NOT USE IN PRODUCTION!!!");
        debug!(
            "seccomp_trace {{\"event\": \"minijail_create\", \"name\": \"{}\", \"jail_addr\": \"0x{:x}\"}}",
            config.seccomp_policy_name,
            read_jail_addr(&jail),
        );
        jail.parse_seccomp_bytes(FILTER_RET_LOG_BLOCK.as_bytes())
            .unwrap();
    }

    #[cfg(not(feature = "seccomp_trace"))]
    if let Some(seccomp_policy_dir) = config.seccomp_policy_dir {
        let seccomp_policy_path = seccomp_policy_dir.join(config.seccomp_policy_name);
        // By default we'll prioritize using the pre-compiled .bpf over the .policy file (the .bpf
        // is expected to be compiled using "trap" as the failure behavior instead of the default
        // "kill" behavior) when a policy path is supplied in the command line arugments. Otherwise
        // the built-in pre-compiled policies will be used.
        // Refer to the code comment for the "seccomp-log-failures" command-line parameter for an
        // explanation about why the |log_failures| flag forces the use of .policy files (and the
        // build-time alternative to this run-time flag).
        let bpf_policy_file = seccomp_policy_path.with_extension("bpf");
        if bpf_policy_file.exists() && !config.log_failures {
            jail.parse_seccomp_program(&bpf_policy_file)
                .with_context(|| {
                    format!(
                        "failed to parse precompiled seccomp policy: {}",
                        bpf_policy_file.display()
                    )
                })?;
        } else {
            // Use TSYNC only for the side effect of it using SECCOMP_RET_TRAP, which will correctly
            // kill the entire device process if a worker thread commits a seccomp violation.
            jail.set_seccomp_filter_tsync();
            if config.log_failures {
                jail.log_seccomp_filter_failures();
            }
            let bpf_policy_file = seccomp_policy_path.with_extension("policy");
            jail.parse_seccomp_filters(&bpf_policy_file)
                .with_context(|| {
                    format!(
                        "failed to parse seccomp policy: {}",
                        bpf_policy_file.display()
                    )
                })?;
        }
    } else {
        let bpf_program = EMBEDDED_BPFS
            .get(&config.seccomp_policy_name)
            .with_context(|| {
                format!(
                    "failed to find embedded seccomp policy: {}",
                    &config.seccomp_policy_name
                )
            })?;
        jail.parse_seccomp_bytes(bpf_program).with_context(|| {
            format!(
                "failed to parse embedded seccomp policy: {}",
                &config.seccomp_policy_name
            )
        })?;
    }

    jail.use_seccomp_filter();
    // Don't do init setup.
    jail.run_as_init();
    // Set up requested remount mode instead of default MS_PRIVATE.
    if let Some(mode) = config.remount_mode {
        jail.set_remount_mode(mode);
    }

    Ok(jail)
}

/// Creates a basic [Minijail] if `jail_config` is present.
///
/// Returns `None` if `jail_config` is none.
pub fn simple_jail(jail_config: &Option<JailConfig>, policy: &str) -> Result<Option<Minijail>> {
    if let Some(jail_config) = jail_config {
        let config = SandboxConfig::new(jail_config, policy);
        Ok(Some(create_sandbox_minijail(
            &jail_config.pivot_root,
            MAX_OPEN_FILES_DEFAULT,
            &config,
        )?))
    } else {
        Ok(None)
    }
}

/// Creates [Minijail] for gpu processes.
pub fn create_gpu_minijail(
    root: &Path,
    config: &SandboxConfig,
    render_node_only: bool,
) -> Result<Minijail> {
    let mut jail = create_sandbox_minijail(root, MAX_OPEN_FILES_FOR_GPU, config)?;

    // Device nodes required for DRM.
    let sys_dev_char_path = Path::new("/sys/dev/char");
    jail.mount_bind(sys_dev_char_path, sys_dev_char_path, false)?;

    // Necessary for CGROUP control of the vGPU threads
    // This is not necessary UNLESS one wants to make use
    // of the gpu cgroup command line options.
    let sys_cpuset_path = Path::new("/sys/fs/cgroup/cpuset");
    if sys_cpuset_path.exists() {
        jail.mount_bind(sys_cpuset_path, sys_cpuset_path, true)?;
    }

    let sys_devices_path = Path::new("/sys/devices");
    jail.mount_bind(sys_devices_path, sys_devices_path, false)?;

    jail_mount_bind_drm(&mut jail, render_node_only)?;

    // If the ARM specific devices exist on the host, bind mount them in.
    let mali0_path = Path::new("/dev/mali0");
    if mali0_path.exists() {
        jail.mount_bind(mali0_path, mali0_path, true)?;
    }

    let pvr_sync_path = Path::new("/dev/pvr_sync");
    if pvr_sync_path.exists() {
        jail.mount_bind(pvr_sync_path, pvr_sync_path, true)?;
    }

    // If the udmabuf driver exists on the host, bind mount it in.
    let udmabuf_path = Path::new("/dev/udmabuf");
    if udmabuf_path.exists() {
        jail.mount_bind(udmabuf_path, udmabuf_path, true)?;
    }

    // Libraries that are required when mesa drivers are dynamically loaded.
    jail_mount_bind_if_exists(
        &mut jail,
        &[
            "/usr/lib",
            "/usr/lib64",
            "/lib",
            "/lib64",
            "/usr/share/drirc.d",
            "/usr/share/glvnd",
            "/usr/share/libdrm",
            "/usr/share/vulkan",
        ],
    )?;

    // pvr driver requires read access to /proc/self/task/*/comm.
    mount_proc(&mut jail)?;

    // To enable perfetto tracing, we need to give access to the perfetto service IPC
    // endpoints.
    let perfetto_path = Path::new("/run/perfetto");
    if perfetto_path.exists() {
        jail.mount_bind(perfetto_path, perfetto_path, true)?;
    }

    Ok(jail)
}

/// Selectively bind mount drm nodes into `jail` based on `render_node_only`
///
/// This function will not return an error if drm nodes don't exist
pub fn jail_mount_bind_drm(jail: &mut Minijail, render_node_only: bool) -> Result<()> {
    if render_node_only {
        const DRM_NUM_NODES: u32 = 63;
        const DRM_RENDER_NODE_START: u32 = 128;
        for offset in 0..DRM_NUM_NODES {
            let path_str = format!("/dev/dri/renderD{}", DRM_RENDER_NODE_START + offset);
            let drm_dri_path = Path::new(&path_str);
            if !drm_dri_path.exists() {
                break;
            }
            jail.mount_bind(drm_dri_path, drm_dri_path, false)?;
        }
    } else {
        let drm_dri_path = Path::new("/dev/dri");
        if drm_dri_path.exists() {
            jail.mount_bind(drm_dri_path, drm_dri_path, false)?;
        }
    }

    Ok(())
}

/// Mirror-mount all the directories in `dirs` into `jail` on a best-effort basis.
///
/// This function will not return an error if any of the directories in `dirs` is missing.
pub fn jail_mount_bind_if_exists<P: AsRef<std::ffi::OsStr>>(
    jail: &mut Minijail,
    dirs: &[P],
) -> Result<()> {
    for dir in dirs {
        let dir_path = Path::new(dir);
        if dir_path.exists() {
            jail.mount_bind(dir_path, dir_path, false)?;
        }
    }

    Ok(())
}

/// Mount proc in the sandbox.
pub fn mount_proc(jail: &mut Minijail) -> Result<()> {
    jail.mount(
        Path::new("proc"),
        Path::new("/proc"),
        "proc",
        (libc::MS_NOSUID | libc::MS_NODEV | libc::MS_NOEXEC | libc::MS_RDONLY) as usize,
    )?;
    Ok(())
}

/// Read minijail internal struct address for uniquely identifying and tracking jail's lifetime
#[cfg(feature = "seccomp_trace")]
pub fn read_jail_addr(jail: &Minijail) -> usize {
    // We can only hope minijail's rust object will always only contain a pointer to C jail struct
    assert_eq_size!(Minijail, usize);
    // Safe because it's only doing a read within bound checked by static assert
    unsafe { *(jail as *const Minijail as *const usize) }
}

/// Set the uid/gid for the jailed process and give a basic id map. This is
/// required for bind mounts to work.
fn add_current_user_to_jail(jail: &mut Minijail) -> Result<()> {
    let crosvm_uid = geteuid();
    let crosvm_gid = getegid();

    jail.uidmap(&format!("{0} {0} 1", crosvm_uid))
        .context("error setting UID map")?;
    jail.gidmap(&format!("{0} {0} 1", crosvm_gid))
        .context("error setting GID map")?;

    if crosvm_uid != 0 {
        jail.change_uid(crosvm_uid);
    }
    if crosvm_gid != 0 {
        jail.change_gid(crosvm_gid);
    }
    Ok(())
}
