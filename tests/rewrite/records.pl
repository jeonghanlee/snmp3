#!/usr/bin/env perl
# Judge and helper of the record cell. test_records.bash keeps what the fixture agent and the record test program produced in the
# output directory (OUT); this script writes the configuration and judges. The cases baseline, shutdown, queued-shutdown and edges
# and accounting are moved so far.
#   records.pl port                  print a free UDP port of the loopback address
#   records.pl sockets PID PORT OUT  write the sockets that the process holds as JSON to OUT
#   records.pl config OUT PORT CASE  write OUT/records.json for the agent on PORT and, for a case with fault proxies, for the
#                                    ports that OUT/NAME.port holds
#   records.pl faultport FILE        print the port that the fault proxy announced in FILE (exit 1 when it announced none)
#   records.pl judge OUT CASE        judge a case, write OUT/results.json, print one line for each check and the verdict;
#                                    exit 0 (every check passed) or 1
# Exit status 2: the observations could not be read.
# The checks and their order are those of the former Python harness: the agent, the exit of the record test program, the four
# checks of the record path and the pressure phase, the checks that the case adds, the libraries of the program, the stop of the
# agent and the scan.
# Language level: Perl 5.26.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use IO::Socket::INET;
use JSON::PP;

my $ROOT = abs_path(dirname(__FILE__) . '/../..');
my @CASES = qw(baseline shutdown queued-shutdown edges accounting);
my $AGENT = 'agent-ipv4-1';
# The fault proxies that a case starts after the agent, in the order of their start, and the windows of the Retired send that
# the case holds.
my %PROXIES = (accounting => [qw(active-response-delay active-response-drop)]);
my %WINDOWS = (accounting => 7);

# The bindings of the case: name, object index, operation, value type, capacity.
my @SPECS = (['IntegerRead', 1, 'get', 'integer', 1], ['Counter32Read', 2, 'get', 'counter32', 1],
             ['Counter64Read', 5, 'get', 'counter64', 1], ['OctetsRead', 6, 'get', 'octets', 1024],
             ['OidRead', 7, 'get', 'oid', 128], ['FloatRead', 9, 'get', 'opaqueFloat', 1],
             ['DoubleRead', 10, 'get', 'opaqueDouble', 1], ['IpRead', 8, 'get', 'ipAddress', 1],
             ['IntegerWrite', 1, 'set', 'integer', 1], ['Counter64Write', 5, 'set', 'counter64', 1],
             ['OctetsWrite', 6, 'set', 'octets', 1024], ['MaxOctetsRead', 6, 'get', 'octets', 32766],
             ['MaxOctetsWrite', 6, 'set', 'octets', 32766], ['FloatWrite', 9, 'set', 'opaqueFloat', 1]);

sub read_file {
    my ($path) = @_;
    open(my $fh, '<:raw', $path) or die "cannot read $path: $!\n";
    local $/;
    my $text = <$fh>;
    close($fh);
    return defined $text ? $text : '';
}

sub write_file {
    my ($path, $text) = @_;
    open(my $fh, '>:raw', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub is_number {
    my ($value) = @_;
    return defined $value && !ref $value && $value =~ /^-?\d+(?:\.\d+)?(?:[eE][-+]?\d+)?\z/ ? 1 : 0;
}

# The value of a key that the event must hold; a missing key ends the judgment as the former harness did (KeyError).
sub required {
    my ($event, $key) = @_;
    exists $event->{$key} or die "event without $key\n";
    return $event->{$key};
}

sub free_port {
    my $socket = IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1', LocalPort => 0)
        or die "cannot bind a UDP socket: $!\n";
    print $socket->sockport(), "\n";
    close($socket);
    return 0;
}

sub sockets {
    my ($pid, $port, $path) = @_;
    defined $path && $pid =~ /^\d+$/ or die "usage: records.pl sockets PID PORT OUT\n";
    my %inodes;
    opendir(my $dh, "/proc/$pid/fd") or die "cannot read the descriptors of $pid: $!\n";
    for my $name (readdir $dh) {
        my $target = readlink("/proc/$pid/fd/$name");
        $inodes{$1} = 1 if defined $target && $target =~ /^socket:\[(\d+)\]\z/;
    }
    closedir($dh);
    my @listeners;
    for my $table (qw(udp udp6 tcp tcp6)) {
        open(my $fh, '<', "/proc/$pid/net/$table") or next;
        my $header = <$fh>;
        while (my $line = <$fh>) {
            my @fields = split ' ', $line;
            next unless @fields > 9 && $inodes{ $fields[9] };
            push @listeners, { table => $table, local => $fields[1], state => $fields[3], inode => $fields[9] };
        }
        close($fh);
    }
    write_file($path, JSON::PP->new->canonical->pretty->encode({ pid => $pid + 0, sockets => \@listeners }));
    return 0;
}

sub config {
    my ($dir, $port, $case) = @_;
    defined $port && $port =~ /^\d+$/ or die "usage: records.pl config OUT PORT CASE\n";
    $case //= 'baseline';
    my @bindings;
    for my $spec (@SPECS) {
        my ($name, $index, $operation, $type, $capacity) = @$spec;
        push @bindings, { id => $name, endpoint => 'Local', oid => "1.3.6.1.4.1.53864.4.$index.0", operation => $operation,
                          valueType => $type, capacity => $capacity + 0 };
    }
    my %local = (id => 'Local', version => '2c', communityFile => "$dir/$AGENT-community", timeoutMs => 100, retries => 0,
                 maxVarbinds => 16);
    my @profiles = (\%local);
    my @endpoints = ({ id => 'Local', address => '127.0.0.1', port => $port + 0, profile => 'Local' });
    if ($case eq 'accounting') {
        my ($delayed, $dropped) = map { (split ' ', read_file("$dir/$_.port"))[0] + 0 } @{ $PROXIES{$case} };
        push @profiles, { %local, id => 'Delayed', timeoutMs => 1500 }, { %local, id => 'NativeRetry', retries => 1 };
        push @endpoints, { id => 'Delayed', address => '127.0.0.1', port => $delayed, profile => 'Delayed' },
                         { id => 'Dropped', address => '127.0.0.1', port => $dropped, profile => 'Local' },
                         { id => 'NativeRetry', address => '127.0.0.1', port => $dropped, profile => 'NativeRetry' };
        my @outputs = grep { $_->{operation} eq 'set' } @bindings;
        my ($float) = grep { $_->{id} eq 'FloatWrite' } @outputs;
        push @bindings, (map { { %$_, id => "Active$_->{id}", endpoint => 'Delayed' } } @outputs),
                        { %$float, id => 'RetryFloatWrite', endpoint => 'Dropped' },
                        { %$float, id => 'NativeRetryFloatWrite', endpoint => 'NativeRetry' };
    }
    write_file("$dir/records.json", JSON::PP->new->canonical->pretty->encode({ schema => 1, profiles => \@profiles,
        endpoints => \@endpoints, bindings => \@bindings }));
    return 0;
}

# The port that a fault proxy announced in its first fault_ready event.
sub fault_port {
    my ($path) = @_;
    defined $path or die "usage: records.pl faultport FILE\n";
    -f $path or return 1;
    for my $line (split /\n/, read_file($path)) {
        next unless substr($line, 0, 1) eq '{';
        my $event = eval { JSON::PP->new->utf8->decode($line) };
        next unless ref $event eq 'HASH' && ($event->{event} // '') eq 'fault_ready' && $event->{port};
        print "$event->{port}\n";
        return 0;
    }
    return 1;
}

# The libraries that the loader initialized, as real paths (LD_DEBUG=libs prints "calling init:").
sub libraries {
    my ($path) = @_;
    return () unless -f $path;
    my %found;
    for my $line (split /\n/, read_file($path)) {
        next unless $line =~ /calling init:\s*(\S+)/;
        my $real = abs_path($1);
        $found{$real} = 1 if defined $real && -f $real;
    }
    return sort keys %found;
}

sub digests {
    my (@paths) = @_;
    my @existing = grep { -f $_ } @paths;
    return {} unless @existing;
    open(my $fh, '-|', 'sha256sum', '--', @existing) or die "cannot run sha256sum: $!\n";
    my %found;
    while (my $line = <$fh>) {
        $found{$2} = $1 if $line =~ /^([0-9a-f]{64})\s+(\S.*)$/;
    }
    close($fh);
    return \%found;
}

# The JSON objects of the record test program: each line that begins with { is one event.
sub events {
    my ($text) = @_;
    my @found;
    for my $line (split /\n/, $text) {
        next unless substr($line, 0, 1) eq '{';
        my $event = JSON::PP->new->utf8->decode($line);
        ref $event eq 'HASH' or die "record event must be an object\n";
        push @found, $event;
    }
    return @found;
}

# True when at least one event of the kind has every named field true.
sub any_event {
    my ($observed, $kind, @fields) = @_;
    for my $event (@$observed) {
        next unless ($event->{event} // '') eq $kind;
        return 1 unless grep { !$event->{$_} } @fields;
    }
    return 0;
}

# How many events of the kind have the field true.
sub count_events {
    my ($observed, $kind, $field) = @_;
    return scalar(grep { ($_->{event} // '') eq $kind && $_->{$field} } @$observed);
}

# The checks that the former harness adds for a case after the checks that every case shares.
sub case_checks {
    my ($checks, $case, $observed) = @_;
    if ($case eq 'shutdown') {
        push @$checks, ['actual-blocked-shutdown', any_event($observed, 'blocked_shutdown', qw(waited_for_entered restart_rejected))];
    } elsif ($case eq 'queued-shutdown') {
        push @$checks, ['actual-queued-shutdown', any_event($observed, 'queued_shutdown',
            qw(storage_retained_until_cleanup abandoned_finalized_once restart_rejected))];
    } elsif ($case eq 'accounting') {
        my @windows = grep { ($_->{event} // '') eq 'accounting_window' } @$observed;
        my @seen = grep { required($_, 'observed') } @windows;
        my $window_ok = 1;
        for my $event (@seen) {
            next if required($event, 'peak_count') == 2 && required($event, 'peak_bytes') == 2 * required($event, 'single_bytes') &&
                required($event, 'retirement_pending') == 1 && required($event, 'queued') == 1;
            $window_ok = 0;
            last;
        }
        push @$checks, ['two-generation-window-observed', @seen && $window_ok];
        my $latest = @windows == 3 ? 1 : 0;
        for my $event ($latest ? @windows : ()) {
            next if required($event, 'generations') == 2 && required($event, 'sevr') eq 'NO_ALARM';
            $latest = 0;
            last;
        }
        push @$checks, ['two-generation-trials-latest-sent', $latest];
        my ($limit) = grep { ($_->{event} // '') eq 'accounting_limit' } @$observed;
        $limit ||= {};
        push @$checks, ['count-limit-rejects-reprocess-synchronously', ($limit->{limited_generations} // -1) == 1 &&
            ($limit->{limited_stat} // '') eq 'WRITE' && ($limit->{limited_sevr} // '') eq 'INVALID' && ($limit->{limited_pact} // -1) == 0];
        push @$checks, ['raised-limit-admits-explicit-request', ($limit->{explicit_sevr} // '') eq 'NO_ALARM' &&
            ($limit->{explicit_completions} // -1) == 1];
        push @$observed, { event => 'accounting_summary', trials => scalar(@windows), window_observed => scalar(@seen) };
    } elsif ($case eq 'edges') {
        push @$checks, ['actual-input-edges', any_event($observed, 'input_edges', qw(precision_rejected binary_exact))];
        my $order = 0;
        for my $event (@$observed) {
            next unless ($event->{event} // '') eq 'base_order';
            $order = 1 if ($event->{flnk_source_pact} // -1) == 1 && ($event->{target_completions} // -1) == 1;
        }
        push @$checks, ['actual-Base-FLNK-order', $order];
        push @$checks, ['actual-six-input-simulation', count_events($observed, 'simulation_bypass', 'terminal_released') == 6];
        push @$checks, ['actual-normal-mode-native-publication', count_events($observed, 'simulation_bypass', 'normal_native_published') == 4];
        my $output = 0;
        for my $event (@$observed) {
            next unless ($event->{event} // '') eq 'output_edges';
            $output = 1 if ($event->{rounding_modes} // -1) == 4 && ($event->{wire_bit_fixtures} // -1) == 36 && $event->{invalid_set_rejected};
        }
        push @$checks, ['actual-output-edges', $output];
        my $payload = 0;
        for my $event (@$observed) {
            next unless ($event->{event} // '') eq 'max_payload_queue';
            $payload = 1 if ($event->{data_bytes} // -1) == 32766 && $event->{rejected_without_admission} && $event->{retained_before_release};
        }
        push @$checks, ['actual-max-payload-queue', $payload];
    }
    return;
}

sub judge {
    my ($dir, $case) = @_;
    defined $case && -d $dir or die "usage: records.pl judge OUT CASE\n";
    grep { $_ eq $case } @CASES or die "unknown case $case\n";
    my $aborted = -e "$dir/aborted" ? 1 : 0;
    my @checks;
    my @tokens;
    for my $kind (qw(community auth privacy)) {
        my $path = "$dir/$AGENT-$kind";
        next unless -f $path;
        my $token = read_file($path);
        $token =~ s/\s+\z//;
        push @tokens, $token if length $token;
    }
    # The agent.
    if (-f "$dir/$AGENT.ready") {
        my $ready = read_file("$dir/$AGENT.ready") =~ /^1/ ? 1 : 0;
        push @checks, ["$AGENT:bind-ready", $ready];
        if ($ready) {
            my $state = decode_json(read_file("$dir/$AGENT.sockets.json"));
            my @listeners = @{ $state->{sockets} };
            my $port = (split ' ', read_file("$dir/$AGENT.port"))[0];
            push @checks, ["$AGENT:only-owned-loopback-listener", @listeners == 1 && $listeners[0]{table} eq 'udp' &&
                           $listeners[0]{local} eq sprintf('0100007F:%04X', $port)];
        }
    }
    # The fault proxies of the case, in the order of their start.
    my @proxies = @{ $PROXIES{$case} || [] };
    for my $name (@proxies) {
        push @checks, ["$name:ready", read_file("$dir/$name.ready") =~ /^1/ ? 1 : 0] if -f "$dir/$name.ready";
    }
    # The record test program.
    my @observed;
    my @libraries;
    my $judged = eval {
        -f "$dir/records.rc" or die "the record test program did not run\n";
        if ($WINDOWS{$case}) {
            # Without a receipt the hold did not run to its end: the check fails instead of being left out.
            my $held = -f "$dir/records.retired.json" ? decode_json(read_file("$dir/records.retired.json")) : {};
            push @checks, ['records:retired-hold', (!grep { !$held->{$_} } qw(attached held released detached)) && !$held->{error} &&
                           !$held->{cleanup_error} && ($held->{activation} // -1) == 1 &&
                           @{ $held->{windows_held} || [] } == $WINDOWS{$case} ? 1 : 0];
        }
        my ($code, $forced) = split ' ', read_file("$dir/records.rc");
        push @checks, ['records:return', $code == 0 && !$forced];
        @observed = events(read_file("$dir/records.stdout"));
        my ($summary) = grep { ($_->{event} // '') eq 'record_summary' } @observed;
        $summary ||= {};
        push @checks, ['actual-eleven-record-path', is_number($summary->{records}) && $summary->{records} == 11 &&
                       ($summary->{checks} // 0) > 100];
        my @completions = grep { ($_->{event} // '') eq 'record_completed' } @observed;
        my $identities = @completions ? 1 : 0;
        for my $event (@completions) {
            last unless $identities;
            $identities = 0 unless required($event, 'activation') == 1 &&
                required($event, 'revision') == 1 && required($event, 'handle') > 0;
        }
        push @checks, ['actual-record-identities', $identities];
        my @pressure = grep { ($_->{event} // '') eq 'callback_pressure' } @observed;
        push @checks, ['actual-callback-pressure', scalar(grep { ($_->{enqueue_failures} // 0) > 0 && $_->{completed_once} } @pressure) ? 1 : 0];
        my %records = map { (defined $_->{record} ? $_->{record} : "\0absent") => 1 } @pressure;
        push @checks, ['actual-GET-and-SET-pressure', join(',', sort keys %records) eq 'Records_Ao,Records_Longin'];
        case_checks(\@checks, $case, \@observed);
        @libraries = libraries("$dir/records.stderr");
        push @checks, ['IOC-native-free', @libraries && !grep { index($_, 'netsnmp') >= 0 } @libraries];
        write_file("$dir/record-observations.json", JSON::PP->new->canonical->pretty->encode(\@observed));
        1;
    };
    my $judge_error = $@;
    unless ($judged) {
        $aborted = 1;
        write_file("$dir/abort.json", JSON::PP->new->canonical->pretty->encode({ message => "$judge_error" }));
    }
    # The stop of the proxies, last started first, then of the agent, and the scan of the records.
    for my $name (reverse(@proxies), $AGENT) {
        next unless -f "$dir/$name.stop";
        my ($code, $forced) = split ' ', read_file("$dir/$name.stop");
        push @checks, ["$name:normal-stop", $code == 0 && !$forced];
    }
    my (@leaks, @diagnostics);
    opendir(my $dh, $dir) or die "cannot read $dir: $!\n";
    for my $name (sort readdir $dh) {
        next unless $name =~ /\.(?:stdout|stderr|json)\z/ && $name ne 'results.json';
        my $text = read_file("$dir/$name");
        push @leaks, $name if grep { index($text, $_) >= 0 } @tokens;
        push @diagnostics, $name if $name =~ /\.stderr\z/ &&
            grep { index($text, $_) >= 0 } ('ERROR: AddressSanitizer', 'runtime error:', 'AddressSanitizer:DEADLYSIGNAL');
    }
    closedir($dh);
    push @checks, ['secret-sentinels-absent', !@leaks], ['sanitizer-diagnostics-absent', !@diagnostics];
    my $products = -f "$dir/products.txt" ? read_file("$dir/products.txt") : "$ROOT/bin/linux-x86_64";
    chomp $products;
    my @inputs = (sort(glob("$ROOT/snmp3App/src/*.cpp")), sort(glob("$ROOT/snmp3App/src/*.h")),
        map({ "$ROOT/tests/rewrite/$_" } qw(test_records.bash records.pl RecordTest.cpp NativeAgent.cpp db/records.db db/record-output.db)),
        "$ROOT/dbd/snmp3RecordTest.dbd", map { "$products/$_" } qw(snmp3RecordTest snmp3NativeProbe snmp3Worker snmp3NativeAgent));
    push @inputs, map { "$ROOT/tests/rewrite/db/$_" } qw(record-edges.db record-order.db) if $case eq 'edges';
    push @inputs, map { "$ROOT/tests/rewrite/helpers/$_" } qw(retired_hold.py runtime_thread_limit.py) if $WINDOWS{$case};
    my $failed = grep { !$_->[1] } @checks;
    my $passed = !$failed && !$aborted && @checks ? 1 : 0;
    my %result = (passed => $passed ? JSON::PP::true : JSON::PP::false, aborted => $aborted ? JSON::PP::true : JSON::PP::false,
        checks => [map { { name => $_->[0], passed => $_->[1] ? JSON::PP::true : JSON::PP::false } } @checks],
        inputs => digests(@inputs), loaded_libraries => digests(@libraries, map { libraries("$dir/$_.stderr") } @proxies, $AGENT),
        scope => "Record case $case; qualifies this invocation only and does not close the full T1-T14 matrix");
    write_file("$dir/results.json", JSON::PP->new->canonical->pretty->encode(\%result));
    printf "%s %s\n", $_->[1] ? 'PASS' : 'FAIL', $_->[0] for @checks;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $dir;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @arguments) = @_;
    defined $command or die "usage: records.pl port|sockets|config|faultport|judge ...\n";
    return free_port() if $command eq 'port';
    return sockets(@arguments) if $command eq 'sockets';
    return config(@arguments) if $command eq 'config';
    return fault_port(@arguments) if $command eq 'faultport';
    return judge(@arguments) if $command eq 'judge';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "records.pl: $@";
    exit 2;
}
exit $status;
