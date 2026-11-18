#!/usr/bin/env perl
# Startup writer, record reader and judge of the reply-driven cell. test_reply_driven.bash starts and stops the
# fixture agents, the proxies and the production IOCs; this script writes the startup script and the database of a
# case, drives the IOC shell through a FIFO, keeps what it reads as observation files, and judges the retained
# observations afterwards. Every case has its own IOC and its own agents.
#
# The IOCs of this cell read through subscriptions:
#   snmp3Subscription name session period
#   a record with SCAN "I/O Intr" whose link names a subscription processes when the result of a request of that
#   subscription has arrived, once per result, with the value, alarm and message a record-driven request gives.
# Expected values are literals of these rules:
#   the module sends the next request of a subscription when the period has passed and the earlier request has
#     ended and its records were processed: one request per period with a prompt unit, one per reply with a unit
#     that answers later than the period, and the record is never scanned while it is active;
#   a request without a reply is COMM/INVALID with "no reply" at each attempt, the value stays, and the record is
#     without alarm within 30 s of the return of the unit; another address keeps its period meanwhile;
#   an endpoint whose unit shows another engine ID sends nothing but the reading of system information, shown with
#     the alarm of the state ("engine ID changed: restart IOC") and the value of the responder;
#   the objects of one subscription leave in one request while the request size rules allow, and their records of
#     one PRIO process in the order of PHAS; a subscription whose records have all left the I/O Intr list sends
#     no request;
#   a record with SCAN "I/O Intr" on a session, a record with another SCAN on a subscription, an output record
#     on a subscription and a record of a subscription with a group label are rejected at initialization;
#   a request of a subscription that the queue of its address does not take is READ/INVALID with
#     "request not admitted" on every record of it;
#   a subscription under the name of a session, on an unknown session, or with a period outside 100 to 3600000 ms
#     fails its command.
#
# Table of checks. "obs" names an observation file of the case; a count of requests is the number of GET requests
# in the log of the agent, taken with the reading it belongs to.
#   check                     case         input                                state read                        expected, source
#   period-prompt-requests    period       20 s between two readings of a       requests at the agent in that     within 1 of elapsed / 0.5 s
#                                          record on a subscription of 500 ms   time
#   period-prompt-counter     period       the same readings                    rise of the counter on the        within 1 of the requests
#                                                                               forward link
#   period-delayed-range      period       the same time, a second address      requests and counter, scaled to   both in 24 to 28 (one per
#                                          whose replies are delayed 750 ms     20 s                              750 ms and processing)
#   period-delayed-equal      period       the same readings                    requests and counter              within 1 of each other
#   period-delayed-severity   period       eleven readings in that time         SEVR of the delayed record        NO_ALARM in every reading
#   period-delayed-lcnt       period       the same readings                    LCNT of the delayed record        0 in every reading
#   loss-away-alarm           loss         eleven readings while the agent is   SEVR, STAT, AMSG                  INVALID COMM no reply
#                                          stopped
#   loss-away-kept            loss         the same readings                    VAL, PACT, UDF                    the value before, 0, 0
#   loss-away-attempts        loss         first and last of those readings     rise of the counter               at least 20 (one per attempt
#                                                                                                                 of 600 ms)
#   loss-other-requests       loss         the same time, a second address      requests at its agent             within 1 of elapsed / 0.5 s
#   loss-other-counter        loss         the same readings                    rise of its counter               within 1 of the requests
#   loss-back                 loss         the agent started again              time to the first reading         within 30 s
#                                                                               without alarm
#   loss-back-value           loss         that reading                         VAL; rise of the counter since    the value before; risen
#                                                                               the last reading away
#   engine-before             engine       SNMPv3, a value record and a system  SEVR and VAL of both              NO_ALARM NO_ALARM -123 -123
#                                          information record on one object
#   engine-value-alarm        engine       the agent returned with another      SEVR, STAT, AMSG of the value     INVALID COMM engine ID
#                                          engine ID and the value 77; 5 s      record                            changed: restart IOC
#   engine-value-kept         engine       after the state settled              VAL of the value record           -123
#   engine-sys-alarm          engine                                            SEVR, STAT, AMSG of the system    INVALID COMM engine ID
#                                                                               information record                changed: restart IOC
#   engine-sys-value          engine                                            VAL of that record                77
#   engine-one-object         engine       the 5 s after the state settled      objects per request at the agent  1 in every request
#   engine-requests-continue  engine       the same time                        requests at the agent             more than 0
#   one-request-datagrams     one-request  three objects of one subscription    request datagrams at the proxy;   equal, at least 3
#                                          through a proxy that logs datagrams  passes of the first record
#   one-request-order         one-request  records with PHAS 2, 0 and 1         passes whose three numbers of a   0
#                                                                               shared counter do not rise in
#                                                                               the order PHAS 0, 1, 2
#   one-request-idle          one-request  the three records taken off the      request datagrams at the proxy    none in 5 s (a subscription
#                                          I/O Intr list by SCAN "Passive"      in the 5 s after that             without a record on its list
#                                                                                                                 sends nothing)
#   reject-intr-on-session    wrong-records  SCAN "I/O Intr", link to a session   "record initialization          rejected
#   reject-scan-on-subscription          SCAN "1 second", link to a           rejected" lines; a valid record   rejected
#                                          subscription                         beside them is not named
#   reject-output-on-subscription        an output record, link to a          (first check)                     rejected
#                                          subscription
#   reject-group-on-subscription         a record with group= in a link to a                                    rejected
#                                          subscription
#   refused-admission         refused      two records of one subscription on   SEVR, STAT, AMSG of both          INVALID READ request not
#                                          an address whose queue takes one                                       admitted, for both
#                                          request
#   wrong-<name>              wrong-*      one wrong snmp3Subscription line     exit status, standard error,      status 1, the reason line, the
#                                          under "on error break", a marker     marker file                       failed-startup line, no marker,
#                                          line and iocInit after it                                              no initialization
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use POSIX qw(strftime);
use Time::HiRes qw(sleep time clock_gettime CLOCK_MONOTONIC);

my $USER = 'fixtureUser';
my $OID = '1.3.6.1.4.1.53864.4.1.0';
my $PERIOD_MS = 500;
my $SLOW_PERIOD_MS = 2000;
my $LONG_TIMEOUT_MS = 600;
my $STEP_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $RETURN_SECONDS = 30;
my $MODE_GET = 160;
my $BEFORE = '-123';
my $RESPONDER = '77';
my $STATE_MESSAGE = 'engine ID changed: restart IOC';
# Wrong line of each wrong case and the reason line the module prints for it. SESSION stands for a valid session.
my %WRONG = (
    'subscription-session-name'    => { lines => ['SESSION', "snmp3Subscription W W $PERIOD_MS"], reason => 'snmp3: name defined twice' },
    'subscription-unknown-session' => { lines => ["snmp3Subscription S Nobody $PERIOD_MS"], reason => 'snmp3: session absent' },
    'period-low'                   => { lines => ['SESSION', 'snmp3Subscription S W 99'], reason => 'snmp3: integer option out of range' },
    'period-high'                  => { lines => ['SESSION', 'snmp3Subscription S W 3600001'], reason => 'snmp3: integer option out of range' },
);

sub write_file {
    my ($path, $text) = @_;
    open(my $fh, '>', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub read_file {
    my ($path) = @_;
    open(my $fh, '<', $path) or die "cannot read $path: $!\n";
    local $/;
    my $text = <$fh>;
    close($fh);
    return defined $text ? $text : '';
}

sub encode { return JSON::PP->new->canonical->pretty->encode($_[0]); }

sub command_cases {
    print join(' ', 'period', 'loss', 'engine', 'one-request', 'wrong-records', 'refused', map { "wrong-$_" } sort keys %WRONG), "\n";
    return 0;
}

# Prints a UDP port that was free on the loopback address when the command ran.
sub command_port {
    my $socket = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Proto => 'udp')
        or die "cannot bind a loopback UDP socket: $!\n";
    print $socket->sockport, "\n";
    close($socket);
    return 0;
}

sub community_options {
    my ($secrets) = @_;
    return "version=2c communityFile=$secrets/community timeoutMs=300 retries=0";
}

# An input record on a subscription with a counter on its forward link.
sub counted {
    my ($name, $target, @fields) = @_;
    return "record(longin, \"$name\") {\n    field(DTYP, \"snmp3\")\n    field(SCAN, \"I/O Intr\")\n"
        . "    field(INP, \"\@$target $OID type=integer\")\n    field(FLNK, \"${name}Count\")\n"
        . join('', map { "    field($_)\n" } @fields) . "}\n"
        . "record(calc, \"${name}Count\") {\n    field(INPA, \"${name}Count\")\n    field(CALC, \"A+1\")\n}\n";
}

sub plain {
    my ($type, $name, $field, $link, @fields) = @_;
    return "record($type, \"$name\") {\n    field(DTYP, \"snmp3\")\n    field($field, \"$link\")\n"
        . join('', map { "    field($_)\n" } @fields) . "}\n";
}

# Writes startup.cmd and records.db of one case into its directory. Arguments after the case: directory, tree
# root, products directory, secrets directory, then the ports the case needs.
sub command_prepare {
    my ($case, $dir, $root, $products, $secrets, @ports) = @_;
    my @head = ("dbLoadDatabase(\"$root/dbd/snmp3Ioc.dbd\")", 'snmp3Ioc_registerRecordDeviceDriver(pdbbase)',
                "snmp3WorkerPath(\"$products/snmp3Worker\")", "snmp3ProbePath(\"$products/snmp3NativeProbe\")");
    my (@lines, $db);
    $db = '';
    if ($case eq 'period') {
        my ($prompt, $delayed) = @ports;
        push @lines, "snmp3Session P 127.0.0.1:$prompt " . community_options($secrets),
            "snmp3Session D [::1]:$delayed version=2c communityFile=$secrets/community timeoutMs=2000 retries=0",
            "snmp3Subscription SP P $PERIOD_MS", "snmp3Subscription SD D $PERIOD_MS";
        $db .= counted('C_Prompt', 'SP') . counted('C_Delayed', 'SD');
    } elsif ($case eq 'loss') {
        my ($first, $second) = @ports;
        # The timeout of the first address is longer than the period, so that its requests without a reply follow
        # each other with no gap: a requester that held the second address back meanwhile would never let it ask.
        push @lines, "snmp3Session A 127.0.0.1:$first version=2c communityFile=$secrets/community timeoutMs=$LONG_TIMEOUT_MS retries=0",
            "snmp3Session B [::1]:$second " . community_options($secrets),
            "snmp3Subscription SA A $PERIOD_MS", "snmp3Subscription SB B $PERIOD_MS";
        $db .= counted('L_In', 'SA') . counted('L_Other', 'SB');
    } elsif ($case eq 'engine') {
        my ($agent) = @ports;
        push @lines, "snmp3Session E 127.0.0.1:$agent version=3 user=$USER securityLevel=authPriv authAlgorithm=SHA "
            . "authSecretFile=$secrets/auth privAlgorithm=AES privSecretFile=$secrets/privacy timeoutMs=300 retries=0",
            "snmp3Subscription SE E $PERIOD_MS";
        $db .= plain('longin', 'E_Value', 'INP', "\@SE $OID type=integer", 'SCAN, "I/O Intr"');
        $db .= plain('longin', 'E_Sys', 'INP', "\@SE $OID type=integer systemInfo", 'SCAN, "I/O Intr"');
    } elsif ($case eq 'one-request') {
        my ($proxy) = @ports;
        push @lines, "snmp3Session O 127.0.0.1:$proxy " . community_options($secrets), "snmp3Subscription SO O $SLOW_PERIOD_MS";
        # The records are loaded in the order A, B, C and carry PHAS 2, 0 and 1: the order of one pass is B, C, A.
        my %phase = (A => 2, B => 0, C => 1);
        my %next = (A => 'O_Check', B => 'O_Passes', C => '');
        $db .= "record(calc, \"O_Seq\") {\n    field(INPA, \"O_Seq\")\n    field(CALC, \"A+1\")\n}\n";
        for my $name (qw(A B C)) {
            $db .= plain('longin', "O_$name", 'INP', "\@SO $OID type=integer", 'SCAN, "I/O Intr"', "PHAS, \"$phase{$name}\"",
                         "FLNK, \"O_Stamp$name\"");
            $db .= "record(calc, \"O_Stamp$name\") {\n    field(INPA, \"O_Seq PP\")\n    field(CALC, \"A\")\n"
                . ($next{$name} ne '' ? "    field(FLNK, \"$next{$name}\")\n" : '') . "}\n";
        }
        $db .= "record(calc, \"O_Passes\") {\n    field(INPA, \"O_Passes\")\n    field(CALC, \"A+1\")\n}\n";
        # Counts the passes in which the three numbers do not rise by one in the order B, C, A.
        $db .= "record(calc, \"O_Check\") {\n    field(INPA, \"O_StampB\")\n    field(INPB, \"O_StampC\")\n"
            . "    field(INPC, \"O_StampA\")\n    field(INPD, \"O_Check\")\n    field(CALC, \"((B=A+1)&&(C=B+1))?D:D+1\")\n}\n";
    } elsif ($case eq 'wrong-records') {
        my ($agent) = @ports;
        push @lines, "snmp3Session W 127.0.0.1:$agent " . community_options($secrets), "snmp3Subscription SW W 1000";
        $db .= plain('longin', 'X_IntrSession', 'INP', "\@W $OID type=integer", 'SCAN, "I/O Intr"');
        $db .= plain('longin', 'X_ScanSubscription', 'INP', "\@SW $OID type=integer", 'SCAN, "1 second"');
        $db .= plain('longout', 'X_OutSubscription', 'OUT', "\@SW $OID type=integer");
        $db .= plain('longin', 'X_GroupSubscription', 'INP', "\@SW $OID type=integer group=G", 'SCAN, "I/O Intr"');
        $db .= plain('longin', 'X_Good', 'INP', "\@SW $OID type=integer", 'SCAN, "I/O Intr"');
    } elsif ($case eq 'refused') {
        my ($agent) = @ports;
        # The queue of the address takes one request, and the subscription asks for two objects at once.
        push @lines, "snmp3Session R 127.0.0.1:$agent " . community_options($secrets), "snmp3Subscription SR R $PERIOD_MS",
            'snmp3QueueLimit("127.0.0.1",1,1048576)';
        $db .= plain('longin', "F_$_", 'INP', "\@SR $OID type=integer", 'SCAN, "I/O Intr"') for 1, 2;
    } elsif ($case =~ /^wrong-(.+)$/) {
        my $spec = $WRONG{$1} or die "unknown wrong case $case\n";
        my ($agent) = @ports;
        for my $line ($spec->{lines}->@*) {
            push @lines, $line eq 'SESSION' ? "snmp3Session W 127.0.0.1:$agent " . community_options($secrets) : $line;
        }
        push @lines, "echo reached > $dir/marker";
    } else {
        die "unknown case $case\n";
    }
    write_file("$dir/records.db", $db);
    write_file("$dir/startup.cmd", join("\n", 'on error break', @head, @lines,
                                        ($db ne '' ? ("dbLoadRecords(\"$dir/records.db\")") : ()), 'iocInit') . "\n");
    return 0;
}

sub next_number {
    my ($dir) = @_;
    my $path = "$dir/sequence";
    my $number = -s $path ? read_file($path) + 1 : 1;
    write_file($path, "$number\n");
    return $number;
}

sub open_run {
    my ($dir) = @_;
    mkdir "$dir/steps" if !-d "$dir/steps";
    open(my $fifo, '>', "$dir/ioc.stdin") or die "cannot open $dir/ioc.stdin: $!\n";
    $fifo->autoflush(1);
    return { dir => $dir, fifo => $fifo };
}

# Sends shell lines to the IOC and waits until it has executed all of them: the last line writes a marker file
# through the output redirection of the IOC shell, which runs its lines in order.
sub shell {
    my ($run, @lines) = @_;
    my $marker = sprintf('%s/steps/%06d.done', $run->{dir}, next_number($run->{dir}));
    my $fifo = $run->{fifo};
    print {$fifo} map { "$_\n" } @lines, "echo done > $marker";
    my $end = time + $STEP_SECONDS;
    until (-s $marker) {
        time < $end or die "the IOC did not execute a step within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
    }
    return;
}

# Reads fields of records in one step and returns the printed values in the order asked. A record or field the
# IOC does not hold reads as "none", so that its check fails with that value.
sub fields {
    my ($run, @names) = @_;
    my $base = sprintf('%s/steps/%06d', $run->{dir}, next_number($run->{dir}));
    shell($run, map { "dbgf \"$names[$_]\" > $base-$_.txt" } 0 .. $#names);
    return map {
        my $text = read_file("$base-$_.txt");
        my $value = 'none';
        if ($text =~ /^DB[FR]_\w+:\s*(.*?)\s*$/m) {
            $value = $1;
            $value = $1 if $value =~ /^"(.*)"$/ || $value =~ /^(\S+) = 0x[0-9a-fA-F]+$/;
        }
        $value;
    } 0 .. $#names;
}

# The GET requests in the log of an agent: the number of requests, the number of objects they asked for, and the
# largest number of objects in one request. For the log of a proxy: the number of GET request datagrams.
sub log_counts {
    my ($path) = @_;
    my (%objects, $datagrams);
    $datagrams = 0;
    for my $line (split /\n/, -e $path ? read_file($path) : '') {
        next if $line !~ /^\{/;
        my $event = eval { decode_json($line) } or next;
        if (($event->{event} // '') eq 'agent_handler' && ($event->{mode} // 0) == $MODE_GET) {
            $objects{ $event->{request_id} }++;
        } elsif (($event->{event} // '') eq 'fault_request' && ($event->{command} // 0) == $MODE_GET) {
            $datagrams++;
        }
    }
    my ($total, $largest) = (0, 0);
    for my $count (values %objects) {
        $total += $count;
        $largest = $count if $count > $largest;
    }
    return { requests => scalar(keys %objects), objects => $total, largest => $largest, datagrams => $datagrams };
}

# Keeps the named fields, the counts of the named logs and the lines of the runtime report as the observation
# LABEL. A word NAME=PATH names a log; the counts are taken right after the fields were read.
# usage: observe DIR LABEL [NAME=PATH]... FIELD...
sub command_observe {
    my ($dir, $label, @rest) = @_;
    my (%logs, @names);
    for my $word (@rest) {
        if ($word =~ /^(\w+)=(.+)$/) { $logs{$1} = $2; }
        else { push @names, $word; }
    }
    my $run = open_run($dir);
    my %observation = (label => $label, utc => strftime('%Y-%m-%dT%H:%M:%SZ', gmtime), fields => {}, logs => {});
    my @values = @names ? fields($run, @names) : ();
    $observation{mono} = clock_gettime(CLOCK_MONOTONIC);
    @{ $observation{fields} }{@names} = @values;
    $observation{logs}{$_} = log_counts($logs{$_}) for keys %logs;
    my $stdout = "$dir/ioc.stdout";
    my $offset = -s $stdout;
    shell($run, 'snmp3RuntimeReport');
    my $end = time + $STEP_SECONDS;
    my $text = '';
    until ($text =~ /^snmp3 loop: /m) {
        time < $end or die "the report did not reach the standard output file within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
        $text = substr(read_file($stdout), $offset);
    }
    $observation{report} = [grep { /^snmp3 / } split /\n/, $text];
    close($run->{fifo});
    write_file("$dir/observe-$label.json", encode(\%observation));
    return 0;
}

# Sends shell lines to the IOC and waits until it has executed them.
# usage: send DIR LINE...
sub command_send {
    my ($dir, @lines) = @_;
    my $run = open_run($dir);
    shell($run, @lines);
    close($run->{fifo});
    return 0;
}

# Waits until a field has changed since the command began and holds at least MINIMUM, or the bound has passed.
# usage: await DIR FIELD MINIMUM SECONDS
sub command_await {
    my ($dir, $field, $minimum, $seconds) = @_;
    my $run = open_run($dir);
    my ($first) = fields($run, $field);
    my $end = time + $seconds;
    while (time < $end) {
        my ($value) = fields($run, $field);
        last if $value ne $first && $value =~ /^-?\d+/ && $value >= $minimum;
        sleep $POLL_SECONDS;
    }
    close($run->{fifo});
    return 0;
}

my @checks;

sub check {
    my ($name, $actual, $expected) = @_;
    my $passed = (defined $actual && $actual eq $expected) ? 1 : 0;
    push @checks, { name => $name, passed => $passed ? JSON::PP::true : JSON::PP::false,
                    actual => $actual, expected => $expected };
    printf "%s %s actual=[%s] expected=[%s]\n", $passed ? 'PASS' : 'FAIL', $name,
        defined $actual ? $actual : 'undefined', $expected;
    return;
}

sub observation {
    my ($dir, $label) = @_;
    my $path = "$dir/observe-$label.json";
    return -s $path ? decode_json(read_file($path)) : undef;
}

sub observations {
    my ($dir, $prefix) = @_;
    return map { decode_json(read_file($_)) } sort glob("$dir/observe-$prefix-*.json");
}

sub plain_lines {
    my ($path) = @_;
    my @lines = -e $path ? split(/\n/, read_file($path)) : ();
    s/\e\[[0-9;]*m//g for @lines;
    return @lines;
}

# What explains a missing observation: the IOC ended during its startup, with the first line that names a fault.
sub startup_fault {
    my ($dir) = @_;
    return undef if !-e "$dir/ioc-ended";
    my @lines = grep { /not registered|not found|^snmp3: / } plain_lines("$dir/ioc.stderr"), plain_lines("$dir/ioc.stdout");
    return 'IOC ended during startup: ' . (@lines ? $lines[0] : 'no fault line');
}

sub missing {
    my ($dir, $what) = @_;
    return startup_fault($dir) // "no observation $what";
}

sub field_of {
    my ($obs, @names) = @_;
    return join(' ', map { $obs->{fields}{$_} // 'none' } @names);
}

# The rise of a numeric field, or of a count of a log, between two observations.
sub rise {
    my ($first, $last, $field) = @_;
    my ($a, $b) = ($first->{fields}{$field}, $last->{fields}{$field});
    return (defined $a && defined $b && $a =~ /^-?\d+/ && $b =~ /^-?\d+/) ? $b - $a : undef;
}

sub log_rise {
    my ($first, $last, $log, $key) = @_;
    my ($a, $b) = ($first->{logs}{$log}{$key}, $last->{logs}{$log}{$key});
    return (defined $a && defined $b) ? $b - $a : undef;
}

sub within_one {
    my ($value, $target) = @_;
    return defined $value && abs($value - $target) <= 1;
}

# Requests against the period, and the counter against the requests, between the first and the last reading.
sub judge_rate {
    my ($names, $first, $last, $log, $counter) = @_;
    my $elapsed = $last->{mono} - $first->{mono};
    my $expected = $elapsed / ($PERIOD_MS / 1000);
    my $requests = log_rise($first, $last, $log, 'requests');
    my $counted = rise($first, $last, $counter);
    check($names->[0], within_one($requests, $expected) ? 'within 1 of elapsed / 0.5 s'
          : sprintf('%s requests in %.2f s', $requests // 'no', $elapsed), 'within 1 of elapsed / 0.5 s');
    check($names->[1], (defined $counted && within_one($counted, $requests // -9)) ? 'within 1 of the requests'
          : sprintf('counter %s, requests %s', $counted // 'none', $requests // 'none'), 'within 1 of the requests');
    printf "INFO %s: %s requests and a counter rise of %s in %.2f s\n", $names->[0], $requests // 'no', $counted // 'none', $elapsed;
    return;
}

sub judge_period {
    my ($dir) = @_;
    my @readings = observations($dir, 'reading');
    if (@readings < 2) {
        check($_, missing($dir, 'reading'), 'observed') for qw(period-prompt-requests period-prompt-counter
            period-delayed-range period-delayed-equal period-delayed-severity period-delayed-lcnt);
        return;
    }
    my ($first, $last) = @readings[0, -1];
    judge_rate(['period-prompt-requests', 'period-prompt-counter'], $first, $last, 'prompt', 'C_PromptCount');
    my $elapsed = $last->{mono} - $first->{mono};
    my $requests = log_rise($first, $last, 'delayed', 'requests');
    my $counted = rise($first, $last, 'C_DelayedCount');
    my @scaled = map { defined $_ ? $_ * 20 / $elapsed : -1 } $requests, $counted;
    check('period-delayed-range', (!grep { $_ < 24 || $_ > 28 } @scaled) ? 'both in 24 to 28'
          : sprintf('requests %.1f, counter %.1f per 20 s', @scaled), 'both in 24 to 28');
    check('period-delayed-equal', (defined $counted && within_one($counted, $requests // -9)) ? 'within 1 of each other'
          : sprintf('requests %s, counter %s', $requests // 'none', $counted // 'none'), 'within 1 of each other');
    printf "INFO period-delayed: %s requests and a counter rise of %s in %.2f s\n", $requests // 'no', $counted // 'none', $elapsed;
    my %severity = map { ($_->{fields}{'C_Delayed.SEVR'} // 'none') => 1 } @readings;
    check('period-delayed-severity', join(' ', sort keys %severity), 'NO_ALARM');
    my %lcnt = map { ($_->{fields}{'C_Delayed.LCNT'} // 'none') => 1 } @readings;
    check('period-delayed-lcnt', join(' ', sort keys %lcnt), '0');
    return;
}

sub judge_loss {
    my ($dir) = @_;
    my $before = observation($dir, 'before');
    my @away = observations($dir, 'away');
    my @back = observations($dir, 'back');
    my @names = qw(loss-away-alarm loss-away-kept loss-away-attempts loss-other-requests loss-other-counter loss-back
                   loss-back-value);
    if (!defined $before || @away < 2) {
        check($_, missing($dir, 'away'), 'observed') for @names;
        return;
    }
    my %alarm = map { field_of($_, 'L_In.SEVR', 'L_In.STAT', 'L_In.AMSG') => 1 } @away;
    check('loss-away-alarm', join(' | ', sort keys %alarm), 'INVALID COMM no reply');
    my $value = $before->{fields}{'L_In'} // 'none';
    my %kept = map { field_of($_, 'L_In', 'L_In.PACT', 'L_In.UDF') => 1 } @away;
    check('loss-away-kept', join(' | ', sort keys %kept), "$value 0 0");
    my $attempts = rise($away[0], $away[-1], 'L_InCount');
    check('loss-away-attempts', (defined $attempts && $attempts >= 20) ? 'at least 20' : 'counter rise ' . ($attempts // 'none'),
          'at least 20');
    judge_rate(['loss-other-requests', 'loss-other-counter'], $away[0], $away[-1], 'other', 'L_OtherCount');
    my %marks = -s "$dir/marks.jsonl" ? (map { my $m = decode_json($_); ($m->{name} => $m->{mono}) }
                                         grep { /\S/ } split /\n/, read_file("$dir/marks.jsonl")) : ();
    my ($first) = grep { ($_->{fields}{'L_In.SEVR'} // '') eq 'NO_ALARM' } @back;
    my $actual = 'never';
    if (defined $first && defined $marks{returned}) {
        my $elapsed = $first->{mono} - $marks{returned};
        $actual = $elapsed <= $RETURN_SECONDS ? "within $RETURN_SECONDS s" : sprintf('after %.1f s', $elapsed);
    }
    check('loss-back', $actual, "within $RETURN_SECONDS s");
    my $risen = defined $first ? rise($away[-1], $first, 'L_InCount') : undef;
    check('loss-back-value', defined $first ? join(' ', $first->{fields}{'L_In'} // 'none', ($risen // 0) > 0 ? 'risen' : 'not risen')
          : 'never', "$value risen");
    return;
}

sub judge_engine {
    my ($dir) = @_;
    my ($before, $settled, $after) = map { observation($dir, $_) } qw(before settled after);
    check('engine-before', defined $before ? field_of($before, 'E_Value.SEVR', 'E_Sys.SEVR', 'E_Value', 'E_Sys') : missing($dir, 'before'),
          "NO_ALARM NO_ALARM $BEFORE $BEFORE");
    my @names = qw(engine-value-alarm engine-value-kept engine-sys-alarm engine-sys-value engine-one-object engine-requests-continue);
    if (!defined $settled || !defined $after) {
        check($_, missing($dir, 'after the state settled'), 'observed') for @names;
        return;
    }
    check('engine-value-alarm', field_of($after, 'E_Value.SEVR', 'E_Value.STAT', 'E_Value.AMSG'), "INVALID COMM $STATE_MESSAGE");
    check('engine-value-kept', field_of($after, 'E_Value'), $BEFORE);
    check('engine-sys-alarm', field_of($after, 'E_Sys.SEVR', 'E_Sys.STAT', 'E_Sys.AMSG'), "INVALID COMM $STATE_MESSAGE");
    check('engine-sys-value', field_of($after, 'E_Sys'), $RESPONDER);
    my $requests = log_rise($settled, $after, 'returned', 'requests');
    my $objects = log_rise($settled, $after, 'returned', 'objects');
    check('engine-one-object', (defined $requests && $requests > 0 && $objects == $requests) ? '1 in every request'
          : sprintf('%s objects in %s requests', $objects // 'no', $requests // 'no'), '1 in every request');
    check('engine-requests-continue', ($requests // 0) > 0 ? 'more than 0' : '0', 'more than 0');
    return;
}

sub judge_one_request {
    my ($dir) = @_;
    my $obs = observation($dir, 'passes');
    if (!defined $obs) {
        check($_, missing($dir, 'passes'), 'observed') for qw(one-request-datagrams one-request-order one-request-idle);
        return;
    }
    my $passes = $obs->{fields}{'O_Passes'} // 'none';
    my $datagrams = $obs->{logs}{proxy}{datagrams} // 'none';
    check('one-request-datagrams', ($passes =~ /^\d+$/ && $passes >= 3 && $datagrams eq $passes) ? 'equal, at least 3'
          : "datagrams $datagrams, passes $passes", 'equal, at least 3');
    check('one-request-order', field_of($obs, 'O_Check'), '0');
    my ($first, $last) = map { observation($dir, $_) } qw(idle-1 idle-2);
    my $more = (defined $first && defined $last) ? log_rise($first, $last, 'proxy', 'datagrams') : undef;
    check('one-request-idle', !defined $more ? missing($dir, 'idle') : $more == 0 ? 'none in 5 s' : "$more in 5 s", 'none in 5 s');
    return;
}

sub judge_wrong_records {
    my ($dir) = @_;
    my %rejected = map { /^snmp3: record initialization rejected: (\S+)$/ ? ($1 => 1) : () } plain_lines("$dir/ioc.stderr");
    my $fault = startup_fault($dir);
    my $good = $rejected{X_Good} ? ', the valid record rejected' : '';
    check('reject-intr-on-session', $fault // (($rejected{X_IntrSession} ? 'rejected' : 'accepted') . $good), 'rejected');
    check('reject-scan-on-subscription', $fault // ($rejected{X_ScanSubscription} ? 'rejected' : 'accepted'), 'rejected');
    check('reject-output-on-subscription', $fault // ($rejected{X_OutSubscription} ? 'rejected' : 'accepted'), 'rejected');
    check('reject-group-on-subscription', $fault // ($rejected{X_GroupSubscription} ? 'rejected' : 'accepted'), 'rejected');
    return;
}

sub judge_wrong {
    my ($name, $dir) = @_;
    my $spec = $WRONG{$name};
    my $stderr = -e "$dir/ioc.stderr" ? read_file("$dir/ioc.stderr") : '';
    my $stdout = -e "$dir/ioc.stdout" ? read_file("$dir/ioc.stdout") : '';
    my $status = -s "$dir/status" ? read_file("$dir/status") : 'none';
    $status =~ s/\s+\z//;
    my @module = grep { /^snmp3/ } split /\n/, $stderr;
    my @parts = ("status=$status",
                 (grep { $_ eq $spec->{reason} } @module) ? 'reason' : 'no reason line (' . (@module ? $module[0] : 'none') . ')',
                 (grep { $_ eq 'snmp3Ioc: startup script failed' } @module) ? 'failed-startup' : 'no failed-startup line',
                 -e "$dir/marker" ? 'marker' : 'no-marker',
                 "$stderr$stdout" =~ /iocRun: All initialization complete/ ? 'initialized' : 'not-initialized');
    check("wrong-$name", join(' ', @parts), 'status=1 reason failed-startup no-marker not-initialized');
    return;
}

# Appends a named instant to the marks of a directory.
sub command_mark {
    my ($dir, $name) = @_;
    open(my $fh, '>>', "$dir/marks.jsonl") or die "cannot append to $dir/marks.jsonl: $!\n";
    print {$fh} JSON::PP->new->canonical->encode({ name => $name, mono => clock_gettime(CLOCK_MONOTONIC) }), "\n"
        or die "write failed: $!\n";
    close($fh) or die "close failed: $!\n";
    return 0;
}

# Judges the named cases from their retained observations and writes results.json. A case whose directory is
# missing or whose driver did not finish makes the run incomplete.
sub command_judge {
    my ($output, @cases) = @_;
    my $incomplete = 0;
    for my $case (@cases) {
        my $dir = "$output/$case";
        if (!-e "$dir/complete") {
            print "INCOMPLETE $case\n";
            $incomplete = 1;
            next;
        }
        judge_period($dir) if $case eq 'period';
        judge_loss($dir) if $case eq 'loss';
        judge_engine($dir) if $case eq 'engine';
        judge_one_request($dir) if $case eq 'one-request';
        judge_wrong_records($dir) if $case eq 'wrong-records';
        if ($case eq 'refused') {
            my $obs = observation($dir, 'refused');
            check('refused-admission', defined $obs ? join(' | ', map { field_of($obs, "F_$_.SEVR", "F_$_.STAT", "F_$_.AMSG") } 1, 2)
                  : missing($dir, 'refused'), join(' | ', ('INVALID READ request not admitted') x 2));
        }
        judge_wrong($1, $dir) if $case =~ /^wrong-(.+)$/ && $case ne 'wrong-records';
    }
    return 2 if $incomplete;
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'reply-driven', passed => $passed, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: reply_driven.pl cases|port|prepare|observe|send|await|mark|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_cases() if $command eq 'cases' && !@args;
    return command_port() if $command eq 'port' && !@args;
    return command_prepare(@args) if $command eq 'prepare' && @args >= 6;
    return command_observe(@args) if $command eq 'observe' && @args >= 2;
    return command_send(@args) if $command eq 'send' && @args >= 2;
    return command_await(@args) if $command eq 'await' && @args == 4;
    return command_mark(@args) if $command eq 'mark' && @args == 2;
    return command_judge(@args) if $command eq 'judge' && @args >= 2;
    die $usage;
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "reply_driven.pl: $error";
    $status = 2;
};
exit $status;
