#!/usr/bin/env bash
# Operator cell: runs the documented operator commands through the actual production IOC. The cell starts two fixture agents (IPv4
# and IPv6) with secret files of their own, writes a configuration that names them, runs the production IOC on a startup file with
# snmp3QueueLimit, iocInit, the three reports, snmp3Stop and the runtime report again, and ends the agents. r5_operator.pl writes
# the configuration, reads what the run produced and judges: the two agents (ready, only one loopback listener, normal stop), the
# exit and the lines of the reports of the IOC, the libraries that it loaded (none of the native library of the system), the
# absence of the generated secrets from every record and of sanitizer diagnostics.
# usage: test_r5_operator.bash --output NEW_DIRECTORY
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd -P)"
readonly ROOT
readonly JUDGE="${HERE}/r5_operator.pl"
readonly ARCH="linux-x86_64"
readonly PRODUCTS="${ROOT}/bin/${ARCH}"
readonly AGENT_USER="fixtureUser"
readonly READY_SECONDS=5
readonly STOP_SECONDS=8
readonly IOC_SECONDS=10

OUTPUT=""
AGENT_COUNT=0
declare -a AGENT_NAMES=()
declare -a AGENT_PIDS=()
declare -a ENV_RUN=()

function die {
    printf "test_r5_operator: %s\n" "$1" >&2
    stop_agents
    exit 2
}

# Writes a secret file of 48 hexadecimal characters that only the owner can read.
function new_secret {
    local path="$1"
    (umask 077 && od -An -N24 -tx1 /dev/urandom | tr -d ' \n' > "${path}" && printf "\n" >> "${path}") ||
        die "cannot write a secret file"
}

# Starts a fixture agent for the address family and waits until it says that it is ready. Sets AGENT_READY to 1 or 0.
function start_agent {
    local family="$1" name port community auth privacy started
    AGENT_COUNT=$((AGENT_COUNT + 1))
    name="agent-ipv${family}-${AGENT_COUNT}"
    port="$(perl "${JUDGE}" port "${family}")" || die "no free UDP port"
    new_secret "${OUTPUT}/${name}-community"
    new_secret "${OUTPUT}/${name}-auth"
    new_secret "${OUTPUT}/${name}-privacy"
    read -r community < "${OUTPUT}/${name}-community"
    read -r auth < "${OUTPUT}/${name}-auth"
    read -r privacy < "${OUTPUT}/${name}-privacy"
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
        "group nativeGroup usm independentFixtureUser" > "${OUTPUT}/${name}.conf") || die "cannot write the agent configuration"
    "${ENV_RUN[@]}" "${PRODUCTS}/snmp3NativeAgent" "${port}" "${family}" "${OUTPUT}/${name}.conf" \
        > "${OUTPUT}/${name}.stdout" 2> "${OUTPUT}/${name}.stderr" &
    AGENT_NAMES+=("${name}")
    AGENT_PIDS+=("$!")
    printf "%s\t%s\t%s\t%s\n" "${name}" "${family}" "${port}" "$!" >> "${OUTPUT}/agents.tsv"
    AGENT_READY=0
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${READY_SECONDS} ]] && kill -0 "$!" 2> /dev/null; do
        if grep -q '"event":"agent_ready"' "${OUTPUT}/${name}.stdout" 2> /dev/null; then
            AGENT_READY=1
            break
        fi
        sleep 0.01
    done
    printf "%s\n" "${AGENT_READY}" > "${OUTPUT}/${name}.ready"
    if [[ ${AGENT_READY} -eq 1 ]]; then
        perl "${JUDGE}" sockets "${AGENT_PIDS[-1]}" "${port}" "${family}" "${OUTPUT}/${name}.sockets.json" ||
            die "cannot read the sockets of ${name}"
    fi
}

# Ends the agents in the reverse order of their start with SIGTERM and keeps the exit status of each; one that does not end is killed.
function stop_agents {
    local index pid name started status forced alive
    for ((index = ${#AGENT_PIDS[@]} - 1; index >= 0; index--)); do
        pid="${AGENT_PIDS[index]}"
        name="${AGENT_NAMES[index]}"
        [[ -n "${pid}" ]] || continue
        status=0
        forced=0
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
        printf "%s\t%s\n" "${status}" "${forced}" > "${OUTPUT}/${name}.stop"
        AGENT_PIDS[index]=""
    done
}

function main {
    local output="" status=0 script
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            *) die "usage: test_r5_operator.bash --output NEW_DIRECTORY" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    [[ ! -e "${output}" ]] || die "--output must be a new directory"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd -P)"
    for script in snmp3Ioc snmp3Worker snmp3NativeProbe snmp3NativeAgent; do
        [[ -x "${PRODUCTS}/${script}" ]] || die "product ${script} not found in ${PRODUCTS}"
    done
    ENV_RUN=(env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG_OUTPUT -u LD_LIBRARY_PATH MIBS= LD_DEBUG=libs)
    : > "${OUTPUT}/agents.tsv"
    cd "${ROOT}" || die "cannot enter the checkout"
    start_agent 4
    if [[ ${AGENT_READY} -eq 1 ]]; then
        start_agent 6
    fi
    if [[ ${AGENT_READY} -eq 1 ]]; then
        perl "${JUDGE}" config "${OUTPUT}" || die "cannot write the configuration"
        printf "%s\n" \
            "on error break" \
            "dbLoadDatabase(\"${ROOT}/dbd/snmp3Ioc.dbd\")" \
            "snmp3Ioc_registerRecordDeviceDriver(pdbbase)" \
            "snmp3WorkerPath(\"${PRODUCTS}/snmp3Worker\")" \
            "snmp3Load(\"${OUTPUT}/configuration.json\", \"${PRODUCTS}/snmp3NativeProbe\")" \
            "snmp3QueueLimit(\"127.0.0.1\", 1024, 1048576)" \
            "iocInit" "snmp3Report" "snmp3ConfigReport" "snmp3RuntimeReport" "snmp3Stop" "snmp3RuntimeReport" \
            > "${OUTPUT}/operator.cmd" || die "cannot write the startup file"
        status=0
        "${ENV_RUN[@]}" timeout -s KILL "${IOC_SECONDS}" "${PRODUCTS}/snmp3Ioc" "${OUTPUT}/operator.cmd" \
            < /dev/null > "${OUTPUT}/operator.stdout" 2> "${OUTPUT}/operator.stderr" || status=$?
        printf "%s\n" "${status}" > "${OUTPUT}/operator.rc"
    else
        : > "${OUTPUT}/aborted"
    fi
    stop_agents
    status=0
    perl "${JUDGE}" judge "${OUTPUT}" || status=$?
    [[ ${status} -le 1 ]] || die "the judge could not read the observations"
    exit "${status}"
}

main "$@"
