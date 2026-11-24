#!/usr/bin/env bash
# Component cell: runs the four test programs of the module (Ipc, Scheduler, Inventory, Conversion), the digest of nine
# fixtures that sit on the padding boundaries of SHA-256, and the digest of the worker and of the libnetsnmp that the native
# library loads. A program passes when it exits 0, is not ended by the bound of 15 s and prints nothing to the error output.
# A digest passes when snmp3IpcTest --digest prints the value that sha256sum of coreutils prints for the same file: the
# reference does not belong to the product. r5_components.pl reads the receipts and judges. The cell makes no claim about the
# IOC, the worker traffic or acceptance of the module.
# usage: test_r5_components.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers]
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly JUDGE="${HERE}/r5_components.pl"
readonly BOUND_SECONDS=15
readonly -a LENGTHS=(0 1 55 56 63 64 65 4096 65537)
readonly -a PROGRAMS=(Ipc Scheduler Inventory Conversion)

function die {
    printf "test_r5_components: %s\n" "$1" >&2
    exit 2
}

# Runs one command with the minimal environment of the cell and the bound of the cell, and appends one line to receipts.tsv:
# name, exit status, 1 when the bound ended it, bytes of the error output, expected output, actual output, product digest.
# Arguments: name, expected output ("-" when the output is not compared), command...
function invoke {
    local name="$1" expected="$2" status=0 forced=0 actual="-" errors product
    shift 2
    env -i PATH=/usr/bin:/bin LANG=C LC_ALL=C "${SANITIZER_ENV[@]}" timeout -s KILL "${BOUND_SECONDS}" "$@" \
        > "${OUTPUT}/${name}.stdout" 2> "${OUTPUT}/${name}.stderr" || status=$?
    [[ ${status} -ne 137 ]] || forced=1
    errors="$(stat -c %s "${OUTPUT}/${name}.stderr")" || die "cannot read the error output of ${name}"
    if [[ "${expected}" != "-" ]]; then
        actual="$(tr -d '\n' < "${OUTPUT}/${name}.stdout")"
    fi
    product="$(sha256sum "$1" | cut -d' ' -f1)" || die "cannot digest ${1}"
    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "${name}" "${status}" "${forced}" "${errors}" "${expected}" "${actual}" "${product}" \
        >> "${OUTPUT}/receipts.tsv"
}

# Prints the real path of the libnetsnmp that the native library of the checkout loads.
function netsnmp_library {
    local line path=""
    while IFS= read -r line; do
        if [[ "${line}" =~ ^[[:space:]]*libnetsnmp\.so[^[:space:]]*[[:space:]]+=\>[[:space:]]+([^[:space:]]+) ]]; then
            path="${BASH_REMATCH[1]}"
            break
        fi
    done < <(ldd "${ROOT}/lib/linux-x86_64/libsnmp3Native.so")
    [[ -n "${path}" ]] || die "libsnmp3Native.so does not load libnetsnmp"
    readlink -f "${path}"
}

function main {
    local output="" products="${ROOT}/bin/linux-x86_64" sanitizers=no name length fixture reference library
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --products) products="${2:-}"; shift 2 ;;
            --sanitizers) sanitizers=yes; shift ;;
            *) die "usage: test_r5_components.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers]" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    products="$(cd "${products}" && pwd)" || die "no such products directory"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd)"
    SANITIZER_ENV=()
    if [[ "${sanitizers}" == "yes" ]]; then
        SANITIZER_ENV=(ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
    fi
    : > "${OUTPUT}/receipts.tsv"
    # The sources that the run is bound to: the module, the test programs and the files of the cell.
    (
        cd "${ROOT}" && {
            find snmp3App \( -name '*.cpp' -o -name '*.h' \) -type f | sort
            printf "%s\n" tests/rewrite/IpcTest.cpp tests/rewrite/SchedulerTest.cpp tests/rewrite/InventoryTest.cpp \
                tests/rewrite/ConversionTest.cpp tests/rewrite/test_r5_components.bash tests/rewrite/r5_components.pl
        } | xargs sha256sum
    ) > "${OUTPUT}/sources.txt" || die "cannot digest the sources"
    for name in "${PROGRAMS[@]}"; do
        invoke "$(printf "%s" "${name}" | tr '[:upper:]' '[:lower:]')" "-" "${products}/snmp3${name}Test"
    done
    for length in "${LENGTHS[@]}"; do
        fixture="${OUTPUT}/digest-${length}.bin"
        (umask 077 && perl -e 'print map { chr(($_ * 13 + 7) % 256) } 0 .. $ARGV[0] - 1' "${length}" > "${fixture}") ||
            die "cannot write a fixture"
        reference="$(sha256sum "${fixture}" | cut -d' ' -f1)" || die "cannot digest a fixture"
        invoke "digest-${length}" "${reference}" "${products}/snmp3IpcTest" --digest "${fixture}"
    done
    reference="$(sha256sum "${products}/snmp3Worker" | cut -d' ' -f1)" || die "cannot digest the worker"
    invoke "digest-executable" "${reference}" "${products}/snmp3IpcTest" --digest "${products}/snmp3Worker"
    library="$(netsnmp_library)" || exit 2
    reference="$(sha256sum "${library}" | cut -d' ' -f1)" || die "cannot digest the library"
    invoke "digest-native-library" "${reference}" "${products}/snmp3IpcTest" --digest "${library}"
    perl "${JUDGE}" "${OUTPUT}"
    exit $?
}

main "$@"
