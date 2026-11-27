#!/usr/bin/env bash
# Record cell: runs the record test program (snmp3RecordTest, with the actual DSET, the callback path, the worker and the native
# library) against a fixture agent on a loopback port, and records.pl judges what they produced. Only the case baseline is moved so
# far; the other cases of the former harness test_records.py still run there. The cell starts the fixture agent, writes the
# configuration, runs the program, stops the agent and judges.
# usage: test_records.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers] [--case baseline]
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged (a case that is not moved is refused)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/records.pl"
readonly ARCH="linux-x86_64"
readonly AGENT_USER="fixtureUser"
readonly AGENT_NAME="agent-ipv4-1"
readonly READY_SECONDS=5
readonly STOP_SECONDS=8
readonly PROCESS_SECONDS=30
readonly -a CASES=(baseline)

OUTPUT=""
PRODUCTS="${ROOT}/bin/${ARCH}"
SANITIZERS=0
AGENT_PID=""
declare -a ENV_RUN=()

function die {
    printf "test_records: %s\n" "$1" >&2
    stop_agent
    exit 2
}

# Writes a secret file of 48 hexadecimal characters that only the owner can read.
function new_secret {
    local path="$1"
    (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${path}" && printf "\n" >> "${path}") ||
        die "cannot write a secret file"
}

# Ends the agent with SIGTERM, waits for it and keeps its exit status and whether it had to be killed in OUTPUT/NAME.stop.
function stop_agent {
    local status=0 forced=0 started alive
    [[ -n "${AGENT_PID}" ]] || return 0
    kill -TERM "${AGENT_PID}" 2> /dev/null
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${STOP_SECONDS} ]]; do
        alive="$(ps -o stat= -p "${AGENT_PID}" 2> /dev/null)"
        [[ -n "${alive}" && "${alive}" != Z* ]] || break
        sleep 0.01
    done
    alive="$(ps -o stat= -p "${AGENT_PID}" 2> /dev/null)"
    if [[ -n "${alive}" && "${alive}" != Z* ]]; then
        kill -KILL "${AGENT_PID}" 2> /dev/null
        forced=1
    fi
    wait "${AGENT_PID}" 2> /dev/null || status=$?
    printf "%s\t%s\n" "${status}" "${forced}" > "${OUTPUT}/${AGENT_NAME}.stop"
    AGENT_PID=""
}

# Starts the fixture agent and waits until it says that it is ready. Returns 0 when it is ready.
function start_agent {
    local port community auth privacy started
    port="$(perl "${JUDGE}" port)" || die "no free UDP port"
    new_secret "${OUTPUT}/${AGENT_NAME}-community"
    new_secret "${OUTPUT}/${AGENT_NAME}-auth"
    new_secret "${OUTPUT}/${AGENT_NAME}-privacy"
    read -r community < "${OUTPUT}/${AGENT_NAME}-community"
    read -r auth < "${OUTPUT}/${AGENT_NAME}-auth"
    read -r privacy < "${OUTPUT}/${AGENT_NAME}-privacy"
    (umask 077 && printf "%s\n" \
        "com2sec nativeComV4 127.0.0.1 ${community}" \
        "com2sec6 nativeComV6 ::1 ${community}" \
        "group nativeCommunityGroup v1 nativeComV4" \
        "group nativeCommunityGroup v2c nativeComV4" \
        "group nativeCommunityGroup v1 nativeComV6" \
        "group nativeCommunityGroup v2c nativeComV6" \
        "access nativeCommunityGroup \"\" any noauth exact nativeView nativeView none" \
        "createUser ${AGENT_USER} SHA ${auth} AES ${privacy}" \
        "group nativeGroup usm ${AGENT_USER}" \
        "view nativeView included .1.3.6.1.4.1.53864" \
        "view nativeView included .1.3.6.1.6.3.10.2.1.4" \
        "access nativeGroup \"\" usm noauth prefix nativeView nativeView none" \
        "createUser independentFixtureUser SHA ${auth} AES ${privacy}" \
        "group nativeGroup usm independentFixtureUser" > "${OUTPUT}/${AGENT_NAME}.conf") || die "cannot write the agent configuration"
    "${ENV_RUN[@]}" "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${OUTPUT}/${AGENT_NAME}.conf" \
        > "${OUTPUT}/${AGENT_NAME}.stdout" 2> "${OUTPUT}/${AGENT_NAME}.stderr" &
    AGENT_PID=$!
    printf "%s\n" "${port}" > "${OUTPUT}/${AGENT_NAME}.port"
    printf "0\n" > "${OUTPUT}/${AGENT_NAME}.ready"
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${READY_SECONDS} ]] && kill -0 "${AGENT_PID}" 2> /dev/null; do
        if grep -q '"event":"agent_ready"' "${OUTPUT}/${AGENT_NAME}.stdout" 2> /dev/null; then
            printf "1\n" > "${OUTPUT}/${AGENT_NAME}.ready"
            perl "${JUDGE}" sockets "${AGENT_PID}" "${port}" "${OUTPUT}/${AGENT_NAME}.sockets.json" ||
                die "cannot read the sockets of the agent"
            return 0
        fi
        sleep 0.01
    done
    return 1
}

# Runs the case baseline. Sets the status that the judge returns in CASE_STATUS.
function run_case {
    local case="$1" port aborted=0 status=0
    printf "%s\n" "${PRODUCTS}" > "${OUTPUT}/products.txt"
    if start_agent; then
        port="$(< "${OUTPUT}/${AGENT_NAME}.port")"
        perl "${JUDGE}" config "${OUTPUT}" "${port}" || die "cannot write the configuration"
        timeout -s KILL "${PROCESS_SECONDS}" "${ENV_RUN[@]}" "${PRODUCTS}/snmp3RecordTest" "${OUTPUT}/records.json" \
            "${PRODUCTS}/snmp3NativeProbe" "${PRODUCTS}/snmp3Worker" "${ROOT}/dbd/snmp3RecordTest.dbd" \
            "${ROOT}/tests/rewrite/db/records.db" "${ROOT}/tests/rewrite/db/record-output.db" "${OUTPUT}/workers.stderr" "${case}" \
            < /dev/null > "${OUTPUT}/records.stdout" 2> "${OUTPUT}/records.stderr" || status=$?
        # An exit of the program under a kill by the bound (124 or 137) is a forced end.
        if [[ ${status} -eq 124 || ${status} -eq 137 ]]; then
            printf "%s\t1\n" "${status}" > "${OUTPUT}/records.rc"
        else
            printf "%s\t0\n" "${status}" > "${OUTPUT}/records.rc"
        fi
    else
        aborted=1
    fi
    [[ ${aborted} -eq 0 ]] || : > "${OUTPUT}/aborted"
    stop_agent
    CASE_STATUS=0
    perl "${JUDGE}" judge "${OUTPUT}" "${case}" || CASE_STATUS=$?
}

function main {
    local output="" selected=baseline case any=0
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --products) PRODUCTS="$(cd "${2:-}" 2> /dev/null && pwd -P)" || die "--products must name a directory"; shift 2 ;;
            --sanitizers) SANITIZERS=1; shift ;;
            --case) selected="${2:-}"; shift 2 ;;
            *) die "usage: test_records.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers] [--case baseline]" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    [[ ! -e "${output}" ]] || die "--output must be a new directory"
    for case in "${CASES[@]}"; do
        [[ "${case}" != "${selected}" ]] || any=1
    done
    [[ ${any} -eq 1 ]] || die "case ${selected} is not moved to this cell; test_records.py runs it"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd -P)"
    for case in snmp3RecordTest snmp3Worker snmp3NativeAgent snmp3NativeProbe; do
        [[ -x "${PRODUCTS}/${case}" ]] || die "product ${case} not found in ${PRODUCTS}"
    done
    cd "${ROOT}" || die "cannot enter the checkout"
    ENV_RUN=(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG_OUTPUT -u LD_LIBRARY_PATH MIBS= LD_DEBUG=libs)
    if [[ ${SANITIZERS} -eq 1 ]]; then
        ENV_RUN+=(ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
    fi
    run_case "${selected}"
    [[ ${CASE_STATUS} -le 1 ]] || die "the judge could not read the observations of ${selected}"
    exit "${CASE_STATUS}"
}

main "$@"
