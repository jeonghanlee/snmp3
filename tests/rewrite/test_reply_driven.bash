#!/usr/bin/env bash
# Reply-driven cell: production IOCs whose records read through subscriptions, against fixture agents. Every case
# has its own IOC, its own agents and its own directory and sets its own first state. This script starts and stops
# the processes and decides the order of the steps; reply_driven.pl writes the startup script and the database of
# a case, reads the records and judges the retained observations afterwards.
# usage: test_reply_driven.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only CASE]
# exit status: 0 every check passed, 1 a check failed, 2 a case could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/reply_driven.pl"
readonly PROXY="${HERE}/helpers/udp_fault.py"
readonly AGENT_USER="fixtureUser"
readonly AGENT_VIEW=".1.3.6.1.4.1.53864"
readonly OTHER_ENGINE="80000000010a0b0c0d"
readonly DELAY_MS=750
readonly READY_TRIES=1500
readonly PAUSE="0.01"
readonly STOP_TRIES=500
readonly SETTLE_SECONDS=3
readonly READINGS=10
readonly READING_SECONDS=2
readonly BACK_SECONDS=40
readonly STATE_SECONDS=40
readonly AFTER_SECONDS=5
readonly PASS_SECONDS=30
readonly WRONG_SECONDS=30

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

# Writes three secret files and the agent configuration that uses them into a new directory. The community is
# accepted from both loopback addresses.
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
        "rwcommunity6 ${community} ::1 ${AGENT_VIEW}" \
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

# Starts a proxy in front of an agent port and waits for its ready line. Arguments: output file stem, family,
# agent port, mode, delay in ms. Sets PROXY_PORT.
function start_proxy {
    local stem="$1" family="$2" port="$3" mode="$4" delay="$5" pid
    python3 "${PROXY}" --family "${family}" --agent-port "${port}" --mode "${mode}" --delay-ms "${delay}" \
        > "${stem}.stdout" 2> "${stem}.stderr" &
    pid=$!
    CHILDREN+=("${pid}")
    wait_for_text "${stem}.stdout" '"event": "fault_ready"' "${pid}" || return 1
    PROXY_PORT="$(sed -n 's/.*"event": "fault_ready".*"port": \([0-9]*\).*/\1/p' "${stem}.stdout")"
    [[ -n "${PROXY_PORT}" ]]
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

# One record on a subscription with a prompt agent, one on a second address whose replies are delayed.
function case_period {
    local prompt_port delayed_port count=0
    prompt_port="$(perl "${TOOL}" port)" || return 2
    delayed_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent-prompt" "${prompt_port}" 4 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    start_agent "${CASE_DIR}/agent-delayed" "${delayed_port}" 6 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    start_proxy "${CASE_DIR}/proxy" 6 "${delayed_port}" delay "${DELAY_MS}" || return 2
    prepare "${prompt_port}" "${PROXY_PORT}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    sleep "${SETTLE_SECONDS}"
    while [[ ${count} -le ${READINGS} ]]; do
        tool observe "${CASE_DIR}" "reading-$(printf '%02d' "${count}")" \
            "prompt=${CASE_DIR}/agent-prompt.stdout" "delayed=${CASE_DIR}/agent-delayed.stdout" \
            C_PromptCount C_DelayedCount C_Prompt.SEVR C_Delayed.SEVR C_Delayed.LCNT || return 2
        count=$((count + 1))
        [[ ${count} -le ${READINGS} ]] && sleep "${READING_SECONDS}"
    done
    finish_ioc
}

function observe_loss {
    tool observe "${CASE_DIR}" "$1" "other=${CASE_DIR}/agent-other.stdout" \
        L_In L_In.SEVR L_In.STAT L_In.AMSG L_In.PACT L_In.UDF L_InCount L_OtherCount L_Other.SEVR
}

# The agent of the first address is stopped and started again while the second address keeps its period.
function case_loss {
    local first_port second_port first count=0 end
    first_port="$(perl "${TOOL}" port)" || return 2
    second_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent-1" "${first_port}" 4 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    first="${AGENT_PID}"
    start_agent "${CASE_DIR}/agent-other" "${second_port}" 6 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    prepare "${first_port}" "${second_port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    sleep "${SETTLE_SECONDS}"
    observe_loss before || return 2
    stop_agent "${first}" || return 2
    sleep "${SETTLE_SECONDS}"
    while [[ ${count} -le ${READINGS} ]]; do
        observe_loss "away-$(printf '%02d' "${count}")" || return 2
        count=$((count + 1))
        [[ ${count} -le ${READINGS} ]] && sleep "${READING_SECONDS}"
    done
    start_agent "${CASE_DIR}/agent-2" "${first_port}" 4 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    tool mark "${CASE_DIR}" returned || return 2
    # The first reading without alarm ends the loop; the bound ends it when a defect keeps the alarm, and the
    # judge then reports the time as too long or never.
    count=0
    end=$((SECONDS + BACK_SECONDS))
    while [[ ${SECONDS} -lt ${end} ]]; do
        count=$((count + 1))
        observe_loss "back-$(printf '%03d' "${count}")" || return 2
        grep -q '"L_In.SEVR" : "NO_ALARM"' "${CASE_DIR}/observe-back-$(printf '%03d' "${count}").json" && break
        sleep 1
    done
    finish_ioc
}

function observe_engine {
    tool observe "${CASE_DIR}" "$1" "returned=${CASE_DIR}/agent-2.stdout" \
        E_Value E_Value.SEVR E_Value.STAT E_Value.AMSG E_Sys E_Sys.SEVR E_Sys.STAT E_Sys.AMSG
}

# The agent returns with another engine ID and another value; the state of the endpoint settles as changed.
function case_engine {
    local port end
    port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent-1" "${port}" 4 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    prepare "${port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    sleep "${SETTLE_SECONDS}"
    observe_engine before || return 2
    stop_agent "${AGENT_PID}" || return 2
    start_agent "${CASE_DIR}/agent-2" "${port}" 4 "${CASE_DIR}/secrets/agent.conf" "${OTHER_ENGINE}" \
        SNMP3_FIXTURE_INTEGER=77 || return 2
    # The loop ends when the report shows the settled state; the bound ends it when a defect keeps another
    # state, and the judge then finds no observation of the settled state.
    end=$((SECONDS + STATE_SECONDS))
    while [[ ${SECONDS} -lt ${end} ]]; do
        observe_engine waiting || return 2
        if grep -q 'snmp3 endpoint: id=E .*state=changed' "${CASE_DIR}/observe-waiting.json"; then
            observe_engine settled || return 2
            sleep "${AFTER_SECONDS}"
            observe_engine after || return 2
            break
        fi
        sleep 1
    done
    finish_ioc
}

# Three objects of one subscription through a proxy that logs every datagram; the reading is taken right after a
# pass, long before the next request of the subscription. Then the records are taken off the I/O Intr list.
function case_one_request {
    local port
    port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${port}" 4 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    start_proxy "${CASE_DIR}/proxy" 4 "${port}" pass 0 || return 2
    prepare "${PROXY_PORT}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    tool await "${CASE_DIR}" O_Passes 3 "${PASS_SECONDS}" || return 2
    tool observe "${CASE_DIR}" passes "proxy=${CASE_DIR}/proxy.stdout" O_Passes O_Check || return 2
    # The records leave the I/O Intr list; a request that was on its way then has time to end before the first
    # of the two readings.
    tool send "${CASE_DIR}" 'dbpf "O_A.SCAN" "Passive"' 'dbpf "O_B.SCAN" "Passive"' 'dbpf "O_C.SCAN" "Passive"' || return 2
    sleep "${SETTLE_SECONDS}"
    tool observe "${CASE_DIR}" idle-1 "proxy=${CASE_DIR}/proxy.stdout" O_Passes || return 2
    sleep "${AFTER_SECONDS}"
    tool observe "${CASE_DIR}" idle-2 "proxy=${CASE_DIR}/proxy.stdout" O_Passes || return 2
    finish_ioc
}

# Two records of one subscription on an address whose queue takes one request.
function case_refused {
    local port
    port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${port}" 4 "${CASE_DIR}/secrets/agent.conf" "" || return 2
    prepare "${port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    sleep "${SETTLE_SECONDS}"
    tool observe "${CASE_DIR}" refused F_1.SEVR F_1.STAT F_1.AMSG F_2.SEVR F_2.STAT F_2.AMSG || return 2
    finish_ioc
}

function case_wrong_records {
    local port
    port="$(perl "${TOOL}" port)" || return 2
    prepare "${port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
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

function run_case {
    local status=0
    CASE="$1"
    CASE_DIR="${OUTPUT}/${CASE}"
    mkdir "${CASE_DIR}" || return 2
    new_secrets "${CASE_DIR}/secrets" || return 2
    case "${CASE}" in
        period) case_period || status=$? ;;
        loss) case_loss || status=$? ;;
        engine) case_engine || status=$? ;;
        one-request) case_one_request || status=$? ;;
        wrong-records) case_wrong_records || status=$? ;;
        refused) case_refused || status=$? ;;
        wrong-*) case_wrong || status=$? ;;
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
            *) printf "usage: test_reply_driven.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only CASE]\n" >&2
               exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_reply_driven: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] ||
        { printf "test_reply_driven: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_reply_driven: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
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
