# Verify the worker transport

The worker verification of the input/output controller (IOC) runs the shipped code against real native loopback agents and injects faults only at the outer operating system and socket boundaries. IPC is inter-process communication between the IOC and a worker. This procedure builds the test products, runs the component, supervisor, qualification, and operator checks, and repeats two of them on products instrumented with the address sanitizer (ASan) and the undefined behavior sanitizer (UBSan).

## Prerequisites

- A built checkout of the module (see [Worker commands, limits and timers](snmp-worker-reference.md#startup-commands-of-the-ioc)).
- Python 3 on the build host.
- Output directories that do not exist yet. Each directory must be new and private.

## Steps

1. To prepare the parent directory for test results, run this command from the checkout root:

    ```bash
    mkdir -p work
    ```

    Keep the checkout root as the current directory for the remaining steps. The runners create their own result directories below `work`; they do not create missing parent directories.

2. To build the test products, run:

    ```bash
    make -C tests/rewrite -j4
    ```

3. To check IPC, scheduling, inventory, value conversion, and file digests, run:

    ```bash
    python3 tests/rewrite/test_r5_components.py --output work/r5-components
    ```

4. To check the supervisor through the actual IOC, run:

    ```bash
    python3 tests/rewrite/test_supervisor.py --ioc --output work/r5-ioc
    ```

5. To run the qualification cases, run:

    ```bash
    python3 tests/rewrite/test_qualification.py --output work/r5-qualification
    ```

6. To check the operator commands and reports, run:

    ```bash
    python3 tests/rewrite/test_r5_operator.py --output work/r5-operator
    ```

7. To build the instrumented products, run:

    ```bash
    python3 tests/rewrite/build_r5_sanitizers.py --output work/r5-sanitizer-build
    ```

    The command prints the path of the instrumented products directory.

8. To select the instrumented products for the next two steps, set a variable:

    ```bash
    R5_PRODUCTS="$PWD/work/r5-sanitizer-build/products"
    ```

9. To run the qualification cases on the instrumented products, run:

    ```bash
    python3 tests/rewrite/test_qualification.py --products "$R5_PRODUCTS" --sanitizers --output work/r5-san
    ```

10. To run the supervisor check through the IOC on the instrumented products, run:

    ```bash
    python3 tests/rewrite/test_supervisor.py --products "$R5_PRODUCTS" --sanitizers --ioc --output work/r5-san-ioc
    ```

## Verification

Each completed check prints one line that names its result file. A passing run prints `PASS:` followed by the absolute path of `results.json` in the output directory:

```text
PASS: /home/<user>/<checkout>/work/r5-components/results.json
```

Replace `<user>` and `<checkout>` with the home directory and the checkout directory of your build host. A completed check with failed assertions prints `FAIL:` with the same path. The checks of steps 3 to 6, 9, and 10 must each print `PASS:` and exit with status 0.

A command can stop before it writes `results.json`, for example when the output parent directory is missing. A traceback or a missing result file means the invocation is incomplete; it is not a passing check.

A PASS qualifies the identified invocation only. Full completion also requires current foundation, configuration, lifecycle, and native regressions. The [verification sequence](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/tests/rewrite/README.md) lists those commands.

The ASan and UBSan runs instrument the module, wire, and native libraries, plus the actual worker, owner, and IOC test products. The dependencies stay uninstrumented, and leak detection is disabled. The run keeps the private qualification standard error and scans it, because the production redirect to `/dev/null` cannot establish sanitizer success. Test-only outer IPC forwarding changes real frames while it keeps the selected actual worker and the full native path. The Runtime rejects transport overrides and offers no production selector for them.

The [reference page](snmp-worker-reference.md#diagnostics-of-the-verification-cases) explains how to read the diagnostics of the `wake-loop` and `stop-queued` cases.
