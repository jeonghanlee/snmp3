#!/usr/bin/env bash
# Lifecycle cell: runs the shipped Runtime, IOC and isolated Base lifecycle paths without internal substitutes. The cell runs the
# independence check of the module (run_independence.py, as a program), builds the test programs, starts a caRepeater of its own on
# two free UDP ports, and then runs the IOC and the lifecycle test program in eight cases (normal, cold, before-init-error,
# partial-build-error, explicit-stop, module, thread-failure, isolated) with LD_DEBUG=libs, so that the libraries that the process
# loaded are known. This script gathers the observations of each case; lifecycle.pl judges them. The cell stops at the first failed
# check: after each case the judge is asked whether a check has failed, and the gathering ends when one has. The judge reads the
# observations in the order of the checks and writes results.json at the end.
# usage: test_lifecycle.bash --base EPICS_BASE_DIRECTORY --output NEW_DIRECTORY
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/lifecycle.pl"
readonly ARCH="linux-x86_64"
readonly CASE_SECONDS=30
readonly BUILD_SECONDS=180
readonly INDEPENDENCE_SECONDS=300

BASE=""
OUTPUT=""
REPEATER_PID=""
declare -a ENV_BASE=()
declare -a ENV_CASE=()

# Ends the cell as not judged; a repeater that the cell started is ended first.
function die {
    printf "test_lifecycle: %s\n" "$1" >&2
    if [[ -n "${REPEATER_PID}" ]]; then
        stop_repeater
    fi
    exit 2
}

# Prints the EPICS Base directory as a real path after it has been checked as an installed EPICS Base 7.0.10 tree.
function validate_base {
    local path="$1" resolved header text macro value name
    resolved="$(cd "${path}" 2> /dev/null && pwd -P)" || die "--base must name an installed EPICS Base 7.0.10 tree"
    header="${resolved}/include/epicsVersion.h"
    [[ -f "${header}" ]] || die "--base must name an installed EPICS Base 7.0.10 tree"
    text="$(< "${header}")"
    local -a version=()
    for macro in EPICS_VERSION EPICS_REVISION EPICS_MODIFICATION; do
        if [[ "${text}" =~ (^|$'\n')#define[[:space:]]+${macro}[[:space:]]+([0-9]+) ]]; then
            version+=("${BASH_REMATCH[2]}")
        else
            die "malformed Base version header"
        fi
    done
    value="${version[0]}.${version[1]}.${version[2]}"
    [[ "${value}" == "7.0.10" ]] || die "--base must be version 7.0.10"
    for name in "lib/${ARCH}/libCom.so" "bin/${ARCH}/caRepeater" "configure/CONFIG"; do
        [[ -f "${resolved}/${name}" ]] || die "missing Base prerequisite: ${name}"
    done
    case "${resolved}" in
        *[[:space:]\#\$]*) die "invalid Base path characters" ;;
    esac
    printf "%s\n" "${resolved}"
}

# Asks the judge about the checks of the cases up to the named stage: returns 0 when none has failed, 1 when one has.
function stage {
    local status=0
    perl "${JUDGE}" judge "${OUTPUT}" --stage "$1" > /dev/null || status=$?
    [[ ${status} -le 1 ]] || die "the judge could not read the observations of the stage $1"
    return "${status}"
}

# Runs one command in the checkout with the bound of the cell and keeps what it prints and its exit status.
# Arguments: name, bound in seconds, text for the standard input, command... (the command has its environment prefix)
function invoke {
    local name="$1" bound="$2" input="$3" status=0
    shift 3
    printf "%s\n" "$*" > "${OUTPUT}/${name}.command"
    printf "%s" "${input}" | timeout -s KILL "${bound}" "$@" > "${OUTPUT}/${name}.stdout" 2> "${OUTPUT}/${name}.stderr" || status=$?
    printf "%s\n" "${status}" > "${OUTPUT}/${name}.rc"
}

# Runs one case under the loader trace. Arguments: name, command...
function run_case {
    local name="$1"
    shift
    invoke "${name}" "${CASE_SECONDS}" $'exit\n' "${ENV_CASE[@]}" "$@"
}

# Runs the IOC on a startup file of the case. Arguments: name, script text
function ioc {
    local name="$1" script="$2"
    printf "on error break\ndbLoadDatabase(\"%s\")\nsnmp3Ioc_registerRecordDeviceDriver(pdbbase)\n%s" \
        "${ROOT}/dbd/snmp3Ioc.dbd" "${script}" > "${OUTPUT}/${name}.cmd"
    run_case "${name}" "${ROOT}/bin/${ARCH}/snmp3Ioc" "${OUTPUT}/${name}.cmd"
}

# Starts a caRepeater of the cell and waits until it owns its UDP port.
function start_repeater {
    local server repeat started alive
    server="$(perl "${JUDGE}" port)" || die "no free UDP port"
    repeat="$(perl "${JUDGE}" port)" || die "no free UDP port"
    while [[ "${server}" == "${repeat}" ]]; do
        repeat="$(perl "${JUDGE}" port)" || die "no free UDP port"
    done
    printf "%s %s\n" "${server}" "${repeat}" > "${OUTPUT}/ports.txt"
    ENV_CASE=("${ENV_BASE[@]}" "EPICS_CA_SERVER_PORT=${server}" "EPICS_CA_REPEATER_PORT=${repeat}" EPICS_CA_AUTO_ADDR_LIST=NO
        EPICS_CA_ADDR_LIST=127.0.0.1 EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 EPICS_CAS_BEACON_AUTO_ADDR_LIST=NO
        EPICS_CAS_BEACON_ADDR_LIST=127.0.0.1)
    "${ENV_CASE[@]}" "${BASE}/bin/${ARCH}/caRepeater" < /dev/null > "${OUTPUT}/repeater.stdout" 2> "${OUTPUT}/repeater.stderr" &
    REPEATER_PID=$!
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${CASE_SECONDS} ]]; do
        alive="$(ps -o stat= -p "${REPEATER_PID}" 2> /dev/null)"
        [[ -n "${alive}" && "${alive}" != Z* ]] || break
        if perl "${JUDGE}" owns "${REPEATER_PID}" "${repeat}"; then
            break
        fi
        sleep 0.02
    done
    alive="$(ps -o stat= -p "${REPEATER_PID}" 2> /dev/null)"
    if [[ -n "${alive}" && "${alive}" != Z* ]] && perl "${JUDGE}" owns "${REPEATER_PID}" "${repeat}"; then
        printf "1\n" > "${OUTPUT}/repeater.owned"
    else
        printf "0\n" > "${OUTPUT}/repeater.owned"
    fi
    # The loader trace is for the cases only, never for the repeater.
    ENV_CASE+=(LD_DEBUG=libs)
}

# Ends the repeater with SIGTERM, waits for it, and keeps its exit status; a repeater that does not end is killed.
function stop_repeater {
    local started status=0 forced=0 alive
    [[ -n "${REPEATER_PID}" ]] || return 0
    kill -TERM "${REPEATER_PID}" 2> /dev/null
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${CASE_SECONDS} ]]; do
        alive="$(ps -o stat= -p "${REPEATER_PID}" 2> /dev/null)"
        [[ -n "${alive}" && "${alive}" != Z* ]] || break
        sleep 0.02
    done
    alive="$(ps -o stat= -p "${REPEATER_PID}" 2> /dev/null)"
    if [[ -n "${alive}" && "${alive}" != Z* ]]; then
        kill -KILL "${REPEATER_PID}" 2> /dev/null
        forced=1
    fi
    wait "${REPEATER_PID}" 2> /dev/null || status=$?
    printf "%s\n%s\n" "${REPEATER_PID}" "${status}" > "${OUTPUT}/repeater.returncode"
    printf "%s\n" "${forced}" > "${OUTPUT}/repeater.forced"
    REPEATER_PID=""
}

# Writes the SHA-256 of the inputs of the run to the named file, one line each, with the tool of coreutils.
function digest_sources {
    local -a files=()
    local file
    for file in "${ROOT}"/snmp3App/src/*.cpp "${ROOT}"/snmp3App/src/*.h; do
        files+=("${file}")
    done
    files+=("${ROOT}/snmp3App/src/Makefile" "${ROOT}/tests/rewrite/Makefile" "${ROOT}/tests/rewrite/LifecycleTest.cpp"
        "${HERE}/test_lifecycle.bash" "${JUDGE}" "${ROOT}/tests/rewrite/db/lifecycle.db"
        "${ROOT}/bin/${ARCH}/snmp3Ioc" "${ROOT}/bin/${ARCH}/snmp3LifecycleTest" "${ROOT}/lib/${ARCH}/libsnmp3.so"
        "${ROOT}/dbd/snmp3Ioc.dbd" "${ROOT}/dbd/snmp3LifecycleTest.dbd"
        "${ROOT}/snmp3App/src/O.${ARCH}/snmp3Ioc_registerRecordDeviceDriver.cpp")
    sha256sum -- "${files[@]}" > "$1" || die "cannot digest the inputs of the run"
}

# Gathers the observations stage by stage and ends at the first stage that the judge fails.
function gather {
    local db="${ROOT}/tests/rewrite/db/lifecycle.db" load product="${ROOT}/bin/${ARCH}/snmp3LifecycleTest"
    invoke independence "${INDEPENDENCE_SECONDS}" "" "${ENV_BASE[@]}" python3 "${ROOT}/tests/rewrite/run_independence.py" \
        --base "${BASE}" --output "${OUTPUT}/independence"
    stage independence || return 0
    invoke test-build "${BUILD_SECONDS}" "" "${ENV_BASE[@]}" make -C tests/rewrite -B -j2
    stage build || return 0
    digest_sources "${OUTPUT}/sha256.before"
    start_repeater
    stage repeater || return 0
    load="dbLoadRecords(\"${db}\", \"P=Lifecycle_\")"$'\n'
    ioc normal "${load}"$'iocInit\nsnmp3RuntimeReport\ndbpr("Lifecycle_PiniProbe", 2)\n'
    stage normal || return 0
    ioc cold $'snmp3RuntimeReport\n'
    stage cold || return 0
    ioc before-init-error $'noSuchLifecycleCommand\necho MUST_NOT_RUN\n'
    stage before-init-error || return 0
    ioc partial-build-error "${load}"$'iocBuild\nsnmp3RuntimeReport\nnoSuchLifecycleCommand\n'
    stage partial-build-error || return 0
    ioc explicit-stop "${load}"$'iocInit\nsnmp3Stop\nsnmp3Stop\nsnmp3RuntimeReport\n'
    stage explicit-stop || return 0
    run_case module "${product}" module
    stage module || return 0
    printf "on error continue\necho SUCCESSFUL_SCRIPT\n" > "${OUTPUT}/continue.cmd"
    run_case thread-failure "${product}" failure "${OUTPUT}/continue.cmd"
    stage thread-failure || return 0
    run_case isolated "${product}" isolated "${ROOT}/dbd/snmp3LifecycleTest.dbd" "${db}"
    stage isolated || return 0
    digest_sources "${OUTPUT}/sha256.after"
    return 0
}

function main {
    local base="" output="" local_release status=0
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --base) base="${2:-}"; shift 2 ;;
            --output) output="${2:-}"; shift 2 ;;
            *) die "usage: test_lifecycle.bash --base EPICS_BASE_DIRECTORY --output NEW_DIRECTORY" ;;
        esac
    done
    [[ -n "${base}" && -n "${output}" ]] || die "--base and --output are required"
    BASE="$(validate_base "${base}")" || exit 2
    [[ ! -e "${output}" ]] || die "--output must be a new directory"
    local_release="${ROOT}/configure/RELEASE.local"
    if [[ -e "${local_release}" ]] && ! printf "EPICS_BASE = %s\n" "${BASE}" | cmp -s - "${local_release}"; then
        die "existing configure/RELEASE.local differs"
    fi
    mkdir -p "${output}" || die "cannot create ${output}"
    chmod 700 "${output}" || die "cannot set the mode of ${output}"
    OUTPUT="$(cd "${output}" && pwd -P)"
    printf "%s\n" "${BASE}" > "${OUTPUT}/base.txt"
    cd "${ROOT}" || die "cannot enter the checkout"
    ENV_BASE=(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG -u LD_DEBUG_OUTPUT -u LD_LIBRARY_PATH "EPICS_HOST_ARCH=${ARCH}"
        "LD_LIBRARY_PATH=${ROOT}/lib/${ARCH}:${BASE}/lib/${ARCH}")
    ENV_CASE=("${ENV_BASE[@]}")
    gather
    stop_repeater
    perl "${JUDGE}" judge "${OUTPUT}" || status=$?
    [[ ${status} -le 1 ]] || die "the judge could not read the observations"
    exit "${status}"
}

main "$@"
