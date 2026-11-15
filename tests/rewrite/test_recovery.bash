#!/usr/bin/env bash
# Recovery cell: fixture agents are lost and returned while production IOCs keep running. This script starts and
# stops the processes and decides the instants; recovery.pl takes the samples and judges them afterwards.
# Every scenario has its own agents, IOCs and directory, and the scenarios run side by side.
# usage: test_recovery.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only SCENARIO]
# RECOVERY_NO_SYSTEM_FIELD=1 leaves the system information field out of the configurations, for a product that
# does not know the field.
# exit status: 0 every check passed, 1 a check failed, 2 a scenario could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/recovery.pl"
readonly DATABASE="${HERE}/db/recovery.db"
readonly AGENT_USER="fixtureUser"
readonly AGENT_VIEW=".1.3.6.1.4.1.53864"
readonly OTHER_ENGINE="80000000010a0b0c0d"
readonly SECOND_ENGINE="8000000001aabbccdd"
readonly READY_TRIES=1500
readonly PAUSE="0.01"
readonly STOP_TRIES=500
readonly UPTIME_SECONDS=205
readonly BACK_SECONDS=35
readonly AWAY_SAMPLES=3

declare -a CHILDREN=()
declare -A FIFO=()
declare -A IOC_PID=()

function stop_children {
    local pid tries
    for pid in "${CHILDREN[@]:-}"; do
        [[ -n "${pid}" ]] || continue
        kill -CONT "${pid}" 2>/dev/null
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

# Starts an agent and waits for its ready line. Arguments: output file stem, port, family, configuration, engine
# ID or an empty word, then NAME=VALUE environment words. Sets AGENT_PID.
function start_agent {
    local stem="$1" port="$2" family="$3" conf="$4" engine="$5"
    shift 5
    if [[ -n "${engine}" ]]; then
        env "$@" "${PRODUCTS}/snmp3NativeAgent" "${port}" "${family}" "${conf}" "${engine}" \
            > "${stem}.stdout" 2> "${stem}.stderr" &
    else
        env "$@" "${PRODUCTS}/snmp3NativeAgent" "${port}" "${family}" "${conf}" > "${stem}.stdout" 2> "${stem}.stderr" &
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

# Starts one IOC of a scenario. Arguments: directory, configuration, then one binding prefix per endpoint (A, B).
function start_ioc {
    local directory="$1" config="$2" name handle
    shift 2
    {
        printf "%s\n" "on error break" "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")" \
            "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")" \
            "snmp3Load(\"${config}\",\"${PRODUCTS}/snmp3NativeProbe\")"
        for name in "$@"; do
            printf "%s\n" "dbLoadRecords(\"${DATABASE}\",\"P=${name}_,B=${name}\")"
        done
        printf "%s\n" "iocInit"
    } > "${directory}/startup.cmd" || return 1
    mkfifo "${directory}/ioc.stdin" || return 1
    MIBS="" stdbuf -oL "${PRODUCTS}/snmp3Ioc" "${directory}/startup.cmd" \
        < "${directory}/ioc.stdin" > "${directory}/ioc.stdout" 2> "${directory}/ioc.stderr" &
    IOC_PID["${directory}"]=$!
    CHILDREN+=("$!")
    exec {handle}> "${directory}/ioc.stdin"
    FIFO["${directory}"]="${handle}"
    wait_for_text "${directory}/ioc.stderr" "iocRun: All initialization complete" "${IOC_PID["${directory}"]}" || return 1
    perl "${TOOL}" mark "${directory}" ready
}

function sample {
    local directory="$1" phase="$2"
    shift 2
    perl "${TOOL}" sample "${directory}" "${directory}/ioc.stdin" "${directory}/ioc.stdout" "${phase}" "$@" \
        >> "${directory}/sample.stdout" 2>> "${directory}/sample.stderr"
}

function mark_all {
    local name="$1" directory
    shift
    for directory in "$@"; do
        perl "${TOOL}" mark "${directory}" "${name}" || return 1
    done
}

function sample_all {
    local phase="$1" targets="$2" directory
    shift 2
    for directory in "$@"; do
        # shellcheck disable=SC2086
        sample "${directory}" "${phase}" ${targets} || return 1
    done
}

# Takes samples of every IOC until the given number of seconds has passed since the call.
function sample_for {
    local seconds="$1" phase="$2" targets="$3" end
    shift 3
    end=$((SECONDS + seconds))
    while [[ ${SECONDS} -lt ${end} ]]; do
        sample_all "${phase}" "${targets}" "$@" || return 1
        sleep 1
    done
}

function finish_iocs {
    local directory handle tries
    for directory in "$@"; do
        handle="${FIFO["${directory}"]}"
        printf "exit\n" >&"${handle}"
        exec {handle}>&-
        tries=0
        while kill -0 "${IOC_PID["${directory}"]}" 2>/dev/null && [[ ${tries} -lt ${STOP_TRIES} ]]; do
            sleep "${PAUSE}"
            tries=$((tries + 1))
        done
    done
}

# Recovery scenarios: the engine ID of the returned agent is the one the IOCs used before.
function scenario_recover {
    local scenario="$1" base="${OUTPUT}/$1" port ioc iocs directory agent
    local -a directories=()
    new_secrets "${base}/secrets" || return 2
    port="$(perl "${TOOL}" port)" || return 2
    if [[ "${scenario}" != "late" ]]; then
        start_agent "${base}/agent-1" "${port}" 4 "${base}/secrets/agent.conf" "" || return 2
        agent="${AGENT_PID}"
    fi
    iocs="$(perl "${TOOL}" iocs "${scenario}")" || return 2
    for ioc in ${iocs}; do
        directory="${base}/${ioc}"
        mkdir "${directory}" || return 2
        perl "${TOOL}" config "${ioc}" 0 "${directory}/configuration.json" "${base}/secrets" "${port}" || return 2
        start_ioc "${directory}" "${directory}/configuration.json" A || return 2
        directories+=("${directory}")
    done
    if [[ "${scenario}" == "late" ]]; then
        mark_all lost "${directories[@]}" || return 2
        sample_for 15 away "A_" "${directories[@]}" || return 2
        start_agent "${base}/agent-1" "${port}" 4 "${base}/secrets/agent.conf" "" || return 2
        mark_all returned "${directories[@]}" || return 2
    else
        sample_all before "A_" "${directories[@]}" || return 2
        case "${scenario}" in
            boots|same) SECONDS=0; sample_for "${UPTIME_SECONDS}" before "A_" "${directories[@]}" || return 2 ;;
        esac
        if [[ "${scenario}" == "pause" ]]; then
            kill -STOP "${agent}" || return 2
        else
            stop_agent "${agent}" || return 2
        fi
        mark_all lost "${directories[@]}" || return 2
        sample_for 12 away "A_" "${directories[@]}" || return 2
        case "${scenario}" in
            pause) kill -CONT "${agent}" || return 2 ;;
            boots) start_agent "${base}/agent-2" "${port}" 4 "${base}/secrets/agent.conf" "" \
                       SNMP3_FIXTURE_ENGINE_BOOTS=2 || return 2 ;;
            same)  start_agent "${base}/agent-2" "${port}" 4 "${base}/secrets/agent.conf" "" \
                       SNMP3_FIXTURE_ENGINE_BOOTS=1 || return 2 ;;
        esac
        mark_all returned "${directories[@]}" || return 2
    fi
    sample_for "${BACK_SECONDS}" back "A_" "${directories[@]}" || return 2
    finish_iocs "${directories[@]}"
    : > "${base}/complete"
}

# State scenarios: the returned agent has another engine ID and other values; "unverified" also other secrets.
function scenario_state {
    local scenario="$1" base="${OUTPUT}/$1" port directory system=1 secrets
    [[ "${RECOVERY_NO_SYSTEM_FIELD:-0}" == "1" ]] && system=0
    new_secrets "${base}/secrets" || return 2
    port="$(perl "${TOOL}" port)" || return 2
    start_agent "${base}/agent-1" "${port}" 4 "${base}/secrets/agent.conf" "" || return 2
    directory="${base}/v3d"
    mkdir "${directory}" || return 2
    perl "${TOOL}" config v3d "${system}" "${directory}/configuration.json" "${base}/secrets" "${port}" || return 2
    start_ioc "${directory}" "${directory}/configuration.json" A || return 2
    sample "${directory}" before "A_=5" || return 2
    sample "${directory}" before "A_" || return 2
    stop_agent "${AGENT_PID}" || return 2
    perl "${TOOL}" mark "${directory}" lost || return 2
    sample "${directory}" away "A_" || return 2
    secrets="${base}/secrets"
    if [[ "${scenario}" == "unverified" ]]; then
        new_secrets "${base}/secrets-2" || return 2
        secrets="${base}/secrets-2"
    fi
    start_agent "${base}/agent-2" "${port}" 4 "${secrets}/agent.conf" "${OTHER_ENGINE}" SNMP3_FIXTURE_INTEGER=77 || return 2
    perl "${TOOL}" mark "${directory}" returned || return 2
    sample_for "${BACK_SECONDS}" back "A_" "${directory}" || return 2
    sample "${directory}" back "A_=9" || return 2
    sample_for 30 back "A_" "${directory}" || return 2
    finish_iocs "${directory}"
    : > "${base}/complete"
}

# Two addresses: the unit of the first address returns with another engine ID while the second keeps answering.
function scenario_two {
    local base="${OUTPUT}/two" port_a port_b directory system=1 first
    [[ "${RECOVERY_NO_SYSTEM_FIELD:-0}" == "1" ]] && system=0
    new_secrets "${base}/secrets" || return 2
    port_a="$(perl "${TOOL}" port)" || return 2
    port_b="$(perl "${TOOL}" port)" || return 2
    start_agent "${base}/agent-1" "${port_a}" 4 "${base}/secrets/agent.conf" "" || return 2
    first="${AGENT_PID}"
    start_agent "${base}/agent-B" "${port_b}" 6 "${base}/secrets/agent.conf" "${SECOND_ENGINE}" || return 2
    directory="${base}/v3d"
    mkdir "${directory}" || return 2
    perl "${TOOL}" config v3d "${system}" "${directory}/configuration.json" "${base}/secrets" "${port_a}" "${port_b}" ||
        return 2
    start_ioc "${directory}" "${directory}/configuration.json" A B || return 2
    sample "${directory}" before "A_=5" "B_=5" || return 2
    sample "${directory}" before "A_" "B_" || return 2
    stop_agent "${first}" || return 2
    perl "${TOOL}" mark "${directory}" lost || return 2
    sample "${directory}" away "A_" "B_" || return 2
    start_agent "${base}/agent-2" "${port_a}" 4 "${base}/secrets/agent.conf" "${OTHER_ENGINE}" SNMP3_FIXTURE_INTEGER=77 ||
        return 2
    perl "${TOOL}" mark "${directory}" returned || return 2
    grep -c '"event":"agent_handler","mode":3,' "${base}/agent-B.stdout" > "${base}/agent-B.commits-at-return"
    sample_for "${BACK_SECONDS}" back "A_ B_" "${directory}" || return 2
    sample "${directory}" back "A_" "B_=6" || return 2
    sample "${directory}" back "A_" "B_" || return 2
    finish_iocs "${directory}"
    : > "${base}/complete"
}

function run_scenario {
    local scenario="$1" status=0
    mkdir "${OUTPUT}/${scenario}" || return 2
    trap stop_children EXIT
    case "${scenario}" in
        pause|late|boots|same) scenario_recover "${scenario}" || status=$? ;;
        changed|unverified) scenario_state "${scenario}" || status=$? ;;
        two) scenario_two || status=$? ;;
        *) status=2 ;;
    esac
    stop_children
    return "${status}"
}

function main {
    local scenario scenarios only=""
    local -a jobs=()
    OUTPUT=""
    PRODUCTS=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) OUTPUT="${2:-}"; shift 2 ;;
            --products) PRODUCTS="${2:-}"; shift 2 ;;
            --only) only="${2:-}"; shift 2 ;;
            *) printf "usage: test_recovery.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only SCENARIO]\n" >&2
               exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_recovery: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] ||
        { printf "test_recovery: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_recovery: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
    OUTPUT="$(cd "${OUTPUT}" && pwd)"
    scenarios="$(perl "${TOOL}" scenarios)" || exit 2
    [[ -z "${only}" ]] || scenarios="${only}"
    for scenario in ${scenarios}; do
        (run_scenario "${scenario}") > "${OUTPUT}/${scenario}.stdout" 2> "${OUTPUT}/${scenario}.stderr" &
        jobs+=("$!")
    done
    wait "${jobs[@]}"
    perl "${TOOL}" judge "${OUTPUT}"
    exit $?
}

main "$@"
