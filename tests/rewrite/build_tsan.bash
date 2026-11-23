#!/usr/bin/env bash
# Builds the module, its native owner, the worker and the test programs of the thread sanitizer cell with
# -fsanitize=thread, the way build_r5_sanitizers.py builds the address sanitizer products. EPICS Base and Net-SNMP
# stay uninstrumented. The products are the three libraries the programs load, the worker and the probe that every
# runner of the cell requires, the production IOC, and the test programs of the cell. The fixture agent is copied from
# the ordinary products of the tree: it is a test tool without module code, and built with the thread sanitizer it
# does not stop on SIGTERM, which the runners require.
# Prerequisite: the tree is built with the ordinary products first (make, then make -C tests/rewrite). The generated
# O.<arch>/*_registerRecordDeviceDriver.cpp files and the ordinary snmp3NativeAgent come from that build.
# usage: build_tsan.bash --output NEW_DIRECTORY [--tree DIRECTORY]
#   --tree names the tree whose sources are built (default: the tree this file belongs to); the negative controls of
#   the cell build a copy of the tree in which one lock is taken out.
# exit status: 0 every product was built and imports the thread sanitizer, 1 a product failed, 2 a usage error

set -u

readonly HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ARCH="${EPICS_HOST_ARCH:-linux-x86_64}"

function die {
    printf "build_tsan: %s\n" "$1" >&2
    exit "${2:-2}"
}

# Prints the EPICS Base directory the tree builds against.
function base_directory {
    local tree="$1" line
    while IFS= read -r line; do
        line="${line//$'\r'/}"
        if [[ "${line}" =~ ^[[:space:]]*EPICS_BASE[[:space:]]*=[[:space:]]*(.*)$ ]]; then
            printf "%s\n" "${BASH_REMATCH[1]}"
            return 0
        fi
    done < "${tree}/configure/RELEASE.local"
    return 1
}

# Compiles one product. Arguments: name, kind (lib|prog), uses-native (yes|no), libraries as one string, sources...
function build_product {
    local name="$1" kind="$2" native="$3" libraries="$4"
    shift 4
    local -a command=("${COMPILER[@]}") libs=() shared=()
    [[ "${native}" == "yes" ]] && command+=("${NATIVE_FLAGS[@]}")
    [[ "${kind}" == "lib" ]] && shared=(-shared)
    read -r -a libs <<< "${libraries}"
    command+=("${shared[@]}" "$@" "${LINK[@]}" "${libs[@]}")
    if [[ "${native}" == "yes" ]]; then
        command+=("${NATIVE_LIBS[@]}")
    fi
    command+=(-o "${PRODUCTS}/${name}")
    if ! "${command[@]}" > "${OUTPUT}/${name}.build.stdout" 2> "${OUTPUT}/${name}.build.stderr"; then
        printf "FAIL %s\n" "${name}"
        return 1
    fi
    if ! nm -D "${PRODUCTS}/${name}" | grep -q "__tsan_"; then
        printf "FAIL %s: no import of the thread sanitizer\n" "${name}"
        return 1
    fi
    printf "built %s %s\n" "${name}" "$(sha256sum "${PRODUCTS}/${name}" | cut -c1-12)"
    return 0
}

function main {
    local tree="${HERE}/../.." output="" base src native tests status=0
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --tree) tree="${2:-}"; shift 2 ;;
            *) die "usage: build_tsan.bash --output NEW_DIRECTORY [--tree DIRECTORY]" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    tree="$(cd "${tree}" && pwd)" || die "no such tree"
    base="$(base_directory "${tree}")" || die "EPICS_BASE not found in ${tree}/configure/RELEASE.local"
    command -v net-snmp-config > /dev/null || die "net-snmp-config not found"
    [[ -x "${tree}/bin/${ARCH}/snmp3NativeAgent" && -f "${tree}/snmp3App/src/O.${ARCH}/snmp3Ioc_registerRecordDeviceDriver.cpp" &&
       -f "${tree}/tests/rewrite/O.${ARCH}/snmp3RuntimeTest_registerRecordDeviceDriver.cpp" ]] ||
        die "the tree is not built: run make and make -C tests/rewrite in ${tree} first"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd)"
    PRODUCTS="${OUTPUT}/products"
    mkdir -m 700 "${PRODUCTS}" || die "cannot create ${PRODUCTS}"
    src="${tree}/snmp3App/src"
    native="${tree}/snmp3App/native"
    tests="${tree}/tests/rewrite"
    COMPILER=(g++ -O1 -g -fPIC -pthread -fsanitize=thread -fno-omit-frame-pointer
              "-I${src}" "-I${native}" "-I${base}/include" "-I${base}/include/os/Linux"
              "-I${base}/include/compiler/gcc")
    read -r -a NATIVE_FLAGS <<< "$(net-snmp-config --cflags)"
    read -r -a NATIVE_LIBS <<< "$(net-snmp-config --libs)"
    LINK=("-L${PRODUCTS}" "-L${base}/lib/${ARCH}" "-Wl,-rpath,\$ORIGIN:${base}/lib/${ARCH}")
    local record_libs="-lsnmp3 -lsnmp3Wire -ldbRecStd -ldbCore -lca -lCom"
    local generated="O.${ARCH}"
    build_product libsnmp3Wire.so lib no "-lCom" "${src}/Ipc.cpp" "${src}/Identity.cpp" || status=1
    build_product libsnmp3.so lib no "-lsnmp3Wire -ldbCore -lCom" \
        "${src}/Register.cpp" "${src}/Runtime.cpp" "${src}/Config.cpp" "${src}/Json.cpp" "${src}/Scheduler.cpp" \
        "${src}/Supervisor.cpp" "${src}/Conversion.cpp" "${src}/Request.cpp" "${src}/DeviceSupport.cpp" \
        "${src}/Peer.cpp" "${src}/StateSupport.cpp" || status=1
    build_product libsnmp3Native.so lib yes "-lCom" \
        "${native}/Capabilities.cpp" "${native}/Native.cpp" "${native}/Worker.cpp" || status=1
    build_product snmp3NativeProbe prog yes "-lsnmp3Native -lCom" "${native}/NativeProbe.cpp" || status=1
    if [[ -x "${tree}/bin/${ARCH}/snmp3NativeAgent" ]]; then
        cp "${tree}/bin/${ARCH}/snmp3NativeAgent" "${PRODUCTS}/snmp3NativeAgent"
        printf "copied snmp3NativeAgent %s\n" "$(sha256sum "${PRODUCTS}/snmp3NativeAgent" | cut -c1-12)"
    else
        printf "FAIL snmp3NativeAgent: not built in %s/bin/%s\n" "${tree}" "${ARCH}"
        status=1
    fi
    build_product snmp3Worker prog yes "-lsnmp3Native -lsnmp3Wire -lCom" "${native}/WorkerMain.cpp" || status=1
    local name
    for name in Qualification Supervisor Scheduler; do
        build_product "snmp3${name}Test" prog no "-lsnmp3 -lsnmp3Wire -lCom" "${tests}/${name}Test.cpp" || status=1
    done
    for name in Runtime Record Lifecycle; do
        build_product "snmp3${name}Test" prog no "${record_libs}" "${tests}/${name}Test.cpp" \
            "${tests}/${generated}/snmp3${name}Test_registerRecordDeviceDriver.cpp" || status=1
    done
    build_product snmp3Ioc prog no "${record_libs}" "${src}/Main.cpp" \
        "${src}/${generated}/snmp3Ioc_registerRecordDeviceDriver.cpp" || status=1
    for name in Shutdown Startup; do
        build_product "snmp3${name}Test" prog no "${record_libs}" "${src}/Main.cpp" "${tests}/${name}Test.cpp" \
            "${tests}/${generated}/snmp3${name}Test_registerRecordDeviceDriver.cpp" || status=1
    done
    printf "%s\n" "${PRODUCTS}"
    exit "${status}"
}

main "$@"
