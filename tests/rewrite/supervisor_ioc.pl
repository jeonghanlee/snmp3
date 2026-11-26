#!/usr/bin/env perl
# Judge and helper of the supervisor cell. test_supervisor_ioc.bash starts two fixture agents and the supervisor and runtime test
# programs and keeps what they produced in the output directory (agents.tsv, NAME.ready, NAME.sockets.json, NAME.stop, NAME.stdout,
# NAME.stderr, NAME.two-address.events, two-address.*, ioc.*, ioc.requested, products.txt, aborted); this script writes the configuration and judges.
#   supervisor_ioc.pl port FAMILY                 print a free UDP port of the loopback address of the family (4 or 6)
#   supervisor_ioc.pl sockets PID PORT FAMILY OUT write the sockets that the process holds as JSON to OUT
#   supervisor_ioc.pl config OUTPUT               write OUTPUT/two-address.config.json for the agents of agents.tsv
#   supervisor_ioc.pl judge OUTPUT                judge, write results.json, print one line for each check and the verdict; exit
#                                                 0 (every check passed) or 1
# Exit status 2: the observations could not be read (no results.json is written).
# The checks, in this order, are those of the former Python cell: for each agent bind-ready and only-owned-loopback-listener; when
# every agent was ready the return of the supervisor test, its seven checks and the equal-deadline batch of each agent, and with
# --ioc the eight checks of the runtime test; the normal-stop of each agent in the reverse order; the two checks of the scan.
# Language level: Perl 5.26.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use IO::Socket::INET;
use IO::Socket::IP;
use JSON::PP;

my $ARCH = 'linux-x86_64';
my $ROOT = abs_path(dirname(__FILE__) . '/../..');

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

sub agents {
    my ($out) = @_;
    return map { [split /\t/] } grep { length } split /\n/, read_file("$out/agents.tsv");
}

sub free_port {
    my ($family) = @_;
    $family =~ /^[46]$/ or die "usage: supervisor_ioc.pl port 4|6\n";
    my $socket = $family == 4
        ? IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1', LocalPort => 0)
        : IO::Socket::IP->new(Proto => 'udp', LocalAddr => '::1', LocalPort => 0);
    $socket or die "cannot bind a UDP socket: $!\n";
    print $socket->sockport(), "\n";
    close($socket);
    return 0;
}

sub sockets {
    my ($pid, $port, $family, $path) = @_;
    defined $path && $pid =~ /^\d+$/ or die "usage: supervisor_ioc.pl sockets PID PORT FAMILY OUT\n";
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
    my ($out) = @_;
    my (@profiles, @endpoints, @bindings);
    for my $agent (agents($out)) {
        my ($name, $family, $port) = @$agent;
        my $label = "Address$family";
        push @profiles, { id => $label, version => '2c', communityFile => "$out/$name-community", timeoutMs => 100, retries => 1,
                          maxVarbinds => 16 };
        push @endpoints, { id => $label, address => $family == 4 ? '127.0.0.1' : '::1', port => $port + 0, profile => $label };
        push @bindings, { id => "${label}Read", endpoint => $label, oid => '1.3.6.1.4.1.53864.4.1.0', operation => 'get',
                          valueType => 'integer', capacity => 1 };
    }
    write_file("$out/two-address.config.json", JSON::PP->new->canonical->pretty->encode(
        { schema => 1, profiles => \@profiles, endpoints => \@endpoints, bindings => \@bindings }));
    return 0;
}

# The libraries that a process loaded, as the real paths of the files that exist, from its loader trace.
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

# The JSON objects among the lines of a text, in order, and the number of lines that are not JSON. With $prefix only the lines that
# begin with it are read.
sub events {
    my ($text, $prefix) = @_;
    my (@found, $bad);
    $bad = 0;
    for my $line (split /\n/, $text) {
        next if defined $prefix && index($line, $prefix) != 0;
        my $decoded = eval { JSON::PP->new->utf8->decode($line) };
        if (ref $decoded eq 'HASH') { push @found, $decoded; } else { ++$bad; }
    }
    return (\@found, $bad);
}

sub is_true {
    my ($value) = @_;
    return defined $value && JSON::PP::is_bool($value) && $value ? 1 : 0;
}

sub supervision {
    my ($events, $code) = @_;
    return grep { ($_->{event} // '') eq 'supervision' && defined $_->{code} && $_->{code} == $code } @$events;
}

sub same_set {
    my ($left, $right) = @_;
    my %a = map { $_ => 1 } @$left;
    my %b = map { $_ => 1 } @$right;
    return 0 unless keys(%a) == keys(%b);
    return !grep { !$b{$_} } keys %a;
}

sub judge {
    my ($out) = @_;
    defined $out && -d $out or die "usage: supervisor_ioc.pl judge OUTPUT\n";
    my @agents = agents($out);
    @agents or die "no agent was started\n";
    my $ioc_requested = read_file("$out/ioc.requested") =~ /^1/ ? 1 : 0;
    my $aborted = -e "$out/aborted" ? 1 : 0;
    my @checks;
    my @tokens;
    for my $agent (@agents) {
        for my $kind (qw(community auth privacy)) {
            my $path = "$out/$agent->[0]-$kind";
            next unless -f $path;
            my $token = read_file($path);
            $token =~ s/\s+\z//;
            push @tokens, $token if length $token;
        }
    }
    for my $agent (@agents) {
        my ($name, $family, $port) = @$agent;
        my $ready = read_file("$out/$name.ready") =~ /^1/ ? 1 : 0;
        push @checks, ["$name:bind-ready", $ready];
        next unless $ready;
        my $state = decode_json(read_file("$out/$name.sockets.json"));
        my @listeners = @{ $state->{sockets} };
        my $wanted = ($family == 4 ? '0100007F' : '00000000000000000000000001000000') . sprintf(':%04X', $port);
        push @checks, ["$name:only-owned-loopback-listener", @listeners == 1 &&
                       $listeners[0]{table} eq ($family == 4 ? 'udp' : 'udp6') && $listeners[0]{local} eq $wanted];
    }
    my @libraries;
    if (!$aborted) {
        my $code = read_file("$out/two-address.rc") =~ /^(\d+)/ ? $1 : -1;
        push @checks, ['two-address:return', $code == 0];
        my ($events, $bad) = events(read_file("$out/two-address.stdout"));
        push @checks, ['two-address:json-output', 0] for 1 .. $bad;
        my @summaries = grep { ($_->{event} // '') eq 'summary' } @$events;
        push @checks, ['real-two-address-consumption', @summaries == 1 && ($summaries[0]{consumed} // -1) == 4 &&
                       ($summaries[0]{addresses} // -1) == 2 && is_true($summaries[0]{settled})];
        my @ready = supervision($events, 3);
        push @checks, ['one-ready-worker-per-address', same_set([map { $_->{address} // '' } @ready], [1, 2]) && @ready == 2];
        my @reaps = supervision($events, 7);
        my @launches = supervision($events, 2);
        push @checks, ['every-worker-exact-reap', @reaps == 2 && @launches == 2 &&
                       same_set([map { $_->{pid} // '' } @reaps], [map { $_->{pid} // '' } @launches]) &&
                       !grep { ($_->{detail} // -1) != 0 } @reaps];
        push @checks, ['no-forced-stop', !(supervision($events, 8) || supervision($events, 9))];
        my $native = -f "$out/native.stderr" ? read_file("$out/native.stderr") : '';
        my $closes = grep { index($_, 'native_event native_close ') == 0 } split /\n/, $native;
        push @checks, ['two-real-native-closes', $closes == 2];
        push @checks, ['native-events-no-drop', !has($native, 'native_dropped') && @tokens && !grep { index($native, $_) >= 0 } @tokens];
        for my $agent (@agents) {
            my $name = $agent->[0];
            my %counts;
            my @handled;
            for my $line (split /\n/, read_file("$out/$name.two-address.events")) {
                next unless index($line, '"event":"agent_handler"') >= 0;
                my $decoded = eval { JSON::PP->new->utf8->decode($line) };
                push @handled, $decoded if ref $decoded eq 'HASH';
            }
            $counts{ $_->{request_id} // '' }++ for @handled;
            my @values = values %counts;
            push @checks, ["$name:actual-equal-deadline-two-member-batch", keys(%counts) == 1 && @values == 1 && $values[0] == 2];
        }
        if ($ioc_requested) {
            my $ioc_code = read_file("$out/ioc.rc") =~ /^(\d+)/ ? $1 : -1;
            my $text = read_file("$out/ioc.stdout");
            my ($ioc_events) = events($text, '{"event":');
            push @checks, ['ioc:real-exit', $ioc_code == 0];
            my @summary = grep { ($_->{event} // '') eq 'ioc_runtime_summary' } @$ioc_events;
            push @checks, ['ioc:two-real-activations-exact-join', @summary == 1 && ($summary[0]{activations} // -1) == 2 &&
                           ($summary[0]{joined} // -1) == 2];
            my @incomplete = grep { ($_->{event} // '') eq 'incomplete_stop' } @$ioc_events;
            push @checks, ['ioc:unconsumed-terminal-retains-full-charge', @incomplete == 1 && ($incomplete[0]{count} // -1) == 1 &&
                           ($incomplete[0]{bytes} // -1) == 4672 && ($incomplete[0]{joined} // -1) == 1];
            push @checks, ['ioc:nonblocking-reconciliation-without-second-join',
                           scalar(grep { ($_->{event} // '') eq 'reconciled_stop' && ($_->{joined} // -1) == 1 } @$ioc_events) > 0];
            for my $activation (1, 2) {
                my $ready_at = index($text, "snmp3 lifecycle: ready activation=$activation");
                my $process_at = $ready_at < 0 ? -1 : index($text, "dbProcess of 'Runtime_PiniProbe'", $ready_at);
                my $after_at = $process_at < 0 ? -1 : index($text, 'snmp3 hook: AfterInitialProcess', $process_at);
                push @checks, ["ioc:ready-before-initial-process:$activation", $ready_at >= 0 && $process_at > $ready_at && $after_at > $process_at];
            }
            my @concurrent = grep { ($_->{event} // '') eq 'concurrent_stop' } @$ioc_events;
            push @checks, ['ioc:concurrent-stop-one-join-retained-native-owner', @concurrent == 1 &&
                           ($concurrent[0]{joined} // -1) == 2 && is_true($concurrent[0]{consumed_retained})];
            my $forced_pid = @concurrent ? ($concurrent[0]{pid} // '') : undef;
            my @ioc_launches = supervision($ioc_events, 2);
            my @ioc_reaps = supervision($ioc_events, 7);
            push @checks, ['ioc:every-real-worker-reaped', @ioc_launches == 4 && @ioc_reaps == 4 &&
                           same_set([map { $_->{pid} // '' } @ioc_launches], [map { $_->{pid} // '' } @ioc_reaps]) &&
                           !grep { ($_->{detail} // -1) != (defined $forced_pid && ($_->{pid} // '') eq $forced_pid ? 9 : 0) } @ioc_reaps];
        }
    }
    for my $agent (reverse @agents) {
        my $name = $agent->[0];
        my ($code, $forced) = -f "$out/$name.stop" ? split(' ', read_file("$out/$name.stop")) : (-1, 1);
        push @checks, ["$name:normal-stop", $code == 0 && !$forced];
    }
    my (@leaks, @diagnostics);
    opendir(my $dh, $out) or die "cannot read $out: $!\n";
    for my $name (sort readdir $dh) {
        next unless $name =~ /\.(?:stdout|stderr|json)\z/ && $name ne 'results.json';
        my $text = read_file("$out/$name");
        push @leaks, $name if grep { index($text, $_) >= 0 } @tokens;
        push @diagnostics, $name if $name =~ /\.stderr\z/ &&
            grep { index($text, $_) >= 0 } ('ERROR: AddressSanitizer', 'runtime error:', 'AddressSanitizer:DEADLYSIGNAL');
    }
    closedir($dh);
    push @checks, ['secret-sentinels-absent', !@leaks], ['sanitizer-diagnostics-absent', !@diagnostics];
    my %loaded_paths;
    for my $path (map({ "$out/$_->[0].stderr" } @agents), "$out/two-address.stderr", "$out/ioc.stderr") {
        $loaded_paths{$_} = 1 for libraries($path);
    }
    my $products = read_file("$out/products.txt");
    chomp $products;
    my @inputs = (__FILE__, "$ROOT/tests/rewrite/test_supervisor_ioc.bash", sort(glob("$ROOT/snmp3App/*/*.cpp")),
                  sort(glob("$ROOT/snmp3App/*/*.h")), "$ROOT/tests/rewrite/SupervisorTest.cpp", "$ROOT/tests/rewrite/NativeAgent.cpp",
                  map { "$products/$_" } qw(snmp3SupervisorTest snmp3Worker snmp3NativeAgent snmp3NativeProbe));
    push @inputs, "$ROOT/tests/rewrite/RuntimeTest.cpp", "$ROOT/dbd/snmp3RuntimeTest.dbd", "$products/snmp3RuntimeTest",
        "$ROOT/tests/rewrite/db/lifecycle.db" if $ioc_requested;
    my $failed = grep { !$_->[1] } @checks;
    my $passed = !$failed && !$aborted ? 1 : 0;
    my %summary = (
        passed => $passed ? JSON::PP::true : JSON::PP::false,
        checks => [map { { name => $_->[0], passed => $_->[1] ? JSON::PP::true : JSON::PP::false } } @checks],
        aborted => $aborted ? JSON::PP::true : JSON::PP::false,
        ioc_requested => $ioc_requested ? JSON::PP::true : JSON::PP::false,
        sha256 => digests(@inputs),
        loaded_libraries => digests(sort keys %loaded_paths),
        scope => 'Real scheduler/IPC/worker/native path; IOC hook/reuse/incomplete-stop included when requested; fault/sanitizer/full R5 gates remain pending',
    );
    write_file("$out/results.json", JSON::PP->new->canonical->pretty->encode(\%summary));
    printf "%s %s\n", $_->[1] ? 'PASS' : 'FAIL', $_->[0] for @checks;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $out;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @arguments) = @_;
    defined $command or die "usage: supervisor_ioc.pl port|sockets|config|judge ...\n";
    return free_port(@arguments) if $command eq 'port';
    return sockets(@arguments) if $command eq 'sockets';
    return config(@arguments) if $command eq 'config';
    return judge(@arguments) if $command eq 'judge';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "supervisor_ioc.pl: $@";
    exit 2;
}
exit $status;
