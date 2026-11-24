#!/usr/bin/env bash
# Spawn descriptor cell: checks that the address worker inherits no descriptor beyond its standard streams and its IPC
# channel. The shipped IOC starts with four extra descriptors that are inheritable (the numbers 3 to 6, the ends of anonymous
# pipes). The cell reads /proc of the IOC and of its worker through the real spawn path and requires that the four pipes are
# open in the IOC and absent in the worker, that descriptors 0 and 1 of the worker are /dev/null, that descriptor 3 is the IPC
# socket and that the worker holds no other pipe. The standard input of the IOC is an anonymous pipe as well, so a worker
# that kept it would hold a pipe. spawn_descriptors.pl reads the observations and judges.
# usage: test_spawn_descriptors.bash --output NEW_DIRECTORY
# exit status: 0 every check passed, 1 a check failed, 2 the cell could not be judged (no results.json is written)

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly HERE
ROOT="$(cd "${HERE}/../.." && pwd)"
readonly ROOT
readonly JUDGE="${HERE}/spawn_descriptors.pl"
readonly ARCH="linux-x86_64"
readonly WAIT_SECONDS=60
readonly STOP_SECONDS=30

IOC_PID=
IOC_SUBSHELL=0

function die {
    printf "test_spawn_descriptors: %s\n" "$1" >&2
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

# The standard input of the IOC: waits for the end of the start-up, asks for the runtime report once, waits for the
# release and ends the shell of the IOC.
function feed {
    local started
    started=${SECONDS}
    until grep -q "AfterInitialProcess" "${OUTPUT}/ioc.stdout" 2> /dev/null || [[ $((SECONDS - started)) -ge ${WAIT_SECONDS} ]]; do
        sleep 0.1
    done
    printf "snmp3RuntimeReport\n"
    started=${SECONDS}
    until [[ -e "${OUTPUT}/release" ]] || [[ $((SECONDS - started)) -ge $((WAIT_SECONDS + STOP_SECONDS)) ]]; do
        sleep 0.1
    done
    printf "exit\n"
}

# Releases the shell of the IOC, waits for it to end, ends it when it does not, and closes the four marker descriptors.
function stop_ioc {
    local started
    : > "${OUTPUT}/release"
    started=${SECONDS}
    while kill -0 "${IOC_SUBSHELL}" 2> /dev/null && [[ $((SECONDS - started)) -lt ${STOP_SECONDS} ]]; do
        sleep 0.1
    done
    if kill -0 "${IOC_SUBSHELL}" 2> /dev/null; then
        [[ -z "${IOC_PID}" ]] || kill -KILL "${IOC_PID}" 2> /dev/null
        printf "killed\n" > "${OUTPUT}/ioc.returncode"
    fi
    wait "${IOC_SUBSHELL}" 2> /dev/null
    exec 3<&- 4>&- 5<&- 6>&-
}

# Prints the descriptors of a process, one line each: the number and the target of its link.
function descriptors {
    local link
    for link in /proc/"$1"/fd/*; do
        [[ -e "${link}" || -L "${link}" ]] || continue
        printf "%s\t%s\n" "${link##*/}" "$(readlink "${link}")"
    done
}

function main {
    local output="" base worker="" started status=0 tries
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            *) die "usage: test_spawn_descriptors.bash --output NEW_DIRECTORY" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd)"
    base="$(base_directory)" || die "configure/RELEASE.local names no EPICS_BASE"
    mkdir -m 700 "${OUTPUT}/config" || die "cannot create the configuration directory"
    cp "${ROOT}/tests/rewrite/config/v2c.json" "${OUTPUT}/config/v2c.json" || die "cannot copy the configuration"
    (umask 077 && printf "public\n" > "${OUTPUT}/config/community.txt") || die "cannot write the community file"
    : > "${OUTPUT}/ioc.stdout"
    : > "${OUTPUT}/ioc.stderr"
    # Four inheritable descriptors, the ends of anonymous pipes, with the numbers 3 to 6: the shipped IOC must not pass them on.
    exec 3< <(:) 4> >(cat > /dev/null) 5< <(:) 6> >(cat > /dev/null)
    : > "${OUTPUT}/markers.txt"
    for tries in 3 4 5 6; do
        printf "%s\n" "$(readlink "/proc/$$/fd/${tries}")" >> "${OUTPUT}/markers.txt"
    done
    (
        cd "${ROOT}" || exit 2
        feed | env -u LD_PRELOAD -u LD_AUDIT -u LD_DEBUG -u LD_LIBRARY_PATH \
            LD_LIBRARY_PATH="${ROOT}/lib/${ARCH}:${base}/lib/${ARCH}" CONFIG_JSON="${OUTPUT}/config/v2c.json" \
            NATIVE_PROBE="${ROOT}/bin/${ARCH}/snmp3NativeProbe" WORKER_PATH="${ROOT}/bin/${ARCH}/snmp3Worker" \
            "${ROOT}/bin/${ARCH}/snmp3Ioc" "${ROOT}/tests/rewrite/config/startup.cmd" \
            > "${OUTPUT}/ioc.stdout" 2> "${OUTPUT}/ioc.stderr"
        printf "%s\n" "$?" > "${OUTPUT}/ioc.returncode"
    ) &
    IOC_SUBSHELL=$!
    # The IOC is the child of the subshell: wait until its process appears, then for the worker line of the report.
    started=${SECONDS}
    while [[ $((SECONDS - started)) -lt ${WAIT_SECONDS} ]] && kill -0 "${IOC_SUBSHELL}" 2> /dev/null; do
        worker="$(grep -oE 'snmp3 worker: address=[0-9]+ epoch=[0-9]+ pid=[0-9]+ ready=1' "${OUTPUT}/ioc.stdout" | tail -1 |
            sed -E 's/.*pid=([0-9]+).*/\1/')"
        [[ -z "${worker}" ]] || break
        sleep 0.1
    done
    printf "%s\n" "${worker:-0}" > "${OUTPUT}/worker.pid"
    IOC_PID="$(pgrep -P "${IOC_SUBSHELL}" -x snmp3Ioc | head -1)"
    if [[ -n "${worker}" ]]; then
        # A worker that is ready but an IOC or a descriptor list that cannot be found is a fault of the cell, not of the module.
        if [[ -z "${IOC_PID}" ]]; then
            stop_ioc
            die "the process of the IOC was not found"
        fi
        descriptors "${IOC_PID}" > "${OUTPUT}/ioc-fds.txt"
        descriptors "${worker}" > "${OUTPUT}/worker-fds.txt"
        if [[ ! -s "${OUTPUT}/ioc-fds.txt" || ! -s "${OUTPUT}/worker-fds.txt" ]]; then
            stop_ioc
            die "the descriptors of the IOC or of the worker could not be read"
        fi
    else
        : > "${OUTPUT}/ioc-fds.txt"
        : > "${OUTPUT}/worker-fds.txt"
    fi
    stop_ioc
    perl "${JUDGE}" "${OUTPUT}" || status=$?
    exit "${status}"
}

main "$@"
