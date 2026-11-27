#!/usr/bin/env bash
# Record startup cell: verifies rejected records and real Runtime startup failures through the exit of the IOC. For each case the
# cell starts a fixture agent, a pass-through fault proxy in front of it and a private caRepeater, writes a configuration and a
# startup file, and runs the production IOC or the startup test program on them: normal, a preflight failure (the worker link is
# removed between snmp3Load and iocInit through a FIFO gate) or a real thread failure (the command runs under a tracer that makes
# the kernel refuse the thread). record_startup.pl writes the configuration, waits at the gate, and judges every case.
# usage: test_record_startup.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers] [--case CASE|all]
# exit status: 0 every case passed, 1 a check failed, 2 the cell could not be judged
# The cases: normal production-break production-continue failure-break failure-continue thread-normal thread-break
# thread-continue production-thread-break production-thread-continue

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/record_startup.pl"
readonly ARCH="linux-x86_64"
readonly AGENT_USER="fixtureUser"
readonly READY_SECONDS=10
readonly PROXY_READY_SECONDS=5
readonly STOP_SECONDS=8
readonly PROCESS_SECONDS=30
readonly -a CASES=(normal production-break production-continue failure-break failure-continue thread-normal thread-break
    thread-continue production-thread-break production-thread-continue)

OUTPUT=""
PRODUCTS="${ROOT}/bin/${ARCH}"
SANITIZERS=0
CASE_DIR=""
declare -a ENV_RUN=()
declare -A CHILD_PIDS=()

function die {
    printf "test_record_startup: %s\n" "$1" >&2
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

# Ends the children of the case in the order of the former harness: the repeater, then the proxy and the agent.
function stop_children {
    local name
    for name in repeater startup-proxy agent-ipv4-1; do
        stop_child "${name}"
    done
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
    while [[ $((SECONDS - started)) -lt ${PROXY_READY_SECONDS} ]] && kill -0 "${CHILD_PIDS[${name}]}" 2> /dev/null; do
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

# Starts the pass-through fault proxy in front of the agent and waits for its port, which it keeps in CASE_DIR/startup-proxy.ready
# (0 when the proxy is not ready). Returns 1 when it is not ready.
function start_proxy {
    local agent_port="$1" name="startup-proxy" started port=""
    "${ENV_RUN[@]}" python3 "${ROOT}/tests/rewrite/helpers/udp_fault.py" --family 4 --agent-port "${agent_port}" --mode pass \
        > "${CASE_DIR}/${name}.stdout" 2> "${CASE_DIR}/${name}.stderr" &
    CHILD_PIDS[${name}]=$!
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${PROXY_READY_SECONDS} ]] && kill -0 "${CHILD_PIDS[${name}]}" 2> /dev/null; do
        port="$(grep '"fault_ready"' "${CASE_DIR}/${name}.stdout" 2> /dev/null | grep -o '"port": *[0-9]*' | grep -o '[0-9]*$' | head -1)"
        [[ -z "${port}" ]] || break
        sleep 0.01
    done
    printf "%s\n" "${port:-0}" > "${CASE_DIR}/${name}.ready"
    [[ -n "${port}" ]]
}

# Runs one case in its own directory. Sets the status that the judge returns in CASE_STATUS.
function run_case {
    local case="$1" failure=0 traced=0 phase production=0 policy="break" executable setup hold worker gate script
    local agent_port proxy_port server repeater base started aborted=0 ioc_pid status
    CASE_DIR="${OUTPUT}/${case}"
    mkdir -m 700 "${CASE_DIR}" || die "cannot create ${CASE_DIR}"
    [[ "${case}" == normal || "${case}" == thread-normal ]] || failure=1
    [[ "${case}" == *thread* ]] && traced=1
    phase=normal
    if [[ ${failure} -eq 1 ]]; then
        phase=preflight
        [[ ${traced} -eq 1 ]] && phase=thread
    fi
    [[ "${case}" == production* ]] && production=1
    [[ "${case}" == *continue ]] && policy="continue"
    executable=snmp3StartupTest
    [[ ${production} -eq 1 ]] && executable=snmp3Ioc
    printf "%s\n" "${PRODUCTS}" > "${CASE_DIR}/products.txt"
    base="$(base_directory)" || die "configure/RELEASE.local names no EPICS_BASE"
    CHILD_PIDS=()
    if ! start_agent; then
        aborted=1
    else
        agent_port="$(< "${CASE_DIR}/agent-ipv4-1.port")"
        if start_proxy "${agent_port}"; then
            proxy_port="$(< "${CASE_DIR}/startup-proxy.ready")"
            server="$(perl "${JUDGE}" port)" || die "no free UDP port"
            repeater="$(perl "${JUDGE}" port)" || die "no free UDP port"
            while [[ "${server}" == "${repeater}" ]]; do
                repeater="$(perl "${JUDGE}" port)" || die "no free UDP port"
            done
            ENV_RUN+=("EPICS_CA_SERVER_PORT=${server}" "EPICS_CA_REPEATER_PORT=${repeater}" EPICS_CA_AUTO_ADDR_LIST=NO
                "EPICS_CA_ADDR_LIST=127.0.0.1:${server}" EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 EPICS_CAS_BEACON_AUTO_ADDR_LIST=NO
                "EPICS_CAS_BEACON_ADDR_LIST=127.0.0.1:${repeater}")
            perl "${JUDGE}" config "${CASE_DIR}" "${proxy_port}" || die "cannot write the configuration"
            worker="${PRODUCTS}/snmp3Worker"
            gate="${CASE_DIR}/load-gate.fifo"
            if [[ "${phase}" == preflight ]]; then
                worker="${CASE_DIR}/missing-worker"
                ln -s "${PRODUCTS}/snmp3Worker" "${worker}" || die "cannot link the worker"
                mkfifo -m 600 "${gate}" || die "cannot create the gate"
            fi
            setup=""
            if [[ ${production} -eq 0 ]]; then
                setup="snmp3StartupSetup(${failure},\"${CASE_DIR}/worker.stderr\",${SANITIZERS})"$'\n'
            fi
            hold=""
            [[ "${phase}" == preflight ]] && hold="< ${gate}"$'\n'
            script="${CASE_DIR}/startup.cmd"
            {
                printf "%s\n" "on error ${policy}" \
                    "dbLoadDatabase(\"${ROOT}/dbd/${executable}.dbd\")" \
                    "${executable}_registerRecordDeviceDriver(pdbbase)"
                printf "%s" "${setup}"
                printf "%s\n" "snmp3WorkerPath(\"${worker}\")" "snmp3Load(\"${CASE_DIR}/records.json\",\"${PRODUCTS}/snmp3NativeProbe\")"
                printf "%s" "${hold}"
                printf "%s\n" "dbLoadRecords(\"${ROOT}/tests/rewrite/db/record-startup.db\",\"P=Startup_\")" iocInit
                [[ ${failure} -eq 0 ]] && printf "%s\n" snmp3StartupExercise
                printf "%s\n" "echo STARTUP_SCRIPT_COMPLETED" snmp3RuntimeReport
            } > "${script}"
            "${ENV_RUN[@]}" "${base}/bin/${ARCH}/caRepeater" < /dev/null > "${CASE_DIR}/repeater.stdout" 2> "${CASE_DIR}/repeater.stderr" &
            CHILD_PIDS[repeater]=$!
            started=${SECONDS}
            while [[ $((SECONDS - started)) -lt ${READY_SECONDS} ]]; do
                perl "${JUDGE}" owns "${CHILD_PIDS[repeater]}" "${repeater}" && break
                sleep 0.01
            done
            if ! perl "${JUDGE}" owns "${CHILD_PIDS[repeater]}" "${repeater}"; then
                aborted=1
            elif [[ ${traced} -eq 1 ]]; then
                timeout -s KILL 60 "${ENV_RUN[@]}" python3 "${ROOT}/tests/rewrite/helpers/runtime_thread_limit.py" --output "${CASE_DIR}" \
                    --inject "${failure}" -- "${PRODUCTS}/${executable}" "${script}" 2> "${CASE_DIR}/helper.stderr" || true
            else
                status=0
                if [[ "${phase}" == preflight ]]; then
                    timeout -s KILL "${PROCESS_SECONDS}" "${ENV_RUN[@]}" "${PRODUCTS}/${executable}" "${script}" \
                        < /dev/null > "${CASE_DIR}/ioc.stdout" 2> "${CASE_DIR}/ioc.stderr" &
                    ioc_pid=$!
                    perl "${JUDGE}" gate "${ioc_pid}" "${gate}" "${worker}" || aborted=1
                    wait "${ioc_pid}" || status=$?
                else
                    timeout -s KILL "${PROCESS_SECONDS}" "${ENV_RUN[@]}" "${PRODUCTS}/${executable}" "${script}" \
                        < /dev/null > "${CASE_DIR}/ioc.stdout" 2> "${CASE_DIR}/ioc.stderr" || status=$?
                fi
                printf "%s\n" "${status}" > "${CASE_DIR}/ioc.rc"
            fi
        else
            aborted=1
        fi
    fi
    [[ ${aborted} -eq 0 ]] || : > "${CASE_DIR}/aborted"
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
            *) die "usage: test_record_startup.bash --output NEW_DIRECTORY [--products DIRECTORY] [--sanitizers] [--case CASE|all]" ;;
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
    for case in snmp3Ioc snmp3StartupTest snmp3Worker snmp3NativeAgent snmp3NativeProbe; do
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
