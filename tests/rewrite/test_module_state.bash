#!/usr/bin/env bash
# Module state cell: a production IOC with the state databases the module installs into db/ and three SNMPv3
# endpoints on one address. Endpoint A keeps its agent and has a group that the agent rejects as too large; the
# agents of B and C are stopped and return with another engine ID, B with the same secrets and C with others.
# A second IOC loads the endpoint database for an endpoint the configuration does not hold.
# This script starts and stops the processes and decides the order of the steps; module_state.pl reads the records
# and the runtime report and judges the retained observations afterwards.
# usage: test_module_state.bash --output NEW_DIRECTORY [--products DIRECTORY]
# exit status: 0 every check passed, 1 a check failed, 2 the scenario could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/module_state.pl"
readonly GROUP_DATABASE="${HERE}/db/request-sizing.db"
readonly ENDPOINT_DATABASE="${HERE}/db/recovery.db"
readonly AGENT_USER="fixtureUser"
readonly AGENT_VIEW=".1.3.6.1.4.1.53864"
readonly ENGINE_B="8000000001aabbccdd"
readonly ENGINE_C="8000000001c0c1c2c3"
readonly RETURNED_ENGINE_B="80000000010a0b0c0d"
readonly RETURNED_ENGINE_C="80000000010e0f1011"
readonly READY_TRIES=1500
readonly PAUSE="0.01"
readonly STOP_TRIES=500
readonly GROUP_ROUNDS=4
readonly RETURN_SECONDS=40
readonly SCAN_SECONDS=5

declare -a CHILDREN=()

function stop_children {
    local pid tries
    for pid in "${CHILDREN[@]:-}"; do
        [[ -n "${pid}" ]] || continue
        kill -TERM "${pid}" 2>/dev/null || continue
        tries=0
        while kill -0 "${pid}" 2>/dev/null && [[ ${tries} -lt ${STOP_TRIES} ]]; do
            sleep "${PAUSE}"
            tries=$((tries + 1))
        done
        kill -KILL "${pid}" 2>/dev/null
        wait "${pid}" 2>/dev/null
    done
    CHILDREN=()
}

function wait_for_text {
    local file="$1" text="$2" pid="$3" tries=0
    while [[ ${tries} -lt ${READY_TRIES} ]]; do
        if [[ -s "${file}" ]] && grep -q -- "${text}" "${file}"; then
            return 0
        fi
        kill -0 "${pid}" 2>/dev/null || return 1
        sleep "${PAUSE}"
        tries=$((tries + 1))
    done
    return 1
}

# Writes three secret files and the agent configuration that uses them into a new directory.
function new_secrets {
    local directory="$1" name community auth privacy
    mkdir "${directory}" || return 1
    for name in community auth privacy; do
        (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${directory}/${name}" &&
            printf "\n" >> "${directory}/${name}") || return 1
    done
    read -r community < "${directory}/community"
    read -r auth < "${directory}/auth"
    read -r privacy < "${directory}/privacy"
    (umask 077 && printf "%s\n" \
        "rwcommunity ${community} 127.0.0.1 ${AGENT_VIEW}" \
        "createUser ${AGENT_USER} SHA ${auth} AES ${privacy}" \
        "group nativeGroup usm ${AGENT_USER}" \
        "view nativeView included ${AGENT_VIEW}" \
        "access nativeGroup \"\" usm noauth prefix nativeView nativeView none" > "${directory}/agent.conf") || return 1
}

# Starts an agent and waits for its ready line. Arguments: output file stem, port, configuration, engine ID or an
# empty word. Sets AGENT_PID.
function start_agent {
    local stem="$1" port="$2" conf="$3" engine="$4"
    if [[ -n "${engine}" ]]; then
        "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${conf}" "${engine}" > "${stem}.stdout" 2> "${stem}.stderr" &
    else
        "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${conf}" > "${stem}.stdout" 2> "${stem}.stderr" &
    fi
    AGENT_PID=$!
    CHILDREN+=("${AGENT_PID}")
    wait_for_text "${stem}.stdout" '"event":"agent_ready"' "${AGENT_PID}"
}

function stop_agent {
    local pid="$1" tries=0
    kill -TERM "${pid}" 2>/dev/null
    while kill -0 "${pid}" 2>/dev/null && [[ ${tries} -lt ${STOP_TRIES} ]]; do
        sleep "${PAUSE}"
        tries=$((tries + 1))
    done
    kill -0 "${pid}" 2>/dev/null && return 1
    wait "${pid}" 2>/dev/null
    return 0
}

# Starts an IOC from the startup lines given as arguments and waits until its initialization is complete. A
# state database that the tree does not hold fails its load line and the IOC starts without those records, so
# that their checks fail instead of the run. Sets IOC_PID and IOC_HANDLE.
function start_ioc {
    local directory="$1"
    shift
    printf "%s\n" "$@" > "${directory}/startup.cmd" || return 1
    mkfifo "${directory}/ioc.stdin" || return 1
    MIBS="" stdbuf -oL "${PRODUCTS}/snmp3Ioc" "${directory}/startup.cmd" \
        < "${directory}/ioc.stdin" > "${directory}/ioc.stdout" 2> "${directory}/ioc.stderr" &
    IOC_PID=$!
    CHILDREN+=("${IOC_PID}")
    exec {IOC_HANDLE}> "${directory}/ioc.stdin"
    wait_for_text "${directory}/ioc.stderr" "iocRun: All initialization complete" "${IOC_PID}"
}

function finish_ioc {
    local tries=0
    printf "exit\n" >&"${IOC_HANDLE}"
    exec {IOC_HANDLE}>&-
    while kill -0 "${IOC_PID}" 2>/dev/null && [[ ${tries} -lt ${STOP_TRIES} ]]; do
        sleep "${PAUSE}"
        tries=$((tries + 1))
    done
}

function tool {
    perl "${TOOL}" "$@" >> "${OUTPUT}/tool.stdout" 2>> "${OUTPUT}/tool.stderr"
}

function round_group {
    tool round "${MAIN}" "${MAIN}/ioc.stdin" A_Round A_R1 A_R2 A_R3 A_R4 A_R5 A_R6 A_R7 A_R8
}

function round_endpoint {
    local name="$1"
    tool round "${MAIN}" "${MAIN}/ioc.stdin" "${name}_Round" "${name}_R1" "${name}_R2" "${name}_Sys"
}

function observe {
    local label="$1"
    tool observe "${MAIN}" "${MAIN}/ioc.stdin" "${MAIN}/ioc.stdout" "${label}" \
        S_RuntimeState SA_PeerState SA_PeerState.SEVR SB_PeerState SB_PeerState.SEVR SC_PeerState SC_PeerState.SEVR \
        SA_BatchLimit SA_BatchSplits SB_BatchLimit SB_BatchSplits A_R8.AMSG
}

function await_change {
    tool await "${MAIN}" "${MAIN}/ioc.stdin" "$1" "$2" "${SCAN_SECONDS}"
}

function scenario {
    local port_a port_b port_c agent_b agent_c count end
    local -a startup
    new_secrets "${OUTPUT}/secrets" || return 2
    new_secrets "${OUTPUT}/secrets-other" || return 2
    port_a="$(perl "${TOOL}" port)" || return 2
    port_b="$(perl "${TOOL}" port)" || return 2
    port_c="$(perl "${TOOL}" port)" || return 2
    start_agent "${OUTPUT}/agent-A" "${port_a}" "${OUTPUT}/secrets/agent.conf" "" || return 2
    start_agent "${OUTPUT}/agent-B" "${port_b}" "${OUTPUT}/secrets/agent.conf" "${ENGINE_B}" || return 2
    agent_b="${AGENT_PID}"
    start_agent "${OUTPUT}/agent-C" "${port_c}" "${OUTPUT}/secrets/agent.conf" "${ENGINE_C}" || return 2
    agent_c="${AGENT_PID}"
    perl "${TOOL}" config "${OUTPUT}/configuration.json" "${OUTPUT}/secrets" "${port_a}" "${port_b}" "${port_c}" || return 2

    MAIN="${OUTPUT}/main"
    mkdir "${MAIN}" || return 2
    startup=("on error break" "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")"
        "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")"
        "snmp3Load(\"${OUTPUT}/configuration.json\",\"${PRODUCTS}/snmp3NativeProbe\")"
        "dbLoadRecords(\"${GROUP_DATABASE}\",\"P=A_\")"
        "dbLoadRecords(\"${ENDPOINT_DATABASE}\",\"P=B_,B=B\")"
        "dbLoadRecords(\"${ENDPOINT_DATABASE}\",\"P=C_,B=C\")"
        "on error continue"
        "dbLoadRecords(\"${ROOT}/db/snmp3Runtime.db\",\"P=S_\")"
        "dbLoadRecords(\"${ROOT}/db/snmp3Endpoint.db\",\"P=SA_,ENDPOINT=A\")"
        "dbLoadRecords(\"${ROOT}/db/snmp3Endpoint.db\",\"P=SB_,ENDPOINT=B\")"
        "dbLoadRecords(\"${ROOT}/db/snmp3Endpoint.db\",\"P=SC_,ENDPOINT=C\")"
        "iocInit")
    start_ioc "${MAIN}" "${startup[@]}" || return 2

    # One pass of B and C lets the worker learn the engine ID of each agent before any agent is replaced.
    round_endpoint B || return 2
    round_endpoint C || return 2
    observe before || return 2

    # The group of eight on A is rejected three times, at 8, 4 and 2 values, and stays at the limit 1.
    count=0
    while [[ ${count} -lt ${GROUP_ROUNDS} ]]; do
        round_group || return 2
        count=$((count + 1))
    done
    await_change SA_BatchSplits 0 || return 2
    await_change SA_BatchLimit 0 || return 2
    observe sized || return 2

    stop_agent "${agent_b}" || return 2
    stop_agent "${agent_c}" || return 2
    start_agent "${OUTPUT}/agent-B2" "${port_b}" "${OUTPUT}/secrets/agent.conf" "${RETURNED_ENGINE_B}" || return 2
    start_agent "${OUTPUT}/agent-C2" "${port_c}" "${OUTPUT}/secrets-other/agent.conf" "${RETURNED_ENGINE_C}" || return 2
    # The log line of each endpoint is the sign that its state is settled; the bound ends the wait when a defect
    # keeps a line away, and the observation that follows is judged as it is.
    end=$((SECONDS + RETURN_SECONDS))
    while [[ ${SECONDS} -lt ${end} ]]; do
        round_endpoint B || return 2
        round_endpoint C || return 2
        if grep -q "^snmp3 engine: endpoint=B " "${MAIN}/ioc.stderr" &&
            grep -q "^snmp3 engine: endpoint=C " "${MAIN}/ioc.stderr"; then
            break
        fi
        sleep 1
    done
    await_change SB_PeerState normal || return 2
    await_change SC_PeerState normal || return 2
    observe returned || return 2

    tool send "${MAIN}" "${MAIN}/ioc.stdin" snmp3Stop || return 2
    await_change S_RuntimeState Running || return 2
    observe stopped || return 2
    finish_ioc

    mkdir "${OUTPUT}/unknown" || return 2
    startup=("on error break" "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")"
        "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")"
        "snmp3Load(\"${OUTPUT}/configuration.json\",\"${PRODUCTS}/snmp3NativeProbe\")"
        "on error continue"
        "dbLoadRecords(\"${ROOT}/db/snmp3Endpoint.db\",\"P=X_,ENDPOINT=Unknown\")"
        "iocInit")
    start_ioc "${OUTPUT}/unknown" "${startup[@]}" || return 2
    finish_ioc
    : > "${OUTPUT}/complete"
}

function main {
    local status=0
    OUTPUT=""
    PRODUCTS=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) OUTPUT="${2:-}"; shift 2 ;;
            --products) PRODUCTS="${2:-}"; shift 2 ;;
            *) printf "usage: test_module_state.bash --output NEW_DIRECTORY [--products DIRECTORY]\n" >&2; exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_module_state: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] ||
        { printf "test_module_state: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_module_state: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
    OUTPUT="$(cd "${OUTPUT}" && pwd)"
    trap stop_children EXIT
    scenario || status=$?
    stop_children
    [[ ${status} -eq 0 ]] || cat "${OUTPUT}/tool.stderr" >&2
    perl "${TOOL}" judge "${OUTPUT}"
    exit $?
}

main "$@"
