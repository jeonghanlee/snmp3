#!/usr/bin/env bash
# Cell tsan: builds the module, the native owner, the worker and the test programs with the thread sanitizer
# (build_tsan.bash), runs the test programs, and hands the reports to tsan.pl, which classifies them by their frames.
# Six programs are started by their own runners with --products naming the thread sanitizer products; the Scheduler and
# Lifecycle test programs have no runner that takes a products directory and are started here. Each run has its own
# log directory, so a report is traced to the program that produced it.
# Two negative controls follow: a copy of the tree in which one lock is taken out is built and run with the cases
# active and rebuild of test_records.py, and the cell fails unless the thread sanitizer reports a race with a frame of
# the function that lost its lock. Predictions, written before the first run of the controls:
#   scheduler        Scheduler::admit without Scheduler.mutex: a record thread writes the queue and the counters of the
#                    address that the servicing thread reads under the lock, so a race with a frame of Scheduler::admit
#   record-requests  Requests::admit without Requests.mutex: a record thread writes the fields of a record context that
#                    the servicing thread reads under the lock, so a race with a frame of Requests::admit
# Prerequisite: the tree is built with the ordinary products first (make, then make -C tests/rewrite); build_tsan.bash
# stops with exit 2 and says so when it is not.
# usage: test_tsan.bash --output NEW_DIRECTORY [--tree DIRECTORY] [--products DIRECTORY] [--no-controls]
#   --tree         the tree whose sources and runners are used (default: the tree this file belongs to)
#   --products     thread sanitizer products that are already built (default: built into OUTPUT/build)
#   --no-controls  the two negative controls are not run
# exit status: 0 no report with a frame in the module and every run ended well, 1 otherwise, 2 the cell could not run

set -u

readonly HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly RUN_TIMEOUT=1800
# The cases of test_records.py that run threads against each other: a stop, a detach, a rebuild, active requests.
# The case wake-loop is not among them: it measures latencies with bounds of 2 to 40 ms, which the slowdown of the thread
# sanitizer breaks without any report.
readonly RECORD_CASES=(active rebuild stop-queued stop-inflight stop-enqueue-failed stop-downstream live-detach)

function die {
    printf "test_tsan: %s\n" "$1" >&2
    exit 2
}

# Prints a UDP port that was free on the loopback address when the command ran.
function free_port {
    perl -MIO::Socket::INET -e '
        my $s = IO::Socket::INET->new(LocalAddr => "127.0.0.1", LocalPort => 0, Proto => "udp") or die "no port\n";
        print $s->sockport, "\n";'
}

# Runs one program or runner under the thread sanitizer and records its name and exit status in runs.tsv of the
# directory in OUTPUT. Arguments: name, command...
function run_program {
    local name="$1" status=0
    shift
    mkdir -p "${OUTPUT}/logs/${name}" || die "cannot create the log directory of ${name}"
    TSAN_OPTIONS="halt_on_error=0:exitcode=0:history_size=4:log_exe_name=1:log_path=${OUTPUT}/logs/${name}/tsan" \
        timeout "${RUN_TIMEOUT}" "$@" > "${OUTPUT}/${name}.stdout" 2> "${OUTPUT}/${name}.stderr" || status=$?
    printf "%s\t%s\n" "${name}" "${status}" >> "${OUTPUT}/runs.tsv"
    printf "ran %s: exit %s\n" "${name}" "${status}"
}

# One negative control. Arguments: name, file, text to replace, replacement, text that a frame must show. The cell
# directory is in MAIN_OUTPUT; the result goes to controls.tsv: the name and 0 when the race was reported.
function run_control {
    local name="$1" file="$2" old="$3" new="$4" frame="$5" copy case status=0
    local directory="${MAIN_OUTPUT}/controls/${name}"
    mkdir -p "${directory}/tree" || die "cannot create ${directory}"
    copy="${directory}/tree"
    tar -C "${TREE}" --exclude=./.git --exclude=./work -cf - . | tar -C "${copy}" -xf - || die "cannot copy the tree"
    perl "${HERE}/tsan.pl" apply "${copy}/${file}" "${old}" "${new}" || die "the control ${name} does not apply"
    bash "${HERE}/build_tsan.bash" --output "${directory}/build" --tree "${copy}" > "${directory}/build.stdout" 2>&1 ||
        die "the build of the control ${name} failed"
    OUTPUT="${directory}"
    : > "${OUTPUT}/runs.tsv"
    for case in active rebuild; do
        run_program "records-${case}" python3 "${copy}/tests/rewrite/test_records.py" --case "${case}" \
            --products "${directory}/build/products" --output "${directory}/o-records-${case}"
    done
    perl "${HERE}/tsan.pl" frames "${directory}" "${frame}" || status=1
    OUTPUT="${MAIN_OUTPUT}"
    printf "%s\t%s\n" "${name}" "${status}" >> "${MAIN_OUTPUT}/controls.tsv"
}

function main {
    local tree="${HERE}/../.." output="" products="" case runners server repeater controls=yes
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --output) output="${2:-}"; shift 2 ;;
            --tree) tree="${2:-}"; shift 2 ;;
            --products) products="${2:-}"; shift 2 ;;
            --no-controls) controls=no; shift ;;
            *) die "usage: test_tsan.bash --output NEW_DIRECTORY [--tree DIRECTORY] [--products DIRECTORY] [--no-controls]" ;;
        esac
    done
    [[ -n "${output}" ]] || die "--output is required"
    tree="$(cd "${tree}" && pwd)" || die "no such tree"
    mkdir -m 700 "${output}" || die "cannot create ${output}"
    OUTPUT="$(cd "${output}" && pwd)"
    MAIN_OUTPUT="${OUTPUT}"
    TREE="${tree}"
    mkdir "${OUTPUT}/logs" || die "cannot create the log directory"
    : > "${OUTPUT}/runs.tsv"
    : > "${OUTPUT}/controls.tsv"
    if [[ -z "${products}" ]]; then
        bash "${HERE}/build_tsan.bash" --output "${OUTPUT}/build" --tree "${tree}" > "${OUTPUT}/build.stdout" 2>&1 ||
            { cat "${OUTPUT}/build.stdout" >&2; die "the thread sanitizer build failed"; }
        products="${OUTPUT}/build/products"
    fi
    [[ -x "${products}/snmp3Ioc" ]] || die "products not found in ${products}"
    runners="${tree}/tests/rewrite"
    run_program scheduler "${products}/snmp3SchedulerTest"
    server="$(free_port)" || die "no free port"
    repeater="$(free_port)" || die "no free port"
    EPICS_CA_SERVER_PORT="${server}" EPICS_CA_REPEATER_PORT="${repeater}" EPICS_CA_AUTO_ADDR_LIST=NO \
        EPICS_CA_ADDR_LIST=127.0.0.1 EPICS_CAS_INTF_ADDR_LIST=127.0.0.1 \
        run_program lifecycle "${products}/snmp3LifecycleTest" module
    run_program supervisor python3 "${runners}/test_supervisor.py" --ioc --products "${products}" \
        --output "${OUTPUT}/o-supervisor"
    run_program qualification python3 "${runners}/test_qualification.py" --products "${products}" \
        --output "${OUTPUT}/o-qualification"
    run_program record-shutdown python3 "${runners}/test_record_shutdown.py" --products "${products}" \
        --output "${OUTPUT}/o-record-shutdown"
    run_program record-startup python3 "${runners}/test_record_startup.py" --products "${products}" \
        --output "${OUTPUT}/o-record-startup"
    for case in "${RECORD_CASES[@]}"; do
        run_program "records-${case}" python3 "${runners}/test_records.py" --case "${case}" \
            --products "${products}" --output "${OUTPUT}/o-records-${case}"
    done
    if [[ "${controls}" == "yes" ]]; then
        run_control scheduler snmp3App/src/Scheduler.cpp \
            "std::lock_guard<std::mutex> guard(mutex); require(accepting);" "require(accepting);" "Scheduler::admit"
        run_control record-requests snmp3App/src/Request.cpp \
            $'    std::lock_guard<std::mutex> guard(mutex);\n    require(producers && entryOpen && context.owner==activation && !context.active &&' \
            $'    require(producers && entryOpen && context.owner==activation && !context.active &&' "Requests::admit"
    fi
    perl "${HERE}/tsan.pl" judge "${OUTPUT}" "${HERE}/tsan.supp"
    exit $?
}

main "$@"
