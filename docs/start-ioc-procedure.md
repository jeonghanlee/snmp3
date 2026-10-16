# Start the IOC and check the workers

You start the input/output controller (IOC) of the module with a loaded configuration and confirm that the runtime is running and that each address worker is ready. The check opens no device session and sends no request.

## Prerequisites

- A configuration and its environment, as prepared in [Configure the module at startup](configure-procedure.md). The variables `CONFIG_JSON`, `NATIVE_PROBE`, and `WORKER_PATH` are set in the shell.
- The module built, with `snmp3Ioc` in `bin/linux-x86_64`.

## Steps

1. To start the IOC with the shipped startup file, run this command from the checkout root:

    ```bash
    bin/linux-x86_64/snmp3Ioc tests/rewrite/config/startup.cmd
    ```

    The IOC prints the startup trace and then the prompt `7.0.10 >`.

2. At the prompt, to print the capability line, enter:

    ```text
    snmp3Report
    ```

3. At the prompt, to print the runtime state, the queues, and the workers, enter:

    ```text
    snmp3RuntimeReport
    ```

4. At the prompt, to stop the IOC, enter:

    ```text
    exit
    ```

5. To read the shutdown report, read the lines the IOC prints after `exit`. They end with a second `snmp3 runtime:` report.

## Verification

The capability line reads:

```text
snmp3: Base 7.0.10; capability=owned-worker-transport; recordSupport=available
```

The runtime report from step 3 shows a running runtime and a ready worker for each configured address:

```text
snmp3 runtime: state=Running admission=1 activation=1 created=1 exited=0 joined=0
snmp3 worker: address=1 epoch=1 pid=<pid> ready=1 closing=0 batch=0 launches=1 reaps=0 forced=0
```

Running means the servicing owner is ready. It does not promise that a device is reachable. The value `ready=1` means the worker matched its bootstrap and its product and library identity. The `<pid>` value is not 0 while the worker lives.

After `exit`, the final report shows a clean stop:

```text
snmp3 runtime: state=Stopped admission=0 activation=1 created=1 exited=1 joined=1
snmp3 worker: address=1 epoch=1 pid=0 ready=0 closing=0 batch=0 launches=1 reaps=1 forced=0
```

The values `joined=1`, `reaps=1`, and `forced=0` show that the IOC joined its thread, reaped the worker, and sent no forced signal. The [worker reference](snmp-worker-reference.md#runtime-report-lines) describes every field.
