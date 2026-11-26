#!/usr/bin/env perl
# Judge and helper of the configuration cell. test_config.bash gathers the observations of each run into the output directory
# (NAME.stdout, NAME.stderr, NAME.rc, NAME.elapsed, cases.tsv written by config_fixtures.pl, repeater.*, ports.txt,
# sha256.before, sha256.after, fixtures.*, sentinels.txt, not-run.txt, base.txt); this script reads them and judges the checks of
# the cell in a fixed order. The cell stops at the first failed check: the checks after it are neither judged nor listed, and a
# check that is listed as failed is the last entry before the check of the receipt (V3.12_secret_receipt), which is always listed.
#   config.pl judge OUTPUT [--stage NAME]  without --stage: judge every stage, write results.json, print the verdict and exit 0
#                                          (every check passed) or 1; with --stage: judge up to the stage, print nothing, and
#                                          exit 0 (no check failed) or 1
#   config.pl port                         print a free UDP port of 127.0.0.1
#   config.pl owns PID PORT                exit 0 when the process owns a UDP socket bound to the port
# Exit status 2: the observations could not be read (no results.json is written).
# Language level: Perl 5.26.
use strict;
use warnings;
use Cwd qw(abs_path);
use File::Basename qw(dirname basename);
use IO::Socket::INET;
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

sub read_number {
    my ($path) = @_;
    my $text = read_file($path);
    $text =~ /^(-?\d+)\n?\z/ or die "malformed number in $path\n";
    return $1 + 0;
}

sub real {
    my ($path) = @_;
    return abs_path($path) // $path;
}

sub has {
    my ($text, $token) = @_;
    return index($text, $token) >= 0 ? 1 : 0;
}

# The reports of the configuration in a text, each as the string "revision,frozen,profiles,endpoints,bindings".
sub config_reports {
    my ($text) = @_;
    my @found;
    while ($text =~ /snmp3 config: revision=(\d+) frozen=(\d+) profiles=(\d+) endpoints=(\d+) bindings=(\d+)/g) {
        push @found, join(',', $1, $2, $3, $4, $5);
    }
    return @found;
}

sub same_reports {
    my ($text, @expected) = @_;
    my @found = config_reports($text);
    return 0 unless @found == @expected;
    for my $i (0 .. $#found) {
        return 0 unless $found[$i] eq $expected[$i];
    }
    return 1;
}

sub base_directory {
    my ($out) = @_;
    my $base = read_file("$out/base.txt");
    chomp $base;
    return $base;
}

sub stdout_of { return read_file("$_[0]/$_[1].stdout"); }
sub stderr_of { return read_file("$_[0]/$_[1].stderr"); }

sub loaded_of {
    my ($out, $name) = @_;
    return map { real($_) } stderr_of($out, $name) =~ /calling init:\s*(\S+)/g;
}

sub receipt {
    my ($text) = @_;
    my $decoded = eval { decode_json($text) };
    return ref $decoded eq 'HASH' ? $decoded : undef;
}

# The checks of one component run: exit status, libraries, and the receipt of the component; with the extras of the line the
# bound on the time and the end of the helper process.
sub component_checks {
    my ($out, $name, $expected, $extras) = @_;
    my $status = read_number("$out/$name.rc");
    my @loaded = loaded_of($out, $name);
    my $base = base_directory($out);
    my @checks = ([$name . '_exit', $status == $expected]);
    my $production = real("$ROOT/lib/$ARCH/libsnmp3.so");
    my $common = real("$base/lib/$ARCH/libCom.so");
    push @checks, [$name . '_component_identity',
                   scalar(grep { $_ eq $production } @loaded) && scalar(grep { $_ eq $common } @loaded) &&
                   !grep { index(lc($_), 'netsnmp') >= 0 } @loaded];
    my $receipt = receipt(stdout_of($out, $name));
    push @checks, [$name . '_real_component',
                   defined $receipt && defined $receipt->{status} && $receipt->{status} eq 'PASS' &&
                   defined $receipt->{checks} && $receipt->{checks} > 0];
    for my $extra (split /,/, $extras) {
        if ($extra eq 'bounded') {
            push @checks, [$name . '_bounded', read_number("$out/$name.elapsed") < 10 * 1000000];
        } elsif ($extra =~ /^reaped=(.+)$/) {
            my $pid_file = $1;
            push @checks, [basename($pid_file, '.pid') . '_reaped', reaped($pid_file)];
        }
    }
    return @checks;
}

# True when the process that wrote the pid file is gone: its start time in /proc differs from the recorded one, or it has no entry.
sub reaped {
    my ($pid_file) = @_;
    my $identity = decode_json(read_file($pid_file));
    my $stat = "/proc/$identity->{pid}/stat";
    return 1 unless -e $stat;
    my @fields = split ' ', read_file($stat);
    return $fields[21] ne $identity->{start} ? 1 : 0;
}

sub ioc_independence {
    my ($out, $name) = @_;
    my @loaded = loaded_of($out, $name);
    my $production = real("$ROOT/lib/$ARCH/libsnmp3.so");
    return !grep({ index(lc($_), 'netsnmp') >= 0 } @loaded) && scalar(grep { $_ eq $production } @loaded);
}

my $FROZEN_BEFORE_PROCESS = sub {
    my ($text) = @_;
    my $frozen = index($text, 'snmp3 config: revision=1 frozen=1');
    my $process = index($text, q{dbProcess of 'Config_PiniProbe'});
    return $frozen >= 0 && $process >= 0 && $frozen < $process;
};

# The check that follows the exit and the independence of an IOC run, by the name of the run.
my %IOC_CHECK = (
    'ioc-startup' => sub {
        my ($out, $text, $errors) = @_;
        return ['V3.10_before_PINI_freeze', same_reports($text, '1,0,6,4,3', '1,1,6,4,3', '1,1,6,4,3') &&
                $FROZEN_BEFORE_PROCESS->($text) && $text =~ /\bVAL\s*:\s*42\b/];
    },
    'ioc-replacement' => sub {
        my ($out, $text) = @_;
        return ['V3.5_IOC_whole_replacement', same_reports($text, '2,0,6,4,3')];
    },
    'ioc-load-error' => sub {
        my ($out, $text, $errors) = @_;
        return ['V3.1_IOC_strict_JSON', has($errors, 'JSON rejected') && !has($text, 'echo MUST_NOT_RUN')];
    },
    'ioc-load-preservation' => sub {
        my ($out, $text, $errors) = @_;
        return ['V3.5_IOC_continue_preservation', same_reports($text, '1,0,6,4,3', '1,0,6,4,3') && has($text, 'CONTINUED') &&
                has($errors, 'JSON rejected')];
    },
    'ioc-utf8-error' => sub {
        my ($out, $text, $errors) = @_;
        return ['V3.1_IOC_invalid_UTF8', same_reports($text, '1,0,6,4,3') && has($errors, 'JSON rejected') &&
                !has($text, 'echo MUST_NOT_RUN')];
    },
    'ioc-utf8-preservation' => sub {
        my ($out, $text, $errors) = @_;
        return ['V3.5_IOC_UTF8_preservation', same_reports($text, '1,0,6,4,3', '1,0,6,4,3') && has($text, 'CONTINUED') &&
                has($errors, 'JSON rejected')];
    },
    'ioc-missing-arguments' => sub {
        my ($out, $text, $errors) = @_;
        return ['IOC_argument_failure', has($errors, 'configuration arguments required')];
    },
    'ioc-help' => sub {
        my ($out, $text) = @_;
        return ['V3.13_command_help', has($text, 'nativeProbe is absolute') && has($text, 'no secret values') &&
                has($text, 'Freeze configuration')];
    },
);
for my $state (qw(build start stop cold-stop)) {
    $IOC_CHECK{"ioc-frozen-$state"} = sub {
        my ($out, $text, $errors) = @_;
        return ["V3.10_frozen_$state", has($errors, 'configuration frozen') && !has($text, 'echo MUST_NOT_RUN')];
    };
}

sub case_lines {
    my ($out) = @_;
    return () unless -f "$out/cases.tsv";
    return map { [split /\t/, $_, -1] } grep { length } split /\n/, read_file("$out/cases.tsv");
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

my $SENTINELS;
sub sentinels {
    my ($out) = @_;
    # The generator writes the file after the checks of the native probe; a cell that ended before has no secret to look for.
    $SENTINELS //= [-f "$out/sentinels.txt" ? grep { length } split /\n/, read_file("$out/sentinels.txt") : ()];
    return @$SENTINELS;
}

sub run_names {
    my ($out) = @_;
    my @names = qw(lifecycle native-capabilities native-cli);
    for my $line (case_lines($out)) {
        push @names, $line->[1] if $line->[0] =~ /^(?:component|run|ioc)$/ && -f "$out/$line->[1].rc";
    }
    return @names;
}

sub step_lifecycle {
    my ($out) = @_;
    my $passed = 0;
    if (-f "$out/lifecycle/results.json") {
        my $decoded = receipt(read_file("$out/lifecycle/results.json"));
        $passed = defined $decoded && defined $decoded->{status} && $decoded->{status} eq 'PASS';
    }
    return (['lifecycle_exit', read_number("$out/lifecycle.rc") == 0],
            ['V3.11_regressions', read_number("$out/lifecycle.rc") == 0 && $passed]);
}

sub step_native {
    my ($out) = @_;
    my $capabilities = receipt(stdout_of($out, 'native-capabilities'));
    my @loaded = loaded_of($out, 'native-capabilities');
    my $good = defined $capabilities && defined $capabilities->{schema} && $capabilities->{schema} == 1 &&
        $capabilities->{version} && ref $capabilities->{authentication} eq 'ARRAY' && @{ $capabilities->{authentication} } &&
        ref $capabilities->{privacy} eq 'ARRAY' && @{ $capabilities->{privacy} } && scalar(grep { index($_, 'libnetsnmp') >= 0 } @loaded);
    return (['native-capabilities_exit', read_number("$out/native-capabilities.rc") == 0],
            ['V3.6_actual_native_catalog', $good],
            ['native-cli_exit', read_number("$out/native-cli.rc") == 2],
            ['native_cli_usage', has(stderr_of($out, 'native-cli'), 'Usage:')]);
}

sub step_case {
    my ($out, $line) = @_;
    my ($kind, $name, $expected, $loader, $input, $bound, $extras) = @$line;
    return (component_checks($out, $name, $expected, $extras)) if $kind eq 'component';
    my $status = read_number("$out/$name.rc");
    my $text = stdout_of($out, $name);
    my $errors = stderr_of($out, $name);
    if ($kind eq 'run') {
        return (["${name}_exit", $status == $expected],
                ['V3.13_documented_startup', same_reports($text, '1,0,1,1,1', '1,1,1,1,1', '1,1,1,1,1') &&
                 $FROZEN_BEFORE_PROCESS->($text)]);
    }
    my @checks = (["${name}_exit", $status == $expected], ["${name}_IOC_independence", ioc_independence($out, $name)]);
    my $specific = $IOC_CHECK{$name} or die "no check for the run $name\n";
    push @checks, $specific->($out, $text, $errors);
    return @checks;
}

sub step_final {
    my ($out) = @_;
    my @secrets = sentinels($out);
    my $leaked = 0;
    for my $name (run_names($out)) {
        for my $text (stdout_of($out, $name), stderr_of($out, $name)) {
            $leaked = 1 if grep { index($text, $_) >= 0 } @secrets;
        }
    }
    my %before = digests("$out/sha256.before");
    my %after = digests("$out/sha256.after");
    my $same = keys(%before) == keys(%after);
    for my $path (keys %before) {
        $same = 0 unless defined $after{$path} && $after{$path} eq $before{$path};
    }
    return (['V3.12_secret_diagnostics', !$leaked], ['V3.11_inputs_unchanged', $same]);
}

# Judges the steps in order up to the named stage (all when none is named). Returns the listed checks; the last one is the failed
# check when there is one.
sub evaluate {
    my ($out, $upto) = @_;
    my @checks;
    my $run = sub {
        for my $check (@_) {
            push @checks, { name => $check->[0], passed => $check->[1] ? 1 : 0 };
            return 0 unless $check->[1];
        }
        return 1;
    };
    return @checks unless $run->(step_lifecycle($out));
    return @checks if defined $upto && $upto eq 'lifecycle';
    return @checks unless $run->(step_native($out));
    return @checks if defined $upto && $upto eq 'native-cli';
    for my $line (case_lines($out)) {
        my ($kind, $name) = @$line;
        if ($kind eq 'stage') {
            return @checks if defined $upto && $upto eq $name;
            next;
        }
        if ($kind eq 'repeater') {
            return @checks unless $run->(['owned_repeater_ready', read_number("$out/repeater.owned") == 1]);
            return @checks if defined $upto && $upto eq 'repeater';
            next;
        }
        return @checks unless $run->(step_case($out, $line));
    }
    return @checks if defined $upto;
    $run->(step_final($out));
    return @checks;
}

sub fixture_state {
    my ($out) = @_;
    my (%sha, %meta, $error);
    %sha = digests("$out/fixtures-pre.sha256") if -f "$out/fixtures-pre.sha256" && -s "$out/fixtures-pre.sha256";
    %sha = (%sha, digests("$out/fixtures.sha256")) if -f "$out/fixtures.sha256" && -s "$out/fixtures.sha256";
    for my $line (split /\n/, -f "$out/fixtures.meta" ? read_file("$out/fixtures.meta") : '') {
        my ($path, $mode, $uid, $size) = split /\t/, $line;
        $meta{$path} = { mode => sprintf('0%o', hex($mode)), uid => $uid + 0, size => $size + 0 };
    }
    for my $line (split /\n/, -f "$out/fixtures.unreadable" ? read_file("$out/fixtures.unreadable") : '') {
        $meta{$line}{hash_read_before_restriction} = JSON::PP::true;
        unless (defined $sha{$line}) {
            $error = 'fixture hash unavailable';
        }
    }
    return (\%sha, \%meta, $error);
}

sub library_digests {
    my ($loaded) = @_;
    my %paths;
    for my $list (values %$loaded) {
        $paths{$_} = 1 for @$list;
    }
    my @existing = grep { -f $_ } sort keys %paths;
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
    my ($out, @options) = @_;
    defined $out && -d $out or die "usage: config.pl judge OUTPUT [--stage NAME]\n";
    my $stage;
    if (@options) {
        @options == 2 && $options[0] eq '--stage' or die "usage: config.pl judge OUTPUT [--stage NAME]\n";
        $stage = $options[1];
    }
    my @checks = evaluate($out, $stage);
    my $failed = grep { !$_->{passed} } @checks;
    return $failed ? 1 : 0 if defined $stage;
    my %cleanup;
    if (-f "$out/repeater.returncode") {
        my ($pid, $status) = split /\n/, read_file("$out/repeater.returncode");
        my $forced = read_number("$out/repeater.forced");
        %cleanup = (repeater => { pid => $pid + 0, returncode => $status + 0, reaped => JSON::PP::true });
        $cleanup{repeater}{forced} = $forced ? JSON::PP::true : JSON::PP::false;
        $cleanup{repeater}{ok} = ($status == 143 && !$forced) ? JSON::PP::true : JSON::PP::false;
    }
    my $cleanup_ok = !%cleanup || $cleanup{repeater}{ok};
    my (%loaded, %not_run);
    for my $name (run_names($out)) {
        $loaded{$name} = [loaded_of($out, $name)];
    }
    if (-f "$out/not-run.txt") {
        for my $line (split /\n/, read_file("$out/not-run.txt")) {
            my ($key, $reason) = split /\t/, $line, 2;
            $not_run{$key} = $reason;
        }
    }
    my (%component_checks, %elapsed);
    for my $line (case_lines($out)) {
        my ($kind, $name, undef, undef, undef, undef, $extras) = @$line;
        next unless $kind eq 'component' && -f "$out/$name.rc";
        my $receipt = receipt(stdout_of($out, $name));
        $component_checks{$name} = $receipt->{checks} + 0 if defined $receipt && defined $receipt->{checks};
        $elapsed{ $name =~ s/^helper-//r } = read_number("$out/$name.elapsed") / 1000000 if $extras =~ /bounded/;
    }
    my ($fixture_sha, $fixture_meta, $fixture_error) = fixture_state($out);
    my %summary = (
        base => base_directory($out),
        checks => [map { { name => $_->{name}, passed => $_->{passed} ? JSON::PP::true : JSON::PP::false } } @checks],
        cleanup => \%cleanup,
        component_checks => \%component_checks,
        elapsed_seconds => \%elapsed,
        fixture_metadata => $fixture_meta,
        fixture_sha256 => $fixture_sha,
        library_sha256 => library_digests(\%loaded),
        loaded_libraries => \%loaded,
        not_run => \%not_run,
    );
    $summary{error} = $fixture_error if defined $fixture_error;
    $summary{ports} = [split ' ', read_file("$out/ports.txt")] if -f "$out/ports.txt";
    $summary{sha256} = { digests("$out/sha256.before") } if -f "$out/sha256.before";
    # The check of the receipt is always listed: it requires that no generated secret appears in the receipt itself.
    my $draft = JSON::PP->new->canonical->encode(\%summary);
    my $receipt_clean = 1;
    for my $secret (sentinels($out)) {
        $receipt_clean = 0 if index($draft, $secret) >= 0;
    }
    push @{ $summary{checks} }, { name => 'V3.12_secret_receipt', passed => $receipt_clean ? JSON::PP::true : JSON::PP::false };
    my $passed = !$failed && $cleanup_ok && $receipt_clean && !defined $fixture_error ? 1 : 0;
    $summary{passed} = $passed ? JSON::PP::true : JSON::PP::false;
    $summary{status} = $passed ? 'PASS' : 'FAIL';
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

# True (exit status 0) when the process holds a socket that is bound to the UDP port.
sub owns {
    my ($pid, $port) = @_;
    $pid =~ /^\d+$/ && $port =~ /^\d+$/ or die "usage: config.pl owns PID PORT\n";
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
            my (undef, $local_port) = split /:/, $columns[1];
            if (hex($local_port) == $port && $sockets{ $columns[9] }) {
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
    defined $command or die "usage: config.pl judge|port|owns ...\n";
    return judge(@arguments) if $command eq 'judge';
    return free_port() if $command eq 'port';
    return owns(@arguments) if $command eq 'owns';
    die "unknown command $command\n";
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "config.pl: $@";
    exit 2;
}
exit $status;
