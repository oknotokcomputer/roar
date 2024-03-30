// Copyright (C) 2019 Alibaba Cloud. All rights reserved.
// SPDX-License-Identifier: Apache-2.0 or BSD-3-Clause

//! Virtio Vhost Backend Drivers
//!
//! Virtio devices use virtqueues to transport data efficiently. The first generation of virtqueue
//! is a set of three different single-producer, single-consumer ring structures designed to store
//! generic scatter-gather I/O. The virtio specification 1.1 introduces an alternative compact
//! virtqueue layout named "Packed Virtqueue", which is more friendly to memory cache system and
//! hardware implemented virtio devices. The packed virtqueue uses read-write memory, that means
//! the memory will be both read and written by both host and guest. The new Packed Virtqueue is
//! preferred for performance.
//!
//! Vhost is a mechanism to improve performance of Virtio devices by delegate data plane operations
//! to dedicated IO service processes. Only the configuration, I/O submission notification, and I/O
//! completion interruption are piped through the hypervisor.
//! It uses the same virtqueue layout as Virtio to allow Vhost devices to be mapped directly to
//! Virtio devices. This allows a Vhost device to be accessed directly by a guest OS inside a
//! hypervisor process with an existing Virtio (PCI) driver.
//!
//! The initial vhost implementation is a part of the Linux kernel and uses ioctl interface to
//! communicate with userspace applications. Dedicated kernel worker threads are created to handle
//! IO requests from the guest.
//!
//! Later Vhost-user protocol is introduced to complement the ioctl interface used to control the
//! vhost implementation in the Linux kernel. It implements the control plane needed to establish
//! virtqueues sharing with a user space process on the same host. It uses communication over a
//! Unix domain socket to share file descriptors in the ancillary data of the message. The protocol
//! defines 2 sides of the communication, frontend and backend. Frontend is the application that
//! shares its virtqueues. Backend is the consumer of the virtqueues. Frontend and backend can be
//! either a client (i.e. connecting) or server (listening) in the socket communication.

use std::fs::File;
use std::io::Error as IOError;
use std::num::TryFromIntError;

use remain::sorted;
use thiserror::Error as ThisError;

mod backend;
pub use backend::*;

pub mod message;
pub use message::VHOST_USER_F_PROTOCOL_FEATURES;

pub mod connection;

mod sys;
pub use connection::Connection;
pub use message::BackendReq;
pub use message::FrontendReq;
pub use sys::SystemStream;
pub use sys::*;

pub(crate) mod backend_client;
pub use backend_client::BackendClient;
mod frontend_server;
pub use self::frontend_server::Frontend;
mod backend_server;
mod frontend_client;
pub use self::backend_server::Backend;
pub use self::backend_server::BackendServer;
pub use self::frontend_client::FrontendClient;
pub use self::frontend_server::FrontendServer;

/// Errors for vhost-user operations
#[sorted]
#[derive(Debug, ThisError)]
pub enum Error {
    /// Failure from the backend side.
    #[error("backend internal error")]
    BackendInternalError,
    /// client exited properly.
    #[error("client exited properly")]
    ClientExit,
    /// Failure to deserialize data.
    #[error("failed to deserialize data")]
    DeserializationFailed,
    /// client disconnected.
    /// If connection is closed properly, use `ClientExit` instead.
    #[error("client closed the connection")]
    Disconnect,
    /// Virtio/protocol features mismatch.
    #[error("virtio features mismatch")]
    FeatureMismatch,
    /// Failure from the frontend side.
    #[error("frontend Internal error")]
    FrontendInternalError,
    /// Fd array in question is too big or too small
    #[error("wrong number of attached fds")]
    IncorrectFds,
    /// Invalid cast to int.
    #[error("invalid cast to int: {0}")]
    InvalidCastToInt(TryFromIntError),
    /// Invalid message format, flag or content.
    #[error("invalid message")]
    InvalidMessage,
    /// Unsupported operations due to that the protocol feature hasn't been negotiated.
    #[error("invalid operation")]
    InvalidOperation,
    /// Invalid parameters.
    #[error("invalid parameters")]
    InvalidParam,
    /// Message is too large
    #[error("oversized message")]
    OversizedMsg,
    /// Only part of a message have been sent or received successfully
    #[error("partial message")]
    PartialMessage,
    /// Provided recv buffer was too small, and data was dropped.
    #[error("buffer for recv was too small, data was dropped: got size {got}, needed {want}")]
    RecvBufferTooSmall {
        /// The size of the buffer received.
        got: usize,
        /// The expected size of the buffer.
        want: usize,
    },
    /// Error from request handler
    #[error("handler failed to handle request: {0}")]
    ReqHandlerError(IOError),
    /// Failure to restore.
    #[error("Failed to restore")]
    RestoreError(anyhow::Error),
    /// Failure to serialize data.
    #[error("failed to serialize data")]
    SerializationFailed,
    /// Failure to run device specific sleep.
    #[error("Failed to run device specific sleep: {0}")]
    SleepError(anyhow::Error),
    /// Failure to snapshot.
    #[error("Failed to snapshot")]
    SnapshotError(anyhow::Error),
    /// The socket is broken or has been closed.
    #[error("socket is broken: {0}")]
    SocketBroken(std::io::Error),
    /// Can't connect to peer.
    #[error("can't connect to peer: {0}")]
    SocketConnect(std::io::Error),
    /// Generic socket errors.
    #[error("socket error: {0}")]
    SocketError(std::io::Error),
    /// Should retry the socket operation again.
    #[error("temporary socket error: {0}")]
    SocketRetry(std::io::Error),
    /// Failure to stop a queue.
    #[error("failed to stop queue")]
    StopQueueError(anyhow::Error),
    /// Error from tx/rx on a Tube.
    #[error("failed to read/write on Tube: {0}")]
    TubeError(base::TubeError),
    /// Error from VFIO device.
    #[error("error occurred in VFIO device: {0}")]
    VfioDeviceError(anyhow::Error),
    /// Error from invalid vring index.
    #[error("Vring index not found: {0}")]
    VringIndexNotFound(usize),
    /// Failure to run device specific wake.
    #[error("Failed to run device specific wake: {0}")]
    WakeError(anyhow::Error),
}

impl From<base::TubeError> for Error {
    fn from(err: base::TubeError) -> Self {
        match err {
            base::TubeError::Disconnected => Error::Disconnect,
            err => Error::TubeError(err),
        }
    }
}

impl From<std::io::Error> for Error {
    fn from(err: std::io::Error) -> Self {
        Error::SocketError(err)
    }
}

impl From<base::Error> for Error {
    /// Convert raw socket errors into meaningful vhost-user errors.
    ///
    /// The base::Error is a simple wrapper over the raw errno, which doesn't means
    /// much to the vhost-user connection manager. So convert it into meaningful errors to simplify
    /// the connection manager logic.
    ///
    /// # Return:
    /// * - Error::SocketRetry: temporary error caused by signals or short of resources.
    /// * - Error::SocketBroken: the underline socket is broken.
    /// * - Error::SocketError: other socket related errors.
    #[allow(unreachable_patterns)] // EWOULDBLOCK equals to EGAIN on linux
    fn from(err: base::Error) -> Self {
        match err.errno() {
            // Retry:
            // * EAGAIN, EWOULDBLOCK: The socket is marked nonblocking and the requested operation
            //   would block.
            // * EINTR: A signal occurred before any data was transmitted
            // * ENOBUFS: The  output  queue  for  a network interface was full.  This generally
            //   indicates that the interface has stopped sending, but may be caused by transient
            //   congestion.
            // * ENOMEM: No memory available.
            libc::EAGAIN | libc::EWOULDBLOCK | libc::EINTR | libc::ENOBUFS | libc::ENOMEM => {
                Error::SocketRetry(err.into())
            }
            // Broken:
            // * ECONNRESET: Connection reset by peer.
            // * EPIPE: The local end has been shut down on a connection oriented socket. In this
            //   case the process will also receive a SIGPIPE unless MSG_NOSIGNAL is set.
            libc::ECONNRESET | libc::EPIPE => Error::SocketBroken(err.into()),
            // Write permission is denied on the destination socket file, or search permission is
            // denied for one of the directories the path prefix.
            libc::EACCES => Error::SocketConnect(IOError::from_raw_os_error(libc::EACCES)),
            // Catch all other errors
            e => Error::SocketError(IOError::from_raw_os_error(e)),
        }
    }
}

/// Result of vhost-user operations
pub type Result<T> = std::result::Result<T, Error>;

/// Result of request handler.
pub type HandlerResult<T> = std::result::Result<T, IOError>;

/// Utility function to convert a vector of files into a single file.
/// Returns `None` if the vector contains no files or more than one file.
pub(crate) fn into_single_file(mut files: Vec<File>) -> Option<File> {
    if files.len() != 1 {
        return None;
    }
    Some(files.swap_remove(0))
}

#[cfg(test)]
mod test_backend;

#[cfg(test)]
mod tests {
    use std::sync::Arc;
    use std::sync::Barrier;
    use std::thread;

    use base::AsRawDescriptor;
    use tempfile::tempfile;

    use super::*;
    use crate::message::*;
    pub(crate) use crate::sys::tests::create_client_server_pair;
    pub(crate) use crate::sys::tests::create_connection_pair;
    pub(crate) use crate::sys::tests::create_pair;
    use crate::test_backend::TestBackend;
    use crate::test_backend::VIRTIO_FEATURES;
    use crate::VhostUserMemoryRegionInfo;
    use crate::VringConfigData;

    /// Utility function to process a header and a message together.
    fn handle_request(h: &mut BackendServer<TestBackend>) -> Result<()> {
        // We assume that a header comes together with message body in tests so we don't wait before
        // calling `process_message()`.
        let (hdr, files) = h.recv_header()?;
        h.process_message(hdr, files)
    }

    #[test]
    fn create_test_backend() {
        let mut backend = TestBackend::new();

        backend.set_owner().unwrap();
        assert!(backend.set_owner().is_err());
    }

    #[test]
    fn test_set_owner() {
        let test_backend = TestBackend::new();
        let (backend_client, mut backend_server) = create_client_server_pair(test_backend);

        assert!(!backend_server.as_ref().owned);
        backend_client.set_owner().unwrap();
        handle_request(&mut backend_server).unwrap();
        assert!(backend_server.as_ref().owned);
        backend_client.set_owner().unwrap();
        assert!(handle_request(&mut backend_server).is_err());
        assert!(backend_server.as_ref().owned);
    }

    #[test]
    fn test_set_features() {
        let mbar = Arc::new(Barrier::new(2));
        let sbar = mbar.clone();
        let test_backend = TestBackend::new();
        let (mut backend_client, mut backend_server) = create_client_server_pair(test_backend);

        thread::spawn(move || {
            handle_request(&mut backend_server).unwrap();
            assert!(backend_server.as_ref().owned);

            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            assert_eq!(
                backend_server.as_ref().acked_features,
                VIRTIO_FEATURES & !0x1
            );

            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            assert_eq!(
                backend_server.as_ref().acked_protocol_features,
                VhostUserProtocolFeatures::all().bits()
            );

            sbar.wait();
        });

        backend_client.set_owner().unwrap();

        // set virtio features
        let features = backend_client.get_features().unwrap();
        assert_eq!(features, VIRTIO_FEATURES);
        backend_client.set_features(VIRTIO_FEATURES & !0x1).unwrap();

        // set vhost protocol features
        let features = backend_client.get_protocol_features().unwrap();
        assert_eq!(features.bits(), VhostUserProtocolFeatures::all().bits());
        backend_client.set_protocol_features(features).unwrap();

        mbar.wait();
    }

    #[test]
    fn test_client_server_process() {
        let mbar = Arc::new(Barrier::new(2));
        let sbar = mbar.clone();
        let test_backend = TestBackend::new();
        let (mut backend_client, mut backend_server) = create_client_server_pair(test_backend);

        thread::spawn(move || {
            // set_own()
            handle_request(&mut backend_server).unwrap();
            assert!(backend_server.as_ref().owned);

            // get/set_features()
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            assert_eq!(
                backend_server.as_ref().acked_features,
                VIRTIO_FEATURES & !0x1
            );

            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            assert_eq!(
                backend_server.as_ref().acked_protocol_features,
                VhostUserProtocolFeatures::all().bits()
            );

            // get_inflight_fd()
            handle_request(&mut backend_server).unwrap();
            // set_inflight_fd()
            handle_request(&mut backend_server).unwrap();

            // get_queue_num()
            handle_request(&mut backend_server).unwrap();

            // set_mem_table()
            handle_request(&mut backend_server).unwrap();

            // get/set_config()
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();

            // set_backend_req_fd
            handle_request(&mut backend_server).unwrap();

            // set_vring_enable
            handle_request(&mut backend_server).unwrap();

            // set_log_base,set_log_fd()
            handle_request(&mut backend_server).unwrap_err();
            handle_request(&mut backend_server).unwrap_err();

            // set_vring_xxx
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();
            handle_request(&mut backend_server).unwrap();

            // get_max_mem_slots()
            handle_request(&mut backend_server).unwrap();

            // add_mem_region()
            handle_request(&mut backend_server).unwrap();

            // remove_mem_region()
            handle_request(&mut backend_server).unwrap();

            sbar.wait();
        });

        backend_client.set_owner().unwrap();

        // set virtio features
        let features = backend_client.get_features().unwrap();
        assert_eq!(features, VIRTIO_FEATURES);
        backend_client.set_features(VIRTIO_FEATURES & !0x1).unwrap();

        // set vhost protocol features
        let features = backend_client.get_protocol_features().unwrap();
        assert_eq!(features.bits(), VhostUserProtocolFeatures::all().bits());
        backend_client.set_protocol_features(features).unwrap();

        // Retrieve inflight I/O tracking information
        let (inflight_info, inflight_file) = backend_client
            .get_inflight_fd(&VhostUserInflight {
                num_queues: 2,
                queue_size: 256,
                ..Default::default()
            })
            .unwrap();
        // Set the buffer back to the backend
        backend_client
            .set_inflight_fd(&inflight_info, inflight_file.as_raw_descriptor())
            .unwrap();

        let num = backend_client.get_queue_num().unwrap();
        assert_eq!(num, 2);

        let event = base::Event::new().unwrap();
        let mem = [VhostUserMemoryRegionInfo {
            guest_phys_addr: 0,
            memory_size: 0x10_0000,
            userspace_addr: 0,
            mmap_offset: 0,
            mmap_handle: event.as_raw_descriptor(),
        }];
        backend_client.set_mem_table(&mem).unwrap();

        backend_client
            .set_config(0x100, VhostUserConfigFlags::WRITABLE, &[0xa5u8])
            .unwrap();
        let buf = [0x0u8; 4];
        let (reply_body, reply_payload) = backend_client
            .get_config(0x100, 4, VhostUserConfigFlags::empty(), &buf)
            .unwrap();
        let offset = reply_body.offset;
        assert_eq!(offset, 0x100);
        assert_eq!(reply_payload[0], 0xa5);

        #[cfg(windows)]
        let tubes = base::Tube::pair().unwrap();
        #[cfg(windows)]
        let descriptor =
            // SAFETY:
            // Safe because we will be importing the Tube in the other thread.
            unsafe { tube_transporter::packed_tube::pack(tubes.0, std::process::id()).unwrap() };

        #[cfg(unix)]
        let descriptor = base::Event::new().unwrap();

        backend_client.set_backend_req_fd(&descriptor).unwrap();
        backend_client.set_vring_enable(0, true).unwrap();

        // unimplemented yet
        backend_client
            .set_log_base(0, Some(event.as_raw_descriptor()))
            .unwrap();
        backend_client
            .set_log_fd(event.as_raw_descriptor())
            .unwrap();

        backend_client.set_vring_num(0, 256).unwrap();
        backend_client.set_vring_base(0, 0).unwrap();
        let config = VringConfigData {
            queue_size: 128,
            flags: VhostUserVringAddrFlags::VHOST_VRING_F_LOG.bits(),
            desc_table_addr: 0x1000,
            used_ring_addr: 0x2000,
            avail_ring_addr: 0x3000,
            log_addr: Some(0x4000),
        };
        backend_client.set_vring_addr(0, &config).unwrap();
        backend_client.set_vring_call(0, &event).unwrap();
        backend_client.set_vring_kick(0, &event).unwrap();
        backend_client.set_vring_err(0, &event).unwrap();

        let max_mem_slots = backend_client.get_max_mem_slots().unwrap();
        assert_eq!(max_mem_slots, 32);

        let region_file = tempfile().unwrap();
        let region = VhostUserMemoryRegionInfo {
            guest_phys_addr: 0x10_0000,
            memory_size: 0x10_0000,
            userspace_addr: 0,
            mmap_offset: 0,
            mmap_handle: region_file.as_raw_descriptor(),
        };
        backend_client.add_mem_region(&region).unwrap();

        backend_client.remove_mem_region(&region).unwrap();

        mbar.wait();
    }

    #[test]
    fn test_error_display() {
        assert_eq!(format!("{}", Error::InvalidParam), "invalid parameters");
        assert_eq!(format!("{}", Error::InvalidOperation), "invalid operation");
    }

    #[test]
    fn test_error_from_base_error() {
        let e: Error = base::Error::new(libc::EAGAIN).into();
        if let Error::SocketRetry(e1) = e {
            assert_eq!(e1.raw_os_error().unwrap(), libc::EAGAIN);
        } else {
            panic!("invalid error code conversion!");
        }
    }
}
