#!/usr/bin/env perl
# Startup writer, record reader and judge of the shell configuration cell. test_shell_config.bash starts and stops
# the fixture agents, the delaying proxy and the production IOCs; this script writes the startup script and the
# database of a case, drives the IOC shell through a FIFO, keeps what it reads as observation files, and judges
# the retained observations afterwards. Every case has its own IOC and its own agents.
#
# The IOCs of this cell are configured by shell commands, and their records name their objects in the link:
#   snmp3ProbePath path, snmp3Session name address [options], snmp3Options pattern [options], snmp3Show pattern 1
#   record link "@<session> <numeric object> type=<type> [deadline_ms=<n>] [group=<label>]"
# Expected values are literals of rules that the cells set-contract, request-sizing and recovery state for a
# configuration from a file, and of the rules for a wrong command:
#   an output record completes when the result of its SET arrives, the agent stores the written value and the
#   next GET returns it; a refused SET is WRITE/INVALID with "<status name> index <n>"; a SET or GET without a
#   reply is COMM/INVALID with "no reply";
#   a rejected request of N values sets the limit of its address to N/2 rounded down, never below 1, so eight
#   values end at the limit 1 after 3 reductions;
#   an endpoint whose unit returns with the engine ID unchanged is without alarm within 30 s of the return;
#   a link without deadline_ms takes a deadline computed from the timeout and the retries of its session;
#   a record that is rejected at initialization leaves no object behind;
#   an option that the profile of a file endpoint cannot take is rejected at the command;
#   a wrong command fails with one line "snmp3: <reason>", the startup script ends there under "on error break"
#   with "snmp3Ioc: startup script failed" and exit status 1, no later line runs, and no secret is printed.
#
# Table of checks. "obs" names the observation file of the case a value is read from.
#   check                    case        input                                  state read                     expected, source
#   write-<type>             write       one put per output record type         SEVR; agent log; VAL of the    NO_ALARM, stored, the written
#                                                                               input record after a GET       literal (5 types)
#   write-delayed            write       a put behind a 750 ms reply delay      PACT right after the put;      1; then 0 NO_ALARM
#                                                                               PACT, SEVR after it settled
#   write-refused            write       an integer put to an octet string      SEVR, STAT, AMSG               INVALID WRITE wrongType index 1
#   write-no-reply           write       a put after the agent is stopped       SEVR, STAT, AMSG               INVALID COMM no reply
#   sizing-v2c, sizing-v3    sizing-*    four passes of a group of eight whose  "snmp3 batch:" line; AMSG of   limit=1 splits=3 tooBig
#                                        eighth object answers tooBig           the eighth record
#   recover-away             recover     every sample while the agent is away   SEVR, STAT, AMSG of the inputs INVALID COMM no reply
#   recover-back             recover     first sample without alarm after the   time since the return          within 30 s
#                                        agent returned with its engine ID
#   wrong-<name>             wrong-*     one wrong line under "on error break"  exit status, standard error,   status 1, the reason line, the
#                                        followed by a marker line and iocInit  marker file, both outputs      failed-startup line, no marker,
#                                                                                                              no initialization, no secret,
#                                                                                                              no address in a module line
#   orphan-object            orphan      a record the IOC rejects after its    "snmp3 object:" lines of       one line, the accepted record
#                                        link was read, and one it accepts      snmp3Show
#   options-values           options     three sessions, two set by a pattern   "snmp3 session:" lines of      A=500 B1=1500 B2=1500
#                                                                               snmp3Show
#   both-forms               both        a file and a command in one script     SEVR of one record of each     NO_ALARM NO_ALARM
#   default-deadline         both        a link of each form without            SEVR of the two records        NO_ALARM NO_ALARM
#                                        deadline_ms
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use POSIX qw(strftime);
use Time::HiRes qw(sleep time clock_gettime CLOCK_MONOTONIC);

my $USER = 'fixtureUser';
my $OID_BASE = '1.3.6.1.4.1.53864.4.';
my ($OID_INTEGER, $OID_COUNTER64, $OID_OCTETS, $OID_FLOAT, $OID_TOO_BIG, $OID_DELAYED) = (1, 5, 6, 9, 11, 16);
my $MEMBERS = 8;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $RETURN_SECONDS = 30;
my $SECRET_MARK = 'shell-config-secret-content-7f3a91';
# Record, object index, value type, written literal, and the input record that reads the object back.
my @TYPES = (['Ao', $OID_FLOAT, 'opaqueFloat', '2.5', 'AoRead'], ['Longout', $OID_INTEGER, 'integer', '41', 'LongoutRead'],
             ['Int64out', $OID_COUNTER64, 'counter64', '123456789012', 'Int64outRead'],
             ['Stringout', $OID_OCTETS, 'octets', 'abc', 'OctetsRead'], ['Lso', $OID_OCTETS, 'octets', 'longer text', 'OctetsRead']);
# Wrong line of each wrong case and the reason line the module prints for it. SESSION stands for a valid session.
my %WRONG = (
    'unknown-option-first' => { lines => ['snmp3Session W ADDRESS noSuchOption=1'], reason => 'snmp3: unknown option' },
    'unknown-option-after' => { lines => ['SESSION', 'snmp3Options W noSuchOption=1'], reason => 'snmp3: unknown option' },
    'timeout-zero'         => { lines => ['SESSION', 'snmp3Options W timeoutMs=0'], reason => 'snmp3: integer option out of range' },
    'session-twice'        => { lines => ['SESSION', 'SESSION'], reason => 'snmp3: name defined twice' },
    'options-unknown'      => { lines => ['snmp3Options Nobody timeoutMs=500'], reason => 'snmp3: session absent' },
    'file-option'          => { lines => ['LOAD', 'snmp3Options F user=someone'], reason => 'snmp3: profile security fields rejected' },
    'secret-kept'          => { lines => ['snmp3Session W ADDRESS version=3 user=fixtureUser securityLevel=authNoPriv '
                                          . 'authAlgorithm=NoSuchAlgorithm authSecretFile=SECRETFILE'],
                                reason => 'snmp3: native algorithm unavailable' },
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
    print join(' ', 'write', 'sizing-v2c', 'sizing-v3', 'recover', (map { "wrong-$_" } sort keys %WRONG), 'options', 'orphan', 'both'), "\n";
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

sub link_text {
    my ($session, $index, $type, @options) = @_;
    return join(' ', "\@$session", "$OID_BASE$index.0", "type=$type", 'deadline_ms=5000', @options);
}

sub record {
    my ($type, $name, $field, $link, @fields) = @_;
    return "record($type, \"$name\") {\n    field(DTYP, \"snmp3\")\n    field($field, \"$link\")\n"
        . join('', map { "    field($_)\n" } @fields) . "}\n";
}

sub community_options {
    my ($secrets) = @_;
    return "version=2c communityFile=$secrets/community";
}

sub usm_options {
    my ($secrets) = @_;
    return "version=3 user=$USER securityLevel=authPriv authAlgorithm=SHA authSecretFile=$secrets/auth "
        . "privAlgorithm=AES privSecretFile=$secrets/privacy";
}

# Writes startup.cmd and records.db of one case into its directory. Arguments after the case: directory, tree
# root, products directory, secrets directory, then the ports the case needs.
sub command_prepare {
    my ($case, $dir, $root, $products, $secrets, @ports) = @_;
    my @head = ("dbLoadDatabase(\"$root/dbd/snmp3Ioc.dbd\")", 'snmp3Ioc_registerRecordDeviceDriver(pdbbase)',
                "snmp3WorkerPath(\"$products/snmp3Worker\")", "snmp3ProbePath(\"$products/snmp3NativeProbe\")");
    my (@lines, $db);
    $db = '';
    if ($case eq 'write') {
        my ($agent, $slow) = @ports;
        push @lines, "snmp3Session W 127.0.0.1:$agent " . community_options($secrets) . ' timeoutMs=300 retries=0',
            "snmp3Session D 127.0.0.1:$slow " . community_options($secrets) . ' timeoutMs=2000 retries=0';
        my %kind = (Ao => 'ao', Longout => 'longout', Int64out => 'int64out', Stringout => 'stringout', Lso => 'lso');
        for my $row (@TYPES) {
            my ($name, $index, $type) = @$row;
            $db .= record($kind{$name}, "W_$name", 'OUT', link_text('W', $index, $type), $name eq 'Lso' ? ('SIZV, "64"') : ());
        }
        $db .= record('ai', 'W_AoRead', 'INP', link_text('W', $OID_FLOAT, 'opaqueFloat'));
        $db .= record('longin', 'W_LongoutRead', 'INP', link_text('W', $OID_INTEGER, 'integer'));
        $db .= record('int64in', 'W_Int64outRead', 'INP', link_text('W', $OID_COUNTER64, 'counter64'));
        $db .= record('stringin', 'W_OctetsRead', 'INP', link_text('W', $OID_OCTETS, 'octets'));
        $db .= record('longout', 'W_Refused', 'OUT', link_text('W', $OID_OCTETS, 'integer'));
        $db .= record('longout', 'W_Delayed', 'OUT', link_text('D', $OID_DELAYED, 'integer'));
    } elsif ($case eq 'sizing-v2c' || $case eq 'sizing-v3') {
        my ($agent) = @ports;
        push @lines, "snmp3Session G 127.0.0.1:$agent "
            . ($case eq 'sizing-v3' ? usm_options($secrets) : community_options($secrets))
            . " timeoutMs=300 retries=0 maxVarbinds=$MEMBERS";
        $db .= "record(fanout, \"G_Round\") {\n    field(SELM, \"All\")\n"
            . join('', map { sprintf("    field(LNK%d, \"G_R%d\")\n", $_ - 1, $_) } 1 .. $MEMBERS) . "}\n";
        $db .= record('longin', "G_R$_", 'INP', link_text('G', $_ == $MEMBERS ? $OID_TOO_BIG : $OID_INTEGER, 'integer', 'group=G'))
            for 1 .. $MEMBERS;
    } elsif ($case eq 'recover') {
        my ($agent) = @ports;
        push @lines, "snmp3Session R 127.0.0.1:$agent " . usm_options($secrets) . ' timeoutMs=300 retries=0';
        $db .= "record(fanout, \"R_Round\") {\n    field(SELM, \"All\")\n    field(LNK0, \"R_In1\")\n    field(LNK1, \"R_In2\")\n}\n";
        $db .= record('longin', "R_In$_", 'INP', link_text('R', $OID_INTEGER, 'integer')) for 1, 2;
    } elsif ($case =~ /^wrong-(.+)$/) {
        my $spec = $WRONG{$1} or die "unknown wrong case $case\n";
        my ($agent) = @ports;
        write_file("$dir/configuration.json", encode({ schema => 1,
            profiles => [{ id => 'File', version => '2c', communityFile => "$secrets/community" }],
            endpoints => [{ id => 'F', address => '127.0.0.1', port => $agent + 0, profile => 'File' }], bindings => [] }));
        write_file("$dir/secret", "$SECRET_MARK\n");
        chmod(0600, "$dir/secret") or die "cannot set the mode of $dir/secret: $!\n";
        my $session = "snmp3Session W 127.0.0.1:$agent " . community_options($secrets);
        for my $line ($spec->{lines}->@*) {
            my $text = $line eq 'SESSION' ? $session : $line eq 'LOAD' ? "snmp3Load(\"$dir/configuration.json\",\"$products/snmp3NativeProbe\")" : $line;
            $text =~ s/ADDRESS/127.0.0.1:$agent/;
            $text =~ s/SECRETFILE/$dir\/secret/;
            push @lines, $text;
        }
        push @lines, "echo reached > $dir/marker";
    } elsif ($case eq 'options') {
        my ($agent) = @ports;
        push @lines, "snmp3Session A 127.0.0.1:$agent " . community_options($secrets) . ' timeoutMs=500',
            "snmp3Session B1 127.0.0.1:$agent " . community_options($secrets),
            "snmp3Session B2 127.0.0.1:$agent " . community_options($secrets),
            'snmp3Options B* timeoutMs=1500';
    } elsif ($case eq 'orphan') {
        my ($agent) = @ports;
        push @lines, "snmp3Session O 127.0.0.1:$agent " . community_options($secrets);
        # A text record cannot hold an integer object: the IOC rejects it after it read the link.
        $db .= record('stringin', 'O_Rejected', 'INP', link_text('O', $OID_INTEGER, 'integer', 'group=G'));
        $db .= record('longin', 'O_Accepted', 'INP', link_text('O', $OID_INTEGER, 'integer'));
    } elsif ($case eq 'both') {
        my ($agent) = @ports;
        write_file("$dir/configuration.json", encode({ schema => 1,
            profiles => [{ id => 'File', version => '2c', communityFile => "$secrets/community", timeoutMs => 300, retries => 0 }],
            endpoints => [{ id => 'F', address => '127.0.0.1', port => $agent + 0, profile => 'File' }],
            bindings => [{ id => 'FromFile', endpoint => 'F', oid => "$OID_BASE$OID_INTEGER.0", operation => 'get',
                           valueType => 'integer', capacity => 1 }] }));
        push @lines, "snmp3Load(\"$dir/configuration.json\",\"$products/snmp3NativeProbe\")",
            "snmp3Session S 127.0.0.1:$agent " . community_options($secrets) . ' timeoutMs=300 retries=0';
        $db .= "record(longin, \"B_File\") {\n    field(DTYP, \"snmp3\")\n    field(INP, \"\@binding=FromFile deadline_ms=5000\")\n}\n";
        $db .= record('longin', 'B_Command', 'INP', link_text('S', $OID_INTEGER, 'integer'));
        $db .= "record(longin, \"B_FilePlain\") {\n    field(DTYP, \"snmp3\")\n    field(INP, \"\@binding=FromFile\")\n}\n";
        $db .= "record(longin, \"B_CommandPlain\") {\n    field(DTYP, \"snmp3\")\n    field(INP, \"\@S $OID_BASE$OID_INTEGER.0 type=integer\")\n}\n";
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

sub settle {
    my ($run, @records) = @_;
    my $end = time + $ROUND_SECONDS;
    while (1) {
        my $active = grep { $_ ne '0' } fields($run, map { "$_.PACT" } @records);
        last if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
    return;
}

# usage: send DIR LINE...
sub command_send {
    my ($dir, @lines) = @_;
    my $run = open_run($dir);
    shell($run, @lines);
    close($run->{fifo});
    return 0;
}

# Processes one record and waits until none of the named records is active or the bound has passed.
# usage: process DIR RECORD WAITED...
sub command_process {
    my ($dir, $record, @waited) = @_;
    my $run = open_run($dir);
    shell($run, "dbpf \"$record.PROC\" 1");
    settle($run, @waited);
    close($run->{fifo});
    return 0;
}

# Writes a value to a record and waits until it is not active or the bound has passed.
# usage: put DIR RECORD VALUE
sub command_put {
    my ($dir, $record, $value) = @_;
    my $run = open_run($dir);
    shell($run, "dbpf \"$record\" \"$value\"");
    settle($run, $record);
    close($run->{fifo});
    return 0;
}

# Waits until none of the named records is active or the bound has passed.
# usage: settle DIR RECORD...
sub command_settle {
    my ($dir, @records) = @_;
    my $run = open_run($dir);
    settle($run, @records);
    close($run->{fifo});
    return 0;
}

# Keeps the named fields and the lines of a report command as the observation LABEL. With "-- LINE..." after the
# fields, the lines are sent first and the fields are read at once after them, without waiting for a record.
# usage: observe DIR LABEL REPORT_COMMAND FIELD... [-- LINE...]
sub command_observe {
    my ($dir, $label, $report, @rest) = @_;
    my (@names, @before);
    my $target = \@names;
    for my $word (@rest) {
        if ($word eq '--') { $target = \@before; next; }
        push @$target, $word;
    }
    my $run = open_run($dir);
    my %observation = (label => $label, utc => strftime('%Y-%m-%dT%H:%M:%SZ', gmtime), fields => {});
    shell($run, @before) if @before;
    $observation{mono} = clock_gettime(CLOCK_MONOTONIC);
    my @values = @names ? fields($run, @names) : ();
    @{ $observation{fields} }{@names} = @values;
    my $stdout = "$dir/ioc.stdout";
    my $offset = -s $stdout;
    shell($run, $report, 'snmp3RuntimeReport');
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

# Appends a named instant to the marks of a directory.
sub command_mark {
    my ($dir, $name) = @_;
    open(my $fh, '>>', "$dir/marks.jsonl") or die "cannot append to $dir/marks.jsonl: $!\n";
    print {$fh} JSON::PP->new->canonical->encode({ name => $name, mono => clock_gettime(CLOCK_MONOTONIC) }), "\n"
        or die "write failed: $!\n";
    close($fh) or die "close failed: $!\n";
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

sub events {
    my ($path) = @_;
    return -e $path ? (map { eval { decode_json($_) } || () } grep { /^\{/ } split /\n/, read_file($path)) : ();
}

sub observation {
    my ($dir, $label) = @_;
    my $path = "$dir/observe-$label.json";
    return -s $path ? decode_json(read_file($path)) : undef;
}

# What explains a missing observation: the IOC ended during its startup, with the last line that names a fault.
sub startup_fault {
    my ($dir) = @_;
    return undef if !-e "$dir/ioc-ended";
    my @all = split /\n/, read_file("$dir/ioc.stderr") . read_file("$dir/ioc.stdout");
    s/\e\[[0-9;]*m//g for @all;
    my @lines = grep { /not registered|not found|^snmp3: / } @all;
    return 'IOC ended during startup: ' . (@lines ? $lines[0] : 'no fault line');
}

sub value_of {
    my ($dir, $label, @names) = @_;
    my $obs = observation($dir, $label);
    return startup_fault($dir) // "no observation $label" if !defined $obs;
    return join(' ', map { $obs->{fields}{$_} // 'none' } @names);
}

sub judge_write {
    my ($dir, $output) = @_;
    my @log = events("$output/write/agent.stdout");
    for my $row (@TYPES) {
        my ($name, $index, $type, $literal, $input) = @$row;
        my $stored = (grep { ($_->{event} // '') eq 'agent_set_value' && $_->{index} == $index } @log) ? 'stored' : 'not stored';
        my $obs = observation($dir, "put-$name");
        check("write-$name", defined $obs ? join(' ', $obs->{fields}{"W_$name.SEVR"} // 'none', $stored,
                                                 $obs->{fields}{"W_$input"} // 'none')
              : startup_fault($dir) // "no observation put-$name", "NO_ALARM stored $literal");
    }
    check('write-delayed', join(' | ', value_of($dir, 'delayed-early', 'W_Delayed.PACT'),
                                value_of($dir, 'delayed-late', 'W_Delayed.PACT', 'W_Delayed.SEVR')), '1 | 0 NO_ALARM');
    check('write-refused', value_of($dir, 'refused', 'W_Refused.SEVR', 'W_Refused.STAT', 'W_Refused.AMSG'),
          'INVALID WRITE wrongType index 1');
    check('write-no-reply', value_of($dir, 'no-reply', 'W_Longout.SEVR', 'W_Longout.STAT', 'W_Longout.AMSG'),
          'INVALID COMM no reply');
    return;
}

sub judge_sizing {
    my ($case, $dir) = @_;
    my $obs = observation($dir, 'sized');
    my $actual = startup_fault($dir) // 'no observation sized';
    if (defined $obs) {
        my ($line) = grep { /^snmp3 batch: / } $obs->{report}->@*;
        $actual = join(' ', (defined $line && $line =~ /(limit=\d+ splits=\d+)$/ ? $1 : 'no batch line'),
                       $obs->{fields}{"G_R$MEMBERS.AMSG"} // 'none');
    }
    check($case, $actual, 'limit=1 splits=3 tooBig');
    return;
}

sub judge_recover {
    my ($dir) = @_;
    my @away = map { decode_json(read_file($_)) } sort glob("$dir/observe-away-*.json");
    my %seen = map { my $f = $_->{fields}; (join(' ', map { $f->{"$_.SEVR"}, $f->{"$_.STAT"}, $f->{"$_.AMSG"} } qw(R_In1 R_In2)) => 1) } @away;
    check('recover-away', @away ? join(' | ', sort keys %seen) : startup_fault($dir) // 'no sample',
          'INVALID COMM no reply INVALID COMM no reply');
    my %marks = -s "$dir/marks.jsonl" ? (map { my $m = decode_json($_); ($m->{name} => $m->{mono}) }
                                         grep { /\S/ } split /\n/, read_file("$dir/marks.jsonl")) : ();
    my @back = map { decode_json(read_file($_)) } sort glob("$dir/observe-back-*.json");
    my ($first) = grep { my $f = $_->{fields}; $f->{'R_In1.SEVR'} eq 'NO_ALARM' && $f->{'R_In2.SEVR'} eq 'NO_ALARM' } @back;
    my $actual = startup_fault($dir) // 'never';
    if (defined $first && defined $marks{returned}) {
        my $elapsed = $first->{mono} - $marks{returned};
        $actual = $elapsed <= $RETURN_SECONDS ? "within $RETURN_SECONDS s" : sprintf('after %.1f s', $elapsed);
    }
    check('recover-back', $actual, "within $RETURN_SECONDS s");
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
                 "$stderr$stdout" =~ /iocRun: All initialization complete/ ? 'initialized' : 'not-initialized',
                 index("$stderr$stdout", $SECRET_MARK) >= 0 ? 'secret-printed' : 'no-secret',
                 (grep { /127\.0\.0\.1/ } @module) ? 'address-in-module-line' : 'no-address');
    check("wrong-$name", join(' ', @parts), 'status=1 reason failed-startup no-marker not-initialized no-secret no-address');
    return;
}

sub judge_options {
    my ($dir) = @_;
    my $obs = observation($dir, 'shown');
    my $actual = startup_fault($dir) // 'no observation shown';
    if (defined $obs) {
        my %timeout = map { /^snmp3 session: name=(\S+) .*\btimeoutMs=(\d+)\b/ ? ($1 => $2) : () } $obs->{report}->@*;
        $actual = join(' ', map { "$_=" . ($timeout{$_} // 'none') } qw(A B1 B2));
    }
    check('options-values', $actual, 'A=500 B1=1500 B2=1500');
    return;
}

sub judge_orphan {
    my ($dir) = @_;
    my $obs = observation($dir, 'shown');
    my $actual = startup_fault($dir) // 'no observation shown';
    if (defined $obs) {
        my @objects = grep { /^snmp3 object: / } $obs->{report}->@*;
        $actual = scalar(@objects) . ' object' . (@objects == 1 ? '' : 's') . ', rejected: '
            . ((-e "$dir/ioc.stderr" && read_file("$dir/ioc.stderr") =~ /^snmp3: record initialization rejected: O_Rejected$/m) ? 'O_Rejected' : 'none');
    }
    check('orphan-object', $actual, '1 object, rejected: O_Rejected');
    return;
}

# Judges the named cases from their retained observations and writes results.json. A case whose directory is missing or
# whose driver did not finish makes the run incomplete.
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
        judge_write($dir, $output) if $case eq 'write';
        judge_sizing($case, $dir) if $case =~ /^sizing-/;
        judge_recover($dir) if $case eq 'recover';
        judge_wrong($1, $dir) if $case =~ /^wrong-(.+)$/;
        judge_options($dir) if $case eq 'options';
        judge_orphan($dir) if $case eq 'orphan';
        check('both-forms', value_of($dir, 'read', 'B_File.SEVR', 'B_Command.SEVR'), 'NO_ALARM NO_ALARM') if $case eq 'both';
        check('default-deadline', value_of($dir, 'read', 'B_FilePlain.SEVR', 'B_CommandPlain.SEVR'), 'NO_ALARM NO_ALARM')
            if $case eq 'both';
    }
    return 2 if $incomplete;
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'shell-config', passed => $passed, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: shell_config.pl cases|port|prepare|send|process|put|settle|observe|mark|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_cases() if $command eq 'cases' && !@args;
    return command_port() if $command eq 'port' && !@args;
    return command_prepare(@args) if $command eq 'prepare' && @args >= 6;
    return command_send(@args) if $command eq 'send' && @args >= 2;
    return command_process(@args) if $command eq 'process' && @args >= 3;
    return command_put(@args) if $command eq 'put' && @args == 3;
    return command_settle(@args) if $command eq 'settle' && @args >= 2;
    return command_observe(@args) if $command eq 'observe' && @args >= 3;
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
    print STDERR "shell_config.pl: $error";
    $status = 2;
};
exit $status;
