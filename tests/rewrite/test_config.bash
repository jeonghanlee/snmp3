#!/usr/bin/env bash
# Configuration cell: runs the shipped configuration, native capability, IOC and regression paths. The cell runs the lifecycle cell,
# digests the inputs of the run, asks the native probe for its capabilities, and then calls config_fixtures.pl, which writes the
# configuration documents, secret files, helper links and IOC startup files and the list of the remaining runs (cases.tsv). The
# cell makes every run of the list with LD_DEBUG=libs, so that the libraries that the process loaded are known, starts a caRepeater
# of its own before the IOC runs, and ends it at the end. config.pl judges: after each group of runs the judge is asked whether a
# check has failed, and the gathering ends when one has; at the end the judge reads the observations in the order of the checks and
# writes results.json. A line of cases.tsv has these tab separated columns: kind (component, run, ioc, stage, repeater), name,
# expected exit status, loader trace (1 or 0), standard input ("exit" or "-"), bound in seconds, extras ("-" or a list of
# bounded and reaped=PIDFILE), and the command with its arguments. The lines stage and repeater have only kind and name.
# usage: test_config.bash --base EPICS_BASE_DIRECTORY --output NEW_DIRECTORY
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/config.pl"
readonly GENERATOR="${HERE}/config_fixtures.pl"
readonly ARCH="linux-x86_64"
readonly RUN_SECONDS=30
readonly LIFECYCLE_SECONDS=420

BASE=""
OUTPUT=""
REPEATER_PID=""
declare -a ENV_BASE=()
declare -a ENV_RUN=()

function die {
    printf "test_config: %s\n" "$1" >&2
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
    printf "%s\n" "${resolved}"
}

# Asks the judge about the checks up to the named stage: returns 0 when none has failed, 1 when one has.
function stage {
    local status=0
    perl "${JUDGE}" judge "${OUTPUT}" --stage "$1" > /dev/null || status=$?
    [[ ${status} -le 1 ]] || die "the judge could not read the observations of the stage $1"
    return "${status}"
}

# Runs one command in the checkout with a bound and keeps what it prints, its exit status and its time in microseconds.
# Arguments: name, bound in seconds, standard input ("exit" or "-"), command... (the command has its environment prefix)
function invoke {
    local name="$1" bound="$2" input="$3" status=0 started finished
    shift 3
    printf "%s\n" "$*" > "${OUTPUT}/${name}.command"
    started="$(date +%s%N)"
    if [[ "${input}" == "exit" ]]; then
        printf "exit\n" | timeout -s KILL "${bound}" "$@" > "${OUTPUT}/${name}.stdout" 2> "${OUTPUT}/${name}.stderr" || status=$?
    else
        timeout -s KILL "${bound}" "$@" < /dev/null > "${OUTPUT}/${name}.stdout" 2> "${OUTPUT}/${name}.stderr" || status=$?
    fi
    finished="$(date +%s%N)"
    printf "%s\n" "${status}" > "${OUTPUT}/${name}.rc"
    printf "%s\n" "$(((finished - started) / 1000))" > "${OUTPUT}/${name}.elapsed"
}

# Starts a caRepeater of the cell and waits until it owns its UDP port; the environment of the later runs names the ports.
function start_repeater {
    local server repeat started alive operator
    server="$(perl "${JUDGE}" port)" || die "no free UDP port"
    repeat="$(perl "${JUDGE}" port)" || die "no free UDP port"
    while [[ "${server}" == "${repeat}" ]]; do
        repeat="$(perl "${JUDGE}" port)" || die "no free UDP port"
    done
    printf "%s %s\n" "${server}" "${repeat}" > "${OUTPUT}/ports.txt"
    operator="$(< "${OUTPUT}/operator-example.path")"
    ENV_RUN=("${ENV_BASE[@]}" "EPICS_CA_SERVER_PORT=${server}" "EPICS_CA_REPEATER_PORT=${repeat}" EPICS_CA_AUTO_ADDR_LIST=NO
        EPICS_CA_ADDR_LIST=127.0.0.1 EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 EPICS_CAS_BEACON_AUTO_ADDR_LIST=NO
        EPICS_CAS_BEACON_ADDR_LIST=127.0.0.1)
    "${ENV_RUN[@]}" "${BASE}/bin/${ARCH}/caRepeater" < /dev/null > "${OUTPUT}/repeater.stdout" 2> "${OUTPUT}/repeater.stderr" &
    REPEATER_PID=$!
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${RUN_SECONDS} ]]; do
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
    # The IOC runs of the cell read the operator example, the native probe and the worker from the environment.
    ENV_RUN+=("CONFIG_JSON=${operator}" "NATIVE_PROBE=${ROOT}/bin/${ARCH}/snmp3NativeProbe" "WORKER_PATH=${ROOT}/bin/${ARCH}/snmp3Worker"
        LD_DEBUG=libs)
}

# Ends the repeater with SIGTERM, waits for it, and keeps its exit status; a repeater that does not end is killed.
function stop_repeater {
    local started status=0 forced=0 alive
    [[ -n "${REPEATER_PID}" ]] || return 0
    kill -TERM "${REPEATER_PID}" 2> /dev/null
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${RUN_SECONDS} ]]; do
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

# Writes the SHA-256 of the inputs of the run to the named file with the tool of coreutils: the files of four directories of the
# checkout except what the build writes (a name that begins with O. and __pycache__), the top Makefile, two headers of Base and
# the products that the cell runs.
function digest_inputs {
    local directory
    : > "$1"
    for directory in snmp3App tests/rewrite docs configure; do
        find -L "${ROOT}/${directory}" \( -name 'O.*' -o -name __pycache__ \) -prune -o -type f -print0 |
            sort -z | xargs -0 sha256sum -- >> "$1" || die "cannot digest the inputs of the run"
    done
    sha256sum -- "${ROOT}/Makefile" "${BASE}/include/epicsVersion.h" "${BASE}/include/yajl_parse.h" \
        "${ROOT}/bin/${ARCH}/snmp3NativeProbe" "${ROOT}/bin/${ARCH}/snmp3ConfigTest" "${ROOT}/bin/${ARCH}/snmp3Ioc" \
        "${ROOT}/lib/${ARCH}/libsnmp3.so" >> "$1" || die "cannot digest the products of the run"
}

# Writes the digests, the metadata and the unreadable files of the fixtures of the run.
function digest_fixtures {
    local path
    : > "${OUTPUT}/fixtures.sha256"
    : > "${OUTPUT}/fixtures.unreadable"
    : > "${OUTPUT}/fixtures.meta"
    for path in "${OUTPUT}"/fixtures/*; do
        [[ -e "${path}" || -L "${path}" ]] || continue
        stat -c '%n	%f	%u	%s' -- "${path}" >> "${OUTPUT}/fixtures.meta"
        if [[ -f "${path}" ]]; then
            if [[ -r "${path}" ]]; then
                sha256sum -- "${path}" >> "${OUTPUT}/fixtures.sha256"
            else
                printf "%s\n" "${path}" >> "${OUTPUT}/fixtures.unreadable"
            fi
        fi
    done
}

# Makes the runs of cases.tsv in order and ends at the first stage that the judge fails.
function run_cases {
    local kind name expected loader input bound extras
    local -a fields=()
    while IFS= read -r line; do
        IFS=$'\t' read -r -a fields <<< "${line}"
        kind="${fields[0]}"
        name="${fields[1]}"
        case "${kind}" in
            stage)
                stage "${name}" || return 0
                ;;
            repeater)
                start_repeater
                stage repeater || return 0
                ;;
            component | run | ioc)
                expected="${fields[2]}"
                loader="${fields[3]}"
                input="${fields[4]}"
                bound="${fields[5]}"
                extras="${fields[6]}"
                : "${expected}" "${loader}" "${extras}"
                invoke "${name}" "${bound}" "${input}" "${ENV_RUN[@]}" "${fields[@]:7}"
                ;;
            *)
                die "unknown line in cases.tsv: ${kind}"
                ;;
        esac
    done < "${OUTPUT}/cases.tsv"
}

# Gathers the observations stage by stage and ends at the first stage that the judge fails.
function gather {
    local fixtures="${OUTPUT}/fixtures"
    : "${fixtures}"
    invoke lifecycle "${LIFECYCLE_SECONDS}" "-" "${ENV_BASE[@]}" bash "${ROOT}/tests/rewrite/test_lifecycle.bash" \
        --base "${BASE}" --output "${OUTPUT}/lifecycle"
    stage lifecycle || return 0
    digest_inputs "${OUTPUT}/sha256.before"
    invoke native-capabilities "${RUN_SECONDS}" "-" "${ENV_BASE[@]}" LD_DEBUG=libs "${ROOT}/bin/${ARCH}/snmp3NativeProbe" --capabilities
    invoke native-cli "${RUN_SECONDS}" "-" "${ENV_BASE[@]}" "${ROOT}/bin/${ARCH}/snmp3NativeProbe" --invalid
    stage native-cli || return 0
    perl "${GENERATOR}" "${OUTPUT}" "${OUTPUT}/native-capabilities.stdout" || die "the generator of the inputs failed"
    ENV_RUN=("${ENV_BASE[@]}" LD_DEBUG=libs)
    run_cases
    digest_inputs "${OUTPUT}/sha256.after"
    return 0
}

function main {
    local base="" output="" local_release status=0
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --base) base="${2:-}"; shift 2 ;;
            --output) output="${2:-}"; shift 2 ;;
            *) die "usage: test_config.bash --base EPICS_BASE_DIRECTORY --output NEW_DIRECTORY" ;;
        esac
    done
    [[ -n "${base}" && -n "${output}" ]] || die "--base and --output are required"
    BASE="$(validate_base "${base}")" || exit 2
    [[ ! -e "${output}" ]] || die "--output must be a new directory"
    output="$(cd "$(dirname "${output}")" 2> /dev/null && pwd -P)/$(basename "${output}")" || die "cannot resolve --output"
    case "${BASE}${output}" in
        *[[:space:]\#\$\"\\]*) die "paths contain unsupported command or Make characters" ;;
    esac
    local_release="${ROOT}/configure/RELEASE.local"
    if [[ -e "${local_release}" ]] && ! printf "EPICS_BASE = %s\n" "${BASE}" | cmp -s - "${local_release}"; then
        die "existing configure/RELEASE.local differs"
    fi
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="${output}"
    mkdir -m 700 "${OUTPUT}/fixtures" || die "cannot create the fixture directory"
    printf "%s\n" "${BASE}" > "${OUTPUT}/base.txt"
    cd "${ROOT}" || die "cannot enter the checkout"
    ENV_BASE=(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG -u LD_DEBUG_OUTPUT -u LD_LIBRARY_PATH "EPICS_HOST_ARCH=${ARCH}"
        "LD_LIBRARY_PATH=${ROOT}/lib/${ARCH}:${BASE}/lib/${ARCH}")
    ENV_RUN=("${ENV_BASE[@]}")
    gather
    stop_repeater
    digest_fixtures
    perl "${JUDGE}" judge "${OUTPUT}" || status=$?
    [[ ${status} -le 1 ]] || die "the judge could not read the observations"
    exit "${status}"
}

main "$@"
