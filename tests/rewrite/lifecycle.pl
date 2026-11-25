#!/usr/bin/env perl
# Judge and helper of the lifecycle cell. test_lifecycle.bash gathers the observations of each case into the output directory
# (NAME.stdout, NAME.stderr, NAME.rc, independence/results.json, repeater.*, ports.txt, sha256.before, sha256.after, base.txt);
# this script reads them and judges the checks of the cell in a fixed order. The cell stops at the first failed check: the
# checks after it are neither judged nor listed, and a check that is listed as failed is the last entry.
#   lifecycle.pl judge OUTPUT [--stage NAME]  without --stage: judge every stage, write results.json, print the verdict and exit
#                                             0 (every check passed) or 1; with --stage: judge up to the stage, print nothing,
#                                             and exit 0 (no check failed) or 1
#   lifecycle.pl port                         print a free UDP port of 127.0.0.1
#   lifecycle.pl owns PID PORT                exit 0 when the process owns a UDP socket bound to the port
# Exit status 2: the observations could not be read (no results.json is written).
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname basename);
use IO::Socket::INET;
use JSON::PP;

my $ARCH = 'linux-x86_64';
my $ROOT = abs_path(dirname(__FILE__) . '/../..');
my $REPORT = qr/snmp3 runtime: state=(\w+) admission=(\d+) activation=(\d+) created=(\d+) exited=(\d+) joined=(\d+)/;
my @SEQUENCE = ('snmp3 hook: AfterFinishDevSup', 'snmp3 lifecycle: ready activation=1',
                q{dbProcess of 'Lifecycle_PiniProbe'}, 'snmp3 hook: AfterInitialProcess', 'snmp3 hook: AtShutdown',
                'snmp3 lifecycle: stop-requested activation=1', 'snmp3 lifecycle: worker-exit activation=1',
                'snmp3 lifecycle: joined activation=1', 'snmp3 hook: AfterStopScan', 'snmp3 hook: AfterStopCallback',
                'snmp3 hook: AfterShutdown', 'snmp3 lifecycle: fallback');

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
    open(my $fh, '>', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub read_number {
    my ($path) = @_;
    my $text = read_file($path);
    $text =~ /^(-?\d+)\n?\z/ or die "malformed number in $path\n";
    return $1 + 0;
}

# The reports of the runtime in a text, each as the string "state,admission,activation,created,exited,joined".
sub reports {
    my ($text) = @_;
    my @found;
    while ($text =~ /$REPORT/g) {
        push @found, join(',', $1, $2, $3, $4, $5, $6);
    }
    return @found;
}

sub same_reports {
    my ($text, @expected) = @_;
    my @found = reports($text);
    return 0 unless @found == @expected;
    for my $i (0 .. $#found) {
        return 0 unless $found[$i] eq $expected[$i];
    }
    return 1;
}

# True when the tokens occur in the text in the given order, each after the end of the one before.
sub ordered {
    my ($text, @tokens) = @_;
    my $position = 0;
    for my $token (@tokens) {
        my $index = index($text, $token, $position);
        return 0 if $index < 0;
        $position = $index + length($token);
    }
    return 1;
}

sub has {
    my ($text, $token) = @_;
    return index($text, $token) >= 0 ? 1 : 0;
}

sub count {
    my ($text, $token) = @_;
    my $total = 0;
    my $position = 0;
    while ((my $index = index($text, $token, $position)) >= 0) {
        ++$total;
        $position = $index + length($token);
    }
    return $total;
}

sub real {
    my ($path) = @_;
    return abs_path($path) // $path;
}

sub base_directory {
    my ($out) = @_;
    my $base = read_file("$out/base.txt");
    chomp $base;
    return $base;
}

# The checks every case has: the exit status, the production library and the identity of the Base libraries that the loader
# trace of the case names.
sub case_checks {
    my ($out, $name, $expected) = @_;
    my $status = read_number("$out/$name.rc");
    my @loaded = map { real($_) } read_file("$out/$name.stderr") =~ /calling init:\s*(\S+)/g;
    my $base_libraries = real(base_directory($out) . "/lib/$ARCH");
    my @base = grep { basename($_) =~ /^lib(?:Com|ca|dbCore|dbRecStd)\.so/ } @loaded;
    my $production = real("$ROOT/lib/$ARCH/libsnmp3.so");
    return ([$name . '_exit', $status == $expected],
            [$name . '_production_library', scalar(grep { $_ eq $production } @loaded) > 0],
            [$name . '_base_identity', @base == 4 && !grep { dirname($_) ne $base_libraries } @base]);
}

sub stdout_of {
    my ($out, $name) = @_;
    return read_file("$out/$name.stdout");
}

sub stderr_of {
    my ($out, $name) = @_;
    return read_file("$out/$name.stderr");
}

sub stage_independence {
    my ($out) = @_;
    my $passed = 0;
    if (read_number("$out/independence.rc") == 0 && -f "$out/independence/results.json") {
        my $decoded = eval { decode_json(read_file("$out/independence/results.json")) };
        $passed = ref $decoded eq 'HASH' && defined $decoded->{status} && $decoded->{status} eq 'PASS';
    }
    return (['V2.7_independence', $passed]);
}

sub stage_build {
    my ($out) = @_;
    return (['test_build', read_number("$out/test-build.rc") == 0]);
}

sub stage_repeater {
    my ($out) = @_;
    return (['owned_repeater_ready', read_number("$out/repeater.owned") == 1]);
}

sub stage_normal {
    my ($out) = @_;
    my $text = stdout_of($out, 'normal');
    return (case_checks($out, 'normal', 0),
            ['V2.2_hook_PINI_shutdown_order', ordered($text, @SEQUENCE)],
            ['V2.2_counts', same_reports($text, 'Running,1,1,1,0,0', 'Stopped,0,1,1,1,1')],
            ['V2.2_actual_Pini_value', $text =~ /\bVAL\s*:\s*42\b/ ? 1 : 0]);
}

sub stage_cold {
    my ($out) = @_;
    return (case_checks($out, 'cold', 0),
            ['V2.3_cold_fallback', same_reports(stdout_of($out, 'cold'), ('Cold,0,0,0,0,0') x 2)]);
}

sub stage_before_init_error {
    my ($out) = @_;
    my $text = stdout_of($out, 'before-init-error');
    return (case_checks($out, 'before-init-error', 1),
            ['V2.3_preinit_error', same_reports($text, 'Cold,0,0,0,0,0') && !has($text, 'echo MUST_NOT_RUN')]);
}

sub stage_partial_build_error {
    my ($out) = @_;
    my $text = stdout_of($out, 'partial-build-error');
    my @found = reports($text);
    return (case_checks($out, 'partial-build-error', 1),
            ['V2.4_partial_cleanup', ordered($text, @SEQUENCE) && @found && $found[-1] eq 'Stopped,0,1,1,1,1' &&
                count($text, 'snmp3 lifecycle: joined activation=1') == 1]);
}

sub stage_explicit_stop {
    my ($out) = @_;
    my $text = stdout_of($out, 'explicit-stop');
    return (case_checks($out, 'explicit-stop', 0),
            ['V2.4_repeat_stop', same_reports($text, ('Stopped,0,1,1,1,1') x 2) &&
                count($text, 'snmp3 lifecycle: joined activation=1') == 1]);
}

sub stage_module {
    my ($out) = @_;
    my $text = stdout_of($out, 'module');
    return (case_checks($out, 'module', 0),
            ['V2.1_V2.6_module', has($text, 'lifecycle test: PASS') && same_reports($text, ('Stopped,0,20,20,20,20') x 2)]);
}

sub stage_thread_failure {
    my ($out) = @_;
    my $text = stdout_of($out, 'thread-failure');
    return (case_checks($out, 'thread-failure', 1),
            ['V2.5_real_limit_failure', has($text, 'create-failed activation=1') &&
                same_reports($text, ('Failed,0,1,0,0,0') x 2) && has(stderr_of($out, 'thread-failure'), 'startup script failed') &&
                has($text, 'SUCCESSFUL_SCRIPT')]);
}

sub stage_isolated {
    my ($out) = @_;
    my $text = stdout_of($out, 'isolated');
    return (case_checks($out, 'isolated', 0),
            ['V2.6_isolated', has($text, 'lifecycle test: PASS') && count($text, 'snmp3 hook: AfterFinishDevSup') == 2 &&
                count($text, q{dbProcess of 'Isolated_PiniProbe'}) == 2 && same_reports($text, ('Stopped,0,2,2,2,2') x 2)]);
}

sub digests {
    my ($path) = @_;
    my %found;
    for my $line (split /\n/, read_file($path)) {
        $line =~ /^([0-9a-f]{64})\s+(\S.*)$/ or die "malformed digest line in $path: $line\n";
        $found{$2} = $1;
    }
    return %found;
}

sub stage_frozen {
    my ($out) = @_;
    my %before = digests("$out/sha256.before");
    my %after = digests("$out/sha256.after");
    my $same = keys(%before) == keys(%after);
    for my $path (keys %before) {
        $same = 0 unless defined $after{$path} && $after{$path} eq $before{$path};
    }
    return (['frozen_inputs', $same]);
}

my @STAGES = (['independence', \&stage_independence], ['build', \&stage_build], ['repeater', \&stage_repeater],
              ['normal', \&stage_normal], ['cold', \&stage_cold], ['before-init-error', \&stage_before_init_error],
              ['partial-build-error', \&stage_partial_build_error], ['explicit-stop', \&stage_explicit_stop],
              ['module', \&stage_module], ['thread-failure', \&stage_thread_failure], ['isolated', \&stage_isolated],
              ['frozen', \&stage_frozen]);

# Judges the stages in order up to the named one (all when none is named). Returns the listed checks; the last one is the
# failed check when there is one.
sub evaluate {
    my ($out, $upto) = @_;
    my @checks;
    for my $stage (@STAGES) {
        my ($name, $code) = @$stage;
        for my $check ($code->($out)) {
            push @checks, { name => $check->[0], passed => $check->[1] ? 1 : 0 };
            return @checks unless $check->[1];
        }
        last if defined $upto && $name eq $upto;
    }
    return @checks;
}

sub loaded_libraries {
    my ($out) = @_;
    my %loaded;
    for my $name (qw(normal cold before-init-error partial-build-error explicit-stop module thread-failure isolated)) {
        next unless -f "$out/$name.stderr";
        $loaded{$name} = [map { real($_) } read_file("$out/$name.stderr") =~ /calling init:\s*(\S+)/g];
    }
    return \%loaded;
}

sub judge {
    my ($out, @options) = @_;
    defined $out && -d $out or die "usage: lifecycle.pl judge OUTPUT [--stage NAME]\n";
    my $stage;
    if (@options) {
        @options == 2 && $options[0] eq '--stage' or die "usage: lifecycle.pl judge OUTPUT [--stage NAME]\n";
        $stage = $options[1];
        grep { $_->[0] eq $stage } @STAGES or die "unknown stage $stage\n";
    }
    my @checks = evaluate($out, $stage);
    my $failed = grep { !$_->{passed} } @checks;
    return $failed ? 1 : 0 if defined $stage;
    my %cleanup;
    if (-f "$out/repeater.returncode") {
        my ($pid, $status) = split /\n/, read_file("$out/repeater.returncode");
        my $forced = read_number("$out/repeater.forced");
        %cleanup = (repeater_pid => $pid + 0, repeater_returncode => $status + 0, forced_termination => $forced ? JSON::PP::true : JSON::PP::false);
        $cleanup{ok} = ($status == 143 && !$forced) ? JSON::PP::true : JSON::PP::false;
    }
    my $cleanup_ok = !%cleanup || $cleanup{ok};
    my $passed = !$failed && $cleanup_ok ? 1 : 0;
    my %summary = (
        passed => $passed ? JSON::PP::true : JSON::PP::false,
        status => $passed ? 'PASS' : 'FAIL',
        base => base_directory($out),
        checks => [map { { name => $_->{name}, passed => $_->{passed} ? JSON::PP::true : JSON::PP::false } } @checks],
        cleanup => \%cleanup,
        loaded_libraries => loaded_libraries($out),
    );
    $summary{ports} = [split ' ', read_file("$out/ports.txt")] if -f "$out/ports.txt";
    $summary{sha256} = { digests("$out/sha256.before") } if -f "$out/sha256.before";
    write_file("$out/results.json", JSON::PP->new->canonical->pretty->encode(\%summary));
    printf "%s %s\n", $_->{passed} ? 'PASS' : 'FAIL', $_->{name} for @checks;
    printf "%s: %s/results.json\n", $summary{status}, $out;
    return $passed ? 0 : 1;
}

sub free_port {
    my $socket = IO::Socket::INET->new(Proto => 'udp', LocalAddr => '127.0.0.1', LocalPort => 0)
        or die "cannot bind a UDP socket: $!\n";
    my $port = $socket->sockport();
    close($socket);
    print "$port\n";
    return 0;
}

# True when the process holds a socket that is bound to the UDP port.
sub owns {
    my ($pid, $port) = @_;
    $pid =~ /^\d+$/ && $port =~ /^\d+$/ or die "usage: lifecycle.pl owns PID PORT\n";
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
            my ($address, $local_port) = split /:/, $columns[1];
            if (hex($local_port) == $port && $sockets{$columns[9]}) {
                close($fh);
                return 0;
            }
        }
        close($fh);
    }
    return 1;
}

sub main {
    my ($command, @arguments) = @_;
    defined $command or die "usage: lifecycle.pl judge|port|owns ...\n";
    return judge(@arguments) if $command eq 'judge';
    return free_port() if $command eq 'port';
    return owns(@arguments) if $command eq 'owns';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "lifecycle.pl: $@";
    exit 2;
}
exit $status;
