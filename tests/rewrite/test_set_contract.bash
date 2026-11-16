#!/usr/bin/env bash
# SET contract cell: starts the fixture agent, a proxy that delays every reply, the Channel Access repeater and
# the production IOC, makes the writes, and hands the reading of records and the judgment to set_contract.pl.
# Plain puts go through the IOC shell; puts that wait for completion go through caput of EPICS Base on private
# loopback ports.
# usage: test_set_contract.bash --output NEW_DIRECTORY [--products DIRECTORY]
# exit status: 0 every check passed, 1 a check failed, 2 the scenario could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly TOOL="${HERE}/set_contract.pl"
readonly PROXY="${HERE}/helpers/udp_fault.py"
readonly DATABASE="${HERE}/db/set-contract.db"
readonly PREFIX="SetContract_"
readonly AGENT_VIEW=".1.3.6.1.4.1.53864"
readonly DELAY_MS=750
readonly READY_TRIES=1500
readonly PAUSE="0.01"
readonly STOP_TRIES=500

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

function tool {
    perl "${TOOL}" "$@" >> "${OUTPUT}/tool.stdout" 2>> "${OUTPUT}/tool.stderr"
}

function send { tool send "${OUTPUT}" "${OUTPUT}/ioc.stdin" "$@"; }

function settle {
    local name
    local -a records=()
    for name in "$@"; do
        records+=("${PREFIX}${name}")
    done
    tool settle "${OUTPUT}" "${OUTPUT}/ioc.stdin" "${records[@]}"
}

# One put to an output record, then a GET of the same object through its input record.
function write_and_read {
    local record="$1" value="$2" index="$3" input="$4"
    send "dbpf \"${PREFIX}${record}\" \"${value}\"" || return 1
    settle "${record}" || return 1
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" "put-${record}" "${PREFIX}${record}.SEVR" || return 1
    tool stored "${OUTPUT}" "put-${record}" "${OUTPUT}/agent.stdout" "${index}" || return 1
    send "dbpf \"${PREFIX}${input}.PROC\" 1" || return 1
    settle "${input}" || return 1
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" "read-${record}" "${PREFIX}${input}.VAL"
}

function scenario {
    local agent_port slow_port server_port repeater_port community base agent proxy ioc value status handle
    base="$(sed -n 's/^EPICS_BASE *= *//p' "${ROOT}/configure/RELEASE.local" | tail -1)"
    [[ -x "${base}/bin/${ARCH}/caput" && -x "${base}/bin/${ARCH}/caRepeater" ]] || return 2
    (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${OUTPUT}/community" && printf "\n" >> "${OUTPUT}/community") ||
        return 2
    read -r community < "${OUTPUT}/community"
    (umask 077 && printf "%s\n" "rwcommunity ${community} 127.0.0.1 ${AGENT_VIEW}" > "${OUTPUT}/agent.conf") || return 2
    agent_port="$(perl "${TOOL}" port)" || return 2
    server_port="$(perl "${TOOL}" port)" || return 2
    repeater_port="$(perl "${TOOL}" port)" || return 2
    "${PRODUCTS}/snmp3NativeAgent" "${agent_port}" 4 "${OUTPUT}/agent.conf" \
        > "${OUTPUT}/agent.stdout" 2> "${OUTPUT}/agent.stderr" &
    agent=$!
    CHILDREN+=("${agent}")
    wait_for_text "${OUTPUT}/agent.stdout" '"event":"agent_ready"' "${agent}" || return 2
    python3 "${PROXY}" --family 4 --agent-port "${agent_port}" --mode delay --delay-ms "${DELAY_MS}" \
        > "${OUTPUT}/proxy.stdout" 2> "${OUTPUT}/proxy.stderr" &
    proxy=$!
    CHILDREN+=("${proxy}")
    wait_for_text "${OUTPUT}/proxy.stdout" '"event": "fault_ready"' "${proxy}" || return 2
    slow_port="$(sed -n 's/.*"event": "fault_ready".*"port": \([0-9]*\).*/\1/p' "${OUTPUT}/proxy.stdout")"
    [[ -n "${slow_port}" ]] || return 2
    perl "${TOOL}" config "${OUTPUT}/configuration.json" "${OUTPUT}/community" "${agent_port}" "${slow_port}" || return 2
    export EPICS_CA_SERVER_PORT="${server_port}" EPICS_CA_REPEATER_PORT="${repeater_port}"
    export EPICS_CA_AUTO_ADDR_LIST="NO" EPICS_CA_ADDR_LIST="127.0.0.1:${server_port}"
    export EPICS_CAS_INTF_ADDR_LIST="127.0.0.1" EPICS_CAS_BEACON_AUTO_ADDR_LIST="NO"
    export EPICS_CAS_BEACON_ADDR_LIST="127.0.0.1:${repeater_port}"
    "${base}/bin/${ARCH}/caRepeater" > "${OUTPUT}/repeater.stdout" 2> "${OUTPUT}/repeater.stderr" &
    CHILDREN+=("$!")
    printf "%s\n" "on error break" "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")" \
        "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")" \
        "snmp3Load(\"${OUTPUT}/configuration.json\",\"${PRODUCTS}/snmp3NativeProbe\")" \
        "dbLoadRecords(\"${DATABASE}\",\"P=${PREFIX}\")" "iocInit" > "${OUTPUT}/startup.cmd" || return 2
    mkfifo "${OUTPUT}/ioc.stdin" || return 2
    MIBS="" stdbuf -oL "${PRODUCTS}/snmp3Ioc" "${OUTPUT}/startup.cmd" \
        < "${OUTPUT}/ioc.stdin" > "${OUTPUT}/ioc.stdout" 2> "${OUTPUT}/ioc.stderr" &
    ioc=$!
    CHILDREN+=("${ioc}")
    exec {handle}> "${OUTPUT}/ioc.stdin"
    wait_for_text "${OUTPUT}/ioc.stderr" "iocRun: All initialization complete" "${ioc}" || return 2

    write_and_read Ao "2.5" 9 AoRead || return 2
    write_and_read Longout "41" 1 LongoutRead || return 2
    write_and_read Int64out "123456789012" 5 Int64outRead || return 2
    write_and_read Stringout "abc" 6 OctetsRead || return 2
    write_and_read Lso "longer text" 6 OctetsRead || return 2

    # The reply to this put is held back by the proxy, so the record is read while its request is pending.
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" delayed-early "${PREFIX}Delayed.PACT" -- \
        "dbpf \"${PREFIX}Delayed\" 55" || return 2
    settle Delayed || return 2
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" delayed-late "${PREFIX}Delayed.PACT" "${PREFIX}Delayed.SEVR" \
        "${PREFIX}Delayed.AMSG" || return 2

    send "dbpf \"${PREFIX}Refused\" 3" || return 2
    settle Refused || return 2
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" refused "${PREFIX}Refused.SEVR" "${PREFIX}Refused.STAT" \
        "${PREFIX}Refused.AMSG" || return 2

    # Three puts that each wait for the completion of the record.
    : > "${OUTPUT}/sequence.status"
    for value in 7 8 9; do
        status=0
        "${base}/bin/${ARCH}/caput" -c -w 10 "${PREFIX}Sequence" "${value}" \
            >> "${OUTPUT}/caput.stdout" 2>> "${OUTPUT}/caput.stderr" || status=$?
        printf "%s\n" "${status}" >> "${OUTPUT}/sequence.status"
    done

    # Three plain puts in one write: the first is pending behind the delayed reply when the others arrive.
    send "dbpf \"${PREFIX}Burst\" 7" "dbpf \"${PREFIX}Burst\" 8" "dbpf \"${PREFIX}Burst\" 9" || return 2
    settle Burst || return 2
    sleep 0.2
    settle Burst || return 2
    send "dbpf \"${PREFIX}BurstRead.PROC\" 1" || return 2
    settle BurstRead || return 2
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" burst "${PREFIX}Burst.VAL" "${PREFIX}BurstRead.VAL" || return 2

    kill -TERM "${agent}" 2>/dev/null
    wait "${agent}" 2>/dev/null
    send "dbpf \"${PREFIX}Longout\" 77" || return 2
    settle Longout || return 2
    tool observe "${OUTPUT}" "${OUTPUT}/ioc.stdin" no-reply "${PREFIX}Longout.SEVR" "${PREFIX}Longout.STAT" \
        "${PREFIX}Longout.AMSG" || return 2

    printf "exit\n" >&"${handle}"
    exec {handle}>&-
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
            *) printf "usage: test_set_contract.bash --output NEW_DIRECTORY [--products DIRECTORY]\n" >&2; exit 2 ;;
        esac
    done
    [[ -n "${OUTPUT}" ]] || { printf "test_set_contract: --output is required\n" >&2; exit 2; }
    if [[ -z "${PRODUCTS}" ]]; then
        PRODUCTS="$(printf "%s\n" "${ROOT}"/bin/*/snmp3Ioc)"
        PRODUCTS="${PRODUCTS%/snmp3Ioc}"
    fi
    ARCH="${PRODUCTS##*/}"
    [[ -x "${PRODUCTS}/snmp3Ioc" && -x "${PRODUCTS}/snmp3NativeAgent" ]] ||
        { printf "test_set_contract: products not found in %s\n" "${PRODUCTS}" >&2; exit 2; }
    mkdir -m 700 "${OUTPUT}" || { printf "test_set_contract: cannot create %s\n" "${OUTPUT}" >&2; exit 2; }
    OUTPUT="$(cd "${OUTPUT}" && pwd)"
    trap stop_children EXIT
    scenario || status=$?
    stop_children
    [[ ${status} -eq 0 ]] || cat "${OUTPUT}/tool.stderr" >&2
    perl "${TOOL}" judge "${OUTPUT}"
    exit $?
}

main "$@"
