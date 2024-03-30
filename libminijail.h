/* Copyright 2012 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The general pattern of use here:
 * 1) Construct a minijail with minijail_new()
 * 2) Apply the desired restrictions to it
 * 3) Enter it, which locks the current process inside it, or:
 * 3) Run a process inside it
 * 4) Destroy it.
 */

#ifndef _LIBMINIJAIL_H_
#define _LIBMINIJAIL_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/types.h>

/*
 * Rust's bindgen needs the actual definition of sock_fprog in order to
 * generate usable bindings.
 */
#ifdef USE_BINDGEN
#include <linux/filter.h>
#endif

/*
 * The specified function arguments may not be NULL.  Params starts counting
 * from 1, not 0.  If no params are specified, then all function arguments are
 * marked as non-NULL.  Thus, params should only be specified if a function
 * accepts NULL pointers for any of the arguments.
 */
#define MINIJAIL_ATTRIBUTE_NONNULL(params) __attribute__((__nonnull__ params))

#ifdef __cplusplus
extern "C" {
#endif

/* Possible exit status codes returned by minijail_wait(). */
enum {
	/* Command can be found but cannot be run */
	MINIJAIL_ERR_NO_ACCESS = 126,

	/* Command cannot be found */
	MINIJAIL_ERR_NO_COMMAND = 127,

	/* (MINIJAIL_ERR_SIG_BASE + n) if process killed by signal n != SIGSYS
	 */
	MINIJAIL_ERR_SIG_BASE = 128,

	/* Cannot mount a file or folder in mount namespace */
	MINIJAIL_ERR_MOUNT = 251,

	MINIJAIL_ERR_PRELOAD = 252,

	/* Process killed by SIGSYS */
	MINIJAIL_ERR_JAIL = 253,

	MINIJAIL_ERR_INIT = 254,
};

struct minijail;
struct sock_fprog;

/*
 * A hook that can be used to execute code at various events during minijail
 * setup in the forked process. These can only be used if the jailed process is
 * not going to be invoked with LD_PRELOAD.
 *
 * If the return value is non-zero, it will be interpreted as -errno and the
 * process will abort.
 */
typedef int (*minijail_hook_t)(void *context);

/*
 * The events during minijail setup in which hooks can run. All the events are
 * run in the new process.
 */
typedef enum {
	/* The hook will run just before dropping capabilities. */
	MINIJAIL_HOOK_EVENT_PRE_DROP_CAPS,

	/* The hook will run just before calling execve(2). */
	MINIJAIL_HOOK_EVENT_PRE_EXECVE,

	/* The hook will run just before calling chroot(2) / pivot_root(2). */
	MINIJAIL_HOOK_EVENT_PRE_CHROOT,

	/* Sentinel for error checking. Must be last. */
	MINIJAIL_HOOK_EVENT_MAX,
} minijail_hook_event_t;

/* Allocates a new minijail with no restrictions. */
struct minijail *minijail_new(void);

/*
 * These functions add restrictions to the minijail. They are not applied until
 * minijail_enter() is called. See the documentation in minijail0.1 for
 * explanations in detail of what the restrictions do.
 */
void minijail_change_uid(struct minijail *j, uid_t uid)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_change_gid(struct minijail *j, gid_t gid)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* Copies |list|. */
void minijail_set_supplementary_gids(struct minijail *j, size_t size,
				     const gid_t *list)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_keep_supplementary_gids(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* Stores user to change to and copies |user| for internal consistency. */
int minijail_change_user(struct minijail *j, const char *user)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* Does not take ownership of |group|. */
int minijail_change_group(struct minijail *j, const char *group)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_use_seccomp(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_no_new_privs(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_use_seccomp_filter(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_set_seccomp_filter_tsync(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* Sets using_minimalistic_mountns to true. */
void minijail_set_using_minimalistic_mountns(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_set_enable_default_runtime(struct minijail *j,
					 bool enable_default_runtime)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_set_enable_new_sessions(struct minijail *j,
				      bool enable_new_sessions)
    MINIJAIL_ATTRIBUTE_NONNULL();
/*
 * Sets enable_profile_fs_restrictions to true.
 *
 * Exposed for unit tests and allowlisting services.
 */
bool minijail_get_enable_default_runtime(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_disable_fs_restrictions(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_set_enable_profile_fs_restrictions(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_add_minimalistic_mountns_fs_rules(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_enable_default_fs_restrictions(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
/*
 * Allow speculative execution features that may cause data leaks across
 * processes, by setting the SECCOMP_FILTER_FLAG_SPEC_ALLOW seccomp flag.
 *
 * WARNING: Enabling this may make the process vulnerable to speculative
 * execution attacks (Branch Target Injection, and Speculative Store Bypass).
 * This is only safe to use for processes that do not execute untrusted code.
 */
void minijail_set_seccomp_filter_allow_speculation(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* Does not take ownership of |filter|. */
void minijail_set_seccomp_filters(struct minijail *j,
				  const struct sock_fprog *filter)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_parse_seccomp_filters(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_parse_seccomp_filters_from_fd(struct minijail *j, int fd)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_log_seccomp_filter_failures(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* 'minijail_use_caps' and 'minijail_capbset_drop' are mutually exclusive. */
void minijail_use_caps(struct minijail *j, uint64_t capmask)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_capbset_drop(struct minijail *j, uint64_t capmask)
    MINIJAIL_ATTRIBUTE_NONNULL();
/* 'minijail_set_ambient_caps' requires 'minijail_use_caps'. */
void minijail_set_ambient_caps(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_reset_signal_mask(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_reset_signal_handlers(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_namespace_vfs(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_namespace_enter_vfs(struct minijail *j, const char *ns_path)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_new_session_keyring(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_skip_setting_securebits(struct minijail *j,
				      uint64_t securebits_skip_mask)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * This option is *dangerous* as it negates most of the functionality of
 * minijail_namespace_vfs(). You very likely don't need this.
 */
void minijail_skip_remount_private(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_remount_mode(struct minijail *j, unsigned long mode)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_namespace_ipc(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_namespace_uts(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
int minijail_namespace_set_hostname(struct minijail *j, const char *name)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Starts a new network namespace with or without a loopback interface.
 * It is rare for jails to not bring up the loopback interface, and having it
 * available is not a security concern. Please double check you actually need to
 * disable it before using this API.
 *
 * @j minijail to apply restriction to.
 * @enable_loopback indicates whether the loopback interface should be enabled
 * in this new network namespace.
 */
void minijail_namespace_net_loopback(struct minijail *j, bool enable_loopback)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Starts a new network namespace featuring a loopback interface.
 * This has the same effect as `minijail_namespace_net_loopback(j, true)`.
 */
void minijail_namespace_net(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

void minijail_namespace_enter_net(struct minijail *j, const char *ns_path)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_namespace_cgroups(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Closes all open file descriptors after forking. */
void minijail_close_open_fds(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Implies namespace_vfs and remount_proc_readonly.
 * WARNING: this is NOT THREAD SAFE. See the block comment in </libminijail.c>.
 */
void minijail_namespace_pids(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Implies namespace_vfs.
 * WARNING: this is NOT THREAD SAFE. See the block comment in </libminijail.c>.
 * Minijail will by default remount /proc read-only when using a PID namespace.
 * Certain complex applications expect to be able to do their own sandboxing
 * which might require writing to /proc, so support a weaker version of PID
 * namespacing with a RW /proc.
 */
void minijail_namespace_pids_rw_proc(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();

void minijail_namespace_user(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_namespace_user_disable_setgroups(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
int minijail_uidmap(struct minijail *j, const char *uidmap)
    MINIJAIL_ATTRIBUTE_NONNULL();
int minijail_gidmap(struct minijail *j, const char *gidmap)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_remount_proc_readonly(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_run_as_init(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();
int minijail_write_pid_file(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();
void minijail_inherit_usergroups(struct minijail *j)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Changes the jailed process's syscall table to the alt_syscall table
 * named |table|.
 */
int minijail_use_alt_syscall(struct minijail *j, const char *table)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Sets the given runtime limit. See getrlimit(2). */
int minijail_rlimit(struct minijail *j, int type, rlim_t cur, rlim_t max)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Adds the jailed process to the cgroup given by |path|.  |path| should be the
 * full path to the cgroups "tasks" file.
 * Example: /sys/fs/cgroup/cpu/jailed_procs/tasks adds to the "jailed_procs" cpu
 * cgroup.
 */
int minijail_add_to_cgroup(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * These functions are used for filesystem restrictions.
 */

/*
 * Indicates whether minijail_add_fs_restriction_* functions are available. Even
 * if they are unavailable, minijail_add_fs_restriction_* functions would still
 * succeed, however without any actual effects.
 */
bool minijail_is_fs_restriction_available(void);

/* Adds a read-execute path. */
int minijail_add_fs_restriction_rx(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Adds a read-only path. */
int minijail_add_fs_restriction_ro(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Adds a path with read and basic write permissions. */
int minijail_add_fs_restriction_rw(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Adds a path with read and advanced write permissions. */
int minijail_add_fs_restriction_advanced_rw(struct minijail *j,
					    const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Adds a path with read and write permissions that exclude create. */
int minijail_add_fs_restriction_edit(struct minijail *j, const char *path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/* Adds a path with permissions based on a bitmask of Landlock flags. */
int minijail_add_fs_restriction_access_rights(struct minijail *j,
					      const char *path,
					      uint16_t landlock_flags)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Indicates whether a filesystem restriction ruleset has been initialized.
 *
 * Exposed primarily for testing.
 *
 * This only indicates initialization status, not sandboxing status.
 */
bool minijail_is_fs_restriction_ruleset_initialized(const struct minijail *j);

/*
 * Install signal handlers in the minijail process that forward received
 * signals to the jailed child process.
 */
int minijail_forward_signals(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/* The jailed child process should call setsid() to create a new session. */
int minijail_create_session(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_enter_chroot: enables chroot() restriction for @j
 * @j   minijail to apply restriction to
 * @dir directory to chroot() to. Owned by caller.
 *
 * Enters @dir, binding all bind mounts specified with minijail_bind() into
 * place. Requires @dir to contain all necessary directories for bind mounts
 * (i.e., if you have requested a bind mount at /etc, /etc must exist in @dir.)
 *
 * Returns 0 on success.
 */
int minijail_enter_chroot(struct minijail *j, const char *dir)
    MINIJAIL_ATTRIBUTE_NONNULL();
int minijail_enter_pivot_root(struct minijail *j, const char *dir)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_get_original_path: returns the path of a given file outside of the
 * chroot.
 * @j           minijail to obtain the path from.
 * @chroot_path path inside of the chroot() to.
 *
 * When executing a binary in a chroot or pivot_root, return path to the binary
 * outside of the chroot.
 *
 * Returns a string containing the path.  This must be freed by the caller.
 */
char *minijail_get_original_path(struct minijail *j, const char *chroot_path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_mount_tmp: enables mounting of a 64M tmpfs filesystem on /tmp.
 * As be rules of bind mounts, /tmp must exist in chroot.
 */
void minijail_mount_tmp(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_mount_tmp_size: enables mounting of a tmpfs filesystem on /tmp.
 * As be rules of bind mounts, /tmp must exist in chroot.  Size is in bytes.
 */
void minijail_mount_tmp_size(struct minijail *j, size_t size)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_mount_dev: enables mounting of a tmpfs filesystem on /dev.
 * It will then be seeded with a basic set of device nodes.  For the exact
 * list, consult the minijail(0) man page.
 */
void minijail_mount_dev(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_mount_with_data: when entering minijail @j,
 *   mounts @src at @dst with @flags and @data.
 * @j         minijail to bind inside
 * @src       source to bind
 * @dest      location to bind (inside chroot)
 * @type      type of filesystem
 * @flags     flags passed to mount
 * @data      data arguments passed to mount(2), e.g. "mode=755"
 *
 * This may be called multiple times; all mounts will be applied in the order
 * of minijail_mount() calls.
 * If @flags is 0, then MS_NODEV | MS_NOEXEC | MS_NOSUID will be used instead.
 * If @data is NULL or "", and @type is tmpfs, then "mode=0755,size=10M" will
 * be used instead.
 */
int minijail_mount_with_data(struct minijail *j, const char *src,
			     const char *dest, const char *type,
			     unsigned long flags, const char *data)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3, 4));

/*
 * minijail_mount: when entering minijail @j, mounts @src at @dst with @flags
 * @j         minijail to bind inside
 * @src       source to bind
 * @dest      location to bind (inside chroot)
 * @type      type of filesystem
 * @flags     flags passed to mount
 *
 * This may be called multiple times; all mounts will be applied in the order
 * of minijail_mount() calls.
 */
int minijail_mount(struct minijail *j, const char *src, const char *dest,
		   const char *type, unsigned long flags)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_bind: bind-mounts @src into @j as @dest, optionally writeable
 * @j         minijail to bind inside
 * @src       source to bind
 * @dest      location to bind (inside chroot)
 * @writeable 1 if the bind mount should be writeable
 *
 * This may be called multiple times; all bindings will be applied in the order
 * of minijail_bind() calls.
 */
int minijail_bind(struct minijail *j, const char *src, const char *dest,
		  int writeable) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_add_remount: when entering minijail @j, remounts @mount_name and all
 * subdirectories as @remount_mode rather than the default MS_PRIVATE
 * @j             minijail to bind inside
 * @mount_name    mount to remount
 * @remount_mode  remount mode to use
 *
 * This may be called multiple times; this overrides |j->remount_mode| for the
 * given mount.
 */
int minijail_add_remount(struct minijail *j, const char *mount_name,
			 unsigned long remount_mode)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_add_hook: adds @hook to the list of hooks that will be
 * invoked when @event is reached during minijail setup. The caller is
 * responsible for the lifetime of @payload.
 * @j         minijail to add the hook to
 * @hook      the function that will be invoked
 * @payload   an opaque pointer
 * @event     the event that will trigger the hook
 */
int minijail_add_hook(struct minijail *j, minijail_hook_t hook, void *payload,
		      minijail_hook_event_t event)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2));

/*
 * minijail_preserve_fd: preserves @parent_fd and makes it available as
 * @child_fd in the child process. @parent_fd will be closed if no other
 * redirect has claimed it as a @child_fd.  This works even if
 * minijail_close_open_fds() is invoked.
 * @j         minijail to add the fd to
 * @parent_fd the fd in the parent process
 * @child_fd  the fd that will be available in the child process
 */
int minijail_preserve_fd(struct minijail *j, int parent_fd, int child_fd)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_set_preload_path: overrides the default path for
 * libminijailpreload.so.
 */
int minijail_set_preload_path(struct minijail *j, const char *preload_path)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Lock this process into the given minijail. Note that this procedure cannot
 * fail, since there is no way to undo privilege-dropping; therefore, if any
 * part of the privilege-drop fails, minijail_enter() will abort the entire
 * process.
 *
 * Some restrictions cannot be enabled this way (pid namespaces) and attempting
 * to do so will cause an abort.
 */
void minijail_enter(const struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Pass |envp| as the full environment for the child.
 */
int minijail_run_env(struct minijail *j, const char *filename,
		     char *const argv[], char *const envp[])
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * If minijail_namespace_pids() or minijail_namespace_user() are used,
 * this or minijail_fork() is required instead of minijail_enter().
 */
int minijail_run(struct minijail *j, const char *filename, char *const argv[])
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Don't use LD_PRELOAD to do privilege dropping. This is useful when sandboxing
 * static binaries, or on systems without support for LD_PRELOAD.
 */
int minijail_run_no_preload(struct minijail *j, const char *filename,
			    char *const argv[]) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Update |*pchild_pid| with the pid of the child.
 */
int minijail_run_pid(struct minijail *j, const char *filename,
		     char *const argv[], pid_t *pchild_pid)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3));

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Update |*pstdin_fd| with a fd that allows writing to the child's
 * standard input.
 */
int minijail_run_pipe(struct minijail *j, const char *filename,
		      char *const argv[], int *pstdin_fd)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3));

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Update |*pchild_pid| with the pid of the child.
 * Update |*pstdin_fd| with a fd that allows writing to the child's
 * standard input.
 * Update |*pstdout_fd| with a fd that allows reading from the child's
 * standard output.
 * Update |*pstderr_fd| with a fd that allows reading from the child's
 * standard error.
 */
int minijail_run_pid_pipes(struct minijail *j, const char *filename,
			   char *const argv[], pid_t *pchild_pid,
			   int *pstdin_fd, int *pstdout_fd, int *pstderr_fd)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3));

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Pass |envp| as the full environment for the child.
 * Update |*pchild_pid| with the pid of the child.
 * Update |*pstdin_fd| with a fd that allows writing to the child's
 * standard input.
 * Update |*pstdout_fd| with a fd that allows reading from the child's
 * standard output.
 * Update |*pstderr_fd| with a fd that allows reading from the child's
 * standard error.
 */
int minijail_run_env_pid_pipes(struct minijail *j, const char *filename,
			       char *const argv[], char *const envp[],
			       pid_t *pchild_pid, int *pstdin_fd,
			       int *pstdout_fd, int *pstderr_fd)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3));

/*
 * Execute the specified file descriptor in the given minijail,
 * fexecve(3)-style.
 * Pass |envp| as the full environment for the child or NULL to inherit.
 * Update |*pchild_pid| with the pid of the child.
 * Update |*pstdin_fd| with a fd that allows writing to the child's
 * standard input.
 * Update |*pstdout_fd| with a fd that allows reading from the child's
 * standard output.
 * Update |*pstderr_fd| with a fd that allows reading from the child's
 * standard error.
 */
int minijail_run_fd_env_pid_pipes(struct minijail *j, int elf_fd,
				  char *const argv[], char *const envp[],
				  pid_t *pchild_pid, int *pstdin_fd,
				  int *pstdout_fd, int *pstderr_fd)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 3));

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Update |*pchild_pid| with the pid of the child.
 * Update |*pstdin_fd| with a fd that allows writing to the child's
 * standard input.
 * Update |*pstdout_fd| with a fd that allows reading from the child's
 * standard output.
 * Update |*pstderr_fd| with a fd that allows reading from the child's
 * standard error.
 * Don't use LD_PRELOAD to do privilege dropping. This is useful when sandboxing
 * static binaries, or on systems without support for LD_PRELOAD.
 */
int minijail_run_pid_pipes_no_preload(struct minijail *j, const char *filename,
				      char *const argv[], pid_t *pchild_pid,
				      int *pstdin_fd, int *pstdout_fd,
				      int *pstderr_fd)
    MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3));

/*
 * Run the specified command in the given minijail, execve(2)-style.
 * Pass |envp| as the full environment for the child.
 * Update |*pchild_pid| with the pid of the child.
 * Update |*pstdin_fd| with a fd that allows writing to the child's
 * standard input.
 * Update |*pstdout_fd| with a fd that allows reading from the child's
 * standard output.
 * Update |*pstderr_fd| with a fd that allows reading from the child's
 * standard error.
 * Don't use LD_PRELOAD to do privilege dropping. This is useful when sandboxing
 * static binaries, or on systems without support for LD_PRELOAD.
 */
int minijail_run_env_pid_pipes_no_preload(
    struct minijail *j, const char *filename, char *const argv[],
    char *const envp[], pid_t *pchild_pid, int *pstdin_fd, int *pstdout_fd,
    int *pstderr_fd) MINIJAIL_ATTRIBUTE_NONNULL((1, 2, 3));

/*
 * Fork, jail the child, and return. This behaves similar to fork(2), except it
 * puts the child process in a jail before returning.
 * `minijail_fork` returns in both the parent and the child. The pid of the
 * child is returned to the parent. Zero is returned in the child. LD_PRELOAD
 * is not supported.
 * If minijail_namespace_pids() or minijail_namespace_user() are used,
 * this or minijail_run*() is required instead of minijail_enter().
 */
pid_t minijail_fork(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Send SIGTERM to the process in the minijail and wait for it to terminate.
 *
 * Return the same nonnegative exit status as minijail_wait(), or a negative
 * error code (eg -ESRCH if the process has already been waited for).
 *
 * This is most useful if the minijail has been created with PID namespacing
 * since, in this case, all processes inside it are atomically killed.
 */
int minijail_kill(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Wait for the first process spawned in the specified minijail to exit, and
 * return its exit status. A process can only be waited once.
 *
 * Return:
 *   A negative error code if the process cannot be waited for (eg -ECHILD if no
 *   process has been started or if the process has already been waited for).
 *   MINIJAIL_ERR_NO_COMMAND if command cannot be found.
 *   MINIJAIL_ERR_NO_ACCESS if command cannot be run.
 *   MINIJAIL_ERR_JAIL if process was killed by SIGSYS.
 *   (MINIJAIL_ERR_SIG_BASE  + n) if process was killed by signal n != SIGSYS.
 *   (n & 0xFF) if process finished by returning code n.
 */
int minijail_wait(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Frees the given minijail. It does not matter if the process is inside the
 * minijail or not. It will not kill the process, see minijail_kill() if that is
 * desired.
 */
void minijail_destroy(struct minijail *j) MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * Deep copies the minijail in |from| to |out| providing two identical jails
 * that can be used to contain separate children created with minijail_fork().
 *
 * Duplicating a jail is invalid after a jail has been passed to
 * minijail_fork(). Many minijail_*() calls will yield undefined
 * results when called on a jail duplicated post-fork.
 */
int minijail_copy_jail(const struct minijail *from, struct minijail *out)
    MINIJAIL_ATTRIBUTE_NONNULL();

/*
 * minijail_log_to_fd: redirects the module-wide logging to an FD instead of
 * syslog.
 * @fd           FD to log to. Caller must ensure this is available after
 *               jailing (e.g. with minijail_preserve_fd()).
 * @min_priority the minimum logging priority. Same as the priority argument
 *               to syslog(2).
 */
void minijail_log_to_fd(int fd, int min_priority);

/*
 * minijail_syscall_name: Returns the name of the provided system call on the
 * current architecture. This is exposed to make tasks like including the
 * system call name in crash reports possible. Note this does not support
 * alt-syscall or other supported ABIs such as x86 (32-bit) when this is built
 * for x86_64 (64-bit).
 * @j            Optional minijail struct to check if features that affect
 *               syscall tables. If NULL, the current syscall ABI is used.
 * @nr           The system call number.
 *
 * Returns the system call name if found otherwise NULL.
 */
const char *minijail_syscall_name(const struct minijail *j, long nr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !_LIBMINIJAIL_H_ */
