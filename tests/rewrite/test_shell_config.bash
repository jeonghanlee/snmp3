#!/usr/bin/env bash
# Shell configuration cell: production IOCs that are configured by shell commands, with records that name their
# objects in the link, against fixture agents. Every case has its own IOC, its own agents and its own directory
# and sets its own first state. This script starts and stops the processes and decides the order of the steps;
# shell_config.pl writes the startup script and the database of a case, reads the records and judges the retained
# observations afterwards.
# usage: test_shell_config.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only CASE]
# exit status: 0 every check passed, 1 a check failed, 2 a case could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/shell_config.pl"
readonly PROXY="${HERE}/helpers/udp_fault.py"
readonly AGENT_USER="fixtureUser"
readonly AGENT_VIEW=".1.3.6.1.4.1.53864"
readonly DELAY_MS=750
readonly READY_TRIES=1500
readonly PAUSE="0.01"
readonly STOP_TRIES=500
readonly GROUP_ROUNDS=4
readonly AWAY_SAMPLES=3
readonly BACK_SECONDS=40
readonly WRONG_SECONDS=30
readonly REPORT="snmp3ConfigReport"

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

# Starts an agent on a port and waits for its ready line. Sets AGENT_PID.
function start_agent {
    local stem="$1" port="$2" conf="$3"
    "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${conf}" > "${stem}.stdout" 2> "${stem}.stderr" &
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

# Starts the IOC of a case from its startup script. Returns 0 when its initialization is complete and 1 when the
# IOC ended before that; the second is recorded in the case directory, and the judge reports it for the checks of
# the case with the line that names the fault. Sets IOC_PID and IOC_HANDLE.
function start_ioc {
    local directory="$1"
    mkfifo "${directory}/ioc.stdin" || return 2
    MIBS="" stdbuf -oL "${PRODUCTS}/snmp3Ioc" "${directory}/startup.cmd" \
        < "${directory}/ioc.stdin" > "${directory}/ioc.stdout" 2> "${directory}/ioc.stderr" &
    IOC_PID=$!
    CHILDREN+=("${IOC_PID}")
    exec {IOC_HANDLE}> "${directory}/ioc.stdin"
    if wait_for_text "${directory}/ioc.stderr" "iocRun: All initialization complete" "${IOC_PID}"; then
        return 0
    fi
    exec {IOC_HANDLE}>&-
    : > "${directory}/ioc-ended"
    return 1
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
    perl "${TOOL}" "$@" >> "${CASE_DIR}/tool.stdout" 2>> "${CASE_DIR}/tool.stderr"
}

function prepare {
    perl "${TOOL}" prepare "${CASE}" "${CASE_DIR}" "${ROOT}" "${PRODUCTS}" "${CASE_DIR}/secrets" "$@"
}

function case_write {
    local agent_port slow_port agent proxy name value input
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${OUTPUT}/write/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    agent="${AGENT_PID}"
    python3 "${PROXY}" --family 4 --agent-port "${agent_port}" --mode delay --delay-ms "${DELAY_MS}" \
        > "${CASE_DIR}/proxy.stdout" 2> "${CASE_DIR}/proxy.stderr" &
    proxy=$!
    CHILDREN+=("${proxy}")
    wait_for_text "${CASE_DIR}/proxy.stdout" '"event": "fault_ready"' "${proxy}" || return 2
    slow_port="$(sed -n 's/.*"event": "fault_ready".*"port": \([0-9]*\).*/\1/p' "${CASE_DIR}/proxy.stdout")"
    [[ -n "${slow_port}" ]] || return 2
    prepare "${agent_port}" "${slow_port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    while IFS='|' read -r name value input; do
        tool put "${CASE_DIR}" "W_${name}" "${value}" || return 2
        tool process "${CASE_DIR}" "W_${input}" "W_${input}" || return 2
        tool observe "${CASE_DIR}" "put-${name}" "${REPORT}" "W_${name}.SEVR" "W_${input}" "W_${name}.STAT" \
            "W_${name}.AMSG" || return 2
    done <<'ROWS'
Ao|2.5|AoRead
Longout|41|LongoutRead
Int64out|123456789012|Int64outRead
Stringout|abc|OctetsRead
Lso|longer text|OctetsRead
ROWS
    tool observe "${CASE_DIR}" delayed-early "${REPORT}" W_Delayed.PACT -- 'dbpf "W_Delayed" 55' || return 2
    tool settle "${CASE_DIR}" W_Delayed || return 2
    tool observe "${CASE_DIR}" delayed-late "${REPORT}" W_Delayed.PACT W_Delayed.SEVR || return 2
    tool put "${CASE_DIR}" W_Refused 3 || return 2
    tool observe "${CASE_DIR}" refused "${REPORT}" W_Refused.SEVR W_Refused.STAT W_Refused.AMSG || return 2
    stop_agent "${agent}" || return 2
    tool put "${CASE_DIR}" W_Longout 77 || return 2
    tool observe "${CASE_DIR}" no-reply "${REPORT}" W_Longout.SEVR W_Longout.STAT W_Longout.AMSG || return 2
    finish_ioc
}

function case_sizing {
    local agent_port count=0
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    while [[ ${count} -lt ${GROUP_ROUNDS} ]]; do
        tool process "${CASE_DIR}" G_Round G_R1 G_R2 G_R3 G_R4 G_R5 G_R6 G_R7 G_R8 || return 2
        count=$((count + 1))
    done
    tool observe "${CASE_DIR}" sized "${REPORT}" G_R8.AMSG || return 2
    finish_ioc
}

function sample_inputs {
    local label="$1"
    tool process "${CASE_DIR}" R_Round R_In1 R_In2 || return 1
    tool observe "${CASE_DIR}" "${label}" "${REPORT}" R_In1.SEVR R_In1.STAT R_In1.AMSG R_In2.SEVR R_In2.STAT R_In2.AMSG
}

function case_recover {
    local agent_port count=0 end
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent-1" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    sample_inputs before || return 2
    stop_agent "${AGENT_PID}" || return 2
    while [[ ${count} -lt ${AWAY_SAMPLES} ]]; do
        count=$((count + 1))
        sample_inputs "away-${count}" || return 2
    done
    start_agent "${CASE_DIR}/agent-2" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    tool mark "${CASE_DIR}" returned || return 2
    # The first sample without alarm ends the loop; the bound ends it when a defect keeps the alarm, and the
    # judge then reports the time as too long or never.
    count=0
    end=$((SECONDS + BACK_SECONDS))
    while [[ ${SECONDS} -lt ${end} ]]; do
        count=$((count + 1))
        sample_inputs "back-$(printf '%03d' "${count}")" || return 2
        if grep -q '"R_In1.SEVR" : "NO_ALARM"' "${CASE_DIR}/observe-back-$(printf '%03d' "${count}").json" &&
            grep -q '"R_In2.SEVR" : "NO_ALARM"' "${CASE_DIR}/observe-back-$(printf '%03d' "${count}").json"; then
            break
        fi
        sleep 1
    done
    finish_ioc
}

# A wrong startup script ends the IOC by itself; its standard input is empty, so an IOC that accepts the wrong
# line initializes and ends as well, and the judge sees the marker and the initialization line.
function case_wrong {
    local port status=0
    port="$(perl "${TOOL}" port)" || return 2
    prepare "${port}" || return 2
    MIBS="" timeout "${WRONG_SECONDS}" "${PRODUCTS}/snmp3Ioc" "${CASE_DIR}/startup.cmd" \
        < /dev/null > "${CASE_DIR}/ioc.stdout" 2> "${CASE_DIR}/ioc.stderr" || status=$?
    printf "%s\n" "${status}" > "${CASE_DIR}/status"
}

function case_options {
    local port
    port="$(perl "${TOOL}" port)" || return 2
    prepare "${port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    tool observe "${CASE_DIR}" shown "snmp3Show * 1" || return 2
    finish_ioc
}

function case_both {
    local agent_port
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    tool process "${CASE_DIR}" B_File B_File || return 2
    tool process "${CASE_DIR}" B_Command B_Command || return 2
    tool process "${CASE_DIR}" B_FilePlain B_FilePlain || return 2
    tool process "${CASE_DIR}" B_CommandPlain B_CommandPlain || return 2
    tool observe "${CASE_DIR}" read "${REPORT}" B_File.SEVR B_Command.SEVR B_FilePlain.SEVR B_CommandPlain.SEVR || return 2
    finish_ioc
}

function run_case {
    local status=0
    CASE="$1"
    CASE_DIR="${OUTPUT}/${CASE}"
    mkdir "${CASE_DIR}" || return 2
    new_secrets "${CASE_DIR}/secrets" || return 2
    case "${CASE}" in
        write) case_write || status=$? ;;
        sizing-v2c|sizing-v3) case_sizing || status=$? ;;
        recover) case_recover || status=$? ;;
        wrong-*) case_wrong || status=$? ;;
        options|orphan) case_options || status=$? ;;
        both) case_both || status=$? ;;
        *) status=2 ;;
    esac
    stop_children
    [[ ${status} -eq 0 ]] && : > "${CASE_DIR}/complete"
    [[ ${status} -eq 0 ]] || cat "${CASE_DIR}/tool.stderr" >&2 2>/dev/null
    return "${status}"
}

function main {
    local name cases only=""
    OUTPUT=""
    PRODUCTS=""
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) OUTPUT="${2:-}"; shift 2 ;;
            --products) PRODUCTS="${2:-}"; shift 2 ;;
            --only) only="${2:-}"; shift 2 ;;
            *) printf "usage: test_shell_config.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only CASE]\n" >&2
               exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_shell_config: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] ||
        { printf "test_shell_config: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_shell_config: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
    OUTPUT="$(cd "${OUTPUT}" && pwd)"
    trap stop_children EXIT
    cases="$(perl "${TOOL}" cases)" || exit 2
    [[ -z "${only}" ]] || cases="${only}"
    for name in ${cases}; do
        run_case "${name}"
    done
    # shellcheck disable=SC2086
    perl "${TOOL}" judge "${OUTPUT}" ${cases}
    exit $?
}

main "$@"
