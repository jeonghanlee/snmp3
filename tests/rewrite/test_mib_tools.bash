#!/usr/bin/env bash
# MIB tools cell: the checker and the draft program of the module and the probe mode --describe, run from the
# products directory against the fixture MIB, the PowerNet MIB and the TRIPPLITE MIBs, and the production IOC that
# loads the databases they judge and write. This script starts and stops the fixture agent and the IOCs and decides
# the order of the steps; mib_tools.pl writes the MIB files, the databases and the startup scripts, runs the
# tools, reads the records and judges the retained files afterwards.
# usage: test_mib_tools.bash --output NEW_DIRECTORY [--products DIRECTORY] [--baseline-products DIRECTORY]
#                            [--powernet DIRECTORY] [--tripplite DIRECTORY] [--only CASE]
#   --baseline-products  the products of the unchanged tree, whose --resolve the changed probe must equal;
#                        without it the products themselves are used
#   --powernet, --tripplite  the directories that hold PowerNet-MIB and TRIPPLITE-PRODUCTS.MIB with TRIPPLITE.MIB;
#                        read and never changed
# exit status: 0 every check passed, 1 a check failed, 2 a case could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/mib_tools.pl"
readonly AGENT_VIEW=".1.3.6.1.4.1"
readonly READY_TRIES=3000
readonly PAUSE="0.01"
readonly STOP_TRIES=500
readonly WRITTEN=4321

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

function clean_up {
    stop_children
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

function start_agent {
    local stem="$1" port="$2" conf="$3"
    "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${conf}" > "${stem}.stdout" 2> "${stem}.stderr" &
    AGENT_PID=$!
    CHILDREN+=("${AGENT_PID}")
    wait_for_text "${stem}.stdout" '"event":"agent_ready"' "${AGENT_PID}"
}

# Starts the IOC of one run from the startup script in its directory. Returns 0 when its initialization is
# complete and 1 when the IOC ended before that, which is recorded in the directory; sets IOC_PID and IOC_HANDLE.
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
    local agent_port="$1"
    perl "${TOOL}" prepare "${CASE}" "${CASE_DIR}" "${ROOT}" "${PRODUCTS}" "${CASE_DIR}/secrets" "${agent_port}" \
        "${POWERNET}" "${TRIPPLITE}"
}

# Runs one IOC run: the IOC loads the database named in its startup script, optional steps run while it is up.
# An IOC that ends during its startup is recorded and is not a fault of the driver.
function ioc_run {
    local name="$1"
    shift
    start_ioc "${CASE_DIR}/ioc-${name}" || return $(($? == 1 ? 0 : 2))
    if [[ $# -gt 0 ]]; then
        "$@" || return 2
    fi
    finish_ioc
}

# Called through ioc_run, which runs its arguments as a command.
# shellcheck disable=SC2317
function fixture_value {
    local directory="${CASE_DIR}/ioc-draft-fixture"
    tool put "${directory}" "D:fixtureInteger_Set" "${WRITTEN}" || return 2
    tool process "${directory}" "D:fixtureInteger" || return 2
    tool observe "${directory}" "D:fixtureInteger.SEVR" "D:fixtureInteger" || return 2
}

function case_checker {
    prepare 1 || return 2
    tool run-checker "${CASE_DIR}" "${PRODUCTS}" || return 2
}

function case_agreement {
    local agent_port
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    tool run-checker "${CASE_DIR}" "${PRODUCTS}" || return 2
    ioc_run agree
}

function case_draft {
    local agent_port
    agent_port="$(perl "${TOOL}" port)" || return 2
    start_agent "${CASE_DIR}/agent" "${agent_port}" "${CASE_DIR}/secrets/agent.conf" || return 2
    prepare "${agent_port}" || return 2
    tool run-drafts "${CASE_DIR}" "${PRODUCTS}" || return 2
    ioc_run draft-fixture fixture_value || return 2
    ioc_run draft-powernet || return 2
    ioc_run draft-tripplite || return 2
    ioc_run draft-system || return 2
    ioc_run draft-tools || return 2
}

function case_probe {
    prepare 1 || return 2
    tool run-probe "${CASE_DIR}" "${PRODUCTS}" "${BASELINE}" || return 2
}

function run_case {
    local status=0
    CASE="$1"
    CASE_DIR="${OUTPUT}/${CASE}"
    mkdir "${CASE_DIR}" || return 2
    new_secrets "${CASE_DIR}/secrets" || return 2
    case "${CASE}" in
        checker) case_checker || status=$? ;;
        agreement) case_agreement || status=$? ;;
        draft) case_draft || status=$? ;;
        probe) case_probe || status=$? ;;
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
    BASELINE=""
    POWERNET="${HOME}/gitsrc/apcpdu/mibs"
    TRIPPLITE="${HOME}/gitsrc/alliocs/tripplitepdu/mibs"
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) OUTPUT="${2:-}"; shift 2 ;;
            --products) PRODUCTS="${2:-}"; shift 2 ;;
            --baseline-products) BASELINE="${2:-}"; shift 2 ;;
            --powernet) POWERNET="${2:-}"; shift 2 ;;
            --tripplite) TRIPPLITE="${2:-}"; shift 2 ;;
            --only) only="${2:-}"; shift 2 ;;
            *) printf "usage: test_mib_tools.bash --output NEW_DIRECTORY [--products DIRECTORY] [--baseline-products DIRECTORY] [--powernet DIRECTORY] [--tripplite DIRECTORY] [--only CASE]\n" >&2
               exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_mib_tools: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    [[ -n "${BASELINE}" ]] || BASELINE="${PRODUCTS}"
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" && -x "${PRODUCTS}/snmp3NativeProbe" ]] ||
        { printf "test_mib_tools: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    [[ -x "${BASELINE}/snmp3NativeProbe" ]] ||
        { printf "test_mib_tools: no probe in %s\n" "${BASELINE}" >&2; exit 2; }
    [[ -f "${POWERNET}/PowerNet-MIB" ]] ||
        { printf "test_mib_tools: PowerNet-MIB not found in %s\n" "${POWERNET}" >&2; exit 2; }
    [[ -f "${TRIPPLITE}/TRIPPLITE-PRODUCTS.MIB" && -f "${TRIPPLITE}/TRIPPLITE.MIB" ]] ||
        { printf "test_mib_tools: TRIPPLITE MIBs not found in %s\n" "${TRIPPLITE}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_mib_tools: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
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
