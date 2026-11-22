#!/usr/bin/env perl
# Configuration writer, record reader and judge of the getbulk cell. test_getbulk.bash starts and stops one fixture
# agent and the production IOC for each case; this script writes the configuration of one case, drives the IOC shell
# through a FIFO, reads record fields, the runtime report and the state record, reads the commands the agent
# handled, and records one line per check. The 24 records read the consecutive instances 1 to 24 of one column of the
# agent (1000 plus the row number), in one group, so that they leave as one batch. Expected values are literals of
# the rule: a batch of consecutive instances is sent as one GETBULK on SNMPv2c and SNMPv3, never on SNMPv1; a reply
# the agent cut to fit the message size of the request is a tooBig reply, and the batch limit of the address is
# then half of the batch.
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use Time::HiRes qw(sleep time);

my $USER = 'fixtureUser';
my $ROWS = 24;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $OID_COLUMN = '1.3.6.1.4.1.53864.5.';
my $COMMAND_GET = 160;
my $COMMAND_GETBULK = 165;

# Case table: SNMP version, the mode of the UDP fault proxy (empty: the agent is addressed directly) and per round
# the expected batch limit, split count, number of requests counted as GETBULK, alarm message of every record, and
# the agent command of the requests of the round (the commands handled since the round before). The proxy mode
# 'missing' takes the last row out of the first reply, which makes it a reply that stops short of the rows asked for
# and carries no end mark; the agent itself never answers so (it answers genErr when its reply would not fit).
my %CASES = (
    'bulk-v2c' => { version => '2c', proxy => '', rounds => [ [0, 0, 1, '', $COMMAND_GETBULK] ] },
    'bulk-v1' => { version => '1', proxy => '', rounds => [ [0, 0, 0, '', $COMMAND_GET] ] },
    'short-reply' => { version => '2c', proxy => 'missing', rounds => [
        [12, 1, 1, 'tooBig', $COMMAND_GETBULK], [12, 1, 3, '', $COMMAND_GETBULK] ] },
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
    my %profile = (id => 'Device', version => $spec->{version}, timeoutMs => 1000, retries => 0, maxVarbinds => 32,
                   communityFile => "$secrets/community");
    my @bindings = map {
        { id => "C$_", endpoint => 'Device', group => 'G', operation => 'get', valueType => 'integer', capacity => 1,
          oid => $OID_COLUMN . $_ }
    } 1 .. $ROWS;
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

# Reads the batch line and the bulk line of a runtime report that the IOC prints on request.
sub report_lines {
    my ($run) = @_;
    my $offset = -s $run->{stdout};
    shell($run, 'snmp3RuntimeReport');
    my $end = time + $STEP_SECONDS;
    while (1) {
        my $text = substr(read_file($run->{stdout}), $offset);
        if ($text =~ /^snmp3 loop: /m) {
            my @batch = ($text =~ /^snmp3 batch: address=\d+ limit=(\d+) splits=(\d+)$/mg);
            my @bulk = ($text =~ /^snmp3 bulk: address=\d+ bulks=(\d+)$/mg);
            # A report without exactly one such line is judged as it is: the values stay undefined.
            return (@batch == 2 ? @batch : (undef, undef), @bulk == 1 ? $bulk[0] : undef);
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

# Processes the 24 records in one pass and waits until none is active; the bound ends the wait when a defect keeps a
# record active, and the checks then judge the state that was reached.
sub round {
    my ($run) = @_;
    shell($run, "dbpf \"$run->{prefix}Round1.PROC\" 1");
    my $end = time + $ROUND_SECONDS;
    while (1) {
        my $active = grep { field($run, "C$_.PACT") ne '0' } 1 .. $ROWS;
        return $active if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
}

# The commands of the requests the agent handled after the given number of its handler lines.
sub agent_commands {
    my ($path, $skip) = @_;
    my @events = grep { /^\{/ && /"event":"agent_handler"/ } split /\n/, read_file($path);
    my %commands = map { decode_json($_)->{command} => 1 } @events[$skip .. $#events];
    return (join(',', sort { $a <=> $b } keys %commands), scalar @events);
}

sub command_drive {
    my ($case, $dir, $fifo_path, $stdout, $agent_out) = @_;
    my $spec = $CASES{$case} or die "unknown case $case\n";
    open(my $fifo, '>', $fifo_path) or die "cannot open $fifo_path: $!\n";
    $fifo->autoflush(1);
    my $run = { case => $case, dir => $dir, fifo => $fifo, stdout => $stdout, prefix => 'Bulk_', checks => [] };
    my $number = 0;
    my $handled = 0;
    for my $expected ($spec->{rounds}->@*) {
        $number++;
        my ($limit, $splits, $bulks, $message, $command) = $expected->@*;
        check($run, "round$number-active-records", round($run), 0);
        my ($actual_limit, $actual_splits, $reported) = report_lines($run);
        check($run, "round$number-limit-splits", defined $actual_limit ? "$actual_limit $actual_splits" : undef, "$limit $splits");
        check($run, "round$number-report-bulks", $reported, $bulks);
        shell($run, "dbpf \"$run->{prefix}Bulks.PROC\" 1");
        check($run, "round$number-record-bulks", field($run, 'Bulks'), $bulks);
        my (@values, %alarms);
        for my $row (1 .. $ROWS) {
            push @values, field($run, "C$row");
            $alarms{ field($run, "C$row.AMSG") } = 1;
        }
        # A round that the unit cut to fit its message leaves the records without a value and with the tooBig alarm.
        check($run, "round$number-messages", join(',', sort keys %alarms), $message);
        check($run, "round$number-values", $message eq '' ? join(',', @values) : 'not read',
              $message eq '' ? join(',', map { 1000 + $_ } 1 .. $ROWS) : 'not read');
        (my $commands, $handled) = agent_commands($agent_out, $handled);
        check($run, "round$number-agent-command", $commands, $command);
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
        { scope => 'getbulk', passed => $passed, cases => \%cases, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my %commands = (cases => [\&command_cases, 0], proxy => [\&command_proxy, 1], port => [\&command_port, 0],
                    config => [\&command_config, 4], drive => [\&command_drive, 5]);
    defined $command or die "usage: getbulk.pl cases|proxy|port|config|drive|summary ARGUMENTS\n";
    return command_summary(@args) if $command eq 'summary' && @args >= 2;
    my $entry = $commands{$command};
    defined $entry && @args == $entry->[1] or die "usage: getbulk.pl cases|proxy|port|config|drive|summary ARGUMENTS\n";
    return $entry->[0]->(@args);
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "getbulk.pl: $error";
    $status = 2;
};
exit $status;
