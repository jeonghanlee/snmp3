#!/usr/bin/env perl
# Judge and helper of the record shutdown cell. test_record_shutdown.bash runs each case in a directory of its own (CASEDIR) and
# keeps what the fixture agent, the fault proxy, the private repeater, the IOC (and its output when the exit is requested, before-exit.stdout) and the Channel Access clients produced there; this
# script writes the configuration, reads the worker of the IOC and judges.
#   record_shutdown.pl port                         print a free UDP port of the loopback address
#   record_shutdown.pl now                          print the monotonic clock in nanoseconds
#   record_shutdown.pl sockets PID PORT OUT         write the sockets that the process holds as JSON to OUT
#   record_shutdown.pl owns PID PORT                exit 0 when the process owns a UDP socket bound to the port
#   record_shutdown.pl config CASEDIR PROXYPORT     write CASEDIR/records.json for the agent and the proxy
#   record_shutdown.pl workers IOCPID PRODUCTS OUT  write the worker processes of the IOC (pid and start time) as JSON to OUT
#   record_shutdown.pl judge CASEDIR CASE           judge a case, write CASEDIR/results.json, print one line for each check and the
#                                                   verdict; exit 0 (every check passed) or 1
#   record_shutdown.pl summary OUTPUT CASE...       write OUTPUT/results.json for the cases that ran; exit 0 (all passed) or 1
# Exit status 2: the observations could not be read.
# The checks and their order are those of the former Python harness.
# Language level: Perl 5.26.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use IO::Socket::INET;
use JSON::PP;
use Time::HiRes qw(clock_gettime CLOCK_MONOTONIC);
use Scalar::Util qw(looks_like_number);

my $ROOT = abs_path(dirname(__FILE__) . '/../..');
my @CASES = qw(inflight retained);
my @PHASES = qw(prepared queued AtShutdown AfterCloseLinks AfterStopCallback AfterShutdown restart);
my @REQUEST_FIELDS = qw(contexts active pending queued running inert entered enqueueFailures completions entryOpen drainFailed detachAllowed);
my @RUNTIME_FIELDS = qw(admission activation created exited joined);
my @CONTEXT_FIELDS = qw(record_null attached published native_success revision activation binding generation admission terminal sent
                        identity_match outcome);
my $NATIVE_TIMEOUT_MS = 30000;
my $DEADLINE_MS = 60000;

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

sub is_int {
    my ($value) = @_;
    return defined $value && !ref $value && $value =~ /^-?\d+\z/ ? 1 : 0;
}

sub is_number {
    my ($value) = @_;
    return defined $value && !ref $value && looks_like_number($value) ? 1 : 0;
}

sub ints {
    my ($item, @names) = @_;
    return 0 unless ref $item eq 'HASH';
    return (!grep { !is_int($item->{$_}) } @names) ? 1 : 0;
}

sub same {
    my ($left, $right) = @_;
    my $json = JSON::PP->new->canonical;
    return $json->encode($left) eq $json->encode($right) ? 1 : 0;
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

sub free_port {
    my $socket = IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1', LocalPort => 0)
        or die "cannot bind a UDP socket: $!\n";
    print $socket->sockport(), "\n";
    close($socket);
    return 0;
}

sub now {
    printf "%d\n", clock_gettime(CLOCK_MONOTONIC) * 1e9;
    return 0;
}

sub sockets {
    my ($pid, $port, $path) = @_;
    defined $path && $pid =~ /^\d+$/ or die "usage: record_shutdown.pl sockets PID PORT OUT\n";
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

sub owns {
    my ($pid, $port) = @_;
    $pid =~ /^\d+$/ && defined $port && $port =~ /^\d+$/ or die "usage: record_shutdown.pl owns PID PORT\n";
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

sub config {
    my ($dir, $proxy) = @_;
    defined $proxy && $proxy =~ /^\d+$/ or die "usage: record_shutdown.pl config CASEDIR PROXYPORT\n";
    my @bindings;
    for my $entry (['ShutdownRead', 'get', 'integer', 1], ['ShutdownWrite', 'set', 'opaqueFloat', 9]) {
        push @bindings, { id => $entry->[0], endpoint => 'Local', operation => $entry->[1], valueType => $entry->[2], capacity => 1,
                          oid => "1.3.6.1.4.1.53864.4.$entry->[3].0" };
    }
    write_file("$dir/records.json", JSON::PP->new->canonical->pretty->encode({ schema => 1,
        profiles => [{ id => 'Local', version => '2c', communityFile => "$dir/agent-ipv4-1-community", timeoutMs => $NATIVE_TIMEOUT_MS + 0,
                       retries => 0 }],
        endpoints => [{ id => 'Local', address => '127.0.0.1', port => $proxy + 0, profile => 'Local' }], bindings => \@bindings }));
    return 0;
}

sub stat_start {
    my ($pid) = @_;
    my $stat = eval { read_file("/proc/$pid/stat") };
    return undef unless defined $stat;
    my ($after) = $stat =~ /\)\s(.*)\z/s;
    return (split ' ', $after)[19];
}

sub workers {
    my ($ioc, $products, $out) = @_;
    defined $out && $ioc =~ /^\d+$/ or die "usage: record_shutdown.pl workers IOCPID PRODUCTS OUT\n";
    my %children;
    if (opendir(my $dh, "/proc/$ioc/task")) {
        for my $task (readdir $dh) {
            next if $task =~ /^\./;
            my $text = eval { read_file("/proc/$ioc/task/$task/children") };
            next unless defined $text;
            $children{$_} = 1 for split ' ', $text;
        }
        closedir($dh);
    }
    my $worker = abs_path("$products/snmp3Worker");
    my @found;
    for my $pid (sort { $a <=> $b } keys %children) {
        my $exe = abs_path("/proc/$pid/exe");
        next unless defined $exe && defined $worker && $exe eq $worker;
        push @found, { pid => $pid + 0, start_time => stat_start($pid) };
    }
    write_file($out, JSON::PP->new->canonical->pretty->encode(\@found));
    return 0;
}

sub worker_gone {
    my ($worker) = @_;
    my $start = stat_start($worker->{pid});
    return 1 unless defined $start;
    return $start ne $worker->{start_time} ? 1 : 0;
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

sub report {
    my ($text) = @_;
    my %result;
    my %fields = (runtime => ['state', @RUNTIME_FIELDS], records => \@REQUEST_FIELDS,
                  worker => [qw(address epoch pid ready closing batch launches reaps forced)], loop => ['passes']);
    for my $kind (qw(runtime records worker loop)) {
        my @lines = $text =~ /snmp3 \Q$kind\E: ([^\n]+)/g;
        @lines == 1 or die "missing or duplicate report: $kind\n";
        my %pairs = map { split /=/, $_, 2 } split ' ', $lines[0];
        join(',', sort keys %pairs) eq join(',', sort @{ $fields{$kind} }) or die "incomplete report: $kind\n";
        $result{$kind} = { map { $_ => ($_ eq 'state' ? $pairs{$_} : $pairs{$_} + 0) } keys %pairs };
    }
    return \%result;
}

sub phase_schema {
    my ($event) = @_;
    return 0 unless ($event->{case} // '') eq 'retained' && defined $event->{phase} && !ref $event->{phase} &&
        ints($event, qw(at_us ready watchdog before_free restart queue_status queue_size queue_used result_batches retired_batches)) &&
        ints($event->{runtime}, @RUNTIME_FIELDS) && defined $event->{runtime}{state} && !ref $event->{runtime}{state} &&
        ints($event->{requests}, @REQUEST_FIELDS);
    my ($records, $contexts, $reservations) = @$event{qw(records contexts reservations)};
    return 0 unless ref $records eq 'ARRAY' && @$records == 3;
    for my $record (@$records) {
        return 0 unless ints($record, qw(dpvt_null attached pact)) && is_number($record->{value});
    }
    return 0 unless ref $contexts eq 'ARRAY' && (@$contexts == 0 || @$contexts == 2);
    return 0 if grep { !ints($_, @CONTEXT_FIELDS) } @$contexts;
    return 0 unless ref $reservations eq 'ARRAY' && @$reservations == 1;
    return 0 if grep { !ints($_, qw(address count bytes undelivered retirement_pending)) } @$reservations;
    return 1;
}

sub phase_inventory {
    my ($observed) = @_;
    my @phases = grep { ($_->{event} // '') eq 'shutdown_phase' } @$observed;
    return 0 unless join("\0", map { $_->{phase} // '' } @phases) eq join("\0", @PHASES);
    return 0 if grep { !phase_schema($_) } @phases;
    for my $index (1 .. $#phases) {
        return 0 unless $phases[$index - 1]{at_us} < $phases[$index]{at_us};
    }
    return 0 if grep { ($_->{event} // '') eq 'shutdown_observer_error' } @$observed;
    return 1;
}

sub values_of {
    my ($records, @indexes) = @_;
    return [map { $records->[$_]{value} } (@indexes ? @indexes : (0 .. $#$records))];
}

sub same_values {
    my ($left, $right) = @_;
    return 0 unless @$left == @$right;
    for my $index (0 .. $#$left) {
        return 0 unless $left->[$index] == $right->[$index];
    }
    return 1;
}

sub contexts_terminal {
    my ($e) = @_;
    return @{ $e->{contexts} } == 2 && !grep { $_->{terminal} != 1 || $_->{sent} != 1 || $_->{identity_match} != 1 || $_->{outcome} != 1 } @{ $e->{contexts} };
}

sub retained_checks {
    my ($checks, $observed) = @_;
    my $valid = phase_inventory($observed);
    push @$checks, ['retained-phase-inventory', $valid];
    return unless $valid;
    my %phases = map { $_->{phase} => $_ } grep { ($_->{event} // '') eq 'shutdown_phase' } @$observed;
    my ($prepared, $queued, $stop, $close, $joined, $final, $restart) = @phases{@PHASES};
    my @all = values %phases;
    push @$checks, ['retained-no-watchdog-or-isolated-free', !grep { $_->{watchdog} || $_->{before_free} } @all];
    push @$checks, ['retained-fixture-prepared', $prepared->{ready} == 1 && $prepared->{requests}{contexts} == 2 &&
        $prepared->{requests}{active} == 0 && $prepared->{requests}{completions} == 0 &&
        same_values(values_of($prepared->{records}), [71, 19.5, 0]) && !grep({ !$_->{attached} } @{ $prepared->{records} }[0, 1])];
    push @$checks, ['retained-native-retired-before-exit', $queued->{ready} == 1 && $queued->{result_batches} == 2 &&
        $queued->{retired_batches} == 2 && @{ $queued->{contexts} } == 2 &&
        !grep({ $_->{terminal} != 1 || $_->{sent} != 1 || $_->{identity_match} != 1 || $_->{outcome} != 1 } @{ $queued->{contexts} }) &&
        !grep({ $_->{count} != 2 || $_->{undelivered} != 2 || $_->{bytes} <= 0 || $_->{retirement_pending} != 0 } @{ $queued->{reservations} })];
    my %queued_want = (contexts => 2, active => 2, queued => 2, pending => 0, running => 0, entered => 0, completions => 0, entryOpen => 1);
    push @$checks, ['retained-two-queued-before-exit', !grep({ $queued->{requests}{$_} != $queued_want{$_} } keys %queued_want) &&
        !grep({ $_->{pact} != 1 } @{ $queued->{records} }[0, 1]) && $queued->{queue_used} == 2];
    push @$checks, ['retained-drain-expiry-recorded', $stop->{requests}{drainFailed} == 1];
    my %stop_want = (contexts => 2, active => 2, queued => 2, entryOpen => 0, entered => 0, detachAllowed => 1, completions => 0);
    push @$checks, ['retained-entry-closed-before-detach', !grep({ $stop->{requests}{$_} != $stop_want{$_} } keys %stop_want)];
    my @after = ($close, $joined, $final, $restart);
    push @$checks, ['retained-record-dpvt-detached', !grep { my $e = $_; grep { $_->{dpvt_null} != 1 } @{ $e->{records} }[0, 1] } @after];
    push @$checks, ['retained-context-record-detached', !grep { my $e = $_; @{ $e->{contexts} } != 2 || grep { $_->{record_null} != 1 } @{ $e->{contexts} } } @after];
    push @$checks, ['retained-contexts-after-callback-join', $joined->{requests}{contexts} == 2];
    push @$checks, ['retained-contexts-after-shutdown', $final->{requests}{contexts} == 2];
    push @$checks, ['retained-late-callbacks-inert', !grep { $_->{requests}{inert} != 2 } ($joined, $final, $restart)];
    push @$checks, ['retained-terminal-preserved', !grep { !contexts_terminal($_) } ($queued, $stop, $close, $joined, $final, $restart)];
    my %own_want = (contexts => 2, active => 2, completions => 0);
    push @$checks, ['retained-ownership-after-detach', !grep { my $e = $_; grep({ $e->{requests}{$_} != $own_want{$_} } keys %own_want) ||
        !same($e->{reservations}, $queued->{reservations}) } ($stop, $close, $joined, $final)];
    push @$checks, ['retained-no-record-publication', !grep { my $e = $_; !same_values(values_of($e->{records}), [71, 19.5, 0]) ||
        !same_values(values_of($queued->{records}), [71, 19.5, 0]) || @{ $e->{contexts} } != 2 ||
        grep({ $_->{published} || $_->{native_success} } @{ $e->{contexts} }) } ($queued, $stop, $close, $joined, $final)];
    my $identities = sub {
        my ($e) = @_;
        return [map { join(',', @$_{qw(revision activation binding generation admission)}) } @{ $e->{contexts} }];
    };
    my $queued_ids = $identities->($queued);
    push @$checks, ['retained-generation-identities', @$queued_ids == 2 && keys(%{ { map { $_ => 1 } @$queued_ids } }) == 2 &&
        !grep({ my $c = $_; grep { $_ <= 0 } @$c{qw(revision activation binding generation admission)} } @{ $queued->{contexts} }) &&
        !grep({ !same($identities->($_), $queued_ids) } ($stop, $close, $joined, $final))];
    push @$checks, ['retained-joined-consumer-with-live-queue', !grep { my $e = $_; $e->{queue_status} != 0 || $e->{queue_size} < 3 ||
        $e->{queue_used} != 0 || grep({ $e->{requests}{$_} != 0 } qw(pending queued running entered)) } ($joined, $final)];
    push @$checks, ['retained-incomplete-stopped', !grep { my $e = $_; $e->{runtime}{state} ne 'IncompleteStopped' || $e->{runtime}{admission} != 0 ||
        $e->{runtime}{created} != 1 || $e->{runtime}{exited} != 1 || $e->{runtime}{joined} != 1 } ($stop, $final)];
    push @$checks, ['retained-restart-rejected-without-activation', $restart->{restart} == 0 && same($restart->{runtime}, $final->{runtime}) &&
        same($restart->{requests}, $final->{requests}) && same($restart->{reservations}, $final->{reservations})];
    my @transport = grep { ($_->{event} // '') eq 'shutdown_transport' } @$observed;
    my $key = sub { join(',', @{ $_[0] }{qw(address epoch batch)}) };
    my %results = map { $key->($_) => 1 } grep { $_->{code} == 4 } @transport;
    my %retired = map { $key->($_) => 1 } grep { $_->{code} == 5 } @transport;
    push @$checks, ['retained-exact-transport-retirement', keys(%results) == 2 && join(',', sort keys %results) eq join(',', sort keys %retired) &&
        !grep({ ($_->{code} == 4 || $_->{code} == 5) && $_->{at_us} > $queued->{at_us} } @transport) &&
        !grep({ $_->{code} == 1 && $_->{at_us} > $final->{at_us} } @transport)];
    return;
}

sub judge {
    my ($dir, $case) = @_;
    defined $case && -d $dir or die "usage: record_shutdown.pl judge CASEDIR CASE\n";
    grep { $_ eq $case } @CASES or die "unknown case $case\n";
    my $aborted = -e "$dir/aborted" ? 1 : 0;
    my (@checks, %child_ok, @tokens);
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
    push @checks, ['shutdown-proxy:ready', (read_file("$dir/shutdown-proxy.ready") =~ /^(\d+)/ ? $1 : 0) > 0] if -f "$dir/shutdown-proxy.ready";
    my @workers;
    my ($exit_ns, $stimulus_ns) = (0, 0);
    if (-f "$dir/ioc.ready") {
        push @checks, ["$case-actual-IOC-ready", read_file("$dir/ioc.ready") =~ /^1/ ? 1 : 0];
        my $clients_ok = 1;
        for my $client (qw(ca-002-caput ca-003-caput ca-004-caget ca-005-caget)) {
            next unless -f "$dir/$client.stop";
            my ($code, $forced) = split ' ', read_file("$dir/$client.stop");
            my $ok = $code == 0 && !$forced ? 1 : 0;
            $child_ok{$client} = $ok;
            push @checks, ["$client:normal-exit", $ok];
        }
        if (-f "$dir/ca-005-caget.stop") {
            my @pact = map { my $t = -f "$dir/$_.stdout" ? read_file("$dir/$_.stdout") : ''; $t =~ s/\s+\z//; $t } qw(ca-004-caget ca-005-caget);
            push @checks, ["$case-both-CA-records-active", join(',', @pact) eq '1,1'];
        }
        if (-f "$dir/wire.ready") {
            push @checks, ["$case-actual-request-before-exit", read_file("$dir/wire.ready") =~ /^1/ ? 1 : 0];
        }
        if (-f "$dir/workers.json") {
            @workers = @{ decode_json(read_file("$dir/workers.json")) };
            push @checks, ["$case-actual-owned-worker", @workers == 1];
        }
        if (-f "$dir/exit.ns") {
            my @marks = split ' ', read_file("$dir/exit.ns");
            ($stimulus_ns, $exit_ns) = @marks[0, 1];
            my $before = eval { report(read_file("$dir/before-exit.stdout")) };
            push @checks, ["$case-pending-before-exit", $before && $before->{runtime}{state} eq 'Running' && $before->{records}{active} == 2 &&
                $before->{records}{completions} == 0];
            push @checks, ["$case-exit-before-deadlines", $exit_ns - $stimulus_ns > 0 &&
                $exit_ns - $stimulus_ns < (($NATIVE_TIMEOUT_MS < $DEADLINE_MS ? $NATIVE_TIMEOUT_MS : $DEADLINE_MS) * 1000000)];
        }
    }
    for my $name (qw(ioc repeater shutdown-proxy agent-ipv4-1)) {
        next unless -f "$dir/$name.stop";
        my ($code, $forced) = split ' ', read_file("$dir/$name.stop");
        my $ok = ($name eq 'repeater' ? ($code == 0 || $code == 143) : $code == 0) && !$forced ? 1 : 0;
        $child_ok{$name} = $ok;
        push @checks, [$name =~ /^(?:ioc|repeater)$/ ? "$name:normal-exit" : "$name:normal-stop", $ok];
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
    my @observed;
    my $parsed = eval {
        my $text = read_file("$dir/ioc.stdout");
        @observed = events($text);
        my @tail = split /snmp3 lifecycle: fallback\n/, $text, -1;
        push @checks, ["$case-single-fallback", @tail == 2];
        my $final = report($tail[-1]);
        write_file("$dir/fallback.json", JSON::PP->new->canonical->pretty->encode($final));
        my @hooks = $text =~ /snmp3 hook: (AtShutdown|AfterStopScan|AfterStopCallback|AfterShutdown)/g;
        push @checks, ["$case-actual-Base-shutdown-order", join(',', @hooks) eq 'AtShutdown,AfterStopScan,AfterStopCallback,AfterShutdown'];
        push @checks, ["$case-worker-reaped", @workers == 1 && !grep({ !worker_gone($_) } @workers) &&
            $final->{worker}{launches} == 1 && $final->{worker}{reaps} == 1 && $final->{worker}{forced} == 0];
        if ($case eq 'inflight') {
            my %want = (state => 'Stopped', admission => 0, activation => 1, created => 1, exited => 1, joined => 1);
            push @checks, ['inflight-stopped', $final->{runtime}{state} eq 'Stopped' && !grep({ $final->{runtime}{$_} != $want{$_} } grep { $_ ne 'state' } keys %want)];
            my %records = map { $_ => 0 } @REQUEST_FIELDS;
            @records{qw(contexts completions detachAllowed)} = (2, 2, 1);
            push @checks, ['inflight-completed-before-link-close', same($final->{records}, \%records)];
        } else {
            retained_checks(\@checks, \@observed);
            my $r = $final->{records};
            push @checks, ['retained-fallback-incomplete', $final->{runtime}{state} eq 'IncompleteStopped' && $r->{contexts} == 2 &&
                $r->{active} == 2 && $r->{inert} == 2 && $r->{completions} == 0 && $r->{drainFailed} == 1];
            my @wire = events(read_file("$dir/shutdown-proxy.stdout"));
            my @requests = grep { ($_->{event} // '') eq 'fault_request' } @wire;
            my @responses = grep { ($_->{event} // '') eq 'fault_response' } @wire;
            my @agent = events(read_file("$dir/agent-ipv4-1.stdout"));
            push @checks, ['retained-real-GET-SET-responses', join(',', sort { $a <=> $b } map { $_->{command} } @requests) eq '160,163' &&
                @responses == 2 && !grep({ $_->{monotonic_ns} >= $exit_ns } (@requests, @responses)) &&
                scalar(grep { ($_->{event} // '') eq 'agent_set_value' && ($_->{index} // -1) == 9 && ($_->{value} // '') eq '19.5' } @agent) > 0];
        }
        1;
    };
    my $parse_error = $@;
    unless ($parsed) {
        push @checks, ["$case-parse-complete-observations", 0];
        write_file("$dir/parse-error.json", JSON::PP->new->canonical->pretty->encode({ message => "$parse_error" }));
    }
    my @libraries = libraries("$dir/ioc.stderr");
    my @all_children = (qw(ioc repeater shutdown-proxy agent-ipv4-1), grep { -f "$dir/$_.stop" } qw(ca-002-caput ca-003-caput ca-004-caget ca-005-caget));
    my $clean = !grep({ !$child_ok{$_} } @all_children) ? 1 : 0;
    push @checks, ["$case-all-child-receipts-clean", $clean && @all_children >= 4];
    push @checks, ["$case-IOC-native-free", @libraries && !grep { index($_, 'netsnmp') >= 0 } @libraries];
    my $products = -f "$dir/products.txt" ? read_file("$dir/products.txt") : "$ROOT/bin/linux-x86_64";
    chomp $products;
    my $executable = $case eq 'inflight' ? 'snmp3Ioc' : 'snmp3ShutdownTest';
    my @inputs = (sort(glob("$ROOT/snmp3App/src/*.cpp")), sort(glob("$ROOT/snmp3App/src/*.h")), sort(glob("$ROOT/snmp3App/native/*.cpp")),
        sort(glob("$ROOT/snmp3App/native/*.h")),
        map({ "$ROOT/tests/rewrite/$_" } qw(ShutdownTest.cpp NativeAgent.cpp run_independence.py helpers/udp_fault.py db/record-shutdown.db
            Makefile snmp3ShutdownTestRegistrar.dbd test_record_shutdown.bash record_shutdown.pl)), "$ROOT/dbd/$executable.dbd",
        map({ "$products/$_" } ($executable, qw(snmp3Worker snmp3NativeAgent snmp3NativeProbe))));
    my $failed = grep { !$_->[1] } @checks;
    my $passed = !$failed && !$aborted ? 1 : 0;
    my %result = (case => $case, passed => $passed ? JSON::PP::true : JSON::PP::false, aborted => $aborted ? JSON::PP::true : JSON::PP::false,
        checks => [map { { name => $_->[0], passed => $_->[1] ? JSON::PP::true : JSON::PP::false } } @checks],
        cleanup_passed => $clean ? JSON::PP::true : JSON::PP::false, workers => \@workers, exit_requested_ns => $exit_ns + 0,
        observations => \@observed, inputs => digests(@inputs), ioc_libraries => digests(@libraries),
        scope => 'Real non-isolated Base shutdown; ai/ao; dependencies uninstrumented; leaks disabled');
    write_file("$dir/results.json", JSON::PP->new->canonical->pretty->encode(\%result));
    printf "%s %s\n", $_->[1] ? 'PASS' : 'FAIL', $_->[0] for @checks;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $dir;
    return $passed ? 0 : 1;
}

sub summary {
    my ($out, @cases) = @_;
    @cases or die "usage: record_shutdown.pl summary OUTPUT CASE...\n";
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
    defined $command or die "usage: record_shutdown.pl port|now|sockets|owns|config|workers|judge|summary ...\n";
    return free_port() if $command eq 'port';
    return now() if $command eq 'now';
    return sockets(@arguments) if $command eq 'sockets';
    return owns(@arguments) if $command eq 'owns';
    return config(@arguments) if $command eq 'config';
    return workers(@arguments) if $command eq 'workers';
    return judge(@arguments) if $command eq 'judge';
    return summary(@arguments) if $command eq 'summary';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "record_shutdown.pl: $@";
    exit 2;
}
exit $status;
