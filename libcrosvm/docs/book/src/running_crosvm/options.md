# Command line options and configuration files

It is possible to configure a VM through command-line options and/or a JSON configuration file.

The names and format of configurations options are consistent between both ways of specifying,
however the command-line includes options that are deprecated or unstable, whereas the configuration
file only allows stable options. This section reviews how to use both.

## Command-line options

Command-line options generally take a set of key-value pairs separated by the comma (`,`) character.
The acceptable key-values for each option can be obtained by passing the `--help` option to a crosvm
command:

```sh
crosvm run --help
...
  -b, --block       parameters for setting up a block device.
                    Valid keys:
                        path=PATH - Path to the disk image. Can be specified
                            without the key as the first argument.
                        ro=BOOL - Whether the block should be read-only.
                            (default: false)
                        root=BOOL - Whether the block device should be mounted
                            as the root filesystem. This will add the required
                            parameters to the kernel command-line. Can only be
                            specified once. (default: false)
                        sparse=BOOL - Indicates whether the disk should support
                            the discard operation. (default: true)
                        block-size=BYTES - Set the reported block size of the
                            disk. (default: 512)
                        id=STRING - Set the block device identifier to an ASCII
                            string, up to 20 characters. (default: no ID)
                        direct=BOOL - Use O_DIRECT mode to bypass page cache.
                            (default: false)
...
```

From this help message, we see that the `--block` or `-b` option accepts the `path`, `ro`, `root`,
`sparse`, `block-size`, `id`, and `direct` keys. Keys which default value is mentioned are optional,
which means only the `path` key must always be specified.

One example invocation of the `--block` option could be:

```sh
--block path=/path/to/bzImage,root=true,block-size=4096
```

Keys taking a boolean parameters can be enabled by specifying their name witout any value, so the
previous option can also be written as

```sh
--block path=/path/to/bzImage,root,block-size=4096
```

Also, the name of the first key can be entirely omitted, which further simplifies our option as:

```sh
--block /path/to/bzImage,root,block-size=4096
```

## Configuration files

Configuration files are specified using the `--cfg` argument. Here is an example configuration file
specifying a basic VM with a few devices:

```json
{
    "kernel": "/path/to/bzImage",
    "cpus": {
        "num-cores": 8
    },
    "mem": {
        "size": 2048
    },
    "block": [
        {
            "path": "/path/to/root.img",
            "root": true
        }
    ],
    "serial": [
        {
            "type": "stdout",
            "hardware": "virtio-console",
            "console": true,
            "stdin": true
        }
    ],
    "net": [
        {
            "tap-name": "crosvm_tap"
        }
    ]
}
```

The equivalent command-line options corresponding to this configuration file would be:

```sh
--kernel path/to/bzImage \
--cpus num-cores=8 --mem size=2048 \
--block path=/path/to/root.img,root \
--serial type=stdout,hardware=virtio-console,console,stdin \
--net tap-name=crosvm_tap
```

Or, if we apply the simplification rules discussed in the previous section:

```sh
--kernel /path/to/bzImage \
--cpus 8 --mem 2048 \
--block /path/to/root.img,root \
--serial stdout,hardware=virtio-console,console,stdin \
--net tap-name=crosvm_tap
```

Note that so `cfg` directive can also be used within configuration files, allowing a form of
configuration inclusion:

```json
{
  ...
  "cfg": [ "net.json", "gpu.json" ],
  ...
}

```

Included files are always applied first. So in this example, the `net.json` is the base
configuration to which `gpu.json` is applied, and finally the parent file that included these two.
This order does not matter if each file specifies different options, but in case of overlap
parameters from the parent will take precedence over included ones, regardless of where the `cfg`
directive appears in the file.

## Combining configuration files and command-line options

One useful use of configuration files is to specify a base configuration that can be augmented or
modified by other configuration files or command-line arguments.

All the configuration files specified with `--cfg` are merged by order of appearance into a single
configuration. The merge rules are generally that arguments that can only be specified once are
overwritten by the newest configuration, while arguments that can be specified many times (like
devices) are extended.

Finally, the other command-line parameters are merged into the configuration, regardless of their
position relative to a `--cfg` argument (i.e. even if they come before it). This means that
command-line arguments take precedence over anything in configuration files.

For instance, considering this configuration file `vm.json`:

```json
{
    "kernel": "/path/to/bzImage",
    "block": [
        {
            "path": "/path/to/root.img",
            "root": true
        }
    ]
}
```

And the following crosvm invocation:

```sh
crosvm run --cfg vm.json --block /path/to/home.img
```

Then the created VM will have two block devices, the first one pointing to `root.img` and the second
one to `home.img`.

For options that can be specified only once, like `--kernel`, the one specified on the command-line
will take precedence over the one in the configuration file. For instance, with the same `vm.json`
file and the following command-line:

```sh
crosvm run --cfg vm.json --kernel /path/to/another/bzImage
```

Then the loaded kernel will be `/path/to/another/bzImage`, and the `kernel` option in the
configuration file will become a no-op.
