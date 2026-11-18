#!/usr/bin/env perl
# Configuration writer, record reader and judge of the module state cell. test_module_state.bash starts and stops
# the fixture agents and the production IOCs and decides the order of the steps; this script writes the
# configuration, drives the IOC shell through a FIFO, keeps what it reads as observation files, and judges the
# retained observations afterwards. Reading and judging are separate commands.
#
# The state records are the ones the module installs into db/: snmp3Runtime.db gives $(P)RuntimeState and
# snmp3Endpoint.db gives $(P)PeerState, $(P)BatchLimit and $(P)BatchSplits of one endpoint. The main IOC has three
# endpoints A, B and C on one address with three ports. Expected values are literals of the rules of the module:
#   the state of an endpoint is normal until a responder at its address shows another engine ID, then changed
#   when the responder authenticates under the profile's secrets and unverified when it does not; changed and
#   unverified carry MAJOR on the state record;
#   a rejected request of N values sets the limit of its address to N/2 rounded down, never below 1, and counts
#   one reduction, so eight values end at the limit 1 after 3 reductions (8 to 4 to 2 to 1);
#   the runtime state is Running while the module runs and Stopped after snmp3Stop;
#   a state record with SCAN "I/O Intr" processes when the state it reads changes, without a periodic scan;
#   the runtime report prints one line per endpoint, "snmp3 endpoint: id=<id> address=N state=<state>", with
#   " since=<time>" for a settled changed or unverified endpoint, the time of its "snmp3 engine:" log line;
#   a state record whose link names an endpoint the configuration does not hold is rejected at initialization
#   and named on the standard error of the IOC.
#
# Table of checks. "obs" names the observation file a value is read from.
#   check                 input                                   state read                        expected, source
#   applied-engine-B/C    ready line of the returned agent        agent stdout                      engine ID as set
#   applied-log-B/C       IOC standard error                      "snmp3 engine:" line of endpoint  changed 1 / unverified 0
#   applied-toobig        obs sized                               AMSG of the eighth group record   tooBig
#   applied-stop          obs stopped                             "snmp3 runtime:" line             state=Stopped
#   peer-before-A/B/C     obs before                              VAL, SEVR of PeerState            normal NO_ALARM
#   peer-after-A          obs returned                            VAL, SEVR of PeerState            normal NO_ALARM
#   peer-after-B          obs returned                            VAL, SEVR of PeerState            changed MAJOR
#   peer-after-C          obs returned                            VAL, SEVR of PeerState            unverified MAJOR
#   endpoint-before       obs before                              "snmp3 endpoint:" lines           A, B, C normal, address of the batch line
#   endpoint-after-A      obs returned                            "snmp3 endpoint:" line of A       normal, no since
#   endpoint-after-B/C    obs returned                            "snmp3 endpoint:" line; log line  state and since = time of the log line
#   peer-line-before      obs before                              "snmp3 peer:" lines               one line, normal
#   peer-line-after       obs returned                            "snmp3 peer:" lines               one line, unverified (the gravest)
#   batch-before          obs before                              VAL of BatchLimit, BatchSplits    0 0
#   batch-after           obs sized                               VAL of BatchLimit, BatchSplits    1 3
#   batch-shared-B        obs sized                               VAL of the two records of B       1 3 (B shares the address of A)
#   batch-line-after      obs sized                               "snmp3 batch:" lines              one line, limit=1 splits=3
#   runtime-running       obs before                              VAL of RuntimeState               Running
#   runtime-stopped       obs stopped                             VAL of RuntimeState               Stopped
#   intr-batch            obs intr-sized, read at once after the  VAL of the records loaded with    1 3 I/O Intr
#                         group rounds, before any wait           SCAN "I/O Intr"; their SCAN
#   intr-peer-changed     obs intr-returned, read at once after   VAL and SCAN of that PeerState    changed I/O Intr
#                         the log line of endpoint B
#   intr-runtime-stopped  obs intr-stopped, read at once after    VAL and SCAN of that              Stopped I/O Intr
#                         snmp3Stop                               RuntimeState
#   intr-start            third IOC: a group of eight with PINI   VAL of BatchLimit, BatchSplits    4 1 tooBig (one rejection of
#                         whose first request is rejected before  loaded with SCAN "I/O Intr";      eight values, before the records
#                         Base scans                              AMSG of the eighth record         could be processed)
#   unknown-ready         second IOC, standard error              initialization complete line      present
#   unknown-<record>      second IOC, standard error              rejection line of the record      present
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use POSIX qw(strftime);
use Time::HiRes qw(sleep time);

my $USER = 'fixtureUser';
my $MEMBERS = 8;
my $OID_BASE = '1.3.6.1.4.1.53864.4.';
my $OID_VALUE = 1;
my $OID_TOO_BIG = 11;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my @ENDPOINTS = qw(A B C);
my %RETURNED_ENGINE = (B => '80000000010a0b0c0d', C => '80000000010e0f1011');
my %RETURNED_STATE = (B => 'changed', C => 'unverified');
my @UNKNOWN_RECORDS = qw(X_PeerState X_BatchLimit X_BatchSplits);
my $TIME_PATTERN = '\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ';

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

# Prints a UDP port that was free on the loopback address when the command ran.
sub command_port {
    my $socket = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Proto => 'udp')
        or die "cannot bind a loopback UDP socket: $!\n";
    print $socket->sockport, "\n";
    close($socket);
    return 0;
}

# Writes the configuration: one SNMPv3 profile with engine discovery and three endpoints on 127.0.0.1. Endpoint A
# has a group of eight values whose eighth is the object the agent answers with tooBig; B and C have the four
# bindings of the recovery database, with Sys marked as system information so that a responder can be asked to
# authenticate.
sub command_config {
    my ($path, $secrets, @ports) = @_;
    my %profile = (id => 'Device', version => '3', timeoutMs => 300, retries => 0, maxVarbinds => $MEMBERS,
                   user => $USER, securityLevel => 'authPriv', authAlgorithm => 'SHA',
                   authSecretFile => "$secrets/auth", privAlgorithm => 'AES', privSecretFile => "$secrets/privacy");
    my @endpoints = map {
        { id => $ENDPOINTS[$_], address => '127.0.0.1', port => $ports[$_] + 0, profile => 'Device' }
    } 0 .. $#ENDPOINTS;
    my @bindings = map {
        { id => "R$_", endpoint => 'A', group => 'G', operation => 'get', valueType => 'integer', capacity => 1,
          oid => $OID_BASE . ($_ == $MEMBERS ? $OID_TOO_BIG : $OID_VALUE) . '.0' }
    } 1 .. $MEMBERS;
    for my $name (qw(B C)) {
        push @bindings, map {
            my %row = (id => "$name$_", endpoint => $name, oid => "$OID_BASE$OID_VALUE.0", valueType => 'integer',
                       capacity => 1, operation => $_ eq 'Out' ? 'set' : 'get');
            $row{systemInfo} = JSON::PP::true if $_ eq 'Sys';
            \%row;
        } qw(In1 In2 Sys Out);
    }
    write_file($path, encode({ schema => 1, profiles => [\%profile], endpoints => \@endpoints, bindings => \@bindings }));
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
    my ($dir, $fifo_path) = @_;
    mkdir "$dir/steps" if !-d "$dir/steps";
    open(my $fifo, '>', $fifo_path) or die "cannot open $fifo_path: $!\n";
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

# usage: send DIR FIFO LINE...
sub command_send {
    my ($dir, $fifo_path, @lines) = @_;
    my $run = open_run($dir, $fifo_path);
    shell($run, @lines);
    close($run->{fifo});
    return 0;
}

# Processes a fanout record and waits until none of the named records is active or the bound has passed.
# usage: round DIR FIFO FANOUT RECORD...
sub command_round {
    my ($dir, $fifo_path, $fanout, @records) = @_;
    my $run = open_run($dir, $fifo_path);
    shell($run, "dbpf \"$fanout.PROC\" 1");
    my $end = time + $ROUND_SECONDS;
    while (1) {
        my $active = grep { $_ ne '0' } fields($run, map { "$_.PACT" } @records);
        last if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
    close($run->{fifo});
    return 0;
}

# Waits until a field no longer reads the given value, or until the bound has passed. The bound is not an
# error: the observation that follows records the field as it is, and its check judges it.
# usage: await DIR FIFO FIELD OLD SECONDS
sub command_await {
    my ($dir, $fifo_path, $field, $old, $seconds) = @_;
    my $run = open_run($dir, $fifo_path);
    my $end = time + $seconds;
    while (1) {
        my ($value) = fields($run, $field);
        last if $value ne $old || time >= $end;
        sleep $POLL_SECONDS;
    }
    close($run->{fifo});
    return 0;
}

# Keeps the named fields and the lines of the runtime report as the observation LABEL.
# usage: observe DIR FIFO STDOUT LABEL FIELD...
sub command_observe {
    my ($dir, $fifo_path, $stdout, $label, @names) = @_;
    my $run = open_run($dir, $fifo_path);
    my %observation = (label => $label, utc => strftime('%Y-%m-%dT%H:%M:%SZ', gmtime), fields => {});
    my @values = fields($run, @names);
    @{ $observation{fields} }{@names} = @values;
    my $offset = -s $stdout;
    shell($run, 'snmp3RuntimeReport');
    my $end = time + $STEP_SECONDS;
    my $text = '';
    until ($text =~ /^snmp3 loop: /m) {
        time < $end or die "the runtime report did not reach the standard output file within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
        $text = substr(read_file($stdout), $offset);
    }
    $observation{report} = [grep { /^snmp3 / } split /\n/, $text];
    close($run->{fifo});
    write_file("$dir/observe-$label.json", encode(\%observation));
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
    return map { eval { decode_json($_) } || () } grep { /^\{/ } split /\n/, read_file($path);
}

sub lines_of {
    my ($observation, $kind) = @_;
    return grep { /^snmp3 \Q$kind\E: / } $observation->{report}->@*;
}

sub joined {
    my (@lines) = @_;
    return @lines ? join(' | ', @lines) : 'no line';
}

sub field_pair {
    my ($observation, @names) = @_;
    return join(' ', map { $observation->{fields}{$_} // 'none' } @names);
}

# Judges the main IOC from its observations and the second IOC from its standard error, and writes results.json.
# A missing observation makes the run incomplete.
sub command_judge {
    my ($output) = @_;
    my $main = "$output/main";
    my %obs;
    for my $label (qw(before sized returned stopped intr-sized intr-returned intr-stopped)) {
        my $path = "$main/observe-$label.json";
        if (!-s $path || !-e "$output/complete") {
            print "INCOMPLETE observation $label\n";
            return 2;
        }
        $obs{$label} = decode_json(read_file($path));
    }
    my $log = read_file("$main/ioc.stderr");
    my %log_time;
    for my $name (qw(B C)) {
        my ($ready) = grep { ($_->{event} // '') eq 'agent_ready' } events("$output/agent-${name}2.stdout");
        check("applied-engine-$name", defined $ready ? $ready->{engine_id} : 'no ready line', $RETURNED_ENGINE{$name});
        my @lines = grep { /^snmp3 engine: endpoint=\Q$name\E / } split /\n/, $log;
        my $summary = scalar(@lines) . ' lines';
        if (@lines == 1 && $lines[0] =~ /^snmp3 engine: endpoint=\S+ state=(\w+) previous=\S+ current=(\S+) authenticated=(\d) time=($TIME_PATTERN)$/) {
            $summary = "$1 $2 $3";
            $log_time{$name} = $4;
        }
        check("applied-log-$name", $summary,
              "$RETURNED_STATE{$name} $RETURNED_ENGINE{$name} " . ($RETURNED_STATE{$name} eq 'changed' ? 1 : 0));
    }
    check('applied-toobig', $obs{sized}{fields}{"A_R$MEMBERS.AMSG"} // 'none', 'tooBig');
    check('applied-stop', (joined(lines_of($obs{stopped}, 'runtime')) =~ /^snmp3 runtime: state=(\w+) /)[0] // 'no line',
          'Stopped');

    check("peer-before-$_", field_pair($obs{before}, "S${_}_PeerState", "S${_}_PeerState.SEVR"), 'normal NO_ALARM')
        for @ENDPOINTS;
    check('peer-after-A', field_pair($obs{returned}, 'SA_PeerState', 'SA_PeerState.SEVR'), 'normal NO_ALARM');
    check("peer-after-$_", field_pair($obs{returned}, "S${_}_PeerState", "S${_}_PeerState.SEVR"),
          "$RETURNED_STATE{$_} MAJOR") for qw(B C);

    # The address number of the endpoint lines comes from the batch line, which the report printed before this cell.
    my ($address) = map { /^snmp3 batch: address=(\d+) / ? $1 : () } lines_of($obs{before}, 'batch');
    $address //= 'unknown';
    check('endpoint-before', joined(lines_of($obs{before}, 'endpoint')),
          joined(map { "snmp3 endpoint: id=$_ address=$address state=normal" } @ENDPOINTS));
    my %after = map { /^snmp3 endpoint: id=(\w+) / ? ($1 => $_) : () } lines_of($obs{returned}, 'endpoint');
    check('endpoint-after-A', $after{A} // 'no line', "snmp3 endpoint: id=A address=$address state=normal");
    check("endpoint-after-$_", $after{$_} // 'no line', "snmp3 endpoint: id=$_ address=$address state=$RETURNED_STATE{$_} since="
          . ($log_time{$_} // 'no log line')) for qw(B C);
    check('peer-line-before', joined(lines_of($obs{before}, 'peer')), "snmp3 peer: address=$address state=normal");
    check('peer-line-after', joined(lines_of($obs{returned}, 'peer')), "snmp3 peer: address=$address state=unverified");

    check('batch-before', field_pair($obs{before}, 'SA_BatchLimit', 'SA_BatchSplits'), '0 0');
    check('batch-after', field_pair($obs{sized}, 'SA_BatchLimit', 'SA_BatchSplits'), '1 3');
    check('batch-shared-B', field_pair($obs{sized}, 'SB_BatchLimit', 'SB_BatchSplits'), '1 3');
    check('batch-line-after', joined(lines_of($obs{sized}, 'batch')), "snmp3 batch: address=$address limit=1 splits=3");
    check('runtime-running', field_pair($obs{before}, 'S_RuntimeState'), 'Running');
    check('runtime-stopped', field_pair($obs{stopped}, 'S_RuntimeState'), 'Stopped');
    check('intr-batch', field_pair($obs{'intr-sized'}, 'IA_BatchLimit', 'IA_BatchSplits', 'IA_BatchLimit.SCAN'), '1 3 I/O Intr');
    check('intr-peer-changed', field_pair($obs{'intr-returned'}, 'IB_PeerState', 'IB_PeerState.SCAN'), 'changed I/O Intr');
    check('intr-runtime-stopped', field_pair($obs{'intr-stopped'}, 'I_RuntimeState', 'I_RuntimeState.SCAN'), 'Stopped I/O Intr');
    my $start = "$output/start/observe-intr-start.json";
    check('intr-start', -s $start ? field_pair(decode_json(read_file($start)), 'IS_BatchLimit', 'IS_BatchSplits', "T_R$MEMBERS.AMSG")
          : 'no observation intr-start', '4 1 tooBig');

    my $unknown = -e "$output/unknown/ioc.stderr" ? read_file("$output/unknown/ioc.stderr") : '';
    check('unknown-ready', $unknown =~ /^iocRun: All initialization complete$/m ? 'present' : 'absent', 'present');
    check("unknown-$_", $unknown =~ /^snmp3: record initialization rejected: \Q$_\E$/m ? 'present' : 'absent', 'present')
        for @UNKNOWN_RECORDS;

    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'module-state', passed => $passed, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: module_state.pl port|config|send|round|await|observe|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_port() if $command eq 'port' && !@args;
    return command_config(@args) if $command eq 'config' && @args == 5;
    return command_send(@args) if $command eq 'send' && @args >= 3;
    return command_round(@args) if $command eq 'round' && @args >= 4;
    return command_await(@args) if $command eq 'await' && @args == 5;
    return command_observe(@args) if $command eq 'observe' && @args >= 5;
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
    print STDERR "module_state.pl: $error";
    $status = 2;
};
exit $status;
