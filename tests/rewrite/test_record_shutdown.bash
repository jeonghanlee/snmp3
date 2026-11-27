#!/usr/bin/env bash
# Record shutdown cell: verifies pending and retained record work through the exit of a real, non-isolated IOC. For each case the
# cell starts a fixture agent, a fault proxy in front of it (it drops every answer in the case inflight and passes them in the case
# retained) and a private caRepeater, writes a configuration and a startup file, starts the IOC with its shell on a FIFO, makes two
# records active through Channel Access, reads the worker of the IOC and the runtime report, and ends the IOC with exit. The case
# inflight runs the production IOC; the case retained runs the shutdown test program, which holds two retired native results in the
# Base callback queue. record_shutdown.pl writes the configuration, reads the worker and judges every case.
# usage: test_record_shutdown.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers] [--case inflight|retained|all]
# exit status: 0 every case passed, 1 a check failed, 2 the cell could not be judged

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/record_shutdown.pl"
readonly ARCH="linux-x86_64"
readonly AGENT_USER="fixtureUser"
readonly READY_SECONDS=10
readonly AGENT_SECONDS=5
readonly STOP_SECONDS=8
readonly PROCESS_SECONDS=40
readonly CLIENT_SECONDS=40
readonly -a CASES=(inflight retained)

OUTPUT=""
PRODUCTS="${ROOT}/bin/${ARCH}"
SANITIZERS=0
CASE_DIR=""
IOC_PID=""
declare -a ENV_RUN=()
declare -A CHILD_PIDS=()

function die {
    printf "test_record_shutdown: %s\n" "$1" >&2
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

# Waits until a command succeeds, for at most the given seconds. Arguments: seconds, command...
function wait_for {
    local seconds="$1" started=${SECONDS}
    shift
    while [[ $((SECONDS - started)) -lt ${seconds} ]]; do
        if "$@"; then
            return 0
        fi
        sleep 0.01
    done
    "$@"
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
    for name in repeater shutdown-proxy agent-ipv4-1; do
        stop_child "${name}"
    done
}

# Sends exit to the IOC, waits for it, and keeps its exit status and whether it had to be killed in CASE_DIR/ioc.stop.
function end_ioc {
    local status=0 forced=0 started alive
    [[ -n "${IOC_PID}" ]] || return 0
    if kill -0 "${IOC_PID}" 2> /dev/null; then
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

# Starts the fixture agent of the case and waits until it says that it is ready. Returns 0 when it is ready.
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

# Starts the fault proxy in front of the agent in the given mode and waits for its port, which it keeps in
# CASE_DIR/shutdown-proxy.ready (0 when the proxy is not ready). Returns 1 when it is not ready.
function start_proxy {
    local agent_port="$1" mode="$2" name="shutdown-proxy" started port=""
    "${ENV_RUN[@]}" python3 "${ROOT}/tests/rewrite/helpers/udp_fault.py" --family 4 --agent-port "${agent_port}" --mode "${mode}" \
        > "${CASE_DIR}/${name}.stdout" 2> "${CASE_DIR}/${name}.stderr" &
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

# Runs one Channel Access client with a bound on its time and keeps its exit status in CASE_DIR/NAME.stop. Arguments: name, tool,
# arguments... Returns 1 when the client did not end with status 0.
function ca_client {
    local name="$1" tool="$2" base status=0 forced=0
    shift 2
    base="$(base_directory)" || die "configure/RELEASE.local names no EPICS_BASE"
    timeout -s KILL "${CLIENT_SECONDS}" "${ENV_RUN[@]}" "${base}/bin/${ARCH}/${tool}" -w 5 -t "$@" \
        < /dev/null > "${CASE_DIR}/${name}.stdout" 2> "${CASE_DIR}/${name}.stderr" || status=$?
    [[ ${status} -ne 137 ]] || forced=1
    printf "%s\t%s\n" "${status}" "${forced}" > "${CASE_DIR}/${name}.stop"
    [[ ${status} -eq 0 && ${forced} -eq 0 ]]
}

# shellcheck disable=SC2317  # run through wait_for
function ioc_ready {
    kill -0 "${IOC_PID}" 2> /dev/null &&
        grep -q "iocRun: All initialization complete" "${CASE_DIR}/ioc.stderr" 2> /dev/null &&
        { [[ "$1" == inflight ]] || grep -q '"phase":"prepared"' "${CASE_DIR}/ioc.stdout" 2> /dev/null; }
}

# shellcheck disable=SC2317  # run through wait_for
function wire_seen {
    grep -q '"fault_request"' "${CASE_DIR}/shutdown-proxy.stdout" 2> /dev/null
}

# shellcheck disable=SC2317  # run through wait_for
function loop_reported {
    grep -Eq 'snmp3 loop: passes=[0-9]+' "${CASE_DIR}/ioc.stdout" 2> /dev/null
}

# shellcheck disable=SC2317  # run through wait_for
function queued_seen {
    grep -q '"phase":"queued"' "${CASE_DIR}/ioc.stdout" 2> /dev/null
}

# shellcheck disable=SC2317  # run through wait_for
function repeater_owned {
    perl "${JUDGE}" owns "${CHILD_PIDS[repeater]}" "$1"
}

# Runs one case in its own directory. Sets CASE_STATUS to the status that the judge returns.
function run_case {
    local case="$1" executable mode setup prepare script base agent_port proxy_port server repeater aborted=0 stimulus exit_ns
    CASE_DIR="${OUTPUT}/${case}"
    mkdir -m 700 "${CASE_DIR}" || die "cannot create ${CASE_DIR}"
    executable=snmp3Ioc
    mode=drop-all
    if [[ "${case}" == retained ]]; then
        executable=snmp3ShutdownTest
        mode=pass
    fi
    printf "%s\n" "${PRODUCTS}" > "${CASE_DIR}/products.txt"
    base="$(base_directory)" || die "configure/RELEASE.local names no EPICS_BASE"
    CHILD_PIDS=()
    IOC_PID=""
    if ! start_agent; then
        aborted=1
    elif ! start_proxy "$(< "${CASE_DIR}/agent-ipv4-1.port")" "${mode}"; then
        aborted=1
    else
        proxy_port="$(< "${CASE_DIR}/shutdown-proxy.ready")"
        server="$(perl "${JUDGE}" port)" || die "no free UDP port"
        repeater="$(perl "${JUDGE}" port)" || die "no free UDP port"
        while [[ "${server}" == "${repeater}" ]]; do
            repeater="$(perl "${JUDGE}" port)" || die "no free UDP port"
        done
        ENV_RUN+=("EPICS_CA_SERVER_PORT=${server}" "EPICS_CA_REPEATER_PORT=${repeater}" EPICS_CA_AUTO_ADDR_LIST=NO
            "EPICS_CA_ADDR_LIST=127.0.0.1:${server}" EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 EPICS_CAS_BEACON_AUTO_ADDR_LIST=NO
            "EPICS_CAS_BEACON_ADDR_LIST=127.0.0.1:${repeater}")
        perl "${JUDGE}" config "${CASE_DIR}" "${proxy_port}" || die "cannot write the configuration"
        setup=""
        prepare=""
        if [[ "${case}" == retained ]]; then
            setup="snmp3ShutdownSetup(\"${CASE_DIR}/worker.stderr\",${SANITIZERS})"$'\n'
            prepare="snmp3ShutdownPrepare"$'\n'
        fi
        script="${CASE_DIR}/startup.cmd"
        {
            printf "%s\n" "on error break" "dbLoadDatabase(\"${ROOT}/dbd/${executable}.dbd\")" \
                "${executable}_registerRecordDeviceDriver(pdbbase)"
            printf "%s" "${setup}"
            printf "%s\n" "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")" \
                "snmp3Load(\"${CASE_DIR}/records.json\",\"${PRODUCTS}/snmp3NativeProbe\")" \
                "dbLoadRecords(\"${ROOT}/tests/rewrite/db/record-shutdown.db\",\"P=Shutdown_\")" iocInit
            printf "%s" "${prepare}"
        } > "${script}"
        "${ENV_RUN[@]}" "${base}/bin/${ARCH}/caRepeater" < /dev/null > "${CASE_DIR}/repeater.stdout" 2> "${CASE_DIR}/repeater.stderr" &
        CHILD_PIDS[repeater]=$!
        if ! wait_for "${READY_SECONDS}" repeater_owned "${repeater}"; then
            aborted=1
        else
            mkfifo -m 600 "${CASE_DIR}/ioc.stdin" || die "cannot create the shell FIFO"
            exec 7<> "${CASE_DIR}/ioc.stdin"
            "${ENV_RUN[@]}" "${PRODUCTS}/${executable}" "${script}" < "${CASE_DIR}/ioc.stdin" \
                > "${CASE_DIR}/ioc.stdout" 2> "${CASE_DIR}/ioc.stderr" &
            IOC_PID=$!
            if wait_for "${READY_SECONDS}" ioc_ready "${case}"; then
                printf "1\n" > "${CASE_DIR}/ioc.ready"
            else
                printf "0\n" > "${CASE_DIR}/ioc.ready"
                aborted=1
            fi
        fi
        if [[ ${aborted} -eq 0 ]]; then
            stimulus="$(perl "${JUDGE}" now)"
            ca_client ca-002-caput caput Shutdown_Input.PROC 1 || aborted=1
            [[ ${aborted} -eq 1 ]] || ca_client ca-003-caput caput Shutdown_Output.PROC 1 || aborted=1
            [[ ${aborted} -eq 1 ]] || ca_client ca-004-caget caget Shutdown_Input.PACT || aborted=1
            [[ ${aborted} -eq 1 ]] || ca_client ca-005-caget caget Shutdown_Output.PACT || aborted=1
        fi
        if [[ ${aborted} -eq 0 && "${case}" == retained ]]; then
            printf "snmp3ShutdownAwait\n" >&7
            wait_for $((READY_SECONDS + 2)) queued_seen || aborted=1
        fi
        if [[ ${aborted} -eq 0 ]]; then
            if wait_for "${READY_SECONDS}" wire_seen; then
                printf "1\n" > "${CASE_DIR}/wire.ready"
            else
                printf "0\n" > "${CASE_DIR}/wire.ready"
            fi
            perl "${JUDGE}" workers "${IOC_PID}" "${PRODUCTS}" "${CASE_DIR}/workers.json" || die "cannot read the workers"
            printf "snmp3RuntimeReport\n" >&7
            if wait_for "${READY_SECONDS}" loop_reported; then
                # The report that the former harness reads before it ends the IOC; the final output holds the fallback report too.
                cp "${CASE_DIR}/ioc.stdout" "${CASE_DIR}/before-exit.stdout" || die "cannot keep the report before the exit"
                exit_ns="$(perl "${JUDGE}" now)"
                printf "%s %s\n" "${stimulus}" "${exit_ns}" > "${CASE_DIR}/exit.ns"
            else
                aborted=1
            fi
        fi
    fi
    [[ ${aborted} -eq 0 ]] || : > "${CASE_DIR}/aborted"
    end_ioc
    stop_children
    CASE_STATUS=0
    perl "${JUDGE}" judge "${CASE_DIR}" "${case}" || CASE_STATUS=$?
}

function main {
    local output="" selected=all case status=0 any=0
    local -a run=()
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --products) PRODUCTS="$(cd "${2:-}" 2> /dev/null && pwd -P)" || die "--products must name a directory"; shift 2 ;;
            --sanitizers) SANITIZERS=1; shift ;;
            --case) selected="${2:-}"; shift 2 ;;
            *) die "usage: test_record_shutdown.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers] [--case inflight|retained|all]" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    [[ ! -e "${output}" ]] || die "--output must be a new directory"
    if [[ "${selected}" == all ]]; then
        run=("${CASES[@]}")
    else
        for case in "${CASES[@]}"; do
            [[ "${case}" != "${selected}" ]] || any=1
        done
        [[ ${any} -eq 1 ]] || die "unknown case ${selected}"
        run=("${selected}")
    fi
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd -P)"
    for case in snmp3Ioc snmp3ShutdownTest snmp3Worker snmp3NativeAgent snmp3NativeProbe; do
        [[ -x "${PRODUCTS}/${case}" ]] || die "product ${case} not found in ${PRODUCTS}"
    done
    cd "${ROOT}" || die "cannot enter the checkout"
    for case in "${run[@]}"; do
        ENV_RUN=(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG_OUTPUT -u LD_LIBRARY_PATH MIBS= LD_DEBUG=libs)
        if [[ ${SANITIZERS} -eq 1 ]]; then
            ENV_RUN+=(ASAN_OPTIONS=detect_leaks=0:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1)
        fi
        run_case "${case}"
        [[ ${CASE_STATUS} -le 1 ]] || die "the judge could not read the observations of ${case}"
        [[ ${CASE_STATUS} -eq 0 ]] || status=1
    done
    perl "${JUDGE}" summary "${OUTPUT}" "${run[@]}" || true
    exit "${status}"
}

main "$@"
