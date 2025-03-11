extern crate libc;

use std::ffi::CString;
use std::ptr;

fn mount_9p(source: &str, target: &str, fstype: &str, flags: libc::c_ulong, data: &str) -> Result<(), i32> {
    let source_c = CString::new(source).expect("CString::new failed");
    let target_c = CString::new(target).expect("CString::new failed");
    let fstype_c = CString::new(fstype).expect("CString::new failed");
    let data_c = CString::new(data).expect("CString::new failed");

    let res = unsafe {
        libc::mount(
            source_c.as_ptr(),
            target_c.as_ptr(),
            fstype_c.as_ptr(),
            flags,
            data_c.as_ptr() as *const libc::c_void,
        )
    };

    if res == 0 {
        Ok(())
    } else {
        Err(std::io::Error::last_os_error().raw_os_error().unwrap_or(-1))
    }
}

fn main() {
    // Example usage:
    let source = "";
    let target = "/mnt/my9p";
    let fstype = "9p";
    let flags = libc::MS_RDONLY;
    let data = "trans=virtio,version=9p2000.L";

    match mount_9p(source, target, fstype, flags as libc::c_ulong, data) {
        Ok(_) => println!("Mounted successfully."),
        Err(errno) => eprintln!("Mount failed with errno: {}", errno),
    }
}
