#!/usr/bin/env bash
# Request sizing cell: starts one fixture agent and, per case, the UDP fault proxy and the production IOC, then
# hands the IOC shell to request_sizing.pl, which reads the records and judges. Every case has its own IOC and
# one endpoint, so the batch limit of one case never reaches another.
# usage: test_request_sizing.bash --output NEW_DIRECTORY [--products DIRECTORY]
# exit status: 0 every check passed, 1 a check failed, 2 a case could not be judged (no results.json is written)

set -u

readonly HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly ROOT="$(cd "${HERE}/../.." && pwd)"
readonly JUDGE="${HERE}/request_sizing.pl"
readonly PROXY="${HERE}/helpers/udp_fault.py"
readonly DATABASE="${HERE}/db/request-sizing.db"
readonly AGENT_USER="fixtureUser"
readonly AGENT_VIEW=".1.3.6.1.4.1.53864"
readonly READY_TRIES=1000
readonly READY_PAUSE="0.01"
readonly STOP_TRIES=500

declare -a CHILDREN=()

function die {
    printf "test_request_sizing: %s\n" "$1" >&2
    stop_children
    exit 2
}

# Ends every child that is still alive: SIGTERM first, SIGKILL when it outlives the bound.
function stop_children {
    local pid tries
    for pid in "${CHILDREN[@]:-}"; do
        [[ -n "${pid}" ]] || continue
        kill -TERM "${pid}" 2>/dev/null || continue
        tries=0
        while kill -0 "${pid}" 2>/dev/null && [[ ${tries} -lt ${STOP_TRIES} ]]; do
            sleep "${READY_PAUSE}"
            tries=$((tries + 1))
        done
        kill -KILL "${pid}" 2>/dev/null
        wait "${pid}" 2>/dev/null
    done
    CHILDREN=()
}

# Waits until the file holds the text; fails when the process has ended or the bound has passed.
function wait_for_text {
    local file="$1" text="$2" pid="$3" tries=0
    while [[ ${tries} -lt ${READY_TRIES} ]]; do
        if [[ -s "${file}" ]] && grep -q -- "${text}" "${file}"; then
            return 0
        fi
        kill -0 "${pid}" 2>/dev/null || return 1
        sleep "${READY_PAUSE}"
        tries=$((tries + 1))
    done
    return 1
}

function new_secret {
    local path="$1"
    (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${path}" && printf "\n" >> "${path}") ||
        die "cannot write a secret file"
}

function start_agent {
    local port="$1" directory="$2" community auth privacy
    new_secret "${directory}/community"
    new_secret "${directory}/auth"
    new_secret "${directory}/privacy"
    read -r community < "${directory}/community"
    read -r auth < "${directory}/auth"
    read -r privacy < "${directory}/privacy"
    (umask 077 && printf "%s\n" \
        "rwcommunity ${community} 127.0.0.1 ${AGENT_VIEW}" \
        "createUser ${AGENT_USER} SHA ${auth} AES ${privacy}" \
        "group nativeGroup usm ${AGENT_USER}" \
        "view nativeView included ${AGENT_VIEW}" \
        "access nativeGroup \"\" usm noauth prefix nativeView nativeView none" > "${directory}/agent.conf") ||
        die "cannot write the agent configuration"
    "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${directory}/agent.conf" \
        > "${directory}/agent.stdout" 2> "${directory}/agent.stderr" &
    AGENT_PID=$!
    CHILDREN+=("${AGENT_PID}")
    wait_for_text "${directory}/agent.stdout" '"event":"agent_ready"' "${AGENT_PID}" || die "the agent did not start"
}

# Runs one case and returns the status of the judge: 0 or 1 judged, 2 not judged.
function run_case {
    local case="$1" agent_port="$2" directory="${OUTPUT}/$1" mode port proxy_pid="" ioc_pid status=0 tries=0
    mkdir "${directory}" || return 2
    mode="$(perl "${JUDGE}" proxy "${case}")" || return 2
    port="${agent_port}"
    if [[ -n "${mode}" ]]; then
        python3 "${PROXY}" --family 4 --agent-port "${agent_port}" --mode "${mode}" \
            > "${directory}/proxy.stdout" 2> "${directory}/proxy.stderr" &
        proxy_pid=$!
        CHILDREN+=("${proxy_pid}")
        wait_for_text "${directory}/proxy.stdout" '"event": "fault_ready"' "${proxy_pid}" || return 2
        port="$(sed -n 's/.*"event": "fault_ready".*"port": \([0-9]*\).*/\1/p' "${directory}/proxy.stdout")"
        [[ -n "${port}" ]] || return 2
    fi
    perl "${JUDGE}" config "${case}" "${port}" "${OUTPUT}/agent" "${directory}/configuration.json" || return 2
    printf "%s\n" \
        "on error break" \
        "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")" \
        "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" \
        "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")" \
        "snmp3Load(\"${directory}/configuration.json\",\"${PRODUCTS}/snmp3NativeProbe\")" \
        "dbLoadRecords(\"${DATABASE}\",\"P=Sizing_\")" \
        "iocInit" > "${directory}/startup.cmd" || return 2
    mkfifo "${directory}/ioc.stdin" || return 2
    # The IOC reads its shell from the FIFO; line buffering makes the runtime report reach the file when printed.
    MIBS="" stdbuf -oL "${PRODUCTS}/snmp3Ioc" "${directory}/startup.cmd" \
        < "${directory}/ioc.stdin" > "${directory}/ioc.stdout" 2> "${directory}/ioc.stderr" &
    ioc_pid=$!
    CHILDREN+=("${ioc_pid}")
    # Holds the write side open so that the IOC does not read end of file between two writers.
    exec {HOLD}> "${directory}/ioc.stdin"
    if wait_for_text "${directory}/ioc.stderr" "iocRun: All initialization complete" "${ioc_pid}"; then
        perl "${JUDGE}" drive "${case}" "${directory}" "${directory}/ioc.stdin" "${directory}/ioc.stdout" \
            "${directory}/proxy.stdout" > "${directory}/judge.stdout" 2> "${directory}/judge.stderr" || status=$?
    else
        status=2
    fi
    exec {HOLD}>&-
    while kill -0 "${ioc_pid}" 2>/dev/null && [[ ${tries} -lt ${STOP_TRIES} ]]; do
        sleep "${READY_PAUSE}"
        tries=$((tries + 1))
    done
    stop_children_of_case "${ioc_pid}" "${proxy_pid}"
    cat "${directory}/judge.stdout" 2>/dev/null
    [[ ${status} -le 1 ]] || cat "${directory}/judge.stderr" 2>/dev/null >&2
    return "${status}"
}

# Ends the IOC and the proxy of one case and leaves the agent running.
function stop_children_of_case {
    local agent="${AGENT_PID}"
    CHILDREN=("$@")
    stop_children
    CHILDREN=("${agent}")
}

function main {
    local agent_port case status incomplete=0 cases
    OUTPUT=""
    PRODUCTS=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) OUTPUT="${2:-}"; shift 2 ;;
            --products) PRODUCTS="${2:-}"; shift 2 ;;
            *) printf "usage: test_request_sizing.bash --output NEW_DIRECTORY [--products DIRECTORY]\n" >&2; exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || die "--output is required"
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] || die "products not found in ${PRODUCTS}"
    mkdir -m 700 "${OUTPUT}" || die "cannot create ${OUTPUT}"
    OUTPUT="$(cd "${OUTPUT}" && pwd)"
    mkdir "${OUTPUT}/agent" || die "cannot create the agent directory"
    trap stop_children EXIT
    agent_port="$(perl "${JUDGE}" port)" || die "no free port"
    start_agent "${agent_port}" "${OUTPUT}/agent"
    cases="$(perl "${JUDGE}" cases)" || die "cannot list the cases"
    for case in ${cases}; do
        status=0
        run_case "${case}" "${agent_port}" || status=$?
        if [[ ${status} -gt 1 ]]; then
            printf "INCOMPLETE %s\n" "${case}"
            incomplete=1
        fi
    done
    stop_children
    if [[ ${incomplete} -ne 0 ]]; then
        exit 2
    fi
    # shellcheck disable=SC2086
    perl "${JUDGE}" summary "${OUTPUT}" ${cases}
    exit $?
}

main "$@"
