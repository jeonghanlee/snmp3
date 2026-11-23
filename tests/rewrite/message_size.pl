#!/usr/bin/env perl
# Configuration writer, record reader and judge of the message size cell. test_message_size.bash starts and stops one
# fixture agent and the production IOC for each case; this script writes the configuration of one case (an SNMPv3
# profile with the configured message size 1472), drives the IOC shell through a FIFO, reads the record, the state
# record, the runtime report and the requests the agent handled, and records one line per check. Expected values are
# literals of the rule: the session reads snmpEngineMaxMessageSize.0 of the unit when it opens, shows the value read (0 and
# "unread" when the unit does not report it), and advertises the lower of that value and the configured size.
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use Time::HiRes qw(sleep time);

my $USER = 'fixtureUser';
my $CONFIGURED = 1472;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $OID_VALUE = '1.3.6.1.4.1.53864.4.1.0';
my $VALUE_OF_OBJECT = '-123';

# Case table: the value the agent holds for snmpEngineMaxMessageSize.0 (empty: it does not hold the object), the value
# the report and the state record show, and the msgMaxSize that the request of the record carries.
my %CASES = (
    'size-below' => { size => 700, shown => 700, report => '700', advertised => 700 },
    'size-above' => { size => 3000, shown => 3000, report => '3000', advertised => $CONFIGURED },
    'size-absent' => { size => '', shown => 0, report => 'unread', advertised => $CONFIGURED },
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

# Prints the value of the agent of a case, an empty line for a case whose agent does not hold the object.
sub command_size {
    my ($case) = @_;
    exists $CASES{$case} or die "unknown case $case\n";
    print "$CASES{$case}{size}\n";
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
    exists $CASES{$case} or die "unknown case $case\n";
    my %profile = (id => 'Device', version => '3', timeoutMs => 1000, retries => 0, maxVarbinds => 32,
                   maxMessageSize => $CONFIGURED, user => $USER, securityLevel => 'authPriv',
                   authAlgorithm => 'SHA', authSecretFile => "$secrets/auth",
                   privAlgorithm => 'AES', privSecretFile => "$secrets/privacy");
    my @bindings = ({ id => 'G1', endpoint => 'Device', operation => 'get', valueType => 'integer', capacity => 1,
                      oid => $OID_VALUE });
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

# Reads the size line of a runtime report that the IOC prints on request.
sub report_size {
    my ($run) = @_;
    my $offset = -s $run->{stdout};
    shell($run, 'snmp3RuntimeReport');
    my $end = time + $STEP_SECONDS;
    while (1) {
        my $text = substr(read_file($run->{stdout}), $offset);
        if ($text =~ /^snmp3 loop: /m) {
            my @size = ($text =~ /^snmp3 size: id=Device unit=(\S+)$/mg);
            # A report without exactly one such line is judged as it is: the value stays undefined.
            return @size == 1 ? $size[0] : undef;
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

# Processes the record and waits until it is not active; the bound ends the wait when a defect keeps it active, and the
# checks then judge the state that was reached.
sub round {
    my ($run) = @_;
    shell($run, "dbpf \"$run->{prefix}G1.PROC\" 1");
    my $end = time + $ROUND_SECONDS;
    while (1) {
        my $active = field($run, 'G1.PACT') ne '0';
        return $active ? 1 : 0 if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
}

# The msgMaxSize of every request of the record that the agent handled (the agent shows -1 for a version without it).
sub agent_sizes {
    my ($path) = @_;
    my @events = map { decode_json($_) } grep { /^\{/ && /"event":"agent_handler"/ } split /\n/, read_file($path);
    return join(',', map { $_->{msg_max_size} } grep { $_->{index} == 1 } @events);
}

sub command_drive {
    my ($case, $dir, $fifo_path, $stdout, $agent_out) = @_;
    my $spec = $CASES{$case} or die "unknown case $case\n";
    open(my $fifo, '>', $fifo_path) or die "cannot open $fifo_path: $!\n";
    $fifo->autoflush(1);
    my $run = { case => $case, dir => $dir, fifo => $fifo, stdout => $stdout, prefix => 'Size_', checks => [] };
    check($run, 'active-record', round($run), 0);
    check($run, 'record-value', field($run, 'G1'), $VALUE_OF_OBJECT);
    check($run, 'record-message', field($run, 'G1.AMSG'), '');
    shell($run, "dbpf \"$run->{prefix}MessageSize.PROC\" 1");
    check($run, 'state-record', field($run, 'MessageSize'), $spec->{shown});
    check($run, 'report-line', report_size($run), $spec->{report});
    check($run, 'request-advertises-size-used', agent_sizes($agent_out), $spec->{advertised});
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
        { scope => 'message-size', passed => $passed, cases => \%cases, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my %commands = (cases => [\&command_cases, 0], size => [\&command_size, 1], port => [\&command_port, 0],
                    config => [\&command_config, 4], drive => [\&command_drive, 5]);
    defined $command or die "usage: message_size.pl cases|size|port|config|drive|summary ARGUMENTS\n";
    return command_summary(@args) if $command eq 'summary' && @args >= 2;
    my $entry = $commands{$command};
    defined $entry && @args == $entry->[1] or die "usage: message_size.pl cases|size|port|config|drive|summary ARGUMENTS\n";
    return $entry->[0]->(@args);
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "message_size.pl: $error";
    $status = 2;
};
exit $status;
