#!/usr/bin/env perl
# Judge and helper of the record startup cell. test_record_startup.bash runs each case in a directory of its own (CASEDIR) and keeps
# what the fixture agent, the fault proxy, the private repeater and the IOC produced there; this script writes the configuration,
# waits at the load gate, and judges.
#   record_startup.pl port                         print a free UDP port of the loopback address
#   record_startup.pl sockets PID PORT OUT         write the sockets that the process holds as JSON to OUT
#   record_startup.pl owns PID PORT                exit 0 when the process owns a UDP socket bound to the port
#   record_startup.pl config CASEDIR PROXYPORT     write CASEDIR/records.json for the agent and the proxy
#   record_startup.pl gate PID FIFO WORKER         remove the worker link while the IOC waits at the gate FIFO (exit 1: not reached)
#   record_startup.pl judge CASEDIR CASE           judge a case, write CASEDIR/results.json, print one line for each check and the
#                                                  verdict; exit 0 (every check passed) or 1
#   record_startup.pl summary OUTPUT CASE...       write OUTPUT/results.json for the cases that ran; exit 0 (all passed) or 1
# Exit status 2: the observations could not be read.
# The checks and their order are those of the former Python harness: the agent, the proxy and the stop of the children, the scan,
# the lines of the IOC (shutdown order, script policy, diagnostics, failure or GET and SET), the checks of the phase events of the
# startup test program, the receipts of the children and the libraries of the IOC.
# Language level: Perl 5.26.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use IO::Socket::INET;
use JSON::PP;
use Fcntl;
use POSIX qw(ENXIO);
use Scalar::Util qw(looks_like_number);

my $ROOT = abs_path(dirname(__FILE__) . '/../..');
my @CASES = qw(normal production-break production-continue failure-break failure-continue thread-normal thread-break
               thread-continue production-thread-break production-thread-continue);
my @INITIAL_VALUES = (71, 19.5, 81, 29.5);
my @PHASE_FIELDS = qw(at_us before_free restart admission activation created exited joined inventory active pending queued entered
                      completions entry_open detach_allowed drain_failed);
my @CONTEXT_FIELDS = qw(record_null attached handle generation admission published native_success terminal);
my $CLONE3 = 435;
my $CLONE_THREAD = 0x10000;

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

sub has {
    my ($text, $token) = @_;
    return index($text, $token) >= 0 ? 1 : 0;
}

sub count {
    my ($text, $token) = @_;
    my ($total, $at) = (0, 0);
    while (($at = index($text, $token, $at)) >= 0) {
        ++$total;
        $at += length($token);
    }
    return $total;
}

sub is_int {
    my ($value) = @_;
    return defined $value && !ref $value && $value =~ /^-?\d+\z/ ? 1 : 0;
}

sub is_number {
    my ($value) = @_;
    return defined $value && !ref $value && looks_like_number($value) ? 1 : 0;
}

sub is_true {
    my ($value) = @_;
    return defined $value && JSON::PP::is_bool($value) && $value ? 1 : 0;
}

sub is_false {
    my ($value) = @_;
    return defined $value && JSON::PP::is_bool($value) && !$value ? 1 : 0;
}

sub ints {
    my ($item, @names) = @_;
    return 0 unless ref $item eq 'HASH';
    return (!grep { !is_int($item->{$_}) } @names) ? 1 : 0;
}

# The JSON objects of a text: each line that contains {"event": is read from that position.
sub events {
    my ($text) = @_;
    my @found;
    for my $line (split /\n/, $text) {
        my $at = index($line, '{"event":');
        next if $at < 0;
        push @found, JSON::PP->new->utf8->decode(substr($line, $at));
    }
    return @found;
}

sub attributes {
    my ($case) = @_;
    my %a;
    $a{failure} = ($case eq 'normal' || $case eq 'thread-normal') ? 0 : 1;
    $a{traced} = $case =~ /thread/ ? 1 : 0;
    $a{phase} = $a{traced} && $a{failure} ? 'thread' : ($a{failure} ? 'preflight' : 'normal');
    $a{production} = $case =~ /^production/ ? 1 : 0;
    $a{policy} = $case =~ /continue\z/ ? 'continue' : 'break';
    $a{executable} = $a{production} ? 'snmp3Ioc' : 'snmp3StartupTest';
    return \%a;
}

sub free_port {
    my $socket = IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1', LocalPort => 0)
        or die "cannot bind a UDP socket: $!\n";
    print $socket->sockport(), "\n";
    close($socket);
    return 0;
}

# True (exit status 0) when the process holds a socket that is bound to the UDP port.
sub owns {
    my ($pid, $port) = @_;
    $pid =~ /^\d+$/ && defined $port && $port =~ /^\d+$/ or die "usage: record_startup.pl owns PID PORT\n";
    my %sockets;
    opendir(my $dh, "/proc/$pid/fd") or return 1;
    for my $name (readdir $dh) {
        my $target = readlink("/proc/$pid/fd/$name");
        $sockets{$1} = 1 if defined $target && $target =~ /^socket:\[(\d+)\]\z/;
    }
    closedir($dh);
    for my $table (qw(udp udp6)) {
        open(my $fh, '<', "/proc/$pid/net/$table") or next;
        my $header = <$fh>;
        while (my $line = <$fh>) {
            my @columns = split ' ', $line;
            next unless @columns > 9;
            my (undef, $local) = split /:/, $columns[1];
            if (hex($local) == $port && $sockets{ $columns[9] }) {
                close($fh);
                return 0;
            }
        }
        close($fh);
    }
    return 1;
}

sub sockets {
    my ($pid, $port, $path) = @_;
    defined $path && $pid =~ /^\d+$/ or die "usage: record_startup.pl sockets PID PORT OUT\n";
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
    my ($dir, $proxy) = @_;
    defined $proxy && $proxy =~ /^\d+$/ or die "usage: record_startup.pl config CASEDIR PROXYPORT\n";
    my @bindings;
    for my $entry (['StartupRead', 'get', 'integer', 1], ['StartupWrite', 'set', 'opaqueFloat', 9]) {
        push @bindings, { id => $entry->[0], endpoint => 'Local', operation => $entry->[1], valueType => $entry->[2], capacity => 1,
                          oid => "1.3.6.1.4.1.53864.4.$entry->[3].0" };
    }
    write_file("$dir/records.json", JSON::PP->new->canonical->pretty->encode({ schema => 1,
        profiles => [{ id => 'Local', version => '2c', communityFile => "$dir/agent-ipv4-1-community", timeoutMs => 1000, retries => 0 }],
        endpoints => [{ id => 'Local', address => '127.0.0.1', port => $proxy + 0, profile => 'Local' }], bindings => \@bindings }));
    return 0;
}

# True when the process is gone: it has no entry in /proc or it is a zombie that nobody has waited for yet.
sub gone {
    my ($pid) = @_;
    return 1 unless -e "/proc/$pid/stat";
    my $stat = read_file("/proc/$pid/stat");
    return $stat =~ /^\d+ \(.*\) (\S)/s && $1 eq 'Z' ? 1 : 0;
}

sub gate {
    my ($pid, $fifo, $worker) = @_;
    defined $worker or die "usage: record_startup.pl gate PID FIFO WORKER\n";
    my $end = time() + 30;
    my $opened;
    while (time() < $end && !gone($pid)) {
        if (sysopen(my $handle, $fifo, O_WRONLY | O_NONBLOCK)) {
            $opened = $handle;
            last;
        }
        $! == ENXIO or die "cannot open $fifo: $!\n";
        select(undef, undef, undef, 0.01);
    }
    return 1 unless defined $opened;
    unlink($worker) or die "cannot remove $worker: $!\n";
    close($opened);
    return 0;
}

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

sub phase_schema {
    my ($item) = @_;
    return 0 unless ints($item, @PHASE_FIELDS) && defined $item->{state} && !ref $item->{state};
    return 0 unless ref $item->{records} eq 'ARRAY' && @{ $item->{records} } == 4;
    for my $record (@{ $item->{records} }) {
        return 0 unless ints($record, qw(dpvt_null attached pact stat sevr)) && is_number($record->{value});
    }
    return 0 unless ref $item->{contexts} eq 'ARRAY' && (@{ $item->{contexts} } == 0 || @{ $item->{contexts} } == 2);
    return 0 if grep { !ints($_, @CONTEXT_FIELDS) } @{ $item->{contexts} };
    return 0 unless ref $item->{reservations} eq 'ARRAY' && @{ $item->{reservations} } == 1;
    return 0 if grep { !ints($_, qw(count bytes undelivered retirement_pending)) } @{ $item->{reservations} };
    return 1;
}

sub phase_inventory {
    my ($observed, $failure) = @_;
    my @expected = ('initialized', 'started', $failure ? ('attempted', 'stop1', 'stop2') : ('completed'),
                    'AtShutdown', 'AfterCloseLinks', 'AfterStopCallback', 'AfterShutdown');
    push @expected, 'restart' if $failure;
    my @phases = grep { ($_->{event} // '') eq 'startup_phase' } @$observed;
    my @attempts = grep { ($_->{event} // '') eq 'startup_process' } @$observed;
    return 0 unless join("\0", map { $_->{phase} // '' } @phases) eq join("\0", @expected);
    return 0 if grep { !phase_schema($_) } @phases;
    for my $index (1 .. $#phases) {
        return 0 unless $phases[$index - 1]{at_us} < $phases[$index]{at_us};
    }
    return 0 unless join(',', map { $_->{slot} // '' } @attempts) eq '0,1';
    return 0 if grep { !ints($_, 'status') } @attempts;
    return 0 if grep { ($_->{event} // '') eq 'startup_observer_error' } @$observed;
    return 1;
}

sub values_of {
    my ($records) = @_;
    return [map { $_->{value} } @$records];
}

sub same_values {
    my ($left, $right) = @_;
    return 0 unless @$left == @$right;
    for my $index (0 .. $#$left) {
        return 0 unless $left->[$index] == $right->[$index];
    }
    return 1;
}

sub companion_checks {
    my ($checks, $observed, $failure, $thread_failure) = @_;
    my $activation = $thread_failure ? 1 : 0;
    my $valid = phase_inventory($observed, $failure);
    push @$checks, ['startup-phase-inventory', $valid];
    return unless $valid;
    my %phases = map { $_->{phase} => $_ } grep { ($_->{event} // '') eq 'startup_phase' } @$observed;
    my ($initial, $started, $before) = @phases{qw(initialized started AtShutdown)};
    my @detached = map { $phases{$_} } qw(AfterCloseLinks AfterStopCallback AfterShutdown);
    push @detached, $phases{restart} if $failure;
    my @all = values %phases;
    push @$checks, ['startup-two-contexts-before-runtime', $initial->{inventory} == 2 && @{ $initial->{contexts} } == 2 &&
        !grep({ $_->{attached} != 1 || $_->{pact} != 0 } @{ $initial->{records} }[0, 1]) &&
        !grep({ $_->{attached} != 1 || $_->{handle} <= 0 } @{ $initial->{contexts} }) &&
        scalar(keys %{ { map { $_->{handle} => 1 } @{ $initial->{contexts} } } }) == 2 && $initial->{state} eq 'Cold' &&
        !grep({ $initial->{$_} != 0 } qw(active completions admission created))];
    push @$checks, ['startup-rejected-records-unbound', !grep { my $e = $_; grep { $_->{dpvt_null} != 1 } @{ $e->{records} }[2, 3] } @all];
    push @$checks, ['startup-initial-values', same_values(values_of($initial->{records}), \@INITIAL_VALUES)];
    push @$checks, ['startup-no-isolated-cleanup', !grep { $_->{before_free} != 0 } @all];
    push @$checks, ['startup-entry-closed-before-detach', $before->{entry_open} == 0 && $before->{entered} == 0 &&
        $before->{detach_allowed} == 1 && !grep({ $_->{attached} != 1 } @{ $before->{records} }[0, 1])];
    push @$checks, ['startup-record-pointers-cleared', !grep { my $e = $_; grep { $_->{dpvt_null} != 1 } @{ $e->{records} }[0, 1] } @detached];
    push @$checks, ['startup-context-pointers-cleared', !grep { my $e = $_; @{ $e->{contexts} } != 2 ||
        grep { $_->{record_null} != 1 } @{ $e->{contexts} } } @detached];
    push @$checks, ['startup-storage-retained-after-join', $phases{AfterStopCallback}{inventory} == 2];
    push @$checks, ['startup-storage-retained-after-shutdown', $phases{AfterShutdown}{inventory} == 2];
    push @$checks, ['startup-context-inventory-preserved', !grep { $_->{inventory} != 2 } @all];
    my @handles = map { $_->{handle} } @{ $initial->{contexts} };
    push @$checks, ['startup-handles-preserved', !grep { my $e = $_; @{ $e->{contexts} } != 2 ||
        join(',', map { $_->{handle} } @{ $e->{contexts} }) ne join(',', @handles) } @all];
    push @$checks, ['startup-rejected-values-preserved', !grep { !same_values(values_of([@{ $_->{records} }[2, 3]]), [@INITIAL_VALUES[2, 3]]) } @all];
    if ($failure) {
        my @failed = map { $phases{$_} } grep { $_ ne 'initialized' } keys %phases;
        push @$checks, [$thread_failure ? 'startup-thread-failed-before-processing' : 'startup-preflight-failed-before-processing',
            $started->{state} eq 'Failed' && $started->{activation} == $activation && $started->{entry_open} == $activation &&
            !grep({ $started->{$_} != 0 } qw(admission created active completions))];
        push @$checks, ['startup-failure-entry-sequence', $phases{attempted}{entry_open} == $activation &&
            !grep({ $phases{$_}{entry_open} != 0 } qw(stop1 stop2 AtShutdown AfterCloseLinks AfterStopCallback AfterShutdown restart))];
        push @$checks, ['startup-failed-state-preserved', !grep { $_->{state} ne 'Failed' || $_->{admission} != 0 } @failed];
        push @$checks, ['startup-no-runtime-thread', !grep({ my $e = $_; grep { $e->{$_} != 0 } qw(created exited joined) } @all) &&
            $initial->{activation} == 0 && !grep({ $_->{activation} != $activation } @failed)];
        my $attempted = $phases{attempted};
        push @$checks, ['startup-admission-refused-with-alarm', join(',', map { $_->{stat} } @{ $attempted->{records} }[0, 1]) eq '1,2' &&
            !grep({ $_->{sevr} != 3 || $_->{pact} != 0 } @{ $attempted->{records} }[0, 1]) && @{ $attempted->{contexts} } == 2 &&
            !grep({ $_->{generation} != 0 || $_->{admission} != 0 } @{ $attempted->{contexts} })];
        push @$checks, ['startup-no-accepted-work', !grep({ my $e = $_; grep { $e->{$_} != 0 } qw(active pending queued entered completions drain_failed) } @all) &&
            !grep({ my $e = $_; grep { my $q = $_; grep { $q->{$_} != 0 } qw(count bytes undelivered retirement_pending) } @{ $e->{reservations} } } @all)];
        push @$checks, ['startup-failed-values-preserved', !grep { !same_values(values_of($_->{records}), \@INITIAL_VALUES) } @all];
        push @$checks, ['startup-no-native-publication', !grep({ my $e = $_; @{ $e->{contexts} } != 2 ||
            grep { my $c = $_; grep { $c->{$_} != 0 } qw(published native_success terminal generation admission) } @{ $e->{contexts} } } @all)];
        my ($final, $restart) = @phases{qw(AfterShutdown restart)};
        my $stable = 1;
        for my $key (@PHASE_FIELDS[3 .. $#PHASE_FIELDS], qw(state records contexts reservations)) {
            $stable = 0 unless JSON::PP->new->canonical->encode($restart->{$key}) eq JSON::PP->new->canonical->encode($final->{$key});
        }
        push @$checks, ['startup-restart-refused', $restart->{restart} == 0 && $stable];
    } else {
        my $complete = $phases{completed};
        push @$checks, ['startup-valid-records-complete', $started->{state} eq 'Running' && $complete->{completions} == 2 &&
            $complete->{active} == 0 && $complete->{entered} == 0 && @{ $complete->{contexts} } == 2 &&
            !grep({ $_->{generation} != 1 } @{ $complete->{contexts} }) &&
            join(',', map { $_->{native_success} } @{ $complete->{contexts} }) eq '1,0' &&
            join(',', map { $_->{published} } @{ $complete->{contexts} }) eq '1,0' &&
            !grep({ $_->{pact} != 0 || $_->{sevr} != 0 } @{ $complete->{records} }[0, 1]) &&
            same_values(values_of([@{ $complete->{records} }[0, 1]]), [-123, 19.5])];
        push @$checks, ['startup-normal-stopped', !grep { $_->{state} ne 'Stopped' || $_->{completions} != 2 || $_->{active} != 0 ||
            $_->{entry_open} != 0 || $_->{drain_failed} != 0 || $_->{created} != 1 || $_->{exited} != 1 || $_->{joined} != 1 }
            ($before, @detached)];
    }
    return;
}

sub final_report {
    my ($text, $kind) = @_;
    my @tail = split /snmp3 lifecycle: fallback\n/, $text, -1;
    @tail == 2 or die "missing or duplicate fallback\n";
    my @lines = $tail[1] =~ /snmp3 \Q$kind\E: ([^\n]+)/g;
    @lines == 1 or die "missing or duplicate final report\n";
    my %report;
    for my $token (split ' ', $lines[0]) {
        my ($key, $value) = split /=/, $token, 2;
        $report{$key} = $key eq 'state' ? $value : $value + 0;
    }
    return \%report;
}

sub trace_checks {
    my ($checks, $evidence, $failure) = @_;
    my @calls = ref $evidence->{syscalls} eq 'ARRAY' ? @{ $evidence->{syscalls} } : ();
    my @limited = grep { $_->{limited} } @calls;
    my $limits = $evidence->{parent_limit_before};
    push @$checks, ['startup-trace-complete', !defined $evidence->{error} && !@{ $evidence->{cleanup_errors} || [] } &&
        is_true($evidence->{child_reaped}) && is_false($evidence->{forced_cleanup}) && ($evidence->{marker_count} // -1) == 1 &&
        ($evidence->{abi} // '') eq 'x86_64'];
    push @$checks, ['startup-parent-limit-preserved', JSON::PP->new->canonical->encode($evidence->{parent_limit_before}) eq
        JSON::PP->new->canonical->encode($evidence->{parent_limit_after}) && ref $limits eq 'ARRAY' && @$limits == 2];
    unless ($failure) {
        push @$checks, ['startup-traced-normal-unrestricted', !@limited];
        return;
    }
    my $valid = @limited ? 1 : 0;
    for my $e (@limited) {
        my $original = $e->{original_limit};
        $valid = 0 unless ints($e, qw(flags return_value syscall at_ns exited_ns restored_ns)) && is_true($e->{thread}) &&
            ($e->{flags} & $CLONE_THREAD) && is_true($e->{after_starting}) && ref $original eq 'ARRAY' && @$original == 2 &&
            JSON::PP->new->canonical->encode($e->{restored_limit}) eq JSON::PP->new->canonical->encode($original) &&
            JSON::PP->new->canonical->encode($e->{restricted_limit}) eq JSON::PP->new->canonical->encode([0, $original->[1]]) &&
            $e->{at_ns} < $e->{exited_ns} && $e->{exited_ns} <= $e->{restored_ns};
    }
    push @$checks, ['startup-thread-limit-restored', $valid];
    push @$checks, ['startup-real-kernel-thread-refusal', $valid && $limited[-1]{return_value} == -11 &&
        !grep({ $_->{syscall} != $CLONE3 || $_->{return_value} != -38 } @limited[0 .. $#limited - 1])];
    return;
}

sub judge {
    my ($dir, $case) = @_;
    defined $case && -d $dir or die "usage: record_startup.pl judge CASEDIR CASE\n";
    grep { $_ eq $case } @CASES or die "unknown case $case\n";
    my $attr = attributes($case);
    my $aborted = -e "$dir/aborted" ? 1 : 0;
    my @checks;
    my %child_ok;
    # The agent and the proxy.
    my @tokens;
    for my $kind (qw(community auth privacy)) {
        my $path = "$dir/agent-ipv4-1-$kind";
        next unless -f $path;
        my $token = read_file($path);
        $token =~ s/\s+\z//;
        push @tokens, $token if length $token;
    }
    if (-f "$dir/agent-ipv4-1.ready") {
        my $ready = read_file("$dir/agent-ipv4-1.ready") =~ /^1/ ? 1 : 0;
        push @checks, ['agent-ipv4-1:bind-ready', $ready];
        if ($ready) {
            my $state = decode_json(read_file("$dir/agent-ipv4-1.sockets.json"));
            my @listeners = @{ $state->{sockets} };
            my $port = (split ' ', read_file("$dir/agent-ipv4-1.port"))[0];
            push @checks, ['agent-ipv4-1:only-owned-loopback-listener', @listeners == 1 && $listeners[0]{table} eq 'udp' &&
                           $listeners[0]{local} eq sprintf('0100007F:%04X', $port)];
        }
    }
    if (-f "$dir/startup-proxy.ready") {
        push @checks, ['startup-proxy:ready', (read_file("$dir/startup-proxy.ready") =~ /^(\d+)/ ? $1 : 0) > 0];
    }
    # The IOC.
    my $evidence;
    my $ioc_ok = 0;
    if ($attr->{traced}) {
        if (-f "$dir/thread-trace.json") {
            $evidence = decode_json(read_file("$dir/thread-trace.json"));
            $ioc_ok = ($evidence->{returncode} // -1) == ($attr->{failure} ? 1 : 0) && is_true($evidence->{child_reaped}) &&
                is_false($evidence->{forced_cleanup}) && !defined $evidence->{error} && !@{ $evidence->{cleanup_errors} || [] } ? 1 : 0;
            push @checks, ['ioc:expected-exit', $ioc_ok];
            trace_checks(\@checks, $evidence, $attr->{failure});
        }
    } elsif (-f "$dir/ioc.rc") {
        my $code = read_file("$dir/ioc.rc") =~ /^(\d+)/ ? $1 : -1;
        $ioc_ok = $code == ($attr->{failure} ? 1 : 0) ? 1 : 0;
        push @checks, ['ioc:expected-exit', $ioc_ok];
    }
    # The children that the harness stops: the repeater first, then the proxy and the agent in the reverse order of their start.
    if (-f "$dir/repeater.stop") {
        my ($code, $forced) = split ' ', read_file("$dir/repeater.stop");
        $child_ok{repeater} = ($code == 0 || $code == 143) && !$forced ? 1 : 0;
        push @checks, ['repeater:expected-exit', $child_ok{repeater}];
    }
    for my $name ('startup-proxy', 'agent-ipv4-1') {
        next unless -f "$dir/$name.stop";
        my ($code, $forced) = split ' ', read_file("$dir/$name.stop");
        $child_ok{$name} = $code == 0 && !$forced ? 1 : 0;
        push @checks, ["$name:normal-stop", $child_ok{$name}];
    }
    # The scan of the records.
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
    # The lines and the events of the IOC.
    my @observed;
    my $parsed = eval {
        my $text = read_file("$dir/ioc.stdout");
        my $stderr = read_file("$dir/ioc.stderr");
        @observed = events($text);
        my $final = final_report($text, 'runtime');
        my $requests = final_report($text, 'records');
        my @hooks = $text =~ /snmp3 hook: (AtShutdown|AfterStopScan|AfterStopCallback|AfterShutdown)/g;
        push @checks, ['startup-real-shutdown-order', join(',', @hooks) eq 'AtShutdown,AfterStopScan,AfterStopCallback,AfterShutdown'];
        push @checks, ['startup-script-policy', has($text, "STARTUP_SCRIPT_COMPLETED\n") == (($attr->{policy} eq 'continue' || !$attr->{failure}) ? 1 : 0)];
        push @checks, ['startup-rejection-diagnostics',
            count($stderr, 'record initialization rejected: Startup_RejectedInput') == 1 &&
            count($stderr, 'record initialization rejected: Startup_RejectedOutput') == 1 &&
            !has($stderr, 'record initialization rejected: Startup_Input') && !has($stderr, 'record initialization rejected: Startup_Output')];
        my @wire = events(read_file("$dir/startup-proxy.stdout"));
        my @wire_requests = grep { ($_->{event} // '') eq 'fault_request' } @wire;
        if ($attr->{failure}) {
            my $thread = $attr->{phase} eq 'thread';
            my $lines = $thread
                ? (count($text, "snmp3 lifecycle: starting activation=1\n") == 1 && count($text, "snmp3 lifecycle: create-failed activation=1\n") == 1 &&
                   !has($text, 'preflight-failed') && $text !~ /snmp3 lifecycle: (ready|created) /)
                : (count($text, 'preflight-failed activation=0') == 1);
            push @checks, [$thread ? 'startup-real-thread-failure' : 'startup-real-preflight-failure',
                $lines && count($stderr, 'runtime startup failed; admission closed') == 1 && count($stderr, 'snmp3Ioc: startup script failed') == 1];
            my %want = (state => 'Failed', admission => 0, activation => ($thread ? 1 : 0), created => 0, exited => 0, joined => 0);
            push @checks, ['startup-fallback-failed', join(',', map { "$_=$final->{$_}" } sort keys %$final) eq
                join(',', map { "$_=$want{$_}" } sort keys %want)];
            push @checks, ['startup-fallback-idle-retained', $requests->{contexts} == 2 && $requests->{detachAllowed} == 1 &&
                !grep({ $_ ne 'contexts' && $_ ne 'detachAllowed' && $requests->{$_} != 0 } keys %$requests)];
            if ($thread) {
                my $worker = final_report($text, 'worker');
                push @checks, ['startup-no-worker-or-wire', !grep({ $worker->{$_} != 0 } qw(launches reaps forced pid ready closing)) && !@wire_requests];
            } else {
                push @checks, ['startup-no-worker-or-wire', !has($text, 'snmp3 worker:') && !@wire_requests];
            }
        } else {
            my @agent = events(read_file("$dir/agent-ipv4-1.stdout"));
            push @checks, ['startup-real-GET-SET', join(',', sort { $a <=> $b } map { $_->{command} } @wire_requests) eq '160,163' &&
                scalar(grep { ($_->{event} // '') eq 'fault_response' } @wire) == 2 &&
                scalar(grep { ($_->{event} // '') eq 'agent_set_value' && ($_->{index} // -1) == 9 && ($_->{value} // '') eq '19.5' } @agent) > 0];
            my $worker = final_report($text, 'worker');
            push @checks, ['startup-worker-reaped', $worker->{launches} == 1 && $worker->{reaps} == 1 && $worker->{forced} == 0 &&
                $worker->{pid} == 0 && $worker->{ready} == 0 && $worker->{closing} == 0];
        }
        companion_checks(\@checks, \@observed, $attr->{failure}, $attr->{phase} eq 'thread') unless $attr->{production};
        1;
    };
    my $parse_error = $@;
    unless ($parsed) {
        push @checks, ['startup-observations-complete', 0];
        write_file("$dir/parse-error.json", JSON::PP->new->canonical->pretty->encode({ message => "$parse_error" }));
    }
    my @libraries = libraries("$dir/ioc.stderr");
    my $clean = ($child_ok{repeater} && $child_ok{'startup-proxy'} && $child_ok{'agent-ipv4-1'} && $ioc_ok) ? 1 : 0;
    push @checks, ['startup-all-child-receipts-clean', $clean];
    push @checks, ['startup-IOC-native-free', @libraries && !grep { index($_, 'netsnmp') >= 0 }  @libraries];
    my $products = -f "$dir/products.txt" ? read_file("$dir/products.txt") : "$ROOT/bin/linux-x86_64";
    chomp $products;
    my @inputs = (sort(glob("$ROOT/snmp3App/src/*.[ch]*")), map({ "$ROOT/tests/rewrite/$_" } qw(StartupTest.cpp db/record-startup.db
        snmp3StartupTestRegistrar.dbd Makefile NativeAgent.cpp helpers/udp_fault.py helpers/runtime_thread_limit.py run_independence.py
        test_record_startup.bash record_startup.pl)), "$ROOT/dbd/$attr->{executable}.dbd",
        map { "$products/$_" } ($attr->{executable}, qw(snmp3Worker snmp3NativeAgent snmp3NativeProbe)));
    my $failed = grep { !$_->[1] } @checks;
    my $passed = !$failed && !$aborted ? 1 : 0;
    my %result = (case => $case, passed => $passed ? JSON::PP::true : JSON::PP::false, aborted => $aborted ? JSON::PP::true : JSON::PP::false,
        checks => [map { { name => $_->[0], passed => $_->[1] ? JSON::PP::true : JSON::PP::false } } @checks],
        observations => \@observed, cleanup_passed => $clean ? JSON::PP::true : JSON::PP::false,
        ioc_libraries => digests(@libraries), inputs => digests(@inputs), phase => $attr->{phase},
        traced => $attr->{traced} ? JSON::PP::true : JSON::PP::false,
        scope => 'Actual ai/ao initialization and Runtime startup; real OS-limit thread failure when selected; no isolated cleanup');
    write_file("$dir/results.json", JSON::PP->new->canonical->pretty->encode(\%result));
    printf "%s %s\n", $_->[1] ? 'PASS' : 'FAIL', $_->[0] for @checks;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $dir;
    return $passed ? 0 : 1;
}

sub summary {
    my ($out, @cases) = @_;
    @cases or die "usage: record_startup.pl summary OUTPUT CASE...\n";
    my (@entries, $passed);
    $passed = 1;
    for my $case (@cases) {
        my $result = decode_json(read_file("$out/$case/results.json"));
        $passed = 0 unless $result->{passed};
        push @entries, { case => $case, passed => $result->{passed}, result => "$out/$case/results.json" };
    }
    write_file("$out/results.json", JSON::PP->new->canonical->pretty->encode({ passed => $passed ? JSON::PP::true : JSON::PP::false,
        cases => \@entries }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $out;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @arguments) = @_;
    defined $command or die "usage: record_startup.pl port|sockets|config|gate|judge|summary ...\n";
    return free_port() if $command eq 'port';
    return sockets(@arguments) if $command eq 'sockets';
    return owns(@arguments) if $command eq 'owns';
    return config(@arguments) if $command eq 'config';
    return gate(@arguments) if $command eq 'gate';
    return judge(@arguments) if $command eq 'judge';
    return summary(@arguments) if $command eq 'summary';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "record_startup.pl: $@";
    exit 2;
}
exit $status;
