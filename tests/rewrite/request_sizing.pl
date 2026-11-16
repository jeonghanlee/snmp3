#!/usr/bin/env perl
# Configuration writer, record reader and judge of the request sizing cell. test_request_sizing.bash starts and
# stops the fixture agent, the UDP fault proxy and the production IOC; this script writes the configuration of one
# case, drives the IOC shell through a FIFO, reads record fields and the runtime report, and records one line per
# check. Expected values are the literals of the request sizing rule: a rejected batch of N members sets the limit
# of its address to N/2 rounded down, never below 1; a timeout with no unreadable reply changes nothing.
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use Time::HiRes qw(sleep time);

my $USER = 'fixtureUser';
my $ENGINE_ID = '8000000001020304';
my $ADVERTISED = 2000;
my $MEMBERS = 8;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $OID_BASE = '1.3.6.1.4.1.53864.4.';
my $OID_VALUE = 1;
my $OID_TOO_BIG = 11;
my $OID_GEN_ERR = 12;

# Case table: SNMP version, proxy mode (empty: the agent is addressed directly), the object of the eighth binding,
# and per round the expected limit, split count, and alarm message of the first seven records and of the eighth.
# An undefined message means the field is not judged for that round.
my %CASES = (
    'toobig-v2c' => { version => '2c', proxy => '', last => $OID_TOO_BIG, rounds => [
        [4, 1, 'tooBig', 'tooBig'], [2, 2, undef, undef], [1, 3, undef, undef], [1, 3, '', 'tooBig'] ] },
    'toobig-v3' => { version => '3', proxy => 'pass', last => $OID_TOO_BIG, advertised => 1, rounds => [
        [4, 1, 'tooBig', 'tooBig'], [2, 2, undef, undef], [1, 3, undef, undef], [1, 3, '', 'tooBig'] ] },
    # The eighth value of the request is the object that answers genErr, so the unit names index 8.
    'other-status' => { version => '2c', proxy => '', last => $OID_GEN_ERR, never => 'tooBig', rounds => [
        [0, 0, undef, 'genErr index 8'], [0, 0, undef, 'genErr index 8'] ] },
    'answered' => { version => '2c', proxy => '', last => $OID_VALUE, rounds => [ [0, 0, '', ''], [0, 0, '', ''] ] },
    'unreadable' => { version => '3', proxy => 'malformed', last => $OID_VALUE, engine => 1, event => 'fault_mutation',
        rounds => [ [4, 1, 'unreadable rejection', 'unreadable rejection'], [4, 1, '', ''] ] },
    'no-reply' => { version => '2c', proxy => 'drop-all', last => $OID_VALUE, event => 'fault_dropped', rounds => [
        [0, 0, 'no reply', 'no reply'], [0, 0, 'no reply', 'no reply'] ] },
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

# Prints the proxy mode of a case, an empty line for a case without a proxy.
sub command_proxy {
    my ($case) = @_;
    exists $CASES{$case} or die "unknown case $case\n";
    print "$CASES{$case}{proxy}\n";
    return 0;
}

sub command_cases {
    print join(' ', sort keys %CASES), "\n";
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

sub command_config {
    my ($case, $port, $secrets, $path) = @_;
    my $spec = $CASES{$case} or die "unknown case $case\n";
    my %profile = (id => 'Device', version => $spec->{version}, timeoutMs => 300, retries => 0,
                   maxVarbinds => $MEMBERS);
    if ($spec->{version} eq '3') {
        %profile = (%profile, user => $USER, securityLevel => 'authPriv', authAlgorithm => 'SHA',
                    authSecretFile => "$secrets/auth", privAlgorithm => 'AES', privSecretFile => "$secrets/privacy");
        $profile{maxMessageSize} = $ADVERTISED if $spec->{advertised};
        $profile{securityEngineId} = $ENGINE_ID if $spec->{engine};
    } else {
        $profile{communityFile} = "$secrets/community";
    }
    my @bindings = map {
        { id => "R$_", endpoint => 'Device', group => 'G', operation => 'get', valueType => 'integer', capacity => 1,
          oid => $OID_BASE . ($_ == $MEMBERS ? $spec->{last} : $OID_VALUE) . '.0' }
    } 1 .. $MEMBERS;
    my $document = { schema => 1, profiles => [\%profile],
                     endpoints => [{ id => 'Device', address => '127.0.0.1', port => $port + 0, profile => 'Device' }],
                     bindings => \@bindings };
    write_file($path, JSON::PP->new->canonical->pretty->encode($document));
    return 0;
}

my $step = 0;

# Sends shell lines to the IOC and waits until the IOC has executed all of them: the last line writes a marker
# file through the output redirection of the IOC shell, which runs its lines in order.
sub shell {
    my ($run, @lines) = @_;
    $step++;
    my $marker = sprintf('%s/step-%03d.done', $run->{dir}, $step);
    my $fifo = $run->{fifo};
    print {$fifo} map { "$_\n" } @lines, "echo done > $marker";
    my $end = time + $STEP_SECONDS;
    until (-s $marker) {
        time < $end or die "the IOC did not execute step $step within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
    }
    return;
}

# Reads one record field through dbgf with its output redirected to a file and returns the printed value.
sub field {
    my ($run, $name) = @_;
    my $path = sprintf('%s/field-%03d.txt', $run->{dir}, $step + 1);
    shell($run, "dbgf \"$run->{prefix}$name\" > $path");
    my $text = read_file($path);
    $text =~ /^DB[FR]_\w+:\s*(.*?)\s*$/m or die "unexpected dbgf output for $name: $text\n";
    my $value = $1;
    $value = $1 if $value =~ /^"(.*)"$/ || $value =~ /^(\S+) = 0x[0-9a-fA-F]+$/;
    return $value;
}

# Reads the limit and the split count from the batch line of a runtime report that the IOC prints on request.
sub batch_line {
    my ($run) = @_;
    my $offset = -s $run->{stdout};
    shell($run, 'snmp3RuntimeReport');
    my $end = time + $STEP_SECONDS;
    while (1) {
        my $text = substr(read_file($run->{stdout}), $offset);
        if ($text =~ /^snmp3 loop: /m) {
            my @lines = ($text =~ /^snmp3 batch: address=\d+ limit=(\d+) splits=(\d+)$/mg);
            # A report without exactly one batch line is judged as it is: the limit and the count stay undefined.
            return @lines == 2 ? @lines : (undef, undef);
        }
        time < $end or die "the runtime report did not reach the standard output file within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
    }
}

sub check {
    my ($run, $name, $actual, $expected) = @_;
    my $passed = (defined $actual && $actual eq $expected) ? 1 : 0;
    push $run->{checks}->@*, { name => "$run->{case}:$name", passed => $passed ? JSON::PP::true : JSON::PP::false,
                               actual => $actual, expected => $expected };
    printf "%s %s:%s actual=[%s] expected=[%s]\n", $passed ? 'PASS' : 'FAIL', $run->{case}, $name,
        defined $actual ? $actual : 'undefined', $expected;
    return;
}

# Processes the eight records in one pass and waits until none is active; the bound ends the wait when a defect
# keeps a record active, and the checks then judge the state that was reached.
sub round {
    my ($run) = @_;
    shell($run, "dbpf \"$run->{prefix}Round.PROC\" 1");
    my $end = time + $ROUND_SECONDS;
    while (1) {
        my $active = grep { field($run, "R$_.PACT") ne '0' } 1 .. $MEMBERS;
        return $active if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
}

sub proxy_events {
    my ($path) = @_;
    return map { decode_json($_) } grep { /^\{/ } split /\n/, read_file($path);
}

sub command_drive {
    my ($case, $dir, $fifo_path, $stdout, $proxy_out) = @_;
    my $spec = $CASES{$case} or die "unknown case $case\n";
    open(my $fifo, '>', $fifo_path) or die "cannot open $fifo_path: $!\n";
    $fifo->autoflush(1);
    my $run = { case => $case, dir => $dir, fifo => $fifo, stdout => $stdout, prefix => 'Sizing_', checks => [] };
    my $number = 0;
    my %seen;
    for my $expected ($spec->{rounds}->@*) {
        $number++;
        my ($limit, $splits, $first, $last) = $expected->@*;
        check($run, "round$number-active-records", round($run), 0);
        my ($actual_limit, $actual_splits) = batch_line($run);
        check($run, "round$number-limit", $actual_limit, $limit);
        check($run, "round$number-splits", $actual_splits, $splits);
        for my $member (1 .. $MEMBERS) {
            my $message = field($run, "R$member.AMSG");
            my $severity = field($run, "R$member.SEVR");
            $seen{$message} = 1;
            my $want = $member == $MEMBERS ? $last : $first;
            next if !defined $want;
            check($run, "round$number-R$member-message", $message, $want);
            check($run, "round$number-R$member-severity", $severity eq 'INVALID' ? 'INVALID' : 'not INVALID',
                  ($want eq '' && !($member == $MEMBERS && $spec->{last} != $OID_VALUE)) ? 'not INVALID' : 'INVALID');
        }
    }
    check($run, "message-$spec->{never}-never-shown", $seen{ $spec->{never} } ? 'shown' : 'absent', 'absent')
        if defined $spec->{never};
    if ($spec->{proxy} ne '') {
        my @events = proxy_events($proxy_out);
        if (defined $spec->{event}) {
            my $count = grep { $_->{event} eq $spec->{event} } @events;
            check($run, "proxy-$spec->{event}-occurred", $count > 0 ? 'yes' : 'no', 'yes');
        }
        if ($spec->{advertised}) {
            # The engine discovery request of the library carries the library default and no object; the size
            # that the unit applies to a reply is the one of the request that asks for the objects.
            my @sizes = map { $_->{max_size} }
                grep { $_->{event} eq 'fault_request' && $_->{version} == 3 && !$_->{discovery} } @events;
            check($run, 'advertised-requests-observed', @sizes > 0 ? 'yes' : 'no', 'yes');
            my %distinct = map { $_ => 1 } @sizes;
            check($run, 'advertised-size-of-object-requests', join(',', sort keys %distinct), "$ADVERTISED");
        }
    }
    write_file("$dir/checks.json", JSON::PP->new->canonical->pretty->encode($run->{checks}));
    print {$fifo} "exit\n";
    close($fifo);
    return 0;
}

# Joins the checks of every case into results.json; a case without a checks file counts as not passed.
sub command_summary {
    my ($output, @cases) = @_;
    my (%cases, @checks);
    for my $case (@cases) {
        my $path = "$output/$case/checks.json";
        my $list = -s $path ? decode_json(read_file($path)) : [];
        push @checks, $list->@*;
        $cases{$case} = (@$list && !grep { !$_->{passed} } @$list) ? JSON::PP::true : JSON::PP::false;
    }
    my $passed = (!grep { !$_ } values %cases) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", JSON::PP->new->canonical->pretty->encode(
        { scope => 'request sizing', passed => $passed, cases => \%cases, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my %commands = (cases => [\&command_cases, 0], proxy => [\&command_proxy, 1], port => [\&command_port, 0],
                    config => [\&command_config, 4], drive => [\&command_drive, 5]);
    defined $command or die "usage: request_sizing.pl cases|proxy|port|config|drive|summary ARGUMENTS\n";
    return command_summary(@args) if $command eq 'summary' && @args >= 2;
    my $entry = $commands{$command};
    defined $entry && @args == $entry->[1] or die "usage: request_sizing.pl cases|proxy|port|config|drive|summary ARGUMENTS\n";
    return $entry->[0]->(@args);
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "request_sizing.pl: $error";
    $status = 2;
};
exit $status;
