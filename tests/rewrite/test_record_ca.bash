#!/usr/bin/env bash
# Record Channel Access cell: qualifies real Channel Access clients through the production IOC, its records and the native agent.
# The cell starts a fixture agent, a fault proxy that delays every answer by one second, a private caRepeater and the production
# IOC with its shell on a FIFO. Then it runs caget and caput clients, one process for each operation, through the records of
# tests/rewrite/db/record-ca.db: the numeric and text values, the input that runs at start, the active outputs that receive a second
# write while their first request is in flight, and the signed minimum. It reads the worker of the IOC and ends the IOC with exit.
# Every client keeps its output and exit status in OUTPUT/ca-NNN-TOOL.* for record_ca.pl, which writes the configuration, counts
# events while the cell waits, reads the worker and judges.
# usage: test_record_ca.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers]
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/record_ca.pl"
readonly ARCH="linux-x86_64"
readonly AGENT_USER="fixtureUser"
readonly READY_SECONDS=10
readonly AGENT_SECONDS=5
readonly STOP_SECONDS=8
readonly PROCESS_SECONDS=15
readonly DELAY_MS=1000

OUTPUT=""
PRODUCTS="${ROOT}/bin/${ARCH}"
SANITIZERS=0
CASE_DIR=""
BASE=""
IOC_PID=""
EXIT_SENT=0
ORD=0
VALUE=""
CA_NAME=""
NOW_MS=0
NOTIFIER=""
NOTIFIER_PID=""
SETTLE_DELIVERED=0
SETTLE_SECOND=0
REPEATED=""
declare -a ENV_RUN=()
declare -A CHILD_PIDS=()

function die {
    printf "test_record_ca: %s\n" "$1" >&2
    end_ioc
    stop_children
    exit 2
}

# Prints the EPICS Base directory that the tree builds against.
function base_directory {
    local line
    while IFS= read -r line; do
        line="${line//$'\r'/}"
        if [[ "${line}" =~ ^[[:space:]]*EPICS_BASE[[:space:]]*=[[:space:]]*(.*)$ ]]; then
            printf "%s\n" "${BASH_REMATCH[1]}"
            return 0
        fi
    done < "${ROOT}/configure/RELEASE.local"
    return 1
}

# Writes a secret file of 48 hexadecimal characters that only the owner can read.
function new_secret {
    local path="$1"
    (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${path}" && printf "\n" >> "${path}") ||
        die "cannot write a secret file"
}

# Sets NOW_MS to the wall clock in milliseconds.
function now_ms {
    local microseconds="${EPOCHREALTIME/./}"
    NOW_MS=$((microseconds / 1000))
}

# Waits until a command succeeds, for at most the given seconds; the command runs once more when the time is over.
# Arguments: seconds, command...
function wait_for {
    local seconds="$1" end
    shift
    now_ms
    end=$((NOW_MS + seconds * 1000))
    while now_ms; [[ ${NOW_MS} -lt ${end} ]]; do
        if "$@"; then
            return 0
        fi
        sleep 0.01
    done
    "$@"
}

# Sets REPEATED to the character repeated count times. Arguments: character, count.
function repeat {
    local spaces
    printf -v spaces "%*s" "$2" ""
    REPEATED="${spaces// /$1}"
}

# Ends one child with SIGTERM, waits for it and keeps its exit status and whether it had to be killed in CASE_DIR/NAME.stop.
function stop_child {
    local name="$1" pid status=0 forced=0 started alive
    pid="${CHILD_PIDS[${name}]:-}"
    [[ -n "${pid}" ]] || return 0
    kill -TERM "${pid}" 2> /dev/null
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${STOP_SECONDS} ]]; do
        alive="$(ps -o stat= -p "${pid}" 2> /dev/null)"
        [[ -n "${alive}" && "${alive}" != Z* ]] || break
        sleep 0.01
    done
    alive="$(ps -o stat= -p "${pid}" 2> /dev/null)"
    if [[ -n "${alive}" && "${alive}" != Z* ]]; then
        kill -KILL "${pid}" 2> /dev/null
        forced=1
    fi
    wait "${pid}" 2> /dev/null || status=$?
    printf "%s\t%s\n" "${status}" "${forced}" > "${CASE_DIR}/${name}.stop"
    CHILD_PIDS[${name}]=""
}

# Ends the repeater, the proxy and the agent in the order of the former harness.
function stop_children {
    local name
    for name in repeater ca-response-delay agent-ipv4-1; do
        stop_child "${name}"
    done
}

# Sends exit to the IOC when it has not been sent, waits for it, and keeps its exit status and whether it had to be killed in
# CASE_DIR/ioc.stop.
function end_ioc {
    local status=0 forced=0 started alive
    [[ -n "${IOC_PID}" ]] || return 0
    if [[ ${EXIT_SENT} -eq 0 ]] && kill -0 "${IOC_PID}" 2> /dev/null; then
        printf "exit\n" 2> /dev/null >&7 || true
    fi
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${PROCESS_SECONDS} ]]; do
        alive="$(ps -o stat= -p "${IOC_PID}" 2> /dev/null)"
        [[ -n "${alive}" && "${alive}" != Z* ]] || break
        sleep 0.01
    done
    alive="$(ps -o stat= -p "${IOC_PID}" 2> /dev/null)"
    if [[ -n "${alive}" && "${alive}" != Z* ]]; then
        kill -KILL "${IOC_PID}" 2> /dev/null
        forced=1
    fi
    wait "${IOC_PID}" 2> /dev/null || status=$?
    exec 7>&-
    printf "%s\t%s\n" "${status}" "${forced}" > "${CASE_DIR}/ioc.stop"
    IOC_PID=""
}

# Starts the fixture agent and waits until it says that it is ready. Returns 0 when it is ready.
function start_agent {
    local port community auth privacy started name="agent-ipv4-1"
    port="$(perl "${JUDGE}" port)" || die "no free UDP port"
    new_secret "${CASE_DIR}/${name}-community"
    new_secret "${CASE_DIR}/${name}-auth"
    new_secret "${CASE_DIR}/${name}-privacy"
    read -r community < "${CASE_DIR}/${name}-community"
    read -r auth < "${CASE_DIR}/${name}-auth"
    read -r privacy < "${CASE_DIR}/${name}-privacy"
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
        "group nativeGroup usm independentFixtureUser" > "${CASE_DIR}/${name}.conf") || die "cannot write the agent configuration"
    "${ENV_RUN[@]}" "${PRODUCTS}/snmp3NativeAgent" "${port}" 4 "${CASE_DIR}/${name}.conf" \
        > "${CASE_DIR}/${name}.stdout" 2> "${CASE_DIR}/${name}.stderr" &
    CHILD_PIDS[${name}]=$!
    printf "%s\n" "${port}" > "${CASE_DIR}/${name}.port"
    printf "0\n" > "${CASE_DIR}/${name}.ready"
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${AGENT_SECONDS} ]] && kill -0 "${CHILD_PIDS[${name}]}" 2> /dev/null; do
        if grep -q '"event":"agent_ready"' "${CASE_DIR}/${name}.stdout" 2> /dev/null; then
            printf "1\n" > "${CASE_DIR}/${name}.ready"
            perl "${JUDGE}" sockets "${CHILD_PIDS[${name}]}" "${port}" "${CASE_DIR}/${name}.sockets.json" ||
                die "cannot read the sockets of the agent"
            return 0
        fi
        sleep 0.01
    done
    return 1
}

# Starts the fault proxy that delays every answer in front of the agent and waits for its port, which it keeps in
# CASE_DIR/ca-response-delay.ready (0 when the proxy is not ready). Returns 1 when it is not ready.
function start_proxy {
    local agent_port="$1" name="ca-response-delay" started port=""
    "${ENV_RUN[@]}" python3 "${ROOT}/tests/rewrite/helpers/udp_fault.py" --family 4 --agent-port "${agent_port}" --mode delay \
        --delay-ms "${DELAY_MS}" > "${CASE_DIR}/${name}.stdout" 2> "${CASE_DIR}/${name}.stderr" &
    CHILD_PIDS[${name}]=$!
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${AGENT_SECONDS} ]] && kill -0 "${CHILD_PIDS[${name}]}" 2> /dev/null; do
        port="$(grep '"fault_ready"' "${CASE_DIR}/${name}.stdout" 2> /dev/null | grep -o '"port": *[0-9]*' | grep -o '[0-9]*$' | head -1)"
        [[ -z "${port}" ]] || break
        sleep 0.01
    done
    printf "%s\n" "${port:-0}" > "${CASE_DIR}/${name}.ready"
    [[ -n "${port}" ]]
}

# Runs one Channel Access client, in the foreground, with a bound on its time and keeps its exit status in CASE_DIR/ca-NNN-TOOL.rc
# (and 1 in .forced when it had to be killed). A status other than the expected one ends the operations. Sets CA_NAME.
# Arguments: tool, expected status, arguments...
function ca_run {
    local tool="$1" expected="$2" status=0 forced=0
    shift 2
    ORD=$((ORD + 1))
    printf -v CA_NAME "ca-%03d-%s" "${ORD}" "${tool}"
    timeout -s KILL "${PROCESS_SECONDS}" "${ENV_RUN[@]}" "${BASE}/bin/${ARCH}/${tool}" -w 5 "$@" \
        < /dev/null > "${CASE_DIR}/${CA_NAME}.stdout" 2> "${CASE_DIR}/${CA_NAME}.stderr" || status=$?
    [[ ${status} -ne 137 ]] || forced=1
    printf "%s\n" "${status}" > "${CASE_DIR}/${CA_NAME}.rc"
    printf "%s\n" "${forced}" > "${CASE_DIR}/${CA_NAME}.forced"
    if [[ ${status} -ne ${expected} || ${forced} -ne 0 ]]; then
        exit 1
    fi
}

# Reads one value with caget into VALUE. Arguments: name, and long as the second argument for a long string.
function value {
    local flag=-s
    [[ "${2:-}" != long ]] || flag=-S
    ca_run caget 0 -t "${flag}" "$1"
    VALUE=""
    read -r VALUE < "${CASE_DIR}/${CA_NAME}.stdout" || true
}

# Processes a record with caput and callback, and reads its PACT and SEVR. Argument: record name.
function process_record {
    ca_run caput 0 -c -t "$1.PROC" 1
    value "$1.PACT"
    value "$1.SEVR"
}

# Runs a caput client in the background; its exit time (the monotonic clock in nanoseconds) is read at its exit.
# Arguments: name, arguments of caput...
function notifier_body {
    local name="$1" status=0 forced=0
    shift
    timeout -s KILL "${PROCESS_SECONDS}" "${ENV_RUN[@]}" "${BASE}/bin/${ARCH}/caput" "$@" \
        < /dev/null > "${CASE_DIR}/${name}.stdout" 2> "${CASE_DIR}/${name}.stderr" || status=$?
    perl "${JUDGE}" now > "${CASE_DIR}/${name}.exit_ns"
    [[ ${status} -ne 137 ]] || forced=1
    printf "%s\n" "${status}" > "${CASE_DIR}/${name}.rc"
    printf "%s\n" "${forced}" > "${CASE_DIR}/${name}.forced"
}

# Starts the background client. Sets NOTIFIER and NOTIFIER_PID. Arguments: arguments of caput...
function start_notifier {
    ORD=$((ORD + 1))
    printf -v NOTIFIER "ca-%03d-caput" "${ORD}"
    notifier_body "${NOTIFIER}" "$@" &
    NOTIFIER_PID=$!
}

# Prints the number of events of the proxy output. Arguments: event, and the command of the application message when it is needed.
function proxy_count {
    perl "${JUDGE}" count proxy "${CASE_DIR}/ca-response-delay.stdout" "$1" ${2:+"$2"}
}

# Prints the number of values that the agent stored for an index. Argument: index.
function agent_count {
    perl "${JUDGE}" count agent "${CASE_DIR}/agent-ipv4-1.stdout" "$1"
}

# Succeeds when the proxy has at least the given number of events. Arguments: event, command (may be empty), number.
# shellcheck disable=SC2317  # run through wait_for
function proxy_atleast {
    local count
    count="$(proxy_count "$1" "${2:-}")" || count=0
    [[ "${count:-0}" -ge "$3" ]]
}

# shellcheck disable=SC2317  # run through wait_for
function ioc_ready {
    kill -0 "${IOC_PID}" 2> /dev/null && grep -q "iocRun: All initialization complete" "${CASE_DIR}/ioc.stderr" 2> /dev/null
}

# shellcheck disable=SC2317  # run through wait_for
function repeater_owned {
    perl "${JUDGE}" owns "${CHILD_PIDS[repeater]}" "$1"
}

# Waits until the record is idle: reads PACT until it is 0 or the time is over, and once more. Argument: record name.
function wait_idle {
    local end
    now_ms
    end=$((NOW_MS + PROCESS_SECONDS * 1000))
    while now_ms; [[ ${NOW_MS} -lt ${end} ]]; do
        value "$1.PACT"
        [[ "${VALUE}" != 0 ]] || break
        sleep 0.01
    done
    value "$1.PACT"
}

# Waits for the first delayed response, for a second request, for its delivery, and for the record to be idle. Sets
# SETTLE_DELIVERED and SETTLE_SECOND. Arguments: record name, number of requests before, number of deliveries before.
function settle {
    local name="$1" sets="$2" deliveries="$3"
    SETTLE_DELIVERED=0
    SETTLE_SECOND=0
    if wait_for "${PROCESS_SECONDS}" proxy_atleast fault_delayed_delivery "" $((deliveries + 1)); then
        SETTLE_DELIVERED=1
    fi
    wait_for $((3 * DELAY_MS / 1000)) proxy_atleast fault_request 163 $((sets + 2)) || true
    if proxy_atleast fault_request 163 $((sets + 2)); then
        SETTLE_SECOND=1
        wait_for "${PROCESS_SECONDS}" proxy_atleast fault_delayed_delivery "" $((deliveries + 2)) || true
    fi
    wait_idle "${name}"
}

# One active output with a second write while the first request is in flight. Arguments: kind, index, route, first value,
# latest value. The route plain-put writes the second value with a plain put, put-callback with a put that waits for the callback.
function active_step {
    local kind="$1" index="$2" route="$3" first="$4" latest="$5" name field applied=0 sets deliveries stored_before
    local end_requests end_deliveries end_agent
    local -a mode=()
    name="Ca_Active${kind}"
    field="${name}"
    if [[ "${kind}" == Lso ]]; then
        field="${name}.VAL\$"
        mode=(-S)
    fi
    sets="$(proxy_count fault_request 163)"
    deliveries="$(proxy_count fault_delayed_delivery)"
    stored_before="$(agent_count "${index}")"
    ca_run caput 0 -t "${mode[@]}" "${field}" "${first}"
    if wait_for "${PROCESS_SECONDS}" proxy_atleast fault_response 162 $((sets + 1)); then
        applied=1
    fi
    NOTIFIER=""
    if [[ "${route}" == plain-put ]]; then
        ca_run caput 0 -t "${mode[@]}" "${field}" "${latest}"
    else
        start_notifier -c -w 8.0 -t "${mode[@]}" "${field}" "${latest}"
    fi
    value "${name}.PACT"
    value "${name}.RPRO"
    settle "${name}" "${sets}" "${deliveries}"
    if [[ -n "${NOTIFIER}" ]]; then
        wait "${NOTIFIER_PID}" 2> /dev/null || true
    fi
    end_requests="$(proxy_count fault_request 163)"
    end_deliveries="$(proxy_count fault_delayed_delivery)"
    end_agent="$(agent_count "${index}")"
    printf "%s:%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "${name}" "${route}" "${sets}" "${deliveries}" "${stored_before}" \
        "${applied}" "${SETTLE_DELIVERED}" "${SETTLE_SECOND}" "${end_requests}" "${end_deliveries}" "${end_agent}" \
        >> "${CASE_DIR}/active.tsv"
    value "${name}.STAT"
    value "${name}.SEVR"
    value "${name}.AMSG"
    value "${name}.PACT"
    if [[ "${kind}" == Lso ]]; then
        value "${field}" long
    else
        value "${field}"
    fi
    value Ca_ActiveCompleted
}

# The Channel Access operations of the cell, in the order of the former harness, then the worker and the exit of the IOC. It runs
# in a subshell: a client with an unexpected status ends it with status 1.
function operations {
    local number length text started end
    now_ms
    end=$((NOW_MS + READY_SECONDS * 1000))
    while :; do
        value Ca_Pini.PACT
        [[ "${VALUE}" != 0 ]] || break
        now_ms
        [[ ${NOW_MS} -lt ${end} ]] || break
    done
    value Ca_Pini
    value Ca_Completed
    process_record Ca_UnsignedWide
    value Ca_UnsignedWide
    for number in 9007199254740993 9223372036854775807; do
        ca_run caput 0 -c -t Ca_WideOut "${number}"
        process_record Ca_WideIn
        value Ca_WideOut
        value Ca_WideIn
    done
    for length in 0 39 40 200 255; do
        repeat T "${length}"
        text="${REPEATED}"
        ca_run caput 0 -c -t -S 'Ca_TextOut.VAL$' "${text}"
        process_record Ca_TextIn
        value 'Ca_TextOut.VAL$' long
        value 'Ca_TextIn.VAL$' long
        value Ca_TextOut.LEN
        value Ca_TextIn.LEN
    done
    value Ca_TextIn
    repeat B 300
    ca_run caput 1 -c -t -S 'Ca_TextOut.VAL$' "${REPEATED}"
    process_record Ca_TextIn
    value Ca_TextOut.LEN
    value 'Ca_TextOut.VAL$' long
    value 'Ca_TextIn.VAL$' long
    ca_run caput 0 -c -t Ca_ShortOut short-wire
    process_record Ca_ShortIn
    value Ca_ShortIn
    : > "${CASE_DIR}/active.tsv"
    active_step Ao 9 plain-put 1.5 2.5
    active_step Ao 9 put-callback 3.5 4.5
    active_step Longout 1 plain-put 31 32
    active_step Longout 1 put-callback 33 34
    active_step Int64out 5 plain-put 9007199254740993 9007199254740995
    active_step Int64out 5 put-callback 9007199254740997 9007199254740999
    active_step Stringout 6 plain-put first-short latest-short
    active_step Stringout 6 put-callback third-short fourth-short
    repeat F 200
    started="${REPEATED}"
    repeat L 250
    active_step Lso 6 plain-put "${started}" "${REPEATED}"
    repeat T 120
    started="${REPEATED}"
    repeat U 230
    active_step Lso 6 put-callback "${started}" "${REPEATED}"
    ca_run caput 0 -c -t Ca_SignedOut -2147483648
    process_record Ca_IntegerIn
    process_record Ca_SignedWideIn
    value Ca_SignedOut
    value Ca_IntegerIn
    value Ca_SignedWideIn
    process_record Ca_WideIn
    value Ca_WideIn
    ca_run caput 0 -c -t Ca_WideOut -9223372036854775808
    value Ca_WideOut
    value Ca_WideOut.STAT
    value Ca_WideOut.SEVR
    value Ca_WideOut.PACT
    process_record Ca_WideIn
    value Ca_WideIn
    value Ca_IntegerIn
    perl "${JUDGE}" workers "${IOC_PID}" "${PRODUCTS}" "${CASE_DIR}/workers.json" || exit 1
    printf "snmp3Report\nexit\n" >&7
}

# Runs the cell in OUTPUT. Sets CASE_STATUS to the status that the judge returns.
function run_cell {
    local script server repeater proxy_port aborted=0 ops=0
    CASE_DIR="${OUTPUT}"
    printf "%s\n" "${PRODUCTS}" > "${CASE_DIR}/products.txt"
    printf "%s\n" "${BASE}" > "${CASE_DIR}/base.txt"
    if ! start_agent; then
        aborted=1
    elif ! start_proxy "$(< "${CASE_DIR}/agent-ipv4-1.port")"; then
        aborted=1
    else
        proxy_port="$(< "${CASE_DIR}/ca-response-delay.ready")"
        server="$(perl "${JUDGE}" port)" || die "no free UDP port"
        repeater="$(perl "${JUDGE}" port)" || die "no free UDP port"
        while [[ "${server}" == "${repeater}" ]]; do
            repeater="$(perl "${JUDGE}" port)" || die "no free UDP port"
        done
        ENV_RUN+=("EPICS_CA_SERVER_PORT=${server}" "EPICS_CA_REPEATER_PORT=${repeater}" EPICS_CA_AUTO_ADDR_LIST=NO
            "EPICS_CA_ADDR_LIST=127.0.0.1:${server}" EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 EPICS_CAS_BEACON_AUTO_ADDR_LIST=NO
            "EPICS_CAS_BEACON_ADDR_LIST=127.0.0.1:${repeater}")
        printf '{"server": %s, "repeater": %s}\n' "${server}" "${repeater}" > "${CASE_DIR}/ports.json"
        perl "${JUDGE}" config "${CASE_DIR}" "${proxy_port}" || die "cannot write the configuration"
        script="${CASE_DIR}/startup.cmd"
        printf "%s\n" "on error break" "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")" "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" \
            "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")" "snmp3Load(\"${CASE_DIR}/records.json\",\"${PRODUCTS}/snmp3NativeProbe\")" \
            "dbLoadRecords(\"${ROOT}/tests/rewrite/db/record-ca.db\",\"P=Ca_\")" iocInit snmp3Report > "${script}"
        "${ENV_RUN[@]}" "${BASE}/bin/${ARCH}/caRepeater" < /dev/null > "${CASE_DIR}/repeater.stdout" 2> "${CASE_DIR}/repeater.stderr" &
        CHILD_PIDS[repeater]=$!
        if wait_for "${READY_SECONDS}" repeater_owned "${repeater}"; then
            printf "1\n" > "${CASE_DIR}/repeater.owned"
        else
            printf "0\n" > "${CASE_DIR}/repeater.owned"
        fi
        mkfifo -m 600 "${CASE_DIR}/ioc.stdin" || die "cannot create the shell FIFO"
        exec 7<> "${CASE_DIR}/ioc.stdin"
        "${ENV_RUN[@]}" "${PRODUCTS}/snmp3Ioc" "${script}" < "${CASE_DIR}/ioc.stdin" \
            > "${CASE_DIR}/ioc.stdout" 2> "${CASE_DIR}/ioc.stderr" &
        IOC_PID=$!
        if wait_for "${READY_SECONDS}" ioc_ready; then
            printf "1\n" > "${CASE_DIR}/ioc.ready"
            (operations) || ops=$?
            if [[ ${ops} -eq 0 ]]; then
                EXIT_SENT=1
            else
                aborted=1
            fi
        else
            printf "0\n" > "${CASE_DIR}/ioc.ready"
            aborted=1
        fi
    fi
    [[ ${aborted} -eq 0 ]] || : > "${CASE_DIR}/aborted"
    end_ioc
    stop_children
    CASE_STATUS=0
    perl "${JUDGE}" judge "${CASE_DIR}" || CASE_STATUS=$?
}

function main {
    local output="" name
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --products) PRODUCTS="$(cd "${2:-}" 2> /dev/null && pwd -P)" || die "--products must name a directory"; shift 2 ;;
            --sanitizers) SANITIZERS=1; shift ;;
            *) die "usage: test_record_ca.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers]" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    [[ ! -e "${output}" ]] || die "--output must be a new directory"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd -P)"
    for name in snmp3Ioc snmp3Worker snmp3NativeAgent snmp3NativeProbe; do
        [[ -x "${PRODUCTS}/${name}" ]] || die "product ${name} not found in ${PRODUCTS}"
    done
    BASE="$(base_directory)" || die "configure/RELEASE.local names no EPICS_BASE"
    cd "${ROOT}" || die "cannot enter the checkout"
    ENV_RUN=(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG_OUTPUT -u LD_LIBRARY_PATH MIBS= LD_DEBUG=libs)
    if [[ ${SANITIZERS} -eq 1 ]]; then
        ENV_RUN+=(ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
    fi
    run_cell
    [[ ${CASE_STATUS} -le 1 ]] || die "the judge could not read the observations"
    exit "${CASE_STATUS}"
}

main "$@"
