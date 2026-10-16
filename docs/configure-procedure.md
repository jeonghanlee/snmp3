# Configure the module at startup

You prepare a complete configuration with one profile, one endpoint, and one binding, and you load it into the input/output controller (IOC) of the module. The IOC loads the configuration before any record binds to it and freezes it at initialization.

## Prerequisites

- The module built with `make -j4` at the checkout root, so that `bin/linux-x86_64` holds `snmp3Ioc`, `snmp3NativeProbe`, and `snmp3Worker`.
- The numeric address of a device, or `127.0.0.1` for a check without a device.
- The SNMP community string of the device. The example uses SNMP version 2c.
- A shell that runs the commands from the checkout root.

## Steps

1. To create a directory for the configuration, run:

    ```bash
    mkdir -p <config_dir>
    ```

    Replace `<config_dir>` with an absolute path.

2. To copy the shipped example configuration, run:

    ```bash
    cp tests/rewrite/config/v2c.json <config_dir>/config.json
    ```

3. In `<config_dir>/config.json`, set `address` of the endpoint `Local` to the numeric address of your device. The loader rejects host names.

4. To write the community string into the secret file named by `communityFile`, run:

    ```bash
    printf '%s\n' '<community>' > <config_dir>/community.txt
    ```

    Replace `<community>` with the community string. The file holds raw bytes and one optional final line feed.

5. To make the secret file private, run:

    ```bash
    chmod 600 <config_dir>/community.txt
    ```

    The loader requires a regular file that the effective IOC user owns, with no group or other permission bits.

6. To select the configuration, set the environment of the shipped startup file:

    ```bash
    export CONFIG_JSON=<config_dir>/config.json NATIVE_PROBE=$PWD/bin/linux-x86_64/snmp3NativeProbe WORKER_PATH=$PWD/bin/linux-x86_64/snmp3Worker
    ```

7. To load the configuration and leave the IOC at once, run:

    ```bash
    printf 'exit\n' | bin/linux-x86_64/snmp3Ioc tests/rewrite/config/startup.cmd
    ```

## Verification

The output contains three configuration report lines. The `snmp3ConfigReport` command of the startup file prints the first one after the load, before the configuration is frozen. The IOC prints the second one while `iocInit` freezes the configuration. The `snmp3ConfigReport` command prints the third one after `iocInit`:

```text
snmp3 config: revision=1 frozen=0 profiles=1 endpoints=1 bindings=1
snmp3 config: revision=1 frozen=1 profiles=1 endpoints=1 bindings=1
snmp3 config: revision=1 frozen=1 profiles=1 endpoints=1 bindings=1
```

A rejected load leaves the previous snapshot unchanged. The startup file begins with `on error break`, so a failed `snmp3Load` ends the IOC with the message `snmp3Ioc: startup script failed` and exit status 1. The [startup configuration reference](snmp-rewrite-config.md) lists every field and every rejection rule.
