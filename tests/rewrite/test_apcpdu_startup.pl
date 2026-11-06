#!/usr/bin/env perl
use strict;
use warnings;
use Cwd qw(abs_path);
use Digest::SHA qw(sha256_hex);
use Getopt::Long qw(GetOptions);
use IO::Socket::INET;
use JSON::PP;
use Pod::Usage qw(pod2usage);
use POSIX qw(setsid WNOHANG);
use Time::HiRes qw(clock_gettime CLOCK_MONOTONIC sleep time);

# Local qualification uses the actual installed APC main and nested loaders.
# UDP sinks supply only the external device boundary; no replies are required.
my $JSON = JSON::PP->new->canonical->pretty;
my $TIMEOUT = 25;
my (@checks, @commands, %hashes);
my ($apc, $snmp, $output, $control, $help);

sub read_bytes {
    my ($path) = @_;
    open(my $fh, '<:raw', $path) or die "$path: $!\n";
    my $text = do { local $/; <$fh> };
    close($fh) or die "$path: $!\n";
    return defined($text) ? $text : '';
}

sub write_bytes {
    my ($path, $text) = @_;
    open(my $fh, '>:raw', $path) or die "$path: $!\n";
    print {$fh} $text or die "$path: $!\n";
    close($fh) or die "$path: $!\n";
    return;
}

sub check {
    my ($name, $condition, $detail) = @_;
    @_ == 3 or die "Invalid assertion arguments: $name\n";
    push @checks, {name => $name, passed => $condition ? JSON::PP::true : JSON::PP::false, detail => $detail};
    print(($condition ? 'PASS' : 'FAIL') . " $name\n");
    return;
}

sub identify {
    my ($path) = @_;
    $path = abs_path($path) // die "Missing $path\n";
    $hashes{$path} = sha256_hex(read_bytes($path));
    return;
}

sub execute {
    my ($name, $argv, $folder, $stdin) = @_;
    my $pid = fork();
    defined($pid) or die "fork: $!\n";
    if (!$pid) {
        setsid() >= 0 or POSIX::_exit(126);
        $SIG{INT} = $SIG{QUIT} = $SIG{TERM} = 'DEFAULT';
        chdir($folder) or POSIX::_exit(126);
        open(STDIN, '<', $stdin // '/dev/null') or POSIX::_exit(126);
        open(STDOUT, '>', "$folder/$name.stdout") or POSIX::_exit(126);
        open(STDERR, '>', "$folder/$name.stderr") or POSIX::_exit(126);
        { no warnings 'exec'; exec { $argv->[0] } @$argv; }
        POSIX::_exit(127);
    }
    my $deadline = clock_gettime(CLOCK_MONOTONIC) + $TIMEOUT;
    my $native;
    my $captured;
    my $error;
    my $start = time();
    eval {
        local $SIG{INT} = local $SIG{TERM} = sub { die "Interrupted\n"; };
        while (1) {
            my $waited = waitpid($pid, WNOHANG);
            if ($waited == $pid) { $native = $?; last; }
            die "waitpid: $!\n" if $waited == -1;
            if ($name eq 'ioc' && !$captured && -f "$folder/ioc.stdout" &&
                read_bytes("$folder/ioc.stdout") =~ /^M7_AFTER_INIT\s*$/m) {
                my $map = read_bytes("/proc/$pid/maps");
                write_bytes("$folder/loaded-maps.txt", $map);
                for my $line (split /\n/, $map) {
                    identify($1) if $line =~ /\s(\/[^\n]+)$/ && -f $1;
                }
                $captured = 1;
            }
            die "$name deadline exceeded\n" if clock_gettime(CLOCK_MONOTONIC) > $deadline;
            sleep(0.02);
        }
        1;
    } or do {
        $error = "$@";
        # Preserve the original error while stopping this case's process group.
        kill('TERM', -$pid);
        sleep(0.3);
        kill('KILL', -$pid);
        waitpid($pid, 0);
        $native = $?;
    };
    my $result = {name => $name, argv => $argv, pid => $pid, cwd => $folder,
        native_status => $native >> 8, signal => $native & 127, error => $error,
        start_utc => $start, end_utc => time(), maps_captured => $captured ? JSON::PP::true : JSON::PP::false};
    push @commands, $result;
    write_bytes("$output/commands.json", $JSON->encode(\@commands));
    die $error if defined $error;
    return ($result, read_bytes("$folder/$name.stdout") . read_bytes("$folder/$name.stderr"));
}

sub unit_load {
    my ($number) = @_;
    return 'iocshLoad("' . $apc . '/iocsh/ap8932_grouped.iocsh","P=M7START:,R=PDU' . $number .
        ':,U=PDU' . $number . ',L=M7LEG:PDU' . $number . ':,PVX=,OBJ=group")' . "\n";
}

sub module_load {
    my ($configuration) = @_;
    return 'iocshLoad("' . $apc . '/iocsh/snmp3.iocsh","SNMP_CONFIG=' . $configuration . '")' . "\n";
}

sub run_case {
    my ($name, $version, $level, $fault, $later, $templates) = @_;
    my $folder = "$output/$name";
    mkdir($folder, 0700) or die "$folder: $!\n";
    my @sinks;
    for (1 .. 2) {
        push @sinks, IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Proto => 'udp')
            or die "UDP sink: $!\n";
    }
    my $sentinel = 'm7-local-' . substr(sha256_hex("$folder/$$/" . time()), 0, 24);
    my $secret = "$folder/secret.txt";
    write_bytes($secret, "$sentinel\n");
    chmod(0600, $secret) or die "$secret: $!\n";
    my $profile = {id => 'Local', version => $version, timeoutMs => 100, retries => 0};
    if ($version eq '3') {
        $profile->{user} = 'M7Startup';
        $profile->{securityLevel} = $level;
        if ($level ne 'noAuthNoPriv') { $profile->{authAlgorithm} = 'SHA'; $profile->{authSecretFile} = $secret; }
        if ($level eq 'authPriv') { $profile->{privAlgorithm} = 'AES'; $profile->{privSecretFile} = $secret; }
    } else { $profile->{communityFile} = $secret; }
    my $site = {profiles => [$profile], endpoints => [map {
        {id => 'PDU' . ($_ + 1), profile => 'Local', address => '127.0.0.1', port => $sinks[$_]->sockport}
    } 0, 1]};
    write_bytes("$folder/site.json", $JSON->encode($site));
    my ($generated) = execute('generate', ['python3', '-B', "$apc/scripts/gen_ap8932_grouped.py",
        'config', '--site', "$folder/site.json", '--units', 'PDU1,PDU2', '--output', "$folder/config.json"], $folder, undef);
    die "$name: generator failed\n" if $generated->{native_status} || $generated->{signal};
    my $config = $JSON->decode(read_bytes("$folder/config.json"));
    ref($config) eq 'HASH' && ref($config->{bindings}) eq 'ARRAY' or die "Invalid generated configuration\n";
    my $all_get = !grep { $_->{operation} ne 'get' } @{$config->{bindings}};
    check("$name:generated-two-unit-get", @{$config->{endpoints}} == 2 && @{$config->{bindings}} == 792 && $all_get, scalar @{$config->{bindings}});
    my $script = $control ? '' : "on error break\n";
    for my $pair (['SNMP3', $snmp], ['CANDIDATE_TOP', $apc], ['DATABASE_TOP', "$apc/db"] ) {
        $script .= 'epicsEnvSet("' . $pair->[0] . '","' . $pair->[1] . '")' . "\n";
    }
    $script .= 'dbLoadDatabase("' . $apc . '/dbd/apcpdu.dbd")' . "\napcpdu_registerRecordDeviceDriver pdbbase\n";
    if ($fault && $fault eq 'failed-state') {
        $script .= module_load("$folder/config.json") . unit_load(1);
        $script .= "on error continue\n" . 'snmp3WorkerPath("' . $folder . '/missing-worker")' .
            "\niocInit\nsnmp3RuntimeReport\necho M7_SCRIPT_ZERO\n";
    } elsif ($fault) {
        if ($fault eq 'schema') { $config->{schema} = 999; write_bytes("$folder/invalid.json", $JSON->encode($config)); }
        if ($later) {
            $script .= module_load("$folder/config.json") . unit_load(1) . "echo M7_EARLIER_DEVICE\ndbl\n";
        }
        $script .= module_load("$folder/invalid.json") . unit_load($later ? 2 : 1);
    } else {
        $script .= module_load("$folder/config.json") . unit_load(1) . unit_load(2) . "dbl\n";
    }
    $script .= "echo M7_BEFORE_INIT\niocInit\necho M7_AFTER_INIT\nsnmp3RuntimeReport\n"
        unless $fault && $fault eq 'failed-state';
    write_bytes("$folder/startup.cmd", $script);
    write_bytes("$folder/stdin.txt", "echo M7_SHELL\nepicsThreadSleep(0.3)\nsnmp3RuntimeReport\nexit\n");
    local %ENV = %ENV;
    delete @ENV{grep { /^(?:EPICS_CA|EPICS_PVA|LD_)/ } keys %ENV};
    my @ports;
    for (1 .. 3) {
        my $socket = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Listen => 1, Proto => 'tcp')
            or die "Port reservation: $!\n";
        push @ports, $socket->sockport;
        close($socket) or die "Port close: $!\n";
    }
    @ENV{qw(EPICS_CAS_INTF_ADDR_LIST EPICS_PVAS_INTF_ADDR_LIST EPICS_CA_ADDR_LIST)} = ('127.0.0.1') x 3;
    @ENV{qw(EPICS_CA_AUTO_ADDR_LIST EPICS_CAS_AUTO_BEACON_ADDR_LIST EPICS_PVAS_AUTO_BEACON_ADDR_LIST)} = ('NO') x 3;
    @ENV{qw(EPICS_CA_SERVER_PORT EPICS_PVAS_SERVER_PORT EPICS_CA_REPEATER_PORT)} = @ports;
    $ENV{EPICS_PVAS_BROADCAST_PORT} = $ports[1];
    $ENV{EPICS_IOCSH_HISTFILE} = "$folder/history.txt";
    $ENV{LD_DEBUG} = 'libs';
    $ENV{LD_DEBUG_OUTPUT} = "$folder/linker";
    my ($process, $log) = execute('ioc', ["$apc/bin/linux-x86_64/apcpdu", "$folder/startup.cmd"], $folder, "$folder/stdin.txt");
    opendir(my $directory, $folder) or die "$folder: $!\n";
    my @linker_files = grep { /^linker\.\d+$/ } readdir($directory);
    closedir($directory) or die "$folder: $!\n";
    my %loaded;
    for my $file (@linker_files) {
        my $trace = read_bytes("$folder/$file");
        for my $library ($trace =~ /calling init:\s+(\/[^\s]+)/g) {
            identify($library);
            $loaded{$library} = sha256_hex(read_bytes($library));
        }
    }
    write_bytes("$folder/loaded-libraries.json", $JSON->encode(\%loaded));
    check("$name:credentials-absent", index($log, $sentinel) == -1, undef);
    check("$name:no-signal", !$process->{signal}, $process->{signal});
    if ($fault && $fault eq 'failed-state') {
        check("$name:failed-state-exposed", !!($log =~ /state=Failed/ && $log =~ /^M7_SCRIPT_ZERO\s*$/m), undef);
        check("$name:main-rejects-failed-state", !!($process->{native_status} == 1 && $log =~ /startup script failed/), $process->{native_status});
        check("$name:before-shell", $log !~ /^M7_SHELL\s*$/m, undef);
    } elsif ($fault) {
        check("$name:native-rejection", $process->{native_status} == 1, $process->{native_status});
        check("$name:before-init", $log !~ /^M7_(?:BEFORE|AFTER)_INIT\s*$/m && $log !~ /Starting iocInit|iocRun: All initialization complete/, undef);
        check("$name:before-shell", $log !~ /^M7_SHELL\s*$/m, undef);
        check("$name:actual-loader-error", !!($log =~ /snmp3:/ && $log =~ /startup script failed/), undef);
        check("$name:earlier-device-loaded", scalar($log =~ /^M7_EARLIER_DEVICE\s*$/m), undef) if $later;
    } else {
        check("$name:native-success", $process->{native_status} == 0, $process->{native_status});
        check("$name:init-and-shell", !!($log =~ /^M7_AFTER_INIT\s*$/m && $log =~ /^M7_SHELL\s*$/m &&
            $log =~ /iocRun: All initialization complete/ && $log =~ /state=Running/), undef);
        check("$name:loaded-identity", $process->{maps_captured}, undef);
        check("$name:actual-native-library-loaded", scalar(grep { /libsnmp3Native\.so/ } keys %loaded) > 0, undef);
    }
    if (!$fault || $later) {
        my %expected;
        for my $unit (1 .. ($fault ? 1 : 2)) {
            for my $template (@$templates) {
                my $record = $template;
                $record =~ s/\$\(P\)/M7START:/g;
                $record =~ s/\$\(R\)/PDU$unit:/g;
                $record =~ s/\$\(L\)/M7LEG:PDU$unit:/g;
                $expected{$record} = 1;
            }
        }
        my %actual = map { $_ => 1 } $log =~ /^(_?M7(?:START|LEG):\S+)\s*$/mg;
        my @missing = grep { !$actual{$_} } keys %expected;
        my @extra = grep { !$expected{$_} } keys %actual;
        check("$name:record-inventory", !@missing && !@extra && keys(%expected) > 0,
            {expected => scalar(keys %expected), actual => scalar(keys %actual), missing => \@missing, extra => \@extra});
    }
    for my $sink (@sinks) { close($sink) or die "UDP sink close: $!\n"; }
    return;
}

sub main {
    GetOptions('apcpdu=s' => \$apc, 'snmp3=s' => \$snmp, 'output=s' => \$output,
        'control-continue' => \$control, 'help' => \$help) or pod2usage(2);
    pod2usage(-exitval => 0, -verbose => 1) if $help;
    defined($apc) && defined($snmp) && defined($output) && !-e $output or pod2usage(2);
    $output =~ m{^/} or die "Output must be absolute\n";
    mkdir($output, 0700) or die "$output: $!\n";
    for my $file ($0, "$apc/bin/linux-x86_64/apcpdu", "$apc/dbd/apcpdu.dbd", "$apc/db/ap8932.db",
        "$apc/db/ap8932_group.json", "$apc/scripts/gen_ap8932_grouped.py", "$apc/scripts/gen_ap8932.py",
        "$apc/scripts/ap8932_compat.py", "$apc/docs/ap8932-inventory.json", "$apc/mibs/PowerNet-MIB",
        "$snmp/bin/linux-x86_64/snmp3Worker", "$snmp/bin/linux-x86_64/snmp3NativeProbe",
        map { "$apc/iocsh/$_.iocsh" } qw(snmp3 ap8932 ap8932_grouped)) { identify($file); }
    my @templates = read_bytes("$apc/db/ap8932.db") =~ /^record\(\w+,\s*"([^"]+)"\)/mg;
    @templates == 2092 or die "Unexpected current record inventory\n";
    my $error;
    eval {
        if (!$control) {
            for my $valid (['1', undef], ['2c', undef], ['3', 'noAuthNoPriv'], ['3', 'authNoPriv'], ['3', 'authPriv']) {
                run_case('valid-' . ($valid->[1] // $valid->[0]), @$valid, undef, 0, \@templates);
            }
        }
        for my $later ($control ? (0) : (0, 1)) {
            for my $fault ($control ? ('schema') : ('schema', 'missing-file')) {
                run_case(($later ? 'later-' : 'first-') . $fault, '2c', undef, $fault, $later, \@templates);
            }
        }
        run_case('failed-state', '2c', undef, 'failed-state', 0, \@templates) unless $control;
        1;
    } or do { $error = "$@"; };
    for my $file (sort keys %hashes) { check("identity:$file", sha256_hex(read_bytes($file)) eq $hashes{$file}, undef); }
    my $failed = grep { !$_->{passed} } @checks;
    write_bytes("$output/result.json", $JSON->encode({verdict => $error ? 'Incomplete' : $failed ? 'Failed' : 'Passed',
        checks => \@checks, failed => $failed, error => $error, identities => \%hashes,
        control_continue => $control ? JSON::PP::true : JSON::PP::false, commands => \@commands}));
    print "Startup: " . scalar(@checks) . " assertions, $failed failed" . ($error ? ", $error" : "\n");
    return $error ? 2 : $failed ? 1 : 0;
}

my $status;
eval { $status = main(); defined($status) or die "Missing status\n"; 1 }
    or do { print STDERR "check-startup: $@"; $status = 2; };
exit $status;

__END__

=head1 NAME

test_apcpdu_startup.pl - Qualify current grouped APC startup on local UDP sinks

=head1 SYNOPSIS

perl tests/rewrite/test_apcpdu_startup.pl --apcpdu DIR --snmp3 DIR --output NEW_ABSOLUTE_DIR [--control-continue]

=head1 OPTIONS

--apcpdu and --snmp3 identify actual installed products. --output selects a
fresh case directory. --control-continue omits the enclosing script's error
break and runs the first invalid-schema case; its rejection checks must fail.
--help displays usage. This local tool targets Debian 13 with Perl 5.40.1.

=head1 CONTRACT

Five valid security configurations must initialize both actual grouped units
and reach the shell. Invalid schema and missing configuration at the first
and later position must terminate nonzero before initialization or shell entry.
The main must also reject a Failed runtime after the outer script returns zero.
Loaded libraries, real record inventories and credential sentinels are checked.
No remote SNMP response, performance or new hardware claim is made. Each case
has a fresh IOC, loopback sinks, a 25-second command limit and retained logs.

=head1 EXIT STATUS

0 is Passed, 1 is observed Failed, and 2 is usage, execution or missing evidence.
The deliberate continue-after-error control is expected to exit 1.

=cut
