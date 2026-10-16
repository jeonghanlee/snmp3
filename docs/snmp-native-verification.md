# Verify the native transport

The native verification of the input/output controller (IOC) builds the real products, runs a direct native preflight, runs the actual adapter matrix against a real native agent, and repeats that matrix with separate address sanitizer (ASan) and undefined behavior sanitizer (UBSan) products. The tests use the user datagram protocol (UDP) of a real native agent and the Experimental Physics and Industrial Control System (EPICS) Base. A final regression covers the foundation, lifecycle, and configuration paths.

## Prerequisites

- A Linux x86_64 host with Python 3, GNU make, a C++ compiler, and the installed Net-SNMP development and agent libraries.
- A shared-library EPICS Base 7.0.10 installation. The file `configure/RELEASE.local` must already identify it. The procedure installs no package and no Base.
- Output directories that do not exist yet. Each evidence directory must be new.

## Steps

1. To build the real products, run this command from the checkout root:

    ```bash
    make -j2
    ```

2. To run the direct native preflight, run:

    ```bash
    python3 tests/rewrite/test_native.py --phase preflight --output work/native-preflight
    ```

3. To run the actual adapter matrix, run:

    ```bash
    python3 tests/rewrite/test_native.py --phase adapter --output work/native-adapter
    ```

4. To repeat the adapter matrix with the instrumented products, run:

    ```bash
    python3 tests/rewrite/test_native.py --phase adapter --sanitizers --output work/native-sanitized
    ```

5. To select the Base installation for the regression, set a variable to its absolute path:

    ```bash
    BASE_PATH=<base_path>
    ```

6. To run the foundation, lifecycle, and configuration regression with the same Base, run:

    ```bash
    python3 tests/rewrite/test_config.py --base "$BASE_PATH" --output work/native-regression
    ```

The regression rebuilds and runs the real IOC and Config paths.

## Verification

Each check prints one line that names its result file. A passing run prints `PASS:` followed by the absolute path of `results.json` in the output directory:

```text
PASS: /home/<user>/<checkout>/work/native-preflight/results.json
```

Replace `<user>` and `<checkout>` with the home directory and the checkout directory of your build host. The four checks of steps 2, 3, 4, and 6 must each print `PASS:`. Neither phase alone is full implementation acceptance.

The runners create mode-0700 evidence directories and mode-0600 generated secret and config files. A separate real native agent listens only on owned unprivileged IPv4 and IPv6 loopback ports. A SET changes disposable fixture storage. The UDP fault process mutates messages actually exchanged at the outer network boundary, and it never substitutes an internal adapter function. The recorded packet metadata excludes raw packets and credentials. The runners retain failed runs.

A PASS means all checks in that invocation passed. The file `results.json` identifies the sources, the selected headers, the products, and the loaded libraries. The command receipts include the return and reap status, the timing, and the forced-cleanup classification. The runner waits for every owned child. Secret sentinels and sanitizer diagnostics must be absent from the recorded output.

The option `--sanitizers` instruments the native library, the helper, the driver, and the native fixtures with ASan and UBSan. Base, Config support, and the system and vendor native dependencies stay uninstrumented. Leak detection is disabled, so this is an access and undefined-behavior verification, not a leak or resource qualification.

The real file-descriptor exhaustion case first opens and closes a native session and executes an actual local admission rejection. It releases its held descriptors on the failed open before the fixture unwinds. This case qualifies an initialized native process.

Privilege-dependent wrong-owner secret cells can remain explicitly NOT RUN. Historical owner receipts apply only to their original source and product hashes. The [startup configuration reference](snmp-rewrite-config.md) defines the configuration validation, the report and help output, and the startup behavior.
