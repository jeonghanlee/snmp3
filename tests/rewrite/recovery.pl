#!/usr/bin/env perl
# Configuration writer, sampler and judge of the recovery cell. test_recovery.bash starts and stops the fixture
# agents and the production IOCs and decides when an agent is lost and when it returns; this script writes the
# configuration of one IOC, takes samples of its records and its runtime report through the IOC shell, and judges
# the retained samples afterwards. Sampling and judging are separate commands: every sample is on disk before the
# first check runs.
#
# Table of checks. Expected values are the literals of the recovery rules of the module:
#   an endpoint whose unit returns with the engine ID unchanged is normal again within 30 s of the return;
#   a unit that does not answer gives COMM/INVALID with the message "no reply";
#   another engine ID at the address gives the state "changed" when the responder authenticates under the
#   profile's secrets and "unverified" when it does not, within 30 s, held until the IOC stops;
#   in a state every record of the endpoint is COMM/INVALID with the message of the state, no SET is sent,
#   withheld records keep their value, and system information records take the responder's value in "changed";
#   one log line per entry into a state.
#
#   check                  input                                   state read                    expected, source
#   applied-uptime         measured seconds from IOC ready to loss marks ready, lost             >= 200 (boots, same)
#   applied-agent          ready line of the returned agent        agent stdout                  boots, engine ID as set
#   before-normal          last sample before the loss             SEVR of R1, R2, Sys           NO_ALARM
#   away-message           every sample while the agent is away    SEVR, AMSG of R1, R2, Sys     INVALID, "no reply"
#   back-within            first normal sample after the return    SEVR, AMSG; marks             within 30 s
#   back-final             last sample after the return            SEVR, AMSG                    NO_ALARM, ""
#   set-before-seen        SET of 5 before the loss                first agent's handler events  at least one commit
#   before-value           sample after that SET                   VAL of R1                     5
#   state-within           first sample that reports the state     report line; marks            within 30 s
#   state-held             every later sample                      report line                   the same state
#   withheld-records       last sample                             SEVR, STAT, AMSG, VAL of R1, R2  INVALID, COMM, message, 5
#   system-information     last sample                             SEVR, STAT, AMSG, VAL of Sys  INVALID, COMM, message,
#                                                                                                77 in "changed", 5 in "unverified"
#   output-record          last sample, after a SET of 9           SEVR, AMSG of Out             INVALID, message
#   set-after-not-seen     that SET                                returned agent's handler events  no commit
#   log-one-line           IOC standard error                      lines "snmp3 engine:"         exactly one, with both engine IDs
#   other-normal           every sample after the return           SEVR of the second endpoint   NO_ALARM
#   other-set-seen         SET of 6 on the second endpoint in the state  second agent's handler events  one more commit
#   other-state            last sample                             report lines                  one "changed", one "normal"
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use POSIX qw(strftime);
use Time::HiRes qw(sleep time clock_gettime CLOCK_MONOTONIC);

my $USER = 'fixtureUser';
my $ENGINE_ID = '8000000001020304';
my $OTHER_ENGINE_ID = '80000000010a0b0c0d';
my $OID_INTEGER = '1.3.6.1.4.1.53864.4.1.0';
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $RETURN_SECONDS = 30;
my $UPTIME_SECONDS = 200;
my $SET_COMMIT_MODE = 3;
my $VALUE_BEFORE = 5;
my $VALUE_RETURNED = 77;
my @INPUTS = qw(R1 R2 Sys);
my %MESSAGE = (changed => 'engine ID changed: restart IOC', unverified => 'engine ID unverified: restart IOC');

# Scenario table: the IOCs of the scenario, the kind of judgment, and the values the returned agent was given.
my %SCENARIOS = (
    pause      => { iocs => [qw(v2c v3d v3e)], kind => 'recover' },
    late       => { iocs => [qw(v2c v3d v3e)], kind => 'recover', late => 1 },
    boots      => { iocs => [qw(v2c v3d v3e)], kind => 'recover', uptime => 1, boots => 2 },
    same       => { iocs => [qw(v3d v3e)],     kind => 'recover', uptime => 1, boots => 1 },
    changed    => { iocs => [qw(v3d)],         kind => 'state', state => 'changed' },
    unverified => { iocs => [qw(v3d)],         kind => 'state', state => 'unverified' },
    two        => { iocs => [qw(v3d)],         kind => 'two' },
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

sub command_scenarios {
    print join(' ', sort keys %SCENARIOS), "\n";
    return 0;
}

sub command_iocs {
    my ($scenario) = @_;
    my $spec = $SCENARIOS{$scenario} or die "unknown scenario $scenario\n";
    print join(' ', $spec->{iocs}->@*), "\n";
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

# Writes the configuration of one IOC: profile v2c, v3d (engine discovery) or v3e (engine ID set); one endpoint A
# on 127.0.0.1, and a second endpoint B on ::1 when a second port is given. With SYSTEM 1 the Sys bindings are
# marked as system information.
sub command_config {
    my ($profile_kind, $system, $path, $secrets, $port_a, $port_b) = @_;
    my %profile = (id => 'Device', timeoutMs => 300, retries => 0);
    if ($profile_kind eq 'v2c') {
        %profile = (%profile, version => '2c', communityFile => "$secrets/community");
    } elsif ($profile_kind eq 'v3d' || $profile_kind eq 'v3e') {
        %profile = (%profile, version => '3', user => $USER, securityLevel => 'authPriv', authAlgorithm => 'SHA',
                    authSecretFile => "$secrets/auth", privAlgorithm => 'AES', privSecretFile => "$secrets/privacy");
        $profile{securityEngineId} = $ENGINE_ID if $profile_kind eq 'v3e';
    } else {
        die "unknown profile $profile_kind\n";
    }
    my (@endpoints, @bindings);
    for my $pair (['A', '127.0.0.1', $port_a], ['B', '::1', $port_b]) {
        my ($name, $address, $port) = @$pair;
        next if !defined $port;
        push @endpoints, { id => $name, address => $address, port => $port + 0, profile => 'Device' };
        for my $binding (qw(In1 In2 Sys Out)) {
            my %row = (id => "$name$binding", endpoint => $name, oid => $OID_INTEGER, valueType => 'integer',
                       capacity => 1, operation => $binding eq 'Out' ? 'set' : 'get');
            $row{systemInfo} = JSON::PP::true if $binding eq 'Sys' && $system;
            push @bindings, \%row;
        }
    }
    write_file($path, encode({ schema => 1, profiles => [\%profile], endpoints => \@endpoints, bindings => \@bindings }));
    return 0;
}

sub now_record {
    return (mono => clock_gettime(CLOCK_MONOTONIC), utc => strftime('%Y-%m-%dT%H:%M:%SZ', gmtime));
}

# Appends a named instant to the marks of a directory.
sub command_mark {
    my ($dir, $name) = @_;
    open(my $fh, '>>', "$dir/marks.jsonl") or die "cannot append to $dir/marks.jsonl: $!\n";
    print {$fh} JSON::PP->new->canonical->encode({ name => $name, now_record() }), "\n" or die "write failed: $!\n";
    close($fh) or die "close failed: $!\n";
    return 0;
}

sub next_number {
    my ($dir) = @_;
    my $path = "$dir/sequence";
    my $number = -s $path ? read_file($path) + 1 : 1;
    write_file($path, "$number\n");
    return $number;
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

# Reads fields of records in one step and returns the printed values in the order asked.
sub fields {
    my ($run, @names) = @_;
    my $base = sprintf('%s/steps/%06d', $run->{dir}, next_number($run->{dir}));
    shell($run, map { "dbgf \"$names[$_]\" > $base-$_.txt" } 0 .. $#names);
    return map {
        my $text = read_file("$base-$_.txt");
        $text =~ /^DB[FR]_\w+:\s*(.*?)\s*$/m or die "unexpected dbgf output for $names[$_]: $text\n";
        my $value = $1;
        $value = $1 if $value =~ /^"(.*)"$/ || $value =~ /^(\S+) = 0x[0-9a-fA-F]+$/;
        $value;
    } 0 .. $#names;
}

# Takes one sample: an optional SET per endpoint, one pass of the inputs of every endpoint, the fields of every
# record once none is active or the bound has passed, and the lines of the runtime report.
# usage: sample DIR FIFO STDOUT PHASE PREFIX[=VALUE]...
sub command_sample {
    my ($dir, $fifo_path, $stdout, $phase, @targets) = @_;
    mkdir "$dir/steps" if !-d "$dir/steps";
    mkdir "$dir/samples" if !-d "$dir/samples";
    open(my $fifo, '>', $fifo_path) or die "cannot open $fifo_path: $!\n";
    $fifo->autoflush(1);
    my $run = { dir => $dir, fifo => $fifo };
    my (@prefixes, @records);
    for my $target (@targets) {
        my ($prefix, $value) = split /=/, $target, 2;
        push @prefixes, $prefix;
        shell($run, "dbpf \"${prefix}Out\" $value") if defined $value;
        shell($run, "dbpf \"${prefix}Round.PROC\" 1");
        push @records, map { "$prefix$_" } @INPUTS, 'Out';
    }
    my $end = time + $ROUND_SECONDS;
    my $active;
    while (1) {
        $active = grep { $_ ne '0' } fields($run, map { "$_.PACT" } @records);
        last if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
    my %sample = (phase => $phase, now_record(), active => $active, records => {});
    my @values = fields($run, map { ("$_.SEVR", "$_.STAT", "$_.AMSG", "$_.VAL") } @records);
    for my $record (@records) {
        my %row;
        @row{qw(sevr stat amsg val)} = splice(@values, 0, 4);
        $sample{records}{$record} = \%row;
    }
    my $offset = -s $stdout;
    shell($run, 'snmp3RuntimeReport');
    $end = time + $STEP_SECONDS;
    my $text = '';
    until ($text =~ /^snmp3 loop: /m) {
        time < $end or die "the runtime report did not reach the standard output file within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
        $text = substr(read_file($stdout), $offset);
    }
    $sample{report} = [grep { /^snmp3 / } split /\n/, $text];
    close($fifo);
    write_file(sprintf('%s/samples/%06d.json', $dir, next_number($dir)), encode(\%sample));
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

sub samples {
    my ($dir) = @_;
    return map { decode_json(read_file($_)) } sort glob("$dir/samples/*.json");
}

sub marks {
    my ($dir) = @_;
    my %marks;
    for my $line (grep { /\S/ } split /\n/, read_file("$dir/marks.jsonl")) {
        my $mark = decode_json($line);
        $marks{ $mark->{name} } = $mark->{mono};
    }
    return %marks;
}

sub events {
    my ($path) = @_;
    return map { eval { decode_json($_) } || () } grep { /^\{/ } split /\n/, read_file($path);
}

sub commits {
    my ($path) = @_;
    return scalar grep { ($_->{event} // '') eq 'agent_handler' && $_->{mode} == $SET_COMMIT_MODE } events($path);
}

# One line of the state of the inputs of an endpoint in a sample: "normal" when every input is without alarm and
# without a message, otherwise the distinct severity and message pairs.
sub inputs_state {
    my ($sample, $prefix) = @_;
    my %seen = map { my $r = $sample->{records}{"$prefix$_"}; ("$r->{sevr} [$r->{amsg}]" => 1) } @INPUTS;
    my $text = join(', ', sort keys %seen);
    return $text eq 'NO_ALARM []' ? 'normal' : $text;
}

sub peer_states {
    my ($sample) = @_;
    return join(',', sort map { /^snmp3 peer: address=\d+ state=(\w+)$/ ? $1 : () } $sample->{report}->@*);
}

sub seconds_after {
    my ($sample, $instant) = @_;
    return sprintf('%.1f', $sample->{mono} - $instant);
}

sub within {
    my ($label, $found, $instant) = @_;
    my $elapsed = defined $found ? seconds_after($found, $instant) : undef;
    check($label, defined $elapsed && $elapsed <= $RETURN_SECONDS ? "within $RETURN_SECONDS s" :
          defined $elapsed ? "after $elapsed s" : 'never', "within $RETURN_SECONDS s");
    return;
}

sub judge_recover {
    my ($tag, $spec, $dir, %marks) = @_;
    my @all = samples($dir);
    my @before = grep { $_->{phase} eq 'before' } @all;
    my @away = grep { $_->{phase} eq 'away' } @all;
    my @back = grep { $_->{phase} eq 'back' } @all;
    if ($spec->{uptime}) {
        my $run = $marks{lost} - $marks{ready};
        check("$tag:applied-uptime", $run >= $UPTIME_SECONDS ? "at least $UPTIME_SECONDS s" : sprintf('%.1f s', $run),
              "at least $UPTIME_SECONDS s");
    }
    check("$tag:before-normal", @before ? inputs_state($before[-1], 'A_') : 'no sample', 'normal') if !$spec->{late};
    my %away = map { inputs_state($_, 'A_') => 1 } @away;
    check("$tag:away-message", @away ? join(' | ', sort keys %away) : 'no sample', 'INVALID [no reply]');
    my ($first) = grep { inputs_state($_, 'A_') eq 'normal' } @back;
    within("$tag:back-within", $first, $marks{returned});
    check("$tag:back-final", @back ? inputs_state($back[-1], 'A_') : 'no sample', 'normal');
    return;
}

sub judge_state {
    my ($tag, $spec, $dir, $scenario_dir, %marks) = @_;
    my $state = $spec->{state};
    my $message = $MESSAGE{$state};
    my @all = samples($dir);
    my @before = grep { $_->{phase} eq 'before' } @all;
    my @back = grep { $_->{phase} eq 'back' } @all;
    check("$tag:set-before-seen", commits("$scenario_dir/agent-1.stdout") > 0 ? 'seen' : 'not seen', 'seen');
    check("$tag:before-value", @before ? $before[-1]{records}{A_R1}{val} : 'no sample', "$VALUE_BEFORE");
    my ($first) = grep { peer_states($_) eq $state } @back;
    within("$tag:state-within", $first, $marks{returned});
    my @later = defined $first ? grep { $_->{mono} >= $first->{mono} } @back : ();
    my %held = map { peer_states($_) => 1 } @later;
    check("$tag:state-held", @later > 1 ? join(' | ', sort keys %held) : 'fewer than two samples', $state);
    my $last = @back ? $back[-1]{records} : {};
    for my $record (qw(A_R1 A_R2)) {
        my $r = $last->{$record} // {};
        check("$tag:withheld-records-$record", join(' ', map { $r->{$_} // 'none' } qw(sevr stat amsg val)),
              "INVALID COMM $message $VALUE_BEFORE");
    }
    my $sys = $last->{A_Sys} // {};
    check("$tag:system-information", join(' ', map { $sys->{$_} // 'none' } qw(sevr stat amsg val)),
          "INVALID COMM $message " . ($state eq 'changed' ? $VALUE_RETURNED : $VALUE_BEFORE));
    my $out = $last->{A_Out} // {};
    check("$tag:output-record", join(' ', map { $out->{$_} // 'none' } qw(sevr amsg)), "INVALID $message");
    check("$tag:set-after-not-seen", commits("$scenario_dir/agent-2.stdout") == 0 ? 'not seen' : 'seen', 'not seen');
    my @lines = grep { /^snmp3 engine: / } split /\n/, read_file("$dir/ioc.stderr");
    my $pattern = sprintf('^snmp3 engine: endpoint=A state=%s previous=%s current=%s authenticated=%d '
                          . 'time=\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$', $state, $ENGINE_ID, $OTHER_ENGINE_ID,
                          $state eq 'changed' ? 1 : 0);
    check("$tag:log-one-line", @lines == 1 ? ($lines[0] =~ /$pattern/ ? 'one line as specified' : $lines[0])
          : scalar(@lines) . ' lines', 'one line as specified');
    return;
}

sub judge_two {
    my ($tag, $dir, $scenario_dir, %marks) = @_;
    my @back = grep { $_->{phase} eq 'back' } samples($dir);
    my ($first) = grep { peer_states($_) eq 'changed,normal' } @back;
    within("$tag:state-within", $first, $marks{returned});
    my %other = map { inputs_state($_, 'B_') => 1 } @back;
    check("$tag:other-normal", @back ? join(' | ', sort keys %other) : 'no sample', 'normal');
    my $before = -s "$scenario_dir/agent-B.commits-at-return" ? read_file("$scenario_dir/agent-B.commits-at-return") + 0 : -1;
    my $after = commits("$scenario_dir/agent-B.stdout");
    check("$tag:other-set-seen", $before >= 0 && $after > $before ? 'one more commit' : "before $before, after $after",
          'one more commit');
    check("$tag:other-state", @back ? peer_states($back[-1]) : 'no sample', 'changed,normal');
    return;
}

# The agent that returned must be the one the scenario asked for: its ready line carries the boots and the engine ID.
sub judge_agent {
    my ($tag, $spec, $scenario_dir) = @_;
    my $path = "$scenario_dir/agent-2.stdout";
    my ($ready) = -s $path ? grep { ($_->{event} // '') eq 'agent_ready' } events($path) : ();
    if (defined $spec->{boots}) {
        check("$tag:applied-agent-boots", defined $ready ? $ready->{boots} : 'no ready line', "$spec->{boots}");
    }
    if ($spec->{kind} ne 'recover') {
        check("$tag:applied-agent-engine", defined $ready ? $ready->{engine_id} : 'no ready line', $OTHER_ENGINE_ID);
    }
    return;
}

# Judges every scenario from its retained samples and writes results.json; a scenario without samples of an IOC
# is not judged and makes the run incomplete.
sub command_judge {
    my ($output) = @_;
    my (%cases, $incomplete);
    for my $scenario (sort keys %SCENARIOS) {
        my $spec = $SCENARIOS{$scenario};
        my $scenario_dir = "$output/$scenario";
        my $first = scalar @checks;
        my $missing = 0;
        for my $ioc ($spec->{iocs}->@*) {
            my $dir = "$scenario_dir/$ioc";
            if (!-s "$dir/marks.jsonl" || !glob("$dir/samples/*.json") || !-e "$scenario_dir/complete") {
                print "INCOMPLETE $scenario:$ioc\n";
                $missing = 1;
                next;
            }
            my %marks = marks($dir);
            my $tag = "$scenario:$ioc";
            judge_recover($tag, $spec, $dir, %marks) if $spec->{kind} eq 'recover';
            judge_state($tag, $spec, $dir, $scenario_dir, %marks) if $spec->{kind} eq 'state';
            judge_two($tag, $dir, $scenario_dir, %marks) if $spec->{kind} eq 'two';
        }
        judge_agent($scenario, $spec, $scenario_dir) if !$missing && ($spec->{boots} || $spec->{kind} ne 'recover');
        $incomplete = 1 if $missing;
        my @own = @checks[$first .. $#checks];
        $cases{$scenario} = (!$missing && @own && !grep { !$_->{passed} } @own) ? JSON::PP::true : JSON::PP::false;
    }
    return 2 if $incomplete;
    my $passed = (!grep { !$_ } values %cases) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'recovery', passed => $passed, cases => \%cases,
                                                checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: recovery.pl scenarios|iocs|port|config|mark|sample|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_scenarios() if $command eq 'scenarios' && !@args;
    return command_iocs(@args) if $command eq 'iocs' && @args == 1;
    return command_port() if $command eq 'port' && !@args;
    return command_config(@args) if $command eq 'config' && (@args == 5 || @args == 6);
    return command_mark(@args) if $command eq 'mark' && @args == 2;
    return command_sample(@args) if $command eq 'sample' && @args >= 5;
    return command_judge(@args) if $command eq 'judge' && @args == 1;
    die $usage;
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "recovery.pl: $error";
    $status = 2;
};
exit $status;
