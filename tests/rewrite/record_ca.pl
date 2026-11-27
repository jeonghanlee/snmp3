#!/usr/bin/env perl
# Judge and helper of the record Channel Access cell. test_record_ca.bash runs the production IOC with a fixture agent, a delaying
# fault proxy and a private repeater, makes the Channel Access operations of the cell one after the other, and keeps every client in
# files named ca-NNN-TOOL.* (stdout, stderr, rc, forced, and for a client that runs in the background its exit time); this script
# writes the configuration, counts events while the cell waits, and judges. The judge goes through the operations of the cell in the
# same order as the former Python harness and reads the client that belongs to each step: every client has one check ("ca-NNN-TOOL:
# return"), and the named checks follow the clients that they read.
#   record_ca.pl port                          print a free UDP port of the loopback address
#   record_ca.pl now                           print the monotonic clock in nanoseconds
#   record_ca.pl sockets PID PORT OUT          write the sockets that the process holds as JSON to OUT
#   record_ca.pl owns PID PORT                 exit 0 when the process owns a UDP socket bound to the port
#   record_ca.pl config DIR PROXYPORT          write DIR/records.json for the agent and the delaying proxy
#   record_ca.pl workers IOCPID PRODUCTS OUT   write the worker processes of the IOC (pid and start time) as JSON to OUT
#   record_ca.pl count proxy FILE EVENT [CMD]  print the number of events of the proxy output (only lines that end in a newline)
#   record_ca.pl count agent FILE INDEX        print the number of SET values that the agent stored for the index (context 0)
#   record_ca.pl judge DIR                     judge, write DIR/results.json and DIR/record-observations.json, print one line for each
#                                              failed check and the verdict; exit 0 (every check passed) or 1
# Exit status 2: the observations could not be read.
# The cell reads every value that a check names; the former harness stops reading at the first value that is false in a chain of
# comparisons, so after a failure the new cell lists the checks of the clients that the former harness did not make (all of them
# passing when the client itself ended normally).
# Language level: Perl 5.26.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use IO::Socket::INET;
use JSON::PP;
use Time::HiRes qw(clock_gettime CLOCK_MONOTONIC);

my $ROOT = abs_path(dirname(__FILE__) . '/../..');
my $DELAY_MS = 1000;

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

sub int_of {
    my ($path) = @_;
    return -1 unless -f $path;
    my $text = read_file($path);
    return $text =~ /^(-?\d+)/ ? $1 + 0 : -1;
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
    defined $path && $pid =~ /^\d+$/ or die "usage: record_ca.pl sockets PID PORT OUT\n";
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
    $pid =~ /^\d+$/ && defined $port && $port =~ /^\d+$/ or die "usage: record_ca.pl owns PID PORT\n";
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
    defined $proxy && $proxy =~ /^\d+$/ or die "usage: record_ca.pl config DIR PROXYPORT\n";
    my $agent_port = (split ' ', read_file("$dir/agent-ipv4-1.port"))[0];
    my @specs = (['WideRead', 5, 'get', 'counter64', 1, 'Local'], ['WideWrite', 5, 'set', 'counter64', 1, 'Local'],
                 ['TextRead', 6, 'get', 'octets', 1024, 'Local'], ['TextWrite', 6, 'set', 'octets', 1024, 'Local'],
                 ['IntegerRead', 1, 'get', 'integer', 1, 'Local'], ['IntegerWrite', 1, 'set', 'integer', 1, 'Local'],
                 ['ActiveFloatWrite', 9, 'set', 'opaqueFloat', 1, 'Delayed'], ['ActiveIntegerWrite', 1, 'set', 'integer', 1, 'Delayed'],
                 ['ActiveCounter64Write', 5, 'set', 'counter64', 1, 'Delayed'], ['ActiveOctetsWrite', 6, 'set', 'octets', 1024, 'Delayed']);
    my $profile = { id => 'Local', version => '2c', communityFile => "$dir/agent-ipv4-1-community", timeoutMs => 100, retries => 0 };
    my $delayed = { %$profile, id => 'Delayed', timeoutMs => 3 * $DELAY_MS };
    write_file("$dir/records.json", JSON::PP->new->canonical->pretty->encode({ schema => 1, profiles => [$profile, $delayed],
        endpoints => [{ id => 'Local', address => '127.0.0.1', port => $agent_port + 0, profile => 'Local' },
                      { id => 'Delayed', address => '127.0.0.1', port => $proxy + 0, profile => 'Delayed' }],
        bindings => [map { { id => $_->[0], endpoint => $_->[5], operation => $_->[2], valueType => $_->[3], capacity => $_->[4],
                             oid => "1.3.6.1.4.1.53864.4.$_->[1].0" } } @specs] }));
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
    defined $out && $ioc =~ /^\d+$/ or die "usage: record_ca.pl workers IOCPID PRODUCTS OUT\n";
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

# The JSON objects of the lines of a file that a running process may still be writing: only lines that end in a newline are read.
sub complete_events {
    my ($path) = @_;
    return () unless -f $path;
    my $text = read_file($path);
    my $end = rindex($text, "\n");
    return () if $end < 0;
    my @found;
    for my $line (split /\n/, substr($text, 0, $end + 1)) {
        next unless index($line, '{') == 0;
        my $decoded = eval { JSON::PP->new->utf8->decode($line) };
        push @found, $decoded if ref $decoded eq 'HASH';
    }
    return @found;
}

sub proxy_events {
    my ($path, $kind, $command) = @_;
    return grep { ($_->{event} // '') eq $kind && (!defined $command || ($_->{command} // -1) == $command) } complete_events($path);
}

sub agent_values {
    my ($path, $index) = @_;
    return grep { ($_->{event} // '') eq 'agent_set_value' && ($_->{index} // -1) == $index && ($_->{context} // -1) == 0 } complete_events($path);
}

sub count {
    my ($which, $path, @rest) = @_;
    if ($which eq 'proxy') {
        my ($event, $command) = @rest;
        print scalar(proxy_events($path, $event, $command)), "\n";
        return 0;
    }
    if ($which eq 'agent') {
        print scalar(agent_values($path, $rest[0])), "\n";
        return 0;
    }
    die "usage: record_ca.pl count proxy|agent FILE ...\n";
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

sub worker_gone {
    my ($worker) = @_;
    my $start = stat_start($worker->{pid});
    return 1 unless defined $start;
    return $start ne $worker->{start_time} ? 1 : 0;
}

sub expected_stored {
    my ($kind, $payload) = @_;
    return sprintf('%.9g', $payload) if $kind eq 'Ao';
    if ($kind eq 'Stringout' || $kind eq 'Lso') {
        my $data = substr($payload, 0, 32);
        return length($payload) . ':' . unpack('H*', $data);
    }
    return $payload;
}

sub stored {
    my ($event, $text) = @_;
    return $event->{value} if exists $event->{value};
    return $text ? "$event->{length}:$event->{prefix_hex}" : undef;
}

sub judge {
    my ($dir) = @_;
    defined $dir && -d $dir or die "usage: record_ca.pl judge DIR\n";
    my $aborted = -e "$dir/aborted" ? 1 : 0;
    my (@checks, %child_ok, @tokens, @observations);
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
    push @checks, ['ca-response-delay:ready', (read_file("$dir/ca-response-delay.ready") =~ /^(\d+)/ ? $1 : 0) > 0] if -f "$dir/ca-response-delay.ready";
    push @checks, ['private-repeater-owned', read_file("$dir/repeater.owned") =~ /^1/ ? 1 : 0] if -f "$dir/repeater.owned";
    push @checks, ['actual-production-IOC-ready', read_file("$dir/ioc.ready") =~ /^1/ ? 1 : 0] if -f "$dir/ioc.ready";
    my $contexts = () = read_file("$ROOT/tests/rewrite/db/record-ca.db") =~ /field\(DTYP, "snmp3"\)/g;
    my $expected = 1;
    my @workers = -f "$dir/workers.json" ? @{ decode_json(read_file("$dir/workers.json")) } : ();
    # The operations of the cell, in the order of the former harness. A step that has no client file ends the replay, as an
    # operation that failed ended the former harness.
    my $ord = 0;
    my $client = sub {
        my ($tool, $expect) = @_;
        $ord++;
        my $name = sprintf('ca-%03d-%s', $ord, $tool);
        die "no client $name\n" unless -f "$dir/$name.rc";
        my $code = int_of("$dir/$name.rc");
        my $forced = int_of("$dir/$name.forced") == 1 ? 1 : 0;
        my $ok = $code == $expect && !$forced ? 1 : 0;
        my $out = -f "$dir/$name.stdout" ? read_file("$dir/$name.stdout") : '';
        $out =~ s/^\s+|\s+\z//g;
        push @checks, ["$name:return", $ok];
        die "client failed: $name\n" unless $ok;
        return { name => $name, out => $out };
    };
    # The arguments of a call name the value that the former harness read; the driver made the same call, and the judge reads its file.
    my $value = sub {
        return $client->('caget', 0)->{out};
    };
    my $process = sub {
        my ($name) = @_;
        $client->('caput', 0);
        $expected++;
        push @checks, ["$name:PACT-cleared", $value->("$name.PACT") eq '0'];
        push @checks, ["$name:NO_ALARM", $value->("$name.SEVR") eq 'NO_ALARM'];
    };
    my $proxy = "$dir/ca-response-delay.stdout";
    my $agent = "$dir/agent-ipv4-1.stdout";
    my $replay = eval {
        my $v;
        do { $v = $value->('Ca_Pini.PACT') } while ($v ne '0');
        push @checks, ['PINI-native-input', $value->('Ca_Pini') eq '-123'];
        push @checks, ['PINI-Base-FLNK', $value->('Ca_Completed') eq '1'];
        $process->('Ca_UnsignedWide');
        push @checks, ['Counter64-UINT64_MAX-decimal-CA', $value->('Ca_UnsignedWide') eq '18446744073709551615'];
        # Every comparison of the former harness that was a chain of "and" reads all its values here, so that the driver, which
        # cannot stop in the middle of an expression, and the judge agree on the clients.
        for my $number ('9007199254740993', '9223372036854775807') {
            $client->('caput', 0);
            $expected++;
            $process->('Ca_WideIn');
            my ($wide_out, $wide_in) = ($value->('Ca_WideOut'), $value->('Ca_WideIn'));
            push @checks, ["int64-decimal-CA-SET-GET:$number", $wide_out eq $number && $wide_in eq $number];
            push @observations, { event => 'ca_wide', decimal => $number, exact_native_GET => JSON::PP::true };
        }
        for my $length (0, 39, 40, 200, 255) {
            my $text = 'T' x $length;
            $client->('caput', 0);
            $expected++;
            $process->('Ca_TextIn');
            my @read = ($value->('Ca_TextOut.VAL$', 1), $value->('Ca_TextIn.VAL$', 1), $value->('Ca_TextOut.LEN'), $value->('Ca_TextIn.LEN'));
            push @checks, ["long-string-CHAR-CA-SET-GET:$length", $read[0] eq $text && $read[1] eq $text &&
                           $read[2] eq $length + 1 && $read[3] eq $length + 1];
            push @observations, { event => 'ca_long_string', data_bytes => $length, exact_native_GET => JSON::PP::true };
        }
        push @checks, ['CA-DBR_STRING-short-view', $value->('Ca_TextIn') eq 'T' x 39];
        my $rejected_put = $ord + 1;
        $client->('caput', 1);
        my $diagnostic = read_file(sprintf('%s/ca-%03d-caput.stderr', $dir, $rejected_put));
        $process->('Ca_TextIn');
        my @capacity = ($value->('Ca_TextOut.LEN'), $value->('Ca_TextOut.VAL$', 1), $value->('Ca_TextIn.VAL$', 1));
        push @checks, ['CA-client-capacity-rejection', has($diagnostic, 'Invalid element count requested') && $capacity[0] eq '256' &&
                       $capacity[1] eq 'T' x 255 && $capacity[2] eq 'T' x 255];
        push @observations, { event => 'ca_client_capacity', client_data_bytes => 300, rejected_before_put => JSON::PP::true,
                              preserved_data_bytes => 255, exact_native_GET => JSON::PP::true };
        $client->('caput', 0);
        $expected++;
        $process->('Ca_ShortIn');
        push @checks, ['short-string-CA-SET-GET', $value->('Ca_ShortIn') eq 'short-wire'];
        my @steps = map { [split /\t/] } grep { length } split /\n/, (-f "$dir/active.tsv" ? read_file("$dir/active.tsv") : '');
        my @actives = (['Ao', 9, ['1.5', '2.5'], ['3.5', '4.5']], ['Longout', 1, ['31', '32'], ['33', '34']],
                       ['Int64out', 5, ['9007199254740993', '9007199254740995'], ['9007199254740997', '9007199254740999']],
                       ['Stringout', 6, ['first-short', 'latest-short'], ['third-short', 'fourth-short']],
                       ['Lso', 6, ['F' x 200, 'L' x 250], ['T' x 120, 'U' x 230]]);
        my $flnk = 0;
        for my $active (@actives) {
            my ($kind, $index, $plain, $notify) = @$active;
            my $name = "Ca_Active$kind";
            my $long = $kind eq 'Lso' ? 1 : 0;
            my $text = ($kind eq 'Stringout' || $kind eq 'Lso') ? 1 : 0;
            for my $route (['plain-put', @$plain], ['put-callback', @$notify]) {
                my ($route_name, $first, $latest) = @$route;
                my $label = "$name:$route_name";
                my $step = shift @steps or die "no step $label\n";
                my ($step_label, $sets, $deliveries, $stored_before, $applied, $delivered, $second, $end_requests, $end_deliveries, $end_agent) = @$step;
                die "step mismatch $step_label $label\n" unless $step_label eq $label;
                $client->('caput', 0);
                my @all_requests = proxy_events($proxy, 'fault_request', 163);
                my $request = $applied ? $all_requests[$sets] : undef;
                my $notifier;
                if ($route_name eq 'plain-put') {
                    $client->('caput', 0);
                } else {
                    $ord++;
                    $notifier = sprintf('ca-%03d-caput', $ord);
                }
                my $state = { PACT => $value->("$name.PACT"), RPRO => $value->("$name.RPRO") };
                # The wait for an idle record reads until the value is 0, and once more (the former harness evaluated its predicate again).
                my $pact;
                do { $pact = $value->("$name.PACT") } while ($pact ne '0');
                $value->("$name.PACT");
                my $notify_result;
                if (defined $notifier) {
                    die "no notifier $notifier\n" unless -f "$dir/$notifier.rc";
                    $notify_result = { returncode => int_of("$dir/$notifier.rc"), forced_cleanup => int_of("$dir/$notifier.forced") == 1 ? 1 : 0,
                        exit_ns => int_of("$dir/$notifier.exit_ns") };
                }
                # The counts that the driver took when the step was settled: later steps add events to the same files.
                my @requests = $end_requests > $sets ? @all_requests[$sets .. $end_requests - 1] : ();
                my @deliveries_all = $end_deliveries > 0 ? (proxy_events($proxy, 'fault_delayed_delivery'))[0 .. $end_deliveries - 1] : ();
                my $first_delivery = 0;
                for my $delivery (@deliveries_all) {
                    next unless $request && $delivery->{monotonic_ns} > $request->{monotonic_ns};
                    $first_delivery = $delivery->{monotonic_ns} if !$first_delivery || $delivery->{monotonic_ns} < $first_delivery;
                }
                my @agent_all = $end_agent > 0 ? (agent_values($agent, $index))[0 .. $end_agent - 1] : ();
                my @values = $end_agent > $stored_before ? map({ stored($_, $text) } @agent_all[$stored_before .. $end_agent - 1]) : ();
                my %outcome;
                $outcome{$_} = $value->("$name.$_") for qw(STAT SEVR AMSG PACT);
                $outcome{VAL} = $value->($long ? "$name.VAL\$" : $name, $long);
                my $flnk_now = int($value->('Ca_ActiveCompleted'));
                my $second_after = $second && @requests >= 2 && $requests[1]{monotonic_ns} > $first_delivery ? 1 : 0;
                push @observations, { event => 'ca_active_output', record => $name, route => $route_name, state_after_second_write => $state,
                    first_delivered => $delivered ? JSON::PP::true : JSON::PP::false, set_requests => scalar(@requests),
                    agent_stored => \@values, outcome => \%outcome, flnk_delta => $flnk_now - $flnk,
                    second_set_after_first_delivery => $second_after ? JSON::PP::true : JSON::PP::false };
                $flnk = $flnk_now;
                $expected += 2;
                push @checks, ["$label:write-while-active", $applied && $state->{PACT} eq '1'];
                push @checks, ["$label:two-SETs-second-after-first-delivery", $second_after && @requests == 2];
                push @checks, ["$label:agent-stored-first-then-latest", @values == 2 && ($values[0] // '') eq expected_stored($kind, $first) &&
                               ($values[1] // '') eq expected_stored($kind, $latest)];
                my $idle = $outcome{SEVR} eq 'NO_ALARM' && $outcome{PACT} eq '0' &&
                    ($kind eq 'Ao' ? $outcome{VAL} == $latest : $outcome{VAL} eq $latest);
                push @checks, ["$label:idle-without-alarm", $idle];
                if ($notify_result) {
                    push @checks, ["$label:callback-after-second-delivery", $notify_result->{returncode} == 0 && !$notify_result->{forced_cleanup} &&
                        @deliveries_all >= $deliveries + 2 &&
                        !grep({ $notify_result->{exit_ns} < $_->{monotonic_ns} } @deliveries_all[$deliveries .. $#deliveries_all])];
                }
            }
        }
        $client->('caput', 0);
        $process->('Ca_IntegerIn');
        $process->('Ca_SignedWideIn');
        $expected++;
        my @signed = ($value->('Ca_SignedOut'), $value->('Ca_IntegerIn'), $value->('Ca_SignedWideIn'));
        push @checks, ['INT32_MIN-CA-SET-GET', !grep { $_ ne '-2147483648' } @signed];
        $process->('Ca_WideIn');
        my $baseline = $value->('Ca_WideIn');
        $client->('caput', 0);
        my @minimum = ($value->('Ca_WideOut'), $value->('Ca_WideOut.STAT'), $value->('Ca_WideOut.SEVR'), $value->('Ca_WideOut.PACT'));
        $process->('Ca_WideIn');
        my $after = $value->('Ca_WideIn');
        push @checks, ['INT64_MIN-CA-Counter64-rejected-without-SET', join(',', @minimum) eq '-9223372036854775808,WRITE,INVALID,0' && $after eq $baseline];
        push @observations, { event => 'ca_signed_minimum', int32_min => $value->('Ca_IntegerIn'), int64_min_record => \@minimum,
                              native_before => $baseline, native_after => $after };
        push @checks, ['actual-owned-worker-PID', @workers == 1];
        1;
    };
    my $replay_error = $@;
    # The exit of the IOC, the final report and the children, in the order of the former harness.
    my $text = -f "$dir/ioc.stdout" ? read_file("$dir/ioc.stdout") : '';
    my @libraries = libraries("$dir/ioc.stderr");
    for my $name (qw(ioc repeater)) {
        next unless -f "$dir/$name.stop";
        my ($code, $forced) = split ' ', read_file("$dir/$name.stop");
        my $ok = !$forced && ($name eq 'ioc' ? $code == 0 : ($code == 0 || $code == 143)) ? 1 : 0;
        $child_ok{$name} = $ok;
        push @checks, ["$name:normal-exit", $ok];
        next unless $name eq 'ioc';
        push @checks, ['IOC-native-free', @libraries && !grep { index($_, 'netsnmp') >= 0 } @libraries];
        push @checks, ['Base-normal-shutdown', has($text, 'snmp3 hook: AfterShutdown') && has($text, 'state=Stopped admission=0') &&
                       has($text, 'created=1 exited=1 joined=1')];
        push @checks, ['native-worker-reaped', @workers && !grep({ !worker_gone($_) } @workers) && has($text, 'launches=1 reaps=1 forced=0')];
        push @checks, ['exact-native-completion-count-and-closed-gate',
                       has($text, "contexts=$contexts active=0 pending=0 queued=0 running=0 inert=0 entered=0") &&
                       has($text, "completions=$expected entryOpen=0 drainFailed=0 detachAllowed=1")];
    }
    for my $name ('ca-response-delay', 'agent-ipv4-1') {
        next unless -f "$dir/$name.stop";
        my ($code, $forced) = split ' ', read_file("$dir/$name.stop");
        push @checks, ["$name:normal-stop", $code == 0 && !$forced];
    }
    my (@leaks, @diagnostics);
    opendir(my $dh, $dir) or die "cannot read $dir: $!\n";
    for my $name (sort readdir $dh) {
        next unless $name =~ /\.(?:stdout|stderr|json)\z/ && $name ne 'results.json';
        my $content = read_file("$dir/$name");
        push @leaks, $name if grep { index($content, $_) >= 0 } @tokens;
        push @diagnostics, $name if $name =~ /\.stderr\z/ &&
            grep { index($content, $_) >= 0 } ('ERROR: AddressSanitizer', 'runtime error:', 'AddressSanitizer:DEADLYSIGNAL');
    }
    closedir($dh);
    push @checks, ['secret-sentinels-absent', !@leaks], ['sanitizer-diagnostics-absent', !@diagnostics];
    my $products = -f "$dir/products.txt" ? read_file("$dir/products.txt") : "$ROOT/bin/linux-x86_64";
    chomp $products;
    my $base = -f "$dir/base.txt" ? read_file("$dir/base.txt") : '';
    chomp $base;
    my @inputs = (sort(glob("$ROOT/snmp3App/src/*.cpp")), sort(glob("$ROOT/snmp3App/src/*.h")), sort(glob("$ROOT/snmp3App/native/*.cpp")),
        sort(glob("$ROOT/snmp3App/native/*.h")), map({ "$ROOT/tests/rewrite/$_" } qw(db/record-ca.db NativeAgent.cpp
            helpers/udp_fault.py test_record_ca.bash record_ca.pl)), "$ROOT/dbd/snmp3Ioc.dbd",
        "$ROOT/snmp3App/src/O.linux-x86_64/snmp3Ioc_registerRecordDeviceDriver.cpp",
        map({ "$products/$_" } qw(snmp3Ioc snmp3Worker snmp3NativeProbe snmp3NativeAgent)),
        map({ "$base/bin/linux-x86_64/$_" } qw(caget caput caRepeater)));
    my $failed = grep { !$_->[1] } @checks;
    my $passed = !$failed && !$aborted && $replay ? 1 : 0;
    write_file("$dir/record-observations.json", JSON::PP->new->canonical->pretty->encode(\@observations));
    my %result = (passed => $passed ? JSON::PP::true : JSON::PP::false, aborted => ($aborted || !$replay) ? JSON::PP::true : JSON::PP::false,
        checks => [map { { name => $_->[0], passed => $_->[1] ? JSON::PP::true : JSON::PP::false } } @checks],
        inputs => digests(@inputs), loaded_libraries => digests(@libraries),
        scope => 'Actual production IOC/CA/record/native path; selected numeric/text/PINI/active-output/signed-minimum/non-isolated shutdown subset');
    write_file("$dir/results.json", JSON::PP->new->canonical->pretty->encode(\%result));
    printf "FAIL %s\n", $_->[0] for grep { !$_->[1] } @checks;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $dir;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @arguments) = @_;
    defined $command or die "usage: record_ca.pl port|now|sockets|owns|config|workers|count|judge ...\n";
    return free_port() if $command eq 'port';
    return now() if $command eq 'now';
    return sockets(@arguments) if $command eq 'sockets';
    return owns(@arguments) if $command eq 'owns';
    return config(@arguments) if $command eq 'config';
    return workers(@arguments) if $command eq 'workers';
    return count(@arguments) if $command eq 'count';
    return judge(@arguments) if $command eq 'judge';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "record_ca.pl: $@";
    exit 2;
}
exit $status;
