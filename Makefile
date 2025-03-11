SUBDIRS := jetstream jetbridge libcrosvm bldy innitguv
PROJECTS := $(addsuffix /Cargo.toml,$(SUBDIRS))

# Set default environment variables
RUST_TOOLCHAIN ?= stable
export RUST_TOOLCHAIN

# CPU architecture or get it from uname -m 
ARCH ?= $(shell uname -m)
 
.PHONY: all test-all $(SUBDIRS) initd

all: $(SUBDIRS) initd

test-all: $(addsuffix -test,$(SUBDIRS))

$(SUBDIRS):
	@echo "Building $@"
	@cd $@ && cargo build

initd: innitguv
	@echo "Building u-root image"
	@cd okLinux
	@cp /scratch/cargo_target/debug/innitguv bin/init
	@mkuimage -files bin/ -o okLinux.initramfs.${ARCH}.cpio
