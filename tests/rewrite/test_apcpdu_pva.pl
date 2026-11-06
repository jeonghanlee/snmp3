#!/usr/bin/env perl
use strict;
use warnings;
use Cwd qw(abs_path);
use Digest::SHA qw(sha256_hex);
use File::Find qw(find);
use Getopt::Long qw(GetOptions);
use JSON::PP;
use Pod::Usage qw(pod2usage);
use POSIX qw(WNOHANG setsid);
use Time::HiRes qw(clock_gettime CLOCK_MONOTONIC sleep time);

# This local integration tool targets the measured Debian 13 Perl 5.40.1 host.
# The existing APC runner owns the real IOC and shipped outer UDP fixtures.
my $JSON = JSON::PP->new->canonical->pretty;
my $LIMIT_SECONDS = 240;
my $LIMIT_BYTES = 20 * 1024 * 1024;
my $FIELD = 'ap8932.phases.ch01.current';
my $ONLINE = 'ap8932.online';
my @UNITS = qw(PDU1 PDU2);
my @children;
my @commands;
my %identities;
my $output;

sub bytes {
    my ($path) = @_;
    open(my $fh, '<:raw', $path) or die "$path: $!\n";
    my $data = do { local $/; <$fh> };
    close($fh) or die "$path: $!\n";
    return defined($data) ? $data : '';
}

sub save {
    my ($path, $data) = @_;
    my $temporary = "$path.tmp";
    open(my $fh, '>:raw', $temporary) or die "$temporary: $!\n";
    print {$fh} $data or die "$temporary: $!\n";
    close($fh) or die "$temporary: $!\n";
    rename($temporary, $path) or die "$path: $!\n";
    return;
}

sub object {
    my ($path) = @_;
    my $value = $JSON->decode(bytes($path));
    ref($value) eq 'HASH' or die "$path: expected object\n";
    return $value;
}

sub identify {
    my ($path) = @_;
    $identities{$path} = sha256_hex(bytes($path));
    return;
}

sub spawn {
    my ($name, $argv, $environment) = @_;
    my $pid = fork();
    defined($pid) or die "fork $name: $!\n";
    if (!$pid) {
        setsid() >= 0 or POSIX::_exit(126);
        $SIG{INT} = $SIG{QUIT} = $SIG{TERM} = 'DEFAULT';
        open(STDIN, '<', '/dev/null') or POSIX::_exit(126);
        open(STDOUT, '>', "$output/$name.stdout") or POSIX::_exit(126);
        open(STDERR, '>', "$output/$name.stderr") or POSIX::_exit(126);
        %ENV = %$environment;
        { no warnings 'exec'; exec { $argv->[0] } @$argv; }
        POSIX::_exit(127);
    }
    my $child = {name => $name, argv => $argv, pid => $pid,
                 start_utc => time(), start_monotonic => clock_gettime(CLOCK_MONOTONIC)};
    push @children, $child;
    push @commands, $child;
    return $child;
}

sub poll {
    my ($child) = @_;
    return 1 if exists $child->{native_status};
    my $waited = waitpid($child->{pid}, WNOHANG);
    if ($waited == $child->{pid}) {
        my $status = $?;
        $child->{native_status} = $status >> 8;
        $child->{signal} = $status & 127;
        $child->{end_utc} = time();
        $child->{end_monotonic} = clock_gettime(CLOCK_MONOTONIC);
        return 1;
    }
    die "waitpid $child->{pid}: $!\n" if $waited == -1;
    return 0;
}

sub run {
    my ($name, $argv, $environment) = @_;
    my $child = spawn($name, $argv, $environment);
    my $deadline = clock_gettime(CLOCK_MONOTONIC) + 15;
    until (poll($child)) {
        die "$name: command timeout\n" if clock_gettime(CLOCK_MONOTONIC) > $deadline;
        sleep(0.05);
    }
    return $child;
}

sub stop_child {
    my ($child) = @_;
    return if poll($child);
    kill('INT', $child->{pid}) or die "SIGINT $child->{pid}: $!\n";
    my $deadline = clock_gettime(CLOCK_MONOTONIC) + 20;
    until (poll($child)) {
        if (clock_gettime(CLOCK_MONOTONIC) > $deadline) {
            kill('KILL', -$child->{pid}) or die "SIGKILL $child->{pid}: $!\n";
            waitpid($child->{pid}, 0) == $child->{pid} or die "reap $child->{pid}: $!\n";
            my $status = $?;
            $child->{native_status} = $status >> 8;
            $child->{signal} = $status & 127;
            $child->{forced_stop} = JSON::PP::true;
            last;
        }
        sleep(0.05);
    }
    return;
}

sub mappings {
    my ($pid) = @_;
    my $text = bytes("/proc/$pid/maps");
    for my $line (split /\n/, $text) {
        next unless $line =~ /\s(\/[^\n]+)$/;
        my $path = $1;
        identify($path) if -f $path && !exists $identities{$path};
    }
    save("$output/ioc-maps.txt", $text);
    return;
}

sub collect {
    my ($apc, $base, $pvxs) = @_;
    mkdir($output, 0700) or die "$output: $!\n";
    my %environment = %ENV;
    $environment{PYTHONDONTWRITEBYTECODE} = 1;
    $environment{EPICS_BASE} = $base;
    $environment{EPICS_IOCSH_HISTFILE} = "$output/iocsh-history.txt";
    for my $path ($0, "$apc/tests/test_ap8932.py", "$apc/tests/snmp_peer.py",
                  "$apc/mibs/PowerNet-MIB", "$apc/docs/ap8932-inventory.json",
                  "$apc/bin/linux-x86_64/apcpdu", "$apc/dbd/apcpdu.dbd",
                  "$apc/db/ap8932.db", "$apc/db/ap8932_group.json",
                  map { "$apc/iocsh/$_.iocsh" } qw(snmp3 ap8932 ap8932_grouped)) {
        identify(abs_path($path) // die "Missing $path\n");
    }
    for my $tool (qw(pvxget pvxput pvxmonitor)) { identify("$pvxs/bin/linux-x86_64/$tool"); }
    save("$output/contract.json", $JSON->encode({
        scope => 'Current APC group refusal and monitor updates on shipped local fixture lifecycle',
        units => \@UNITS, current_field => $FIELD, online_field => $ONLINE,
        expected_current => 12.3, attempted_current => 99,
        expected_online_transition => [1, 0, 1], expected_alarm_transition => [0, 3, 0],
        timeout_seconds => $LIMIT_SECONDS, evidence_limit_bytes => $LIMIT_BYTES,
        environment => 'Debian 13; Perl 5.40.1; isolated loopback addresses from actual child environment',
        limits => ['No hardware actions', 'No throughput or atomic full-group claim',
                   'Representative live trigger fields; full group mapping is inventoried separately'],
        safeguards => ['Real shipped runner owns fixture state and IOC cleanup',
                       'Native outcomes and raw outputs retained before evaluation',
                       'No sustained test; short real lifecycle is the acceptance duration',
                       'No custom install or service; existing application test entry point is used'],
    }));
    my $runner = spawn('runner', ['python3', "$apc/tests/test_ap8932.py", '--output', "$output/application"], \%environment);
    my ($ioc, %client_env, $problem);
    my @monitors;
    eval {
        local $SIG{INT} = local $SIG{TERM} = sub { die "Interrupted\n"; };
        my $deadline = clock_gettime(CLOCK_MONOTONIC) + $LIMIT_SECONDS;
        until ($ioc) {
            die "Application stopped before IOC discovery\n" if poll($runner);
            die "IOC discovery timeout\n" if clock_gettime(CLOCK_MONOTONIC) > $deadline;
            my $children = bytes("/proc/$runner->{pid}/task/$runner->{pid}/children");
            for my $pid (split /\s+/, $children) {
                next unless -f "/proc/$pid/comm";
                next unless bytes("/proc/$pid/comm") eq "apcpdu\n";
                my @argv = split /\0/, bytes("/proc/$pid/cmdline");
                next unless grep { $_ eq "$output/application/startup.cmd" } @argv;
                $ioc = $pid;
                %client_env = map { split /=/, $_, 2 } split /\0/, bytes("/proc/$pid/environ");
                last;
            }
            sleep(0.02) unless $ioc;
        }
        save("$output/client-environment.json", $JSON->encode({map { $_ => $client_env{$_} }
            grep { /^EPICS_(?:CA|PVA)/ } keys %client_env}));
        mappings($ioc);
        for my $unit (@UNITS) {
            push @monitors, spawn("monitor-$unit", ['stdbuf', '-oL', '-eL', "$pvxs/bin/linux-x86_64/pvxmonitor",
                '-F', 'delta', '-r', "field($FIELD,$ONLINE)", "APCTEST:$unit:group"], \%client_env);
        }
        for my $unit (@UNITS) {
            my $ready;
            for my $attempt (1 .. 30) {
                my $name = "ready-$unit-$attempt";
                my $get = run($name, ["$pvxs/bin/linux-x86_64/pvxget", '-w', '1', '-F', 'delta',
                    '-r', "field($FIELD)", "APCTEST:$unit:group"], \%client_env);
                my $text = bytes("$output/$name.stdout");
                if (!$get->{native_status} && !$get->{signal} &&
                    $text =~ /\Q$FIELD\E\.value\s+double\s+=\s+12\.3\s*$/m &&
                    $text =~ /\Q$FIELD\E\.alarm\.severity\s+int32_t\s+=\s+0\s*$/m) { $ready = 1; last; }
                die "Application stopped before valid PVA value\n" if poll($runner);
                sleep(0.1);
            }
            die "No valid PVA baseline for $unit\n" unless $ready;
            run("put-$unit", ["$pvxs/bin/linux-x86_64/pvxput", '-w', '2',
                "APCTEST:$unit:group", "$FIELD.value=99"], \%client_env);
            run("after-put-$unit", ["$pvxs/bin/linux-x86_64/pvxget", '-w', '2', '-F', 'delta',
                '-r', "field($FIELD)", "APCTEST:$unit:group"], \%client_env);
        }
        until (poll($runner)) {
            die "Application deadline exceeded\n" if clock_gettime(CLOCK_MONOTONIC) > $deadline;
            my $size = 0;
            find({no_chdir => 1, wanted => sub { $size += -s $_ if -f $_; }}, $output);
            die "Evidence size limit exceeded\n" if $size > $LIMIT_BYTES;
            sleep(0.1);
        }
        1;
    } or do { $problem = "$@"; };
    for my $child (reverse @children) {
        eval { stop_child($child); 1 } or do { $problem = ($problem // '') . "$@"; };
    }
    my $ioc_left = $ioc && -d "/proc/$ioc" ? JSON::PP::true : JSON::PP::false;
    $problem = ($problem // '') . "IOC still present\n" if $ioc_left;
    my @changed = grep { !-f $_ || sha256_hex(bytes($_)) ne $identities{$_} } sort keys %identities;
    $problem = ($problem // '') . "Input identity changed\n" if @changed;
    save("$output/identities.json", $JSON->encode(\%identities));
    save("$output/terminal.json", $JSON->encode({commands => \@commands, error => $problem,
        ioc_pid => $ioc, ioc_left => $ioc_left, changed_inputs => \@changed, collected_utc => time()}));
    my %members;
    find({no_chdir => 1, wanted => sub {
        return unless -f $_;
        my $name = substr($_, length($output) + 1);
        $members{$name} = {sha256 => sha256_hex(bytes($_)), bytes => -s $_};
    }}, $output);
    save("$output/manifest.json", $JSON->encode(\%members));
    return;
}

sub sequence {
    my ($text, $field) = @_;
    my @values = $text =~ /^\s*\Q$field\E\s+\S+\s+=\s+(-?\d+)\s*$/mg;
    my @distinct;
    for my $value (@values) { push @distinct, 0 + $value if !@distinct || $distinct[-1] != $value; }
    return \@distinct;
}

sub evaluate {
    my ($directory, $result_path) = @_;
    my $members = object("$directory/manifest.json");
    for my $name (sort keys %$members) {
        die "Invalid manifest name\n" if $name =~ m{(?:^|/)\.\.(?:/|$)} || $name =~ m{^/};
        my $entry = $members->{$name};
        ref($entry) eq 'HASH' && defined($entry->{sha256}) or die "Invalid member $name\n";
        my $text = bytes("$directory/$name");
        die "Hash mismatch: $name\n" unless sha256_hex($text) eq $entry->{sha256};
    }
    my $terminal = object("$directory/terminal.json");
    ref($terminal->{commands}) eq 'ARRAY' or die "Missing command outcomes\n";
    my %command = map { $_->{name} => $_ } @{$terminal->{commands}};
    my @checks;
    my $check = sub {
        my ($name, $condition, $detail) = @_;
        @_ == 3 or die "Invalid assertion arguments: $name\n";
        push @checks, {name => $name, passed => $condition ? JSON::PP::true : JSON::PP::false, detail => $detail};
        return;
    };
    die "Collection incomplete: $terminal->{error}\n" if defined $terminal->{error};
    my $app = object("$directory/application/result.json");
    $check->('application-runner', $app->{passed} && !$command{runner}{native_status} && !$command{runner}{signal},
             {checks => scalar(@{$app->{checks}}), outcome => $command{runner}});
    for my $unit (@UNITS) {
        my $put = $command{"put-$unit"} // die "Missing put outcome\n";
        my $refusal = bytes("$directory/put-$unit.stderr");
        $check->("$unit:write-refused", !!($put->{native_status} != 0 && !$put->{signal} &&
            $refusal !~ /timeout/i && $refusal =~ /writ|not permitted|not allowed/i), $refusal);
        my $after = $command{"after-put-$unit"} // die "Missing get outcome\n";
        my $value = bytes("$directory/after-put-$unit.stdout");
        $check->("$unit:refused-write-value-unchanged", !!(!$after->{native_status} && !$after->{signal} &&
            $value =~ /\Q$FIELD\E\.value\s+double\s+=\s+12\.3\s*$/m), $value);
        my $monitor = bytes("$directory/monitor-$unit.stdout");
        my $online = sequence($monitor, "$ONLINE.value");
        my $severity = sequence($monitor, "$FIELD.alarm.severity");
        $check->("$unit:monitor-valid-value", scalar($monitor =~ /\Q$FIELD\E\.value\s+double\s+=\s+12\.3\s*$/m), undef);
        if ($unit eq 'PDU1') {
            $check->("$unit:monitor-online-loss-recovery", scalar(join(',', @$online) =~ /(?:^|,)1,0,1(?:,|$)/), $online);
            $check->("$unit:monitor-alarm-loss-recovery", scalar(join(',', @$severity) =~ /(?:^|,)0,3,0(?:,|$)/), $severity);
        } else {
            my $start = 0;
            my $bad = 0;
            for my $state (@$online) { $start = 1 if $state == 1; $bad = 1 if $start && $state != 1; }
            $check->("$unit:monitor-remains-online", $start && !$bad, $online);
        }
    }
    my $failed = grep { !$_->{passed} } @checks;
    my $result = {verdict => $failed ? 'Failed' : 'Passed', passed => scalar(@checks) - $failed,
        failed => $failed, checks => \@checks, manifest_sha256 => sha256_hex(bytes("$directory/manifest.json")),
        evaluator_sha256 => sha256_hex(bytes($0))};
    save($result_path, $JSON->encode($result));
    print "$result->{verdict}: $result->{passed} passed, $failed failed\n";
    return $failed ? 1 : 0;
}

sub main {
    my ($apc, $base, $pvxs, $evaluate, $result, $help);
    GetOptions('apcpdu=s' => \$apc, 'base=s' => \$base, 'pvxs=s' => \$pvxs,
        'output=s' => \$output, 'evaluate=s' => \$evaluate, 'result=s' => \$result, 'help' => \$help)
        or pod2usage(2);
    pod2usage(-exitval => 0, -verbose => 1) if $help;
    if ($evaluate) {
        defined($result) && !defined($output) or pod2usage(2);
        return evaluate($evaluate, $result);
    }
    defined($apc) && defined($base) && defined($pvxs) && defined($output) && !-e $output or pod2usage(2);
    $output =~ m{^/} or die "Output must be absolute\n";
    collect($apc, $base, $pvxs);
    return evaluate($output, "$output/result.json");
}

my $status;
eval { $status = main(); defined($status) or die "Missing status\n"; 1 }
    or do { print STDERR "check-pva: $@"; $status = 2; };
exit $status;

__END__

=head1 NAME

test_apcpdu_pva.pl - Observe the current APC application's PVA group contract

=head1 SYNOPSIS

perl tests/rewrite/test_apcpdu_pva.pl --apcpdu DIR --base DIR --pvxs DIR --output NEW_ABSOLUTE_DIR

perl tests/rewrite/test_apcpdu_pva.pl --evaluate DIR --result NEW_RESULT_PATH

=head1 OPTIONS

--apcpdu, --base and --pvxs identify existing installed products. --output
selects a fresh evidence directory. The unchanged test_ap8932.py entry point
owns fixture generation, the actual IOC and its cleanup. --evaluate checks
frozen evidence without running an IOC; --result names its result file.
--help prints this text. No equipment endpoint is accepted.

=head1 EXIT STATUS

0 means the declared assertions passed, 1 means an observed assertion failed,
and 2 means a usage, execution or incomplete-evidence error. The tool does not
claim whole-group atomicity, performance acceptance or hardware verification.

=cut
