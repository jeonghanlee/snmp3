#!/usr/bin/env perl
# Configuration writer, IOC shell helper and judge of the SET contract cell. test_set_contract.bash starts the
# fixture agent, the delaying proxy, the Channel Access repeater and the production IOC and makes the writes; this
# script sends shell lines, waits for records to settle, keeps named observations of record fields on disk, and
# judges them afterwards together with the values the agent logged.
#
# Table of checks. Expected values are the written literals and the rules of the SET contract:
#   an output record completes when the result of its SET arrives; a unit that refuses a request gives WRITE/INVALID
#   with "<status name> index <n>"; a unit that does not answer gives COMM/INVALID with "no reply"; a put that
#   waits for completion is sent on its own, and plain puts during a pending request are merged by Base into one
#   further processing with the latest value.
#
#   check                input                                state read                        expected, source
#   <type>-no-alarm      one put per output record type       SEVR of the record                NO_ALARM
#   <type>-set-seen      the same put                         agent log, values of the object   at least one
#   <type>-set-value     the same put, integer types          agent log, last value of the object  the written literal
#   <type>-readback      a GET of the object after the put    VAL of the input record           the written literal
#   delayed-active       a put behind a 750 ms reply delay    PACT read right after the put     1
#   delayed-complete     the same put after it settled        PACT, SEVR, AMSG                  0, NO_ALARM, ""
#   refused              an integer put to an octet string    SEVR, STAT, AMSG                  INVALID, WRITE,
#                                                                                               "wrongType index 1" (status 7 and
#                                                                                               index 1 read from the agent's reply
#                                                                                               with snmpset, 2026-11-16)
#   sequence-order       three puts that wait (7, 8, 9)       agent log, values of the object   7, 8, 9
#   sequence-status      the same puts                        exit status of each client        0, 0, 0
#   burst-merged         three plain puts at once (7, 8, 9)   agent log, values of the object   7, 9
#   burst-readback       a GET of the object after it         VAL of output and input record    9, 9
#   no-reply             a put after the agent is stopped     SEVR, STAT, AMSG                  INVALID, COMM, "no reply"
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use Time::HiRes qw(sleep time);

my $STEP_SECONDS = 15;
my $SETTLE_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $OID_BASE = '1.3.6.1.4.1.53864.4.';
# Object index of the fixture agent per binding stem, and the value type of the binding.
my %OBJECT = (Float => [9, 'opaqueFloat', 1], Integer => [1, 'integer', 1], Counter64 => [5, 'counter64', 1],
              Octets => [6, 'octets', 1024]);
my ($REFUSED, $DELAYED, $SEQUENCE, $BURST) = (6, 16, 17, 18);
# Record, object index, written literal and the text the agent logs for it, per output record type.
my @TYPES = (['Ao', 9, '2.5', undef, 'AoRead'], ['Longout', 1, '41', '41', 'LongoutRead'],
             ['Int64out', 5, '123456789012', '123456789012', 'Int64outRead'],
             ['Stringout', 6, 'abc', undef, 'OctetsRead'], ['Lso', 6, 'longer text', undef, 'OctetsRead']);

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

sub command_port {
    my $socket = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Proto => 'udp')
        or die "cannot bind a loopback UDP socket: $!\n";
    print $socket->sockport, "\n";
    close($socket);
    return 0;
}

# Writes the configuration: endpoint Direct on the agent's port and endpoint Slow on the port of the delaying proxy.
sub command_config {
    my ($path, $community, $port, $slow) = @_;
    my @bindings;
    my $binding = sub {
        my ($id, $endpoint, $index, $operation, $type, $capacity) = @_;
        push @bindings, { id => $id, endpoint => $endpoint, oid => "$OID_BASE$index.0", operation => $operation,
                          valueType => $type, capacity => $capacity + 0 };
    };
    for my $stem (sort keys %OBJECT) {
        my ($index, $type, $capacity) = $OBJECT{$stem}->@*;
        $binding->("${stem}Write", 'Direct', $index, 'set', $type, $capacity);
        $binding->("${stem}Read", 'Direct', $index, 'get', $type, $capacity);
    }
    $binding->('RefusedWrite', 'Direct', $REFUSED, 'set', 'integer', 1);
    $binding->('DelayedWrite', 'Slow', $DELAYED, 'set', 'integer', 1);
    $binding->('SequenceWrite', 'Direct', $SEQUENCE, 'set', 'integer', 1);
    $binding->('BurstWrite', 'Slow', $BURST, 'set', 'integer', 1);
    $binding->('BurstRead', 'Direct', $BURST, 'get', 'integer', 1);
    my %profile = (version => '2c', communityFile => $community, retries => 0);
    write_file($path, encode({ schema => 1,
        profiles => [{ %profile, id => 'Quick', timeoutMs => 300 }, { %profile, id => 'Patient', timeoutMs => 3000 }],
        endpoints => [{ id => 'Direct', address => '127.0.0.1', port => $port + 0, profile => 'Quick' },
                      { id => 'Slow', address => '127.0.0.1', port => $slow + 0, profile => 'Patient' }],
        bindings => \@bindings }));
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
    mkdir "$dir/observations" if !-d "$dir/observations";
    open(my $fifo, '>', $fifo_path) or die "cannot open $fifo_path: $!\n";
    $fifo->autoflush(1);
    return { dir => $dir, fifo => $fifo };
}

# Sends shell lines to the IOC in one write and waits until it has executed all of them: the last line writes a
# marker file through the output redirection of the IOC shell, which runs its lines in order.
sub shell {
    my ($run, @lines) = @_;
    my $marker = sprintf('%s/steps/%06d.done', $run->{dir}, next_number($run->{dir}));
    my $fifo = $run->{fifo};
    print {$fifo} join('', map { "$_\n" } @lines, "echo done > $marker");
    my $end = time + $STEP_SECONDS;
    until (-s $marker) {
        time < $end or die "the IOC did not execute a step within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
    }
    return;
}

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

# usage: send DIR FIFO LINE...
sub command_send {
    my ($dir, $fifo, @lines) = @_;
    shell(open_run($dir, $fifo), @lines);
    return 0;
}

# Waits until none of the records is active or the bound has passed; the bound ends the wait when a defect keeps
# a record active, and the observations then show the state that was reached.
# usage: settle DIR FIFO RECORD...
sub command_settle {
    my ($dir, $fifo, @records) = @_;
    my $run = open_run($dir, $fifo);
    my $end = time + $SETTLE_SECONDS;
    while (1) {
        my $active = grep { $_ ne '0' } fields($run, map { "$_.PACT" } @records);
        last if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
    return 0;
}

# Keeps the named fields under a name. With lines after "--", sends those lines first and reads the fields in the
# same pass, so that nothing runs between the lines and the reading.
# usage: observe DIR FIFO NAME FIELD... [-- LINE...]
sub command_observe {
    my ($dir, $fifo, $name, @rest) = @_;
    my (@names, @lines);
    my $target = \@names;
    for my $word (@rest) {
        if ($word eq '--') { $target = \@lines; next; }
        push @$target, $word;
    }
    my $run = open_run($dir, $fifo);
    my @values;
    if (@lines) {
        my $base = sprintf('%s/steps/%06d', $dir, next_number($dir));
        shell($run, @lines, map { "dbgf \"$names[$_]\" > $base-$_.txt" } 0 .. $#names);
        @values = map {
            my $text = read_file("$base-$_.txt");
            $text =~ /^DB[FR]_\w+:\s*(.*?)\s*$/m or die "unexpected dbgf output for $names[$_]: $text\n";
            my $value = $1;
            $value = $1 if $value =~ /^"(.*)"$/ || $value =~ /^(\S+) = 0x[0-9a-fA-F]+$/;
            $value;
        } 0 .. $#names;
    } else {
        @values = fields($run, @names);
    }
    my %observation;
    @observation{@names} = @values;
    write_file("$dir/observations/$name.json", encode(\%observation));
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
    my ($dir, $name) = @_;
    my $path = "$dir/observations/$name.json";
    return -s $path ? decode_json(read_file($path)) : {};
}

# The values the agent stored for an object, in the order of its log.
sub stored {
    my ($log, $index) = @_;
    return map { $_->{value} } grep { ($_->{event} // '') eq 'agent_set_value' && $_->{index} == $index }
        map { eval { decode_json($_) } || () } grep { /^\{/ } split /\n/, $log;
}

sub joined {
    my ($observation, @names) = @_;
    return join(' ', map { $observation->{$_} // 'none' } @names);
}

# usage: judge OUTPUT
sub command_judge {
    my ($output) = @_;
    -e "$output/complete" or do { print "INCOMPLETE the scenario did not run to its end\n"; return 2; };
    my $log = read_file("$output/agent.stdout");
    for my $type (@TYPES) {
        my ($record, $index, $literal, $logged, $input) = @$type;
        my $key = lc $record;
        check("$key-no-alarm", observation($output, "put-$record")->{"SetContract_$record.SEVR"}, 'NO_ALARM');
        my @values = stored($log, $index);
        check("$key-set-seen", @values ? 'seen' : 'not seen', 'seen');
        check("$key-set-value", observation($output, "put-$record")->{stored} // 'none', $logged) if defined $logged;
        check("$key-readback", observation($output, "read-$record")->{"SetContract_$input.VAL"}, $literal);
    }
    check('delayed-active', observation($output, 'delayed-early')->{'SetContract_Delayed.PACT'}, '1');
    check('delayed-complete', joined(observation($output, 'delayed-late'), map { "SetContract_Delayed.$_" } qw(PACT SEVR AMSG)),
          '0 NO_ALARM ');
    check('refused', joined(observation($output, 'refused'), map { "SetContract_Refused.$_" } qw(SEVR STAT AMSG)),
          'INVALID WRITE wrongType index 1');
    check('sequence-order', join(',', stored($log, $SEQUENCE)), '7,8,9');
    (my $statuses = -s "$output/sequence.status" ? read_file("$output/sequence.status") : 'none') =~ s/\s+/ /g;
    $statuses =~ s/ \z//;
    check('sequence-status', $statuses, '0 0 0');
    check('burst-merged', join(',', stored($log, $BURST)), '7,9');
    check('burst-readback', joined(observation($output, 'burst'), 'SetContract_Burst.VAL', 'SetContract_BurstRead.VAL'), '9 9');
    check('no-reply', joined(observation($output, 'no-reply'), map { "SetContract_Longout.$_" } qw(SEVR STAT AMSG)),
          'INVALID COMM no reply');
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'SET contract', passed => $passed, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

# Adds to an observation the value the agent stored last for an object, read when the observation is made.
# usage: stored DIR NAME AGENT_LOG INDEX
sub command_stored {
    my ($dir, $name, $log, $index) = @_;
    my $observation = observation($dir, $name);
    my @values = stored(read_file($log), $index);
    $observation->{stored} = @values ? $values[-1] : 'none';
    write_file("$dir/observations/$name.json", encode($observation));
    return 0;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: set_contract.pl port|config|send|settle|observe|stored|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_port() if $command eq 'port' && !@args;
    return command_config(@args) if $command eq 'config' && @args == 4;
    return command_send(@args) if $command eq 'send' && @args >= 3;
    return command_settle(@args) if $command eq 'settle' && @args >= 3;
    return command_observe(@args) if $command eq 'observe' && @args >= 4;
    return command_stored(@args) if $command eq 'stored' && @args == 4;
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
    print STDERR "set_contract.pl: $error";
    $status = 2;
};
exit $status;
