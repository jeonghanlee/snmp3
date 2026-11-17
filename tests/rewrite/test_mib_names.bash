#!/usr/bin/env bash
# MIB names cell: production IOCs whose records name their objects through a MIB, against fixture agents. Every
# case has its own IOC, its own agent, its own MIB files and its own directory. This script starts and stops the
# processes and decides the order of the steps; mib_names.pl writes the MIB files, the startup script and the
# database of a case, reads the records and judges the retained observations afterwards.
# The case place-module puts the fixture MIB into the mibs directory of the tree under test for the time of that
# case and removes it afterwards.
# usage: test_mib_names.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only CASE]
# exit status: 0 every check passed, 1 a check failed, 2 a case could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/mib_names.pl"
readonly MODULE_MIB="${ROOT}/mibs/SNMP3-FIXTURE-MIB"
readonly AGENT_VIEW=".1.3.6.1.4.1"
readonly AGENT_ENGINE="8000000001020304"
readonly READY_TRIES=3000
readonly PAUSE="0.01"
readonly STOP_TRIES=500
readonly WRITTEN=4321
readonly STARTS=3

declare -a CHILDREN=()
PLACED=""

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

# Removes the MIB this run placed in the mibs directory of the tree, and nothing else.
function remove_placed {
    [[ -n "${PLACED}" ]] || return 0
    rm -f -- "${PLACED}"
    PLACED=""
}

function clean_up {
    stop_children
    remove_placed
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

# Writes the community file and the agent configuration that uses it into a new directory.
function new_secrets {
    local directory="$1" community
    mkdir "${directory}" || return 1
    (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${directory}/community" &&
        printf "\n" >> "${directory}/community") || return 1
    read -r community < "${directory}/community"
    (umask 077 && printf "%s\n" "rwcommunity ${community} 127.0.0.1 ${AGENT_VIEW}" > "${directory}/agent.conf") || return 1
}

# Starts an agent on a port and waits for its ready line; a fourth argument is its list of objects.
function start_agent {
    local stem="$1" port="$2" conf="$3" objects="${4:-}"
    if [[ -n "${objects}" ]]; then
        "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${conf}" "${AGENT_ENGINE}" "${objects}" \
            > "${stem}.stdout" 2> "${stem}.stderr" &
    else
        "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${conf}" > "${stem}.stdout" 2> "${stem}.stderr" &
    fi
    AGENT_PID=$!
    CHILDREN+=("${AGENT_PID}")
    wait_for_text "${stem}.stdout" '"event":"agent_ready"' "${AGENT_PID}"
}

# Starts the IOC of a case from its startup script, with its output under the stem given. Returns 0 when its
# initialization is complete and 1 when the IOC ended before that; the second is recorded in the case directory,
# and the judge reports it for the checks of the case with the line that names the fault. Sets IOC_PID and
# IOC_HANDLE.
function start_ioc {
    local directory="$1"
    [[ -p "${directory}/ioc.stdin" ]] || mkfifo "${directory}/ioc.stdin" || return 2
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

# A value is written by number, then the record by number and the record by name read it.
function case_place {
    local agent_port
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    if [[ "${CASE}" == "place-module" && -d "${ROOT}/mibs" ]]; then
        [[ ! -e "${MODULE_MIB}" ]] || return 2
        cp -- "${CASE_DIR}/mib-near/SNMP3-FIXTURE-MIB" "${MODULE_MIB}" || return 2
        PLACED="${MODULE_MIB}"
    fi
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    tool put "${CASE_DIR}" N_Set "${WRITTEN}" || return 2
    tool process "${CASE_DIR}" N_Number || return 2
    tool process "${CASE_DIR}" N_Name || return 2
    tool observe "${CASE_DIR}" read N_Number.SEVR N_Name.SEVR N_Number N_Name || return 2
    finish_ioc
}

function case_rejections {
    local agent_port good="$1"
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
    tool process "${CASE_DIR}" "${good}" || return 2
    tool observe "${CASE_DIR}" read "${good}.SEVR" || return 2
    finish_ioc
}

# Three starts of one IOC with 500 records; the time of each start is taken from just before the process is
# started to the line of complete initialization, and the first record is read in the last start.
function case_start {
    local agent_port count=0 begin end
    agent_port="$(perl "${TOOL}" port)" || return 2
    prepare "${agent_port}" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" "${CASE_DIR}/objects" || return 2
    while [[ ${count} -lt ${STARTS} ]]; do
        count=$((count + 1))
        begin="$(date +%s%N)"
        start_ioc "${CASE_DIR}" || return $(($? == 1 ? 0 : 2))
        end="$(date +%s%N)"
        printf "%d.%03d\n" $(((end - begin) / 1000000000)) $((((end - begin) / 1000000) % 1000)) >> "${CASE_DIR}/start-seconds"
        if [[ ${count} -eq ${STARTS} ]]; then
            tool process "${CASE_DIR}" T_1 || return 2
            tool observe "${CASE_DIR}" read T_1.SEVR T_1 || return 2
        fi
        finish_ioc
        cp -- "${CASE_DIR}/ioc.stdout" "${CASE_DIR}/ioc-${count}.stdout"
        cp -- "${CASE_DIR}/ioc.stderr" "${CASE_DIR}/ioc-${count}.stderr"
    done
}

function run_case {
    local status=0
    CASE="$1"
    CASE_DIR="${OUTPUT}/${CASE}"
    mkdir "${CASE_DIR}" || return 2
    new_secrets "${CASE_DIR}/secrets" || return 2
    case "${CASE}" in
        place-*|two-places) case_place || status=$? ;;
        wrong-records) case_rejections X_Good || status=$? ;;
        none) case_rejections Z_Number || status=$? ;;
        start-*) case_start || status=$? ;;
        *) status=2 ;;
    esac
    clean_up
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
            *) printf "usage: test_mib_names.bash --output NEW_DIRECTORY [--products DIRECTORY] [--only CASE]\n" >&2
               exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_mib_names: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] ||
        { printf "test_mib_names: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_mib_names: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
    OUTPUT="$(cd "${OUTPUT}" && pwd)"
    trap clean_up EXIT
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
