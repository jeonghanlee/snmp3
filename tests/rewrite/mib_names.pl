#!/usr/bin/env perl
# Startup writer, record reader and judge of the MIB names cell. test_mib_names.bash starts and stops the fixture
# agents and the production IOCs; this script writes the MIB files, the startup script and the database of a case,
# drives the IOC shell through a FIFO, keeps what it reads as observation files, and judges the retained
# observations afterwards. Every case has its own IOC, its own agent and its own MIB files.
#
# The IOCs of this cell name objects through a MIB:
#   snmp3Mib path [system]
#   record link "@<session> <MODULE>::<name>.<index> [type=<type>]"
#   snmp3Show pattern 1 prints "snmp3 mib: module=<MODULE> place=<ioc|module|system> file=<path>",
#   "snmp3 mib: probeStarts=<n>" and, on the "snmp3 object:" line of an object given by name,
#   "name=<text> typeSource=<mib|link>".
# Expected values are literals of these rules:
#   a name resolves to the identifier, the type and the access its MIB declares, so a record by name reads the
#     value a record by number reads, and its type is the declared one unless the link carries type=;
#   a MIB module is taken from the first place that holds it, in the order IOC, module, system;
#   a record whose name does not resolve or carries no index, whose record type cannot hold the declared type,
#     that writes an object declared read-only, or that reads an object declared not readable is rejected at
#     initialization and named;
#   a module that no place holds is named once with "snmp3: MIB module <MODULE> not found";
#   the probe is started once for all names of an IOC, and not at all for an IOC without names.
#
# Table of checks. "obs" names the observation file of the case a value is read from.
#   check               case           input                                   state read                      expected, source
#   ioc-value           place-ioc      the MIB in a directory of the IOC;      SEVR and VAL of the record by   NO_ALARM NO_ALARM 4321 4321,
#   module-value        place-module   in the mibs directory of the module;    number and of the one by name   the literal written before
#   system-value        place-system   in a directory given as system          after 4321 was written
#   ioc-shown           place-ioc      the same IOCs                           "snmp3 mib: module=" lines and  the place of the case, the path
#   module-shown        place-module                                           the "snmp3 object:" line of     the MIB was written to, one
#   system-shown        place-system                                           the record by name; in          module line (only a module that
#                                                                              place-ioc also the object       was read is shown), the declared
#                                                                              with type=                      type integer, mib; link for the
#                                                                                                              object with type=
#   two-places          two-places     the MIB as a file of the IOC, and a     SEVR and VAL of both records;   NO_ALARM NO_ALARM 4321 4321
#                                      MIB of the same module with another     place and file of the module    ioc and the file of the IOC
#                                      identifier in the system directory
#   wrong-records       wrong-records  a text record on an integer object, an  "record initialization          the five records, and the
#                                      output record on a read-only object,    rejected" lines; SEVR of the    valid one NO_ALARM
#                                      a name the MIB does not hold, a name    valid record
#                                      without its index, an input record on
#                                      an object declared not-accessible, and
#                                      one valid record by name
#   none-rejected       none           no place holds the MIB; two records by  "record initialization          the two records by name; the
#                                      name and one by number                  rejected" lines; SEVR of the    record by number NO_ALARM
#                                                                              record by number
#   none-module-named   none           the same IOC                            standard error                  one "MIB module ... not found"
#                                                                                                              line with the module
#   start-number        start-number   500 records by number, three starts     "snmp3 mib: probeStarts=";      0, NO_ALARM and the literal of
#                                                                              SEVR, VAL of the first record   the first object
#   start-name          start-name     the same 500 objects by name            the same                        1, NO_ALARM and the literal
# The two start cases also record the time from the start of the process to the line of complete initialization
# for each of their three starts; the times are kept in results.json and are not judged.
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use IO::Socket::INET;
use JSON::PP;
use POSIX qw(strftime);
use Time::HiRes qw(sleep time clock_gettime CLOCK_MONOTONIC);

my $OID_BASE = '1.3.6.1.4.1.53864.4.';
my ($OID_INTEGER, $OID_OCTETS, $OID_HIDDEN, $OID_FAR) = (1, 6, 2, 31);
my $MODULE = 'SNMP3-FIXTURE-MIB';
my $START_MODULE = 'SNMP3-START-MIB';
my $START_BASE = '1.3.6.1.4.1.318.99.1.';
my $START_COUNT = 500;
my $START_FIRST = 7001;
my $WRITTEN = 4321;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my $ASN_INTEGER = 2;
my @PLACES = qw(ioc module system);
my @CASES = ((map { "place-$_" } @PLACES), 'two-places', 'wrong-records', 'none', 'start-number', 'start-name');

sub write_file {
    my ($path, $text) = @_;
    open(my $fh, '>', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub read_file {
    my ($path) = @_;
    open(my $fh, '<', $path) or die "cannot read $path: $!\n";
    local $/;
    my $text = <$fh>;
    close($fh);
    return defined $text ? $text : '';
}

sub encode { return JSON::PP->new->canonical->pretty->encode($_[0]); }

sub command_cases {
    print join(' ', @CASES), "\n";
    return 0;
}

# Prints a UDP port that was free on the loopback address when the command ran.
sub command_port {
    my $socket = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Proto => 'udp')
        or die "cannot bind a loopback UDP socket: $!\n";
    print $socket->sockport, "\n";
    close($socket);
    return 0;
}

# The fixture MIB, written without imports. The integer object sits at the index given.
sub fixture_mib {
    my ($integer_index) = @_;
    return join("\n",
        "$MODULE DEFINITIONS ::= BEGIN", '',
        'fixture OBJECT IDENTIFIER ::= { iso 3 6 1 4 1 53864 }',
        'fixtureObjects OBJECT IDENTIFIER ::= { fixture 4 }', '',
        'fixtureInteger OBJECT-TYPE', '    SYNTAX      INTEGER', '    ACCESS      read-write', '    STATUS      mandatory',
        '    DESCRIPTION "Integer value of the fixture agent."', "    ::= { fixtureObjects $integer_index }", '',
        'fixtureText OBJECT-TYPE', '    SYNTAX      OCTET STRING', '    ACCESS      read-only', '    STATUS      mandatory',
        '    DESCRIPTION "Text value of the fixture agent."', "    ::= { fixtureObjects $OID_OCTETS }", '',
        'fixtureHidden OBJECT-TYPE', '    SYNTAX      INTEGER', '    ACCESS      not-accessible', '    STATUS      mandatory',
        '    DESCRIPTION "A value the MIB declares as not readable."', "    ::= { fixtureObjects $OID_HIDDEN }", '',
        'END', '');
}

sub start_mib {
    my @text = ("$START_MODULE DEFINITIONS ::= BEGIN", '',
                'startObjects OBJECT IDENTIFIER ::= { iso 3 6 1 4 1 318 99 1 }', '');
    for my $n (1 .. $START_COUNT) {
        push @text, "startValue$n OBJECT-TYPE", '    SYNTAX      INTEGER', '    ACCESS      read-only',
            '    STATUS      mandatory', '    DESCRIPTION "Value of the start time test."', "    ::= { startObjects $n }", '';
    }
    return join("\n", @text, 'END', '');
}

sub record {
    my ($type, $name, $field, $link, @fields) = @_;
    return "record($type, \"$name\") {\n    field(DTYP, \"snmp3\")\n    field($field, \"$link\")\n"
        . join('', map { "    field($_)\n" } @fields) . "}\n";
}

# Writes the MIB files, startup.cmd and records.db of one case into its directory, and expected.json with the
# place and file the judge expects. Arguments after the case: directory, tree root, products directory, secrets
# directory, the port of the agent.
sub command_prepare {
    my ($case, $dir, $root, $products, $secrets, $agent) = @_;
    my @head = ("dbLoadDatabase(\"$root/dbd/snmp3Ioc.dbd\")", 'snmp3Ioc_registerRecordDeviceDriver(pdbbase)',
                "snmp3WorkerPath(\"$products/snmp3Worker\")", "snmp3ProbePath(\"$products/snmp3NativeProbe\")");
    my @lines = ("snmp3Session S 127.0.0.1:$agent version=2c communityFile=$secrets/community timeoutMs=300 retries=0");
    my $db = '';
    my %expected;
    my $by_number = "\@S $OID_BASE$OID_INTEGER.0 type=integer";
    my $by_name = "\@S ${MODULE}::fixtureInteger.0";
    mkdir "$dir/$_" or die "cannot create $dir/$_: $!\n" for qw(mib-near mib-far);
    write_file("$dir/mib-near/$MODULE", fixture_mib($OID_INTEGER));
    write_file("$dir/mib-far/$MODULE", fixture_mib($OID_FAR));
    if ($case =~ /^place-(\w+)$/ || $case eq 'two-places') {
        my $place = $case eq 'two-places' ? 'ioc' : $1;
        push @lines, "snmp3Mib $dir/mib-near" if $case eq 'place-ioc';
        push @lines, "snmp3Mib $dir/mib-near system" if $case eq 'place-system';
        push @lines, "snmp3Mib $dir/mib-near/$MODULE", "snmp3Mib $dir/mib-far system" if $case eq 'two-places';
        %expected = (place => $place, file => $place eq 'module' ? "$root/mibs/$MODULE" : "$dir/mib-near/$MODULE");
        $db .= record('longout', 'N_Set', 'OUT', $by_number);
        $db .= record('longin', 'N_Number', 'INP', $by_number);
        $db .= record('longin', 'N_Name', 'INP', $by_name);
        $db .= record('longin', 'N_Override', 'INP', "$by_name type=integer") if $case eq 'place-ioc';
    } elsif ($case eq 'wrong-records') {
        push @lines, "snmp3Mib $dir/mib-near";
        $db .= record('stringin', 'X_Type', 'INP', $by_name);
        $db .= record('stringout', 'X_ReadOnly', 'OUT', "\@S ${MODULE}::fixtureText.0");
        $db .= record('longin', 'X_NoName', 'INP', "\@S ${MODULE}::noSuchObject.0");
        $db .= record('longin', 'X_NoIndex', 'INP', "\@S ${MODULE}::fixtureInteger");
        $db .= record('longin', 'X_Hidden', 'INP', "\@S ${MODULE}::fixtureHidden.0");
        $db .= record('longin', 'X_Good', 'INP', $by_name);
    } elsif ($case eq 'none') {
        $db .= record('longin', 'Z_A', 'INP', $by_name);
        $db .= record('stringin', 'Z_B', 'INP', "\@S ${MODULE}::fixtureText.0");
        $db .= record('longin', 'Z_Number', 'INP', $by_number);
    } elsif ($case eq 'start-number' || $case eq 'start-name') {
        mkdir "$dir/mib-start" or die "cannot create $dir/mib-start: $!\n";
        write_file("$dir/mib-start/$START_MODULE", start_mib());
        write_file("$dir/objects", join('', map { sprintf("%s%d.0 %d %d\n", $START_BASE, $_, $ASN_INTEGER, $START_FIRST + $_ - 1) } 1 .. $START_COUNT));
        chmod(0600, "$dir/objects") or die "cannot set the mode of $dir/objects: $!\n";
        push @lines, "snmp3Mib $dir/mib-start" if $case eq 'start-name';
        for my $n (1 .. $START_COUNT) {
            $db .= record('longin', "T_$n", 'INP', $case eq 'start-name' ? "\@S ${START_MODULE}::startValue$n.0"
                                                                         : "\@S $START_BASE$n.0 type=integer");
        }
    } else {
        die "unknown case $case\n";
    }
    write_file("$dir/expected.json", encode(\%expected));
    write_file("$dir/records.db", $db);
    write_file("$dir/startup.cmd", join("\n", 'on error break', @head, @lines, "dbLoadRecords(\"$dir/records.db\")", 'iocInit') . "\n");
    return 0;
}

sub next_number {
    my ($dir) = @_;
    my $path = "$dir/sequence";
    my $number = -s $path ? read_file($path) + 1 : 1;
    write_file($path, "$number\n");
    return $number;
}

sub open_run {
    my ($dir) = @_;
    mkdir "$dir/steps" if !-d "$dir/steps";
    open(my $fifo, '>', "$dir/ioc.stdin") or die "cannot open $dir/ioc.stdin: $!\n";
    $fifo->autoflush(1);
    return { dir => $dir, fifo => $fifo };
}

# Sends shell lines to the IOC and waits until it has executed all of them: the last line writes a marker file
# through the output redirection of the IOC shell, which runs its lines in order.
sub shell {
    my ($run, @lines) = @_;
    my $marker = sprintf('%s/steps/%06d.done', $run->{dir}, next_number($run->{dir}));
    my $fifo = $run->{fifo};
    print {$fifo} map { "$_\n" } @lines, "echo done > $marker";
    my $end = time + $STEP_SECONDS;
    until (-s $marker) {
        time < $end or die "the IOC did not execute a step within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
    }
    return;
}

# Reads fields of records in one step and returns the printed values in the order asked. A record or field the
# IOC does not hold reads as "none", so that its check fails with that value.
sub fields {
    my ($run, @names) = @_;
    my $base = sprintf('%s/steps/%06d', $run->{dir}, next_number($run->{dir}));
    shell($run, map { "dbgf \"$names[$_]\" > $base-$_.txt" } 0 .. $#names);
    return map {
        my $text = read_file("$base-$_.txt");
        my $value = 'none';
        if ($text =~ /^DB[FR]_\w+:\s*(.*?)\s*$/m) {
            $value = $1;
            $value = $1 if $value =~ /^"(.*)"$/ || $value =~ /^(\S+) = 0x[0-9a-fA-F]+$/;
        }
        $value;
    } 0 .. $#names;
}

sub settle {
    my ($run, @records) = @_;
    my $end = time + $ROUND_SECONDS;
    while (1) {
        my $active = grep { $_ ne '0' } fields($run, map { "$_.PACT" } @records);
        last if !$active || time >= $end;
        sleep $POLL_SECONDS;
    }
    return;
}

# Processes one record and waits until it is not active or the bound has passed.
# usage: process DIR RECORD
sub command_process {
    my ($dir, $record) = @_;
    my $run = open_run($dir);
    shell($run, "dbpf \"$record.PROC\" 1");
    settle($run, $record);
    close($run->{fifo});
    return 0;
}

# Writes a value to a record and waits until it is not active or the bound has passed.
# usage: put DIR RECORD VALUE
sub command_put {
    my ($dir, $record, $value) = @_;
    my $run = open_run($dir);
    shell($run, "dbpf \"$record\" \"$value\"");
    settle($run, $record);
    close($run->{fifo});
    return 0;
}

# Keeps the named fields and the lines of "snmp3Show * 1" as the observation LABEL.
# usage: observe DIR LABEL FIELD...
sub command_observe {
    my ($dir, $label, @names) = @_;
    my $run = open_run($dir);
    my %observation = (label => $label, utc => strftime('%Y-%m-%dT%H:%M:%SZ', gmtime), fields => {});
    my @values = @names ? fields($run, @names) : ();
    @{ $observation{fields} }{@names} = @values;
    my $stdout = "$dir/ioc.stdout";
    my $offset = -s $stdout;
    shell($run, 'snmp3Show * 1', 'snmp3RuntimeReport');
    my $end = time + $STEP_SECONDS;
    my $text = '';
    until ($text =~ /^snmp3 loop: /m) {
        time < $end or die "the report did not reach the standard output file within $STEP_SECONDS s\n";
        sleep $POLL_SECONDS;
        $text = substr(read_file($stdout), $offset);
    }
    $observation{report} = [grep { /^snmp3 / } split /\n/, $text];
    close($run->{fifo});
    write_file("$dir/observe-$label.json", encode(\%observation));
    return 0;
}

my (@checks, %times);

sub check {
    my ($name, $actual, $expected) = @_;
    my $passed = (defined $actual && $actual eq $expected) ? 1 : 0;
    push @checks, { name => $name, passed => $passed ? JSON::PP::true : JSON::PP::false,
                    actual => $actual, expected => $expected };
    printf "%s %s actual=[%s] expected=[%s]\n", $passed ? 'PASS' : 'FAIL', $name,
        defined $actual ? $actual : 'undefined', $expected;
    return;
}

sub observation {
    my ($dir, $label) = @_;
    my $path = "$dir/observe-$label.json";
    return -s $path ? decode_json(read_file($path)) : undef;
}

sub plain_lines {
    my ($path) = @_;
    my @lines = -e $path ? split(/\n/, read_file($path)) : ();
    s/\e\[[0-9;]*m//g for @lines;
    return @lines;
}

# What explains a missing observation: the IOC ended during its startup, with the first line that names a fault.
sub startup_fault {
    my ($dir) = @_;
    return undef if !-e "$dir/ioc-ended";
    my @lines = grep { /not registered|not found|^snmp3: / } plain_lines("$dir/ioc.stderr"), plain_lines("$dir/ioc.stdout");
    return 'IOC ended during startup: ' . (@lines ? $lines[0] : 'no fault line');
}

sub value_of {
    my ($dir, $label, @names) = @_;
    my $obs = observation($dir, $label);
    return startup_fault($dir) // "no observation $label" if !defined $obs;
    return join(' ', map { $obs->{fields}{$_} // 'none' } @names);
}

sub rejected {
    my ($dir) = @_;
    return join(' ', sort map { /^snmp3: record initialization rejected: (\S+)$/ ? $1 : () } plain_lines("$dir/ioc.stderr"));
}

# The place and file of the fixture module, the number of modules shown, the identifier and type of the first
# object given by name, and the type sources of all objects given by name.
sub shown {
    my ($dir) = @_;
    my $obs = observation($dir, 'read');
    return startup_fault($dir) // 'no observation read' if !defined $obs;
    my ($module) = map { /^snmp3 mib: module=\Q$MODULE\E (place=\S+ file=\S+)$/ ? $1 : () } $obs->{report}->@*;
    my @objects = map { /^snmp3 object: .*\boid=(\S+) type=(\S+) .*\bname=\S+ typeSource=(\S+)$/ ? [$1, $2, $3] : () } $obs->{report}->@*;
    my %sources = map { $_->[2] => 1 } @objects;
    my $modules = grep { /^snmp3 mib: module=/ } $obs->{report}->@*;
    return join(' ', $module // 'no module line', "modules=$modules",
                @objects ? ($objects[0][0], $objects[0][1]) : 'no object line',
                %sources ? join(',', sort keys %sources) : 'none');
}

sub judge_place {
    my ($case, $dir) = @_;
    my $place = $case eq 'two-places' ? 'two-places' : ($case =~ /^place-(\w+)$/)[0];
    my $expected = decode_json(read_file("$dir/expected.json"));
    my $values = value_of($dir, 'read', 'N_Number.SEVR', 'N_Name.SEVR', 'N_Number', 'N_Name');
    my $object = "place=$expected->{place} file=$expected->{file} modules=1 $OID_BASE$OID_INTEGER.0 integer";
    if ($case eq 'two-places') {
        check('two-places', "$values | " . shown($dir), "NO_ALARM NO_ALARM $WRITTEN $WRITTEN | $object mib");
        return;
    }
    check("$place-value", $values, "NO_ALARM NO_ALARM $WRITTEN $WRITTEN");
    check("$place-shown", shown($dir), "$object " . ($case eq 'place-ioc' ? 'link,mib' : 'mib'));
    return;
}

sub judge_start {
    my ($case, $dir) = @_;
    my $obs = observation($dir, 'read');
    my $actual = startup_fault($dir) // 'no observation read';
    if (defined $obs) {
        my ($starts) = map { /^snmp3 mib: (probeStarts=\d+)$/ ? $1 : () } $obs->{report}->@*;
        $actual = join(' ', $starts // 'no probeStarts line', $obs->{fields}{'T_1.SEVR'} // 'none', $obs->{fields}{'T_1'} // 'none');
    }
    check($case, $actual, 'probeStarts=' . ($case eq 'start-name' ? 1 : 0) . " NO_ALARM $START_FIRST");
    $times{$case} = [map { $_ + 0 } grep { /\S/ } split /\n/, -e "$dir/start-seconds" ? read_file("$dir/start-seconds") : ''];
    printf "INFO %s seconds to complete initialization: %s\n", $case, join(' ', $times{$case}->@*);
    return;
}

# Judges the named cases from their retained observations and writes results.json. A case whose directory is
# missing or whose driver did not finish makes the run incomplete.
sub command_judge {
    my ($output, @cases) = @_;
    my $incomplete = 0;
    for my $case (@cases) {
        my $dir = "$output/$case";
        if (!-e "$dir/complete") {
            print "INCOMPLETE $case\n";
            $incomplete = 1;
            next;
        }
        judge_place($case, $dir) if $case =~ /^place-/ || $case eq 'two-places';
        check('wrong-records', join(' | ', rejected($dir), value_of($dir, 'read', 'X_Good.SEVR')), 'X_Hidden X_NoIndex X_NoName X_ReadOnly X_Type | NO_ALARM')
            if $case eq 'wrong-records';
        if ($case eq 'none') {
            check('none-rejected', join(' | ', rejected($dir), value_of($dir, 'read', 'Z_Number.SEVR')), 'Z_A Z_B | NO_ALARM');
            my @named = grep { /^snmp3: MIB module / } plain_lines("$dir/ioc.stderr");
            check('none-module-named', @named ? join(' | ', @named) : startup_fault($dir) // 'no module line',
                  "snmp3: MIB module $MODULE not found");
        }
        judge_start($case, $dir) if $case =~ /^start-/;
    }
    return 2 if $incomplete;
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'mib-names', passed => $passed, checks => \@checks,
                                                start_seconds => \%times }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: mib_names.pl cases|port|prepare|process|put|observe|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_cases() if $command eq 'cases' && !@args;
    return command_port() if $command eq 'port' && !@args;
    return command_prepare(@args) if $command eq 'prepare' && @args == 6;
    return command_process(@args) if $command eq 'process' && @args == 2;
    return command_put(@args) if $command eq 'put' && @args == 3;
    return command_observe(@args) if $command eq 'observe' && @args >= 2;
    return command_judge(@args) if $command eq 'judge' && @args >= 2;
    die $usage;
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "mib_names.pl: $error";
    $status = 2;
};
exit $status;
