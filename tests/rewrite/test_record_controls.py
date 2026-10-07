#!/usr/bin/env python3
"""Build defective support copies and require shipped record tests to detect them."""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from test_native import ROOT, digest, write_json


CONTROLS = {
    "communication-alarm": ("DeviceSupport.cpp", "alarms",
                            [("context.alarm=result.outcome==ipc::Outcome::NativeFailure && !transport ?",
                              "context.alarm=result.outcome==ipc::Outcome::NativeFailure ?")],
                            "native timeout communication alarm mismatch: Records_TimeoutAi"),
    "precision": ("Conversion.cpp", "edges",
                  [("require(exactInteger(number, digits));", "(void)digits;")],
                  "record alarm outcome mismatch: Records_WideDouble"),
    "capacity": ("Conversion.cpp", "edges",
                 [("require(bytes.size() <= dataCapacity);", "(void)dataCapacity;"),
                  ("bytes.data()), bytes.size());",
                   "bytes.data()), bytes.size() > dataCapacity ? dataCapacity : bytes.size());")],
                 "record alarm outcome mismatch: Records_Text"),
    "ties": ("Conversion.cpp", "baseline",
             [("(quotient & 1)", "((quotient + 1) & 1)")],
             "ao binary32 wire tie did not select even neighbor"),
    "ambient-rounding": ("Conversion.cpp", "edges",
                         [("const float rounded = roundBinary32(number);", "const float rounded = static_cast<float>(number);")],
                         "actual binary32 SET/GET disagreed with specified IEEE bits"),
    "callback-retry": ("Request.cpp", "baseline",
                       [("if(callbackRequest(&context.callback)) {\n            context.callbackState=CallbackState::Pending;",
                         "if(callbackRequest(&context.callback)) {\n            context.callbackState=CallbackState::Inert;")],
                       "full callback queue lost or prematurely consumed terminal"),
    "terminal-release": ("Request.cpp", "baseline",
                         [("if(context.terminal.result)context.owner->release(context.terminal.id);",
                           "(void)context.terminal.result;")],
                         "native retirement did not settle: Records_Ai"),
}


# Admission-behind-retirement controls: each alters Scheduler.cpp and must fail a named real-path cell
# that passes on the unmodified products. Kinds: "component" runs one SchedulerTest cell, "qualification"
# one qualification case, "record" one record runner case; "check" names the runner check that must fail.
EXPIRE_QUEUED = "auto id=*it; auto& g=*a.generations.at(id);"
STOP_QUEUED = "for(auto id:a.queue) { auto& g=*a.generations.at(id); select(g,ipc::Outcome::Stopping);"
BINDING_LOOKUP = [(EXPIRE_QUEUED, "auto id=*it; auto& g=*a.generations.lower_bound(Key(id.first,0))->second;"),
                  (STOP_QUEUED, "for(auto id:a.queue) { auto& g=*a.generations.lower_bound(Key(id.first,0))->second; select(g,ipc::Outcome::Stopping);")]
# Controls that alter a source file other than Scheduler.cpp.
D7_SOURCES = {"report-never-sent-miscounted": "Runtime.cpp", "rebuild-reuses-scheduler": "Runtime.cpp",
              "never-sent-message-always": "DeviceSupport.cpp", "never-sent-message-absent": "DeviceSupport.cpp",
              "never-sent-message-any-outcome": "DeviceSupport.cpp", "never-sent-message-not-reset": "Request.cpp",
              "waveform-busy-held": "DeviceSupport.cpp", "drain-without-retry": "Request.cpp", "detach-always-allowed": "Request.cpp",
              "detach-allowed-when-idle": "Request.cpp", "stop-permits-detach": "Runtime.cpp",
              "detach-allowed-while-pending": "Request.cpp", "refused-detach-raises-alarm": "DeviceSupport.cpp",
              "refused-detach-perturbs-active-handle": "Request.cpp", "entry-reported-open-after-stop": "Request.cpp", "detach-reported-allowed-after-stop": "Request.cpp", "drain-always-failed": "Request.cpp", "refused-detach-toggles-active-handle": "Request.cpp", "refused-detach-toggles-idle-handle": "Request.cpp", "refused-detach-perturbs-stopped-handle": "Request.cpp", "refused-detach-perturbs-pending-handle": "Request.cpp", "completion-counted-twice": "Request.cpp",
              "retried-completion-leaves-record-active": "Request.cpp", "refusal-releases-record": "Request.cpp",
              "refusal-releases-record-with-alarm": "Request.cpp"}
D7_CONTROLS = {
    "per-handle-bound": ("component", "admission-behind-retirement", None,
                         [("require(existing<=1 && consumed); behind.push_back(existing==1);",
                           "(void)consumed; behind.push_back(existing==1);")]),
    "early-release": ("component", "admission-behind-retirement", None,
                      [("if(g.consumed && g.retired) {", "if(g.consumed) {")]),
    "uncharged-successor": ("component", "admission-behind-retirement", None,
                            [("a.queue.swap(queue); a.count+=ids.size(); a.bytes+=total;",
                              "a.queue.swap(queue); if(!behind[0]) { a.count+=ids.size(); a.bytes+=total; }")]),
    "binding-lookup-component": ("component", "two-generation-lifecycle", None, BINDING_LOOKUP),
    "binding-lookup-qualification": ("qualification", "behind-retirement", None, BINDING_LOOKUP),
    "binding-lookup-record": ("record", "stop-queued", "queued-successor-completes-stopping-once", BINDING_LOOKUP),
    "queued-deadline-restart": ("record", "deadline-queue", "below-threshold-queued-generation-not-sent",
                                [("if(g.command.deadline>nowUs) { ++it; continue; }",
                                  "if(g.command.deadline>nowUs || g.behind) { ++it; continue; }"),
                                 ("if(initial.command.deadline<=nowUs)return d;",
                                  "if(initial.command.deadline<=nowUs && !initial.behind)return d;"),
                                 ("if(g.command.deadline<=nowUs || !compatible(first.first,id.first,initial,g)",
                                  "if((g.command.deadline<=nowUs && !g.behind) || !compatible(first.first,id.first,initial,g)"),
                                 ("for(auto id:a.active) { a.generations.at(id)->active=true;",
                                  "for(auto id:a.active) { auto& moved=*a.generations.at(id); if(moved.behind && moved.command.deadline<=nowUs)moved.command.deadline=ipc::add(nowUs,1000000); moved.active=true;")]),
    "stop-one-generation": ("record", "stop-queued", "queued-successor-completes-stopping-once", [BINDING_LOOKUP[1]]),
    "grace-native-failure": ("component", "grace-outcomes", None,
                             [("const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure);",
                               "const bool answered=g.selected && g.result.outcome==ipc::Outcome::Complete;")]),
    "grace-all-outcomes": ("component", "grace-outcomes", None,
                           [("const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure);",
                             "const bool answered=g.selected;")]),
    "grace-channel-failure": ("component", "grace-exclusion", None,
                              [("const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure);",
                                "const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure || g.result.outcome==ipc::Outcome::ChannelFailure);")]),
    "grace-worker-failure": ("component", "grace-exclusion", None,
                             [("const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure);",
                               "const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure || g.result.outcome==ipc::Outcome::WorkerFailure);")]),
    "grace-stopping": ("component", "grace-exclusion", None,
                       [("const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure);",
                         "const bool answered=g.selected && (g.result.outcome==ipc::Outcome::Complete || g.result.outcome==ipc::Outcome::NativeFailure || g.result.outcome==ipc::Outcome::Stopping);")]),
    "never-sent-overcount": ("component", "behind-classification", None,
                             [("if(!g.selected && g.behind)++a.behindNeverSent;", "if(!g.selected)++a.behindNeverSent;")]),
    "behind-flag-always": ("component", "behind-classification", None,
                           [("g->behind=behind[index];", "g->behind=true;")]),
    "take-without-identity": ("component", "take-identity", None,
                              [("if(it==a.generations.end() || !(it->second->command.id==terminal) || !it->second->selected ||",
                                "if(it==a.generations.end() || !it->second->selected ||")]),
    "never-sent-message-always": ("record", "deadline-queue", "sent-deadline-without-never-sent-message",
                                 [('if(result.outcome==ipc::Outcome::Deadline && !context.terminal.sent)context.message="deadline before send";',
                                   'if(result.outcome==ipc::Outcome::Deadline)context.message="deadline before send";')]),
    "never-sent-message-any-outcome": ("record", "stop-queued", "stopping-successor-has-no-never-sent-message",
                                      [('if(result.outcome==ipc::Outcome::Deadline && !context.terminal.sent)context.message="deadline before send";',
                                        'if(!context.terminal.sent)context.message="deadline before send";')]),
    "never-sent-message-not-reset": ("record", "deadline-queue", "followup-after-never-sent-has-no-stale-message",
                                    [("context.alarm=0; context.message=nullptr;", "context.alarm=0;")]),
    "report-never-sent-miscounted": ("record", "deadline-queue", "queue-report-counters",
                                    [("(unsigned long long)queue.behindNeverSent);", "(unsigned long long)queue.behindAdmitted);")]),
    "queued-deadline-extended": ("record", "deadline-queue", "put-to-dispatch-within-late-application-bound",
                                 [("g->command.deadline=deadline;",
                                   "g->command.deadline=behind[index]?ipc::add(deadline,5000000):deadline;")]),
    "rebuild-reuses-scheduler": ("record", "rebuild", "rebuild-second-activation-new-worker",
                                 [("if(scheduler && scheduler->activationId()==activation && scheduler->configurationRevision()==config.revision)return;",
                                   "if(scheduler && scheduler->configurationRevision()==config.revision)return;")]),
    "never-sent-message-absent": ("record", "deadline-queue", "below-threshold-never-sent-message",
                                  [('if(result.outcome==ipc::Outcome::Deadline && !context.terminal.sent)context.message="deadline before send";', "")]),
    "stop-queued-not-selected": ("record", "stop-inflight", "stop-inflight-every-record-completes-with-alarm",
                                 [("for(auto id:a.queue) { auto& g=*a.generations.at(id); select(g,ipc::Outcome::Stopping);",
                                   "for(auto id:a.queue) { auto& g=*a.generations.at(id);")]),
    "waveform-busy-held": ("record", "stop-inflight", "stop-inflight-waveform-busy-clear",
                           [("if(definition.kind==RecordKind::Waveform)as<waveformRecord>(context.record).busy=FALSE;",
                             "if(definition.kind==RecordKind::Waveform)as<waveformRecord>(context.record).busy=TRUE;")]),
    "drain-without-retry": ("record", "stop-enqueue-failed", "release-after-stop-bound-completes-every-record-once",
                            [("    do {\n        service();\n", "    do {\n")]),
    "detach-always-allowed": ("record", "live-detach", "live-replacement-refused-while-request-in-flight",
                              [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)",
                                "    if(context.record && context.record->dpvt==&context)")]),
    "detach-allowed-when-idle": ("record", "live-detach", "live-replacement-refused-idle-and-after-operator-stop",
                                 [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)",
                                   "    if(!detachAllowed && context.active)return false;\n    if(context.record && context.record->dpvt==&context)")]),
    "detach-allowed-while-pending": ("record", "live-detach", "live-replacement-refused-during-record-drain",
                                     [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)",
                                       "    if(!detachAllowed && context.callbackState!=CallbackState::Pending)return false;\n    if(context.record && context.record->dpvt==&context)")]),
    "refused-detach-raises-alarm": ("record", "live-detach", "in-flight-request-completes-once-after-refusal",
                                    [("    return Requests::instance().detach(*static_cast<RecordContext*>(record->dpvt))?0:S_dev_badInpType;",
                                      "    const bool allowed=Requests::instance().detach(*static_cast<RecordContext*>(record->dpvt));\n    if(!allowed)recGblSetSevr(record,LINK_ALARM,INVALID_ALARM);\n    return allowed?0:S_dev_badInpType;")]),
    "refused-detach-perturbs-active-handle": ("record", "live-detach", "live-replacement-refused-while-request-in-flight",
                                              [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)",
                                                "    if(!detachAllowed){ if(context.active)++context.handle; return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "entry-reported-open-after-stop": ("record", "live-detach", "live-replacement-refused-idle-and-after-operator-stop",
                                       [("result.completions=completions; result.entryOpen=entryOpen;",
                                         "result.completions=completions; result.entryOpen=true;")]),
    "detach-reported-allowed-after-stop": ("record", "live-detach", "live-replacement-refused-idle-and-after-operator-stop",
                                           [("result.drainFailed=drainFailed; result.detachAllowed=detachAllowed;",
                                             "result.drainFailed=drainFailed; result.detachAllowed=!entryOpen;")]),
    "drain-always-failed": ("record", "live-detach", "live-replacement-refused-during-record-drain",
                            [("    if(!drained)drainFailed=true;", "    drainFailed=true;")]),
    "refused-detach-toggles-active-handle": ("record", "live-detach", "live-replacement-refused-during-record-drain",
                                             [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)", "    if(!detachAllowed){ if(context.active)context.handle^=1; return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "refused-detach-toggles-idle-handle": ("record", "live-detach", "live-replacement-refused-idle-and-after-operator-stop",
                                           [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)", "    if(!detachAllowed){ if(!context.active)context.handle^=1; return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "refused-detach-perturbs-stopped-handle": ("record", "live-detach", "live-replacement-refused-idle-and-after-operator-stop",
                                               [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)", "    if(!detachAllowed){ if(!context.active && !entryOpen)++context.handle; return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "refused-detach-perturbs-pending-handle": ("record", "live-detach", "live-replacement-refused-during-record-drain",
                                               [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)", "    if(!detachAllowed){ if(context.callbackState==CallbackState::Pending)++context.handle; return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "completion-counted-twice": ("record", "live-detach", "in-flight-request-completes-once-after-refusal",
                                 [("    ++completions; changed.notify_all();", "    completions+=2; changed.notify_all();")]),
    "retried-completion-leaves-record-active": ("record", "live-detach", "live-replacement-refused-during-record-drain",
                                                [("            record->rset->process(record);\n",
                                                  "            record->rset->process(record);\n            if(self.enqueueFailures)record->pact=TRUE;\n")]),
    "refusal-releases-record": ("record", "live-detach", "live-replacement-refused-while-request-in-flight",
                                [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)", "    if(!detachAllowed){ if(context.active && context.record)context.record->pact=FALSE; return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "refusal-releases-record-with-alarm": ("record", "live-detach", "live-replacement-refused-during-record-drain",
                                           [("    if(!detachAllowed)return false;\n    if(context.record && context.record->dpvt==&context)", "    if(!detachAllowed){ if(context.active && context.record){ context.record->pact=FALSE; context.record->stat=COMM_ALARM; context.record->sevr=INVALID_ALARM; } return false; }\n    if(context.record && context.record->dpvt==&context)")]),
    "stop-permits-detach": ("record", "live-detach", "live-replacement-refused-idle-and-after-operator-stop",
                            [("    epicsThreadMustJoin(target);\n    const bool drained=Requests::instance().drain();\n",
                              "    epicsThreadMustJoin(target);\n    const bool drained=Requests::instance().drain();\n    Requests::instance().permitDetach();\n")]),
    "storage-validation": ("component", "forged-retirement", None,
                           [("for(size_t i=0;i<ids.size();++i)if(!(a.generations.at(a.active[i])->command.id==ids[i]))return false;",
                             "for(size_t i=0;i<ids.size();++i)if(!a.generations.count(key(ids[i])))return false;")]),
}


# Each repeat-detach control targets one field and one observation, including the second refusal.
# Context-record faults also remove the cleanup assertion that would otherwise abort before testdbCleanup;
# they leave the real queue/context destruction intact and never repair the observed pointer.
EMPTY_DETACH = "    if(!record->dpvt)return Requests::instance().attachmentAllowed()?0:S_dev_badInpType;"
FIRST_DETACH = "    return Requests::instance().detach(*static_cast<RecordContext*>(record->dpvt))?0:S_dev_badInpType;"
CONTEXT_CLEANUP_GUARD = [("        require(!context->record);", "        (void)context->record;")]
REPEAT_EXTRA_EDITS = {}
REPEAT_CONTROLS = {}
for label, record_name in (("ai", "Records_TimeoutAi"), ("ao", "Records_TimeoutAo")):
    target = f'std::strcmp(record->name,"{record_name}")==0'
    first_faults = {
        "link": ("DeviceSupport.cpp", "dbGetDevLink(record)->value.instio.string[0]='X';"),
        "link-type": ("DeviceSupport.cpp", "dbGetDevLink(record)->type=CONSTANT;"),
        "dset": ("DeviceSupport.cpp", "record->dset=nullptr;"),
        "dpvt-null": ("Request.cpp", [("if(context.record && context.record->dpvt==&context)context.record->dpvt=nullptr;",
                     f'if(context.record && std::strcmp(context.record->name,"{record_name}")!=0 && '
                     'context.record->dpvt==&context)context.record->dpvt=nullptr;')]),
        "context-record-null": ("Request.cpp", [("    context.record=nullptr;",
                     f'    if(!context.record || std::strcmp(context.record->name,"{record_name}")!=0)context.record=nullptr;')]
                     + CONTEXT_CLEANUP_GUARD),
    }
    for term, (source, fault) in first_faults.items():
        name = f"repeat-first-{label}-{term}"
        if source == "DeviceSupport.cpp":
            edits = [(FIRST_DETACH, "    const bool detached=Requests::instance().detach(*static_cast<RecordContext*>(record->dpvt));\n"
                      f"    if(detached && {target}) {{ {fault} }}\n    return detached?0:S_dev_badInpType;")]
        else:
            edits = [("#include <algorithm>", "#include <algorithm>\n#include <cstring>")] + fault
        REPEAT_CONTROLS[name] = (source, f"repeat-detach-first-{label}-{term}", edits)
    for number in (1, 2):
        for term, fault in {
            "refused": "return -1;",
            "link": "dbGetDevLink(record)->value.instio.string[0]='X';",
            "link-type": "dbGetDevLink(record)->type=CONSTANT;",
            "dset": "record->dset=nullptr;",
            "dpvt-null": "record->dpvt=record;",
            "context-record-null": "saved->record=record;",
            "contexts-retained": "Requests::instance().queuesDestroyed();",
        }.items():
            name = f"repeat-{label}-{number}-{term}"
            save = (f"    static RecordContext* saved=nullptr;\n    if(record->dpvt && {target})"
                    "saved=static_cast<RecordContext*>(record->dpvt);\n") if term == "context-record-null" else ""
            replacement = (save + "    if(!record->dpvt) {\n"
                           f"        if({target}) {{ static unsigned calls=0; if(++calls=={number}) {{ {fault} }} }}\n"
                           "        return Requests::instance().attachmentAllowed()?0:S_dev_badInpType;\n    }")
            REPEAT_CONTROLS[name] = ("DeviceSupport.cpp", f"repeat-detach-{label}-{number}-{term}",
                                     [(EMPTY_DETACH, replacement)])
            if term == "context-record-null":
                REPEAT_EXTRA_EDITS[name] = {"Request.cpp": CONTEXT_CLEANUP_GUARD}
    # A successful del_record can still produce the usual error from add_record while changing link and dset.
    REPEAT_CONTROLS[f"repeat-{label}-empty-accepted"] = ("DeviceSupport.cpp", f"repeat-detach-{label}-1-link",
        [(EMPTY_DETACH, f"    if(!record->dpvt && {target})return 0;\n" + EMPTY_DETACH)])

REPEAT_CONTROLS.update({
    "repeat-release-after-close": ("Register.cpp", "repeat-detach-first-ai-contexts-retained",
        [("    case initHookAfterStopScan:", "    case initHookAfterCloseLinks: snmp3::Requests::instance().queuesDestroyed(); return;\n    case initHookAfterStopScan:")]),
    "repeat-release-after-stop-callback": ("Register.cpp", "repeat-detach-contexts-retained-before-free",
        [('case initHookAfterStopCallback: name = "AfterStopCallback"; break;',
          'case initHookAfterStopCallback: snmp3::Requests::instance().queuesDestroyed(); name = "AfterStopCallback"; break;')]),
    "repeat-release-before-free": ("Register.cpp", "repeat-detach-contexts-retained-before-free",
        [("case initHookBeforeFree: isolatedCleanup=true; return;",
          "case initHookBeforeFree: snmp3::Requests::instance().queuesDestroyed(); isolatedCleanup=true; return;")]),
    "repeat-cleanup-retains-contexts": ("Request.cpp", "repeat-detach-cleanup-contexts-zero",
        [("    contexts.clear(); cursor=contexts.end(); activation.reset();", "    cursor=contexts.end(); activation.reset();")]),
    "repeat-cleanup-not-stopped": ("Runtime.cpp", "repeat-detach-cleanup-stopped",
        [("current.state = drained && (!supervisor || supervisor->reconcile()) ? State::Stopped:State::IncompleteStopped;",
          "current.state = drained && (!supervisor || supervisor->reconcile()) ? State::IncompleteStopped:State::IncompleteStopped;")]),
})
for name, (source, check, edits) in REPEAT_CONTROLS.items():
    D7_SOURCES[name] = source
    D7_CONTROLS[name] = ("record", "repeat-detach", check, edits)


def run_cell(kind, cell, products, output, sanitizers=True):
    # Executes one real-path cell against a product directory and returns its observable outcome.
    output.mkdir(mode=0o700, exist_ok=True)
    if kind == "component":
        argv = [str(products / "snmp3SchedulerTest"), cell]
    elif kind == "qualification":
        argv = [sys.executable, str(ROOT / "tests/rewrite/test_qualification.py"), "--cases", cell,
                "--products", str(products), "--output", str(output / "run")] + (["--sanitizers"] if sanitizers else [])
    else:
        argv = [sys.executable, str(ROOT / "tests/rewrite/test_records.py"), "--case", cell,
                "--products", str(products), "--output", str(output / "run")] + (["--sanitizers"] if sanitizers else [])
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0:abort_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    with (output / "cell.stdout").open("xb") as stdout, (output / "cell.stderr").open("xb") as stderr:
        child = subprocess.Popen(argv, stdout=stdout, stderr=stderr, env=environment)
        code = waited(child, 900)
    failed = []
    checks = []
    loaded_libraries = {}
    aborted = False
    results = output / "run" / "results.json"
    if results.exists():
        data = json.loads(results.read_text())
        checks = data.get("checks", [])
        loaded_libraries = data.get("loaded_libraries", {})
        failed = [check["name"] for check in data.get("checks", []) if not check["passed"]]
        aborted = bool(data.get("aborted"))
        for nested in sorted((output / "run").glob("*/results.json")):
            nested_data = json.loads(nested.read_text())
            failed += [check["name"] for check in nested_data.get("checks", []) if not check["passed"]]
            aborted = aborted or bool(nested_data.get("aborted"))
    # Repeat-detach also requires every external fixture to exit without forced cleanup.
    receipt_pattern = "*.receipt.json" if cell == "repeat-detach" else "records*.receipt.json"
    forced = any(json.loads(receipt.read_text()).get("forced_cleanup")
                 for receipt in sorted((output / "run").glob(receipt_pattern)))
    observations = output / "run" / "record-observations.json"
    stop_events = {"stop-queued": "stop_queued", "stop-inflight": "stop_inflight", "stop-enqueue-failed": "stop_enqueue_failed",
                   "stop-downstream": "stop_downstream", "live-detach": "live_detach", "repeat-detach": "repeat_detach"}
    observed = kind != "record" or cell not in stop_events or (observations.exists() and any(
        event.get("event") == stop_events[cell] for event in json.loads(observations.read_text())))
    return {"argv": argv, "returncode": code, "pid": child.pid, "child_reaped": True, "failed_checks": failed,
            "aborted": aborted, "forced_cleanup": forced, "observed": observed,
            "checks": checks, "loaded_libraries": loaded_libraries,
            "stderr_tail": (output / "cell.stderr").read_text()[-400:]}


def detected(outcome, check):
    # A control is detected when its cell fails without aborting, forced cleanup or a missing stop event;
    # a named check must be among the failed checks.
    return (outcome["returncode"] != 0 and not outcome["aborted"] and not outcome["forced_cleanup"] and
            outcome["observed"] and (check is None or check in outcome["failed_checks"]))


def compile_product(job, original_products, products, source, replacement, output, extra=None):
    argv = ["-L" + str(products) if arg == "-L" + str(original_products) else arg for arg in job["argv"]]
    argv = [str(replacement) if arg == str(source) else arg for arg in argv]
    argv = [(extra or {}).get(arg, arg) for arg in argv]
    product = products / Path(job["argv"][-1]).name
    argv[-1] = str(product)
    with (output / (product.name + ".build.stdout")).open("xb") as stdout:
        with (output / (product.name + ".build.stderr")).open("xb") as stderr:
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
            code = waited(child, 120)
    inputs = [Path(arg) for arg in argv if arg.endswith(".cpp")]
    inputs += list((ROOT / "snmp3App/src").glob("*.h"))
    receipt = {"argv": argv, "returncode": code, "pid": child.pid, "child_reaped": True,
               "inputs": {str(path): digest(path) for path in sorted(inputs)}}
    if code == 0:
        receipt["product_sha256"] = digest(product)
    write_json(output / (product.name + ".build.json"), receipt)
    if code:
        raise RuntimeError("control build failed")
    return product


def waited(child, timeout):
    try:
        return child.wait(timeout=timeout)
    except subprocess.TimeoutExpired:
        child.kill()
        child.wait()
        raise


def resolved_support(executable):
    # Library path the dynamic loader resolves for libsnmp3.so from the given executable.
    listing = subprocess.run(["ldd", str(executable)], capture_output=True, text=True, check=True).stdout
    for line in listing.splitlines():
        if line.strip().startswith("libsnmp3.so"):
            return line.split("=>", 1)[1].split("(", 1)[0].strip()
    return ""


def main_d7(args, jobs, output):
    selected = {Path(job["argv"][-1]).name: job for job in jobs}
    original_products = Path(selected["libsnmp3.so"]["argv"][-1]).parent
    tests = {"component": "snmp3SchedulerTest", "qualification": "snmp3QualificationTest", "record": "snmp3RecordTest"}
    references = {}
    for name in args.d7_controls:
        kind, cell, check, _ = D7_CONTROLS[name]
        if (kind, cell) not in references:
            outcome = run_cell(kind, cell, original_products, output / ("reference-" + kind + "-" + cell))
            references[(kind, cell)] = outcome
        # The reference must pass the very cell or named check the control has to fail; other checks of
        # the same case may fail for unrelated pending work.
        reference = references[(kind, cell)]
        reference.setdefault("passed_for", {})[name] = (not reference["aborted"] and not reference["forced_cleanup"] and
                                                        reference["observed"]) and (
            reference["returncode"] == 0 if check is None else check not in reference["failed_checks"])
        if cell == "repeat-detach":
            matched = [row for row in reference["checks"] if row.get("name") == check]
            reference["passed_for"][name] = (reference["passed_for"][name] and reference["returncode"] == 0 and
                                             len(matched) == 1 and matched[0].get("passed") is True)
    results = []
    for name in args.d7_controls:
        kind, cell, check, edits = D7_CONTROLS[name]
        item = output / name
        item.mkdir(mode=0o700)
        products = item / "products"
        products.mkdir(mode=0o700)
        source = ROOT / "snmp3App/src" / D7_SOURCES.get(name, "Scheduler.cpp")
        text = source.read_text()
        for old, new in edits:
            if text.count(old) != 1:
                raise RuntimeError("control source anchor is not unique")
            text = text.replace(old, new)
        replacement = item / source.name
        replacement.write_text(text)
        extra = {}
        additional_mutations = []
        for filename, extra_edits in REPEAT_EXTRA_EDITS.get(name, {}).items():
            original = ROOT / "snmp3App/src" / filename
            extra_text = original.read_text()
            for old, new in extra_edits:
                if extra_text.count(old) != 1:
                    raise RuntimeError("control source anchor is not unique")
                extra_text = extra_text.replace(old, new)
            changed = item / filename
            changed.write_text(extra_text)
            extra[str(original)] = str(changed)
            additional_mutations.append({"source": str(original), "original_sha256": digest(original),
                                         "mutated_sha256": digest(changed), "edits": extra_edits})
        write_json(item / "mutation.json", {"control": name, "source": str(source), "original_sha256": digest(source),
                   "mutated_sha256": digest(replacement), "edits": edits, "cell": [kind, cell], "check": check,
                   "additional_mutations": additional_mutations})
        rebuilt = ("libsnmp3.so", tests[kind])
        for existing in original_products.iterdir():
            if existing.name not in rebuilt:
                (products / existing.name).symlink_to(existing)
        defective = compile_product(selected["libsnmp3.so"], original_products, products, source, replacement, item, extra)
        executable = compile_product(selected[tests[kind]], original_products, products, source, replacement, item, extra)
        loaded = Path(resolved_support(executable)).resolve() == defective.resolve()
        outcome = run_cell(kind, cell, products, item / "cell")
        if cell == "repeat-detach":
            loaded = loaded and outcome["loaded_libraries"].get(str(defective.resolve())) == digest(defective)
        reference = references[(kind, cell)]
        passed = reference["passed_for"][name] and loaded and detected(outcome, check)
        if cell == "repeat-detach":
            passed = passed and all(any(row.get("name") == required and row.get("passed") is True
                                       for row in outcome["checks"]) for required in
                                    ("records:return", "sanitizer-diagnostics-absent", "secret-sentinels-absent",
                                     "IOC-native-free", "repeat-detach-events", "record-response-drop:normal-stop",
                                     "agent-ipv4-1:normal-stop"))
        results.append({"control": name, "passed": passed, "cell": [kind, cell], "check": check,
                        "reference_passed": reference["passed_for"][name], "defective_library_resolved": loaded,
                        "returncode": outcome["returncode"], "aborted": outcome["aborted"],
                        "forced_cleanup": outcome["forced_cleanup"], "observed": outcome["observed"],
                        "failed_checks": outcome["failed_checks"],
                        "defective_sha256": digest(defective)})
        write_json(output / "results.json", {"passed": all(row["passed"] for row in results),
                   "complete": len(results) == len(args.d7_controls), "controls": results,
                   "references": [dict(value, cell=list(key)) for key, value in references.items()],
                   "inputs": {str(Path(__file__).resolve()): digest(Path(__file__).resolve()),
                              str(args.build_receipt.resolve()): digest(args.build_receipt)}})
    passed = all(row["passed"] for row in results)
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-receipt", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--controls", nargs="+", choices=tuple(CONTROLS), default=list(CONTROLS))
    parser.add_argument("--d7-controls", nargs="+", choices=tuple(D7_CONTROLS))
    args = parser.parse_args()
    output = args.output.resolve()
    output.mkdir(mode=0o700)
    jobs = json.loads(args.build_receipt.read_text())["products"]
    if args.d7_controls:
        return main_d7(args, jobs, output)
    selected = {Path(job["argv"][-1]).name: job for job in jobs}
    support_job = selected["libsnmp3.so"]
    test_job = selected["snmp3RecordTest"]
    original_products = Path(support_job["argv"][-1]).parent
    results = []
    for name in args.controls:
        item = output / name
        item.mkdir(mode=0o700)
        products = item / "products"
        products.mkdir(mode=0o700)
        source_name, case, edits, expected = CONTROLS[name]
        source = ROOT / "snmp3App/src" / source_name
        text = source.read_text()
        for old, new in edits:
            if text.count(old) != 1:
                raise RuntimeError("control source anchor is not unique")
            text = text.replace(old, new)
        replacement = item / source_name
        replacement.write_text(text)
        write_json(item / "mutation.json", {"control": name, "source": str(source),
                   "original_sha256": digest(source), "mutated_sha256": digest(replacement),
                   "edits": edits, "expected_assertion": expected})
        for product_name in ("libsnmp3Wire.so", "libsnmp3Native.so", "snmp3NativeProbe", "snmp3Worker", "snmp3NativeAgent"):
            (products / product_name).symlink_to(original_products / product_name)
        defective = compile_product(support_job, original_products, products, source, replacement, item)
        compile_product(test_job, original_products, products, source, replacement, item)
        argv = [sys.executable, str(ROOT / "tests/rewrite/test_records.py"), "--case", case,
                "--products", str(products), "--sanitizers", "--output", str(item / "run")]
        with (item / "driver.stdout").open("xb") as stdout, (item / "driver.stderr").open("xb") as stderr:
            child = subprocess.Popen(argv, stdout=stdout, stderr=stderr)
            code = waited(child, 60)
        run = item / "run"
        receipt = json.loads((run / "records.receipt.json").read_text())
        diagnostic = (run / "records.stderr").read_text()
        observed = expected in diagnostic
        loaded = receipt["loaded_libraries"].get(str(defective.resolve())) == digest(defective)
        passed = code == 1 and receipt["returncode"] == 1 and not receipt["forced_cleanup"] and observed and loaded
        results.append({"control": name, "passed": passed, "detected_assertion": observed,
                        "actual_defective_library_loaded": loaded, "driver_returncode": code,
                        "driver_pid": child.pid, "driver_reaped": True,
                        "scope": "Shipped Base record/DSET/Runtime/Scheduler/IPC/worker/native/agent path"})
        write_json(output / "results.json", {"passed": all(row["passed"] for row in results),
                   "complete": len(results) == len(args.controls), "controls": results,
                   "inputs": {str(Path(__file__).resolve()): digest(Path(__file__).resolve()),
                              str(args.build_receipt.resolve()): digest(args.build_receipt)},
                   "pending": "Stale-generation control; complete T14 remains pending"})
    passed = all(row["passed"] for row in results)
    print(("PASS: " if passed else "FAIL: ") + str(output / "results.json"))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
