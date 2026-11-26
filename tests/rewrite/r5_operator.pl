#!/usr/bin/env perl
# Judge and helper of the operator cell. test_r5_operator.bash starts two fixture agents and the production IOC and keeps what they
# produced in the output directory (agents.tsv, NAME.ready, NAME.sockets.json, NAME.stop, NAME.stdout, NAME.stderr, operator.stdout,
# operator.stderr, operator.rc, aborted); this script writes the configuration and judges.
#   r5_operator.pl port FAMILY                 print a free UDP port of the loopback address of the family (4 or 6)
#   r5_operator.pl sockets PID PORT FAMILY OUT write the sockets that the process holds as JSON to OUT
#   r5_operator.pl config OUTPUT               write OUTPUT/configuration.json for the agents of agents.tsv
#   r5_operator.pl judge OUTPUT                judge, write results.json, print one line for each check and the verdict; exit 0
#                                              (every check passed) or 1
# Exit status 2: the observations could not be read (no results.json is written).
# The checks, in this order, are those of the former Python cell: for each agent bind-ready and only-owned-loopback-listener; the
# seven checks of the IOC run when every agent was ready; the normal-stop of each agent in the reverse order; and the two checks
# of the scan of the records.
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
    $family =~ /^[46]$/ or die "usage: r5_operator.pl port 4|6\n";
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
    defined $path && $pid =~ /^\d+$/ or die "usage: r5_operator.pl sockets PID PORT FAMILY OUT\n";
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
        push @profiles, { id => $label, version => '2c', communityFile => "$out/$name-community", timeoutMs => 5000, retries => 0,
                          maxVarbinds => 16 };
        push @endpoints, { id => $label, address => $family == 4 ? '127.0.0.1' : '::1', port => $port + 0, profile => $label };
        push @bindings, { id => "${label}Read", endpoint => $label, oid => '1.3.6.1.4.1.53864.4.1.0', operation => 'get',
                          valueType => 'integer', capacity => 1 };
    }
    write_file("$out/configuration.json", JSON::PP->new->canonical->pretty->encode(
        { schema => 1, profiles => \@profiles, endpoints => \@endpoints, bindings => \@bindings }));
    return 0;
}

# The libraries that a process loaded, as the real paths of the files that exist, from its loader trace.
sub libraries {
    my ($path) = @_;
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

sub judge {
    my ($out) = @_;
    defined $out && -d $out or die "usage: r5_operator.pl judge OUTPUT\n";
    my @agents = agents($out);
    @agents or die "no agent was started\n";
    my @checks;
    my $aborted = -e "$out/aborted" ? 1 : 0;
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
        my $code = read_file("$out/operator.rc") =~ /^(\d+)/ ? $1 : -1;
        my $text = read_file("$out/operator.stdout");
        @libraries = libraries("$out/operator.stderr");
        push @checks,
            ['actual-operator-exit', $code == 0],
            ['documented-capability', has($text, 'capability=owned-worker-transport; recordSupport=available')],
            ['actual-running-then-stopped', has($text, 'state=Running admission=1') && has($text, 'state=Stopped admission=0')],
            ['actual-default-limits', has($text, 'countLimit=1024 byteLimit=1048576')],
            ['exact-one-join', has($text, 'activation=1 created=1 exited=1 joined=1')],
            ['frozen-config', has($text, 'revision=1 frozen=1 profiles=2 endpoints=2 bindings=2')],
            ['actual-IOC-loader-native-free', scalar(@libraries) && !grep { index($_, 'netsnmp') >= 0 } @libraries];
    }
    for my $agent (reverse @agents) {
        my $name = $agent->[0];
        my ($code, $forced) = -f "$out/$name.stop" ? split(' ', read_file("$out/$name.stop")) : (-1, 1);
        push @checks, ["$name:normal-stop", $code == 0 && !$forced];
    }
    my @tokens;
    for my $agent (@agents) {
        for my $kind (qw(community auth privacy)) {
            my $path = "$out/$agent->[0]-$kind";
            if (-f $path) {
                my $token = read_file($path);
                $token =~ s/\s+\z//;
                push @tokens, $token if length $token;
            }
        }
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
    my %loaded_paths = map { $_ => 1 } @libraries;
    for my $agent (@agents) {
        $loaded_paths{$_} = 1 for -f "$out/$agent->[0].stderr" ? libraries("$out/$agent->[0].stderr") : ();
    }
    my @inputs = ("$ROOT/tests/rewrite/test_r5_operator.bash", __FILE__, (map { "$ROOT/bin/$ARCH/$_" } qw(snmp3Ioc snmp3Worker snmp3NativeProbe)),
                  sort glob("$ROOT/snmp3App/src/*.cpp"), sort glob("$ROOT/snmp3App/src/*.h"));
    my $failed = grep { !$_->[1] } @checks;
    my $passed = !$failed && !$aborted ? 1 : 0;
    my %summary = (
        passed => $passed ? JSON::PP::true : JSON::PP::false,
        checks => [map { { name => $_->[0], passed => $_->[1] ? JSON::PP::true : JSON::PP::false } } @checks],
        aborted => $aborted ? JSON::PP::true : JSON::PP::false,
        inputs => digests(@inputs),
        loaded_libraries => digests(sort keys %loaded_paths),
        scope => 'Actual documented production IOC commands and loader; application Native traffic qualified separately',
    );
    write_file("$out/results.json", JSON::PP->new->canonical->pretty->encode(\%summary));
    printf "%s %s\n", $_->[1] ? 'PASS' : 'FAIL', $_->[0] for @checks;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $out;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @arguments) = @_;
    defined $command or die "usage: r5_operator.pl port|sockets|config|judge ...\n";
    return free_port(@arguments) if $command eq 'port';
    return sockets(@arguments) if $command eq 'sockets';
    return config(@arguments) if $command eq 'config';
    return judge(@arguments) if $command eq 'judge';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "r5_operator.pl: $@";
    exit 2;
}
exit $status;
