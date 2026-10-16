# Verify the worker transport

The worker verification of the input/output controller (IOC) runs the shipped code against real native loopback agents and injects faults only at the outer operating system and socket boundaries. IPC is inter-process communication between the IOC and a worker. This procedure builds the test products, runs the component, supervisor, qualification, and operator checks, and repeats two of them on products instrumented with the address sanitizer (ASan) and the undefined behavior sanitizer (UBSan).

## Prerequisites

- A built checkout of the module (see [Worker commands, limits and timers](snmp-worker-reference.md#startup-commands-of-the-ioc)).
- Python 3 on the build host.
- Output directories that do not exist yet. Each directory must be new and private.

## Steps

1. To build the test products, run this command from the checkout root:

    ```bash
    make -C tests/rewrite -j4
    ```

2. To check the scheduler, supervisor, and worker components, run:

    ```bash
    python3 tests/rewrite/test_r5_components.py --output work/r5-components
    ```

3. To check the supervisor through the actual IOC, run:

    ```bash
    python3 tests/rewrite/test_supervisor.py --ioc --output work/r5-ioc
    ```

4. To run the qualification cases, run:

    ```bash
    python3 tests/rewrite/test_qualification.py --output work/r5-qualification
    ```

5. To check the operator commands and reports, run:

    ```bash
    python3 tests/rewrite/test_r5_operator.py --output work/r5-operator
    ```

6. To build the instrumented products, run:

    ```bash
    python3 tests/rewrite/build_r5_sanitizers.py --output work/r5-sanitizer-build
    ```

    The command prints the path of the instrumented products directory.

7. To select the instrumented products for the next two steps, set a variable:

    ```bash
    R5_PRODUCTS="$PWD/work/r5-sanitizer-build/products"
    ```

8. To run the qualification cases on the instrumented products, run:

    ```bash
    python3 tests/rewrite/test_qualification.py --products "$R5_PRODUCTS" --sanitizers --output work/r5-san
    ```

9. To run the supervisor check through the IOC on the instrumented products, run:

    ```bash
    python3 tests/rewrite/test_supervisor.py --products "$R5_PRODUCTS" --sanitizers --ioc --output work/r5-san-ioc
    ```

## Verification

Each check prints one line that names its result file. A passing run prints `PASS:` followed by the absolute path of `results.json` in the output directory:

```text
PASS: /home/<user>/<checkout>/work/r5-components/results.json
```

Replace `<user>` and `<checkout>` with the home directory and the checkout directory of your build host. A failing run prints `FAIL:` with the same path. The checks of steps 2 to 5, 8, and 9 must each print `PASS:`.

A PASS qualifies the identified invocation only. Full completion also requires current foundation, configuration, lifecycle, and native regressions. The [verification sequence](https://github.com/jeonghanlee/snmp3/blob/feature/snmp-base-7.0.10-rewrite/tests/rewrite/README.md) lists those commands.

The instrumented runs instrument both libraries and the actual worker, owner, and IOC test products with ASan and UBSan. The dependencies stay uninstrumented, and leak detection is disabled. The run keeps the private qualification standard error and scans it, because the production redirect to `/dev/null` cannot establish sanitizer success. Test-only outer IPC forwarding changes real frames while it keeps the selected actual worker and the full native path. The Runtime rejects transport overrides and offers no production selector for them.

The [reference page](snmp-worker-reference.md#diagnostics-of-the-verification-cases) explains how to read the diagnostics of the `wake-loop` and `stop-queued` cases.
