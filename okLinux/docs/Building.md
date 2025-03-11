{{#include _header.md}}

# Build script
{{ #include _wip.md }}

```bash
{{ #include ../build.sh}}
```

### From kernel docs

In 2020, a better way to delay the mount until the time when we have access to the 9p modules from /lib/modules is to add _netdev as a mount parameter:

```text,file=/etc/fstab
/data   /data   9p  trans=virtio,rw,_netdev 0   0
```

## From CrosVM 

### Build a rootfs disk

This stage enjoys the most flexibility. There aren't any special requirements for a rootfs in crosvm, but you will at a minimum need an init binary. This could even be /bin/bash if that is enough for your purposes. To get you started, a Debian rootfs can be created with debootstrap. Make sure to define $CHROOT_PATH.

```bash
truncate -s 20G debian.ext4
mkfs.ext4 debian.ext4
mkdir -p "${CHROOT_PATH}"
sudo mount debian.ext4 "${CHROOT_PATH}"
sudo debootstrap stable "${CHROOT_PATH}" http://deb.debian.org/debian/
sudo chroot "${CHROOT_PATH}"
passwd
echo "tmpfs /tmp tmpfs defaults 0 0" >> /etc/fstab
echo "tmpfs /var/log tmpfs defaults 0 0" >> /etc/fstab
echo "tmpfs /root tmpfs defaults 0 0" >> /etc/fstab
echo "sysfs /sys sysfs defaults 0 0" >> /etc/fstab
echo "proc /proc proc defaults 0 0" >> /etc/fstab
exit
sudo umount "${CHROOT_PATH}"
```