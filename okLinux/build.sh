#!/bin/bash

# This script is used to build initrd for Linux kernel.

mkdir -p /tmp/okLinux/initrd
cd /tmp/okLinux/initrd || exit

# Create necessary directories
mkdir -p bin dev etc lib proc sbin sys tmp usr var
