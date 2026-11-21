#!/usr/bin/env perl
# Writer, tool runner, record reader and judge of the MIB tools cell. test_mib_tools.bash starts and stops the
# fixture agent and the production IOCs; this script writes the MIB files, the databases and the startup scripts,
# runs the checker, the draft program and the probe as subprocesses, drives the IOC shell through a FIFO, keeps
# what it reads as files, and judges the retained files afterwards.
#
# The contract the checks are written from (the plan of the MIB tools):
#   snmp3MibCheck.pl [--probe PATH] [--mib PLACE]... [--system DIR] DATABASE...
#       reads the records of the databases whose link names an object through a MIB (the link has "::"), resolves
#       the names through the probe, and prints one line for each fault on the standard output:
#           FILE:LINE: RECORD: CLASS: TEXT
#       CLASS is one of name, module, object, index, type, read-only, not-readable. The exit status is 0 for a
#       database without findings, 1 for findings, 2 for a fault of the call.
#   snmp3MibDraft.pl [--probe PATH] [--mib PLACE]... [--system DIR] [--index FIRST..LAST] NAME...
#       writes the draft of a database for the objects MODULE::name on the standard output: for each object an
#       input record, an output record when the object can be written, one record for each index of a column.
#   snmp3NativeProbe --describe SYSTEM PLACES NAME...
#       prints for each MODULE::name (without an index) kind (scalar or column), type, access, description, units
#       and enums (a list of value and label), or an error of module, name or object.
# The tools are installed beside the probe in the products directory.
#
# Table of checks. "file" names the retained file a value is read from.
#   check                  file                         expected, source
#   chk-name .. chk-hidden checker-seeded.stdout        one line for each of the seven seeded records with the
#                                                       class of its fault, the literals of the seeded database
#   chk-status             checker-*.status/.stdout     1 for the seeded database, 0 and no line for the clean one
#   chk-comments           checker-comments.*           a database with commented-out records (a header, braces and
#                                                       quotes in comments) gives the one fault of its live record
#   chk-bare               checker-bare.*               a record written with bare words for its name and its DTYP is
#                                                       read, and its fault is named at the line it begins on; so
#                                                       is a record whose name is a macro written without quotes
#   agree-<fault> (7)      ioc-agree/ioc.stderr         the record of the fault is named as rejected
#   agree-sets             checker-seeded.stdout and    the records the checker names are the records the IOC
#                          ioc-agree/ioc.stderr         rejects, and no record of the clean part is in either
#   draft-fixture-records  drafts/fixture.db            the record names, types, links and descriptions the MIB
#                                                       declares (fixtureInteger can be written, fixtureText not)
#   draft-fixture-value    ioc-draft-fixture            4321 written through the output record is read back
#   draft-powernet-records drafts/powernet.db           one record for each index 1..3 of two columns, the
#                                                       output records only for the writable one, descriptions
#                                                       of at most 40 characters
#   draft-powernet-states  drafts/powernet.db           the states of the two enumerated objects as the comment
#                                                       "# states: VALUE = LABEL, ..." in the order of the values
#   draft-tripplite-units  drafts/tripplite.db          the units of the object that states a short one as EGU
#                                                       on its records, the long one in the text of the draft
#   draft-nonascii         drafts/tools.db              the description and the units of an object whose MIB text
#                                                       is not ASCII, as valid UTF-8 within the sizes of the fields
#   drafts-accepted        draft-*.status, ioc-draft-*   every draft (the fixture, PowerNet, TRIPPLITE, system MIB and
#                                                       the tools MIB) passes the checker and loads in an IOC
#                                                       without a rejected record
#   describe-fixture, describe-powernet, describe-tripplite, resolve-equal  probe-*.json
#                                                       what each MIB file states; --resolve of the changed
#                                                       probe equal to that of the probe of the unchanged tree
#
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use JSON::PP;
use POSIX qw(strftime);
use Time::HiRes qw(sleep time);
use IO::Socket::INET;

my $MODULE = 'SNMP3-FIXTURE-MIB';
my $WRITTEN = 4321;
my $STEP_SECONDS = 15;
my $ROUND_SECONDS = 15;
my $POLL_SECONDS = 0.02;
my @CASES = qw(checker agreement draft probe);
my @FAULTS = (['name', 'F_Name'], ['module', 'F_Module'], ['object', 'F_Object'], ['index', 'F_Index'],
              ['type', 'F_Type'], ['read-only', 'F_ReadOnly'], ['not-readable', 'F_Hidden']);
my @IOCS = qw(agree draft-fixture draft-powernet draft-tripplite draft-system draft-tools);
my $TOOLS_MODULE = 'SNMP3-TOOLS-MIB';

sub write_file {
    my ($path, $text) = @_;
    open(my $fh, '>', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub read_file {
    my ($path) = @_;
    open(my $fh, '<', $path) or return '';
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

sub command_port {
    my $socket = IO::Socket::INET->new(LocalAddr => '127.0.0.1', LocalPort => 0, Proto => 'udp')
        or die "cannot bind a loopback UDP socket: $!\n";
    print $socket->sockport, "\n";
    close($socket);
    return 0;
}

sub record {
    my ($type, $name, $field, $link, @fields) = @_;
    return "record($type, \"\$(P)$name\") {\n    field(DTYP, \"snmp3\")\n    field($field, \"\@\$(SESSION) $link\")\n"
        . join('', map { "    field($_)\n" } @fields) . "}\n";
}

sub seeded_database {
    my $by = sub { return "$MODULE\::$_[0]" };
    return join('',
        record('longin', 'F_Name', 'INP', $by->('noSuchObject.0')),
        record('longin', 'F_Module', 'INP', 'NOPE-MIB::x.0'),
        record('longin', 'F_Object', 'INP', $by->('fixtureObjects')),
        record('longin', 'F_Index', 'INP', $by->('fixtureInteger')),
        record('stringin', 'F_Type', 'INP', $by->('fixtureInteger.0')),
        record('stringout', 'F_ReadOnly', 'OUT', $by->('fixtureText.0')),
        record('longin', 'F_Hidden', 'INP', $by->('fixtureHidden.0')),
        record('longin', 'G_Integer', 'INP', $by->('fixtureInteger.0')),
        record('longout', 'G_IntegerSet', 'OUT', $by->('fixtureInteger.0')),
        record('lsi', 'G_Text', 'INP', $by->('fixtureText.0'), 'SIZV, "256"'));
}

# Writes the fixture MIB with the writer of the cell mib-names, so that the text is the one that cell serves.
sub write_fixture_mib {
    my ($dir, $root, $products, $secrets) = @_;
    my $writer = "$root/tests/rewrite/mib_names.pl";
    mkdir "$dir/fixture" or die "cannot create $dir/fixture: $!\n";
    my $status = system { 'perl' } 'perl', $writer, 'prepare', 'place-ioc', "$dir/fixture", $root, $products, $secrets, '1';
    $status == 0 or die "the writer of the fixture MIB failed with status $status\n";
    mkdir "$dir/mibs" or die "cannot create $dir/mibs: $!\n";
    write_file("$dir/mibs/$MODULE", read_file("$dir/fixture/mib-near/$MODULE"));
    return;
}

# Writes the MIB files, the databases and the startup script of each IOC run into the directory of the case.
# Arguments after the case: directory, tree root, products directory, secrets directory, the port of the agent, the
# directory of the PowerNet MIB and the directory of the TRIPPLITE MIBs.
sub command_prepare {
    my ($case, $dir, $root, $products, $secrets, $agent, $powernet, $tripplite) = @_;
    write_fixture_mib($dir, $root, $products, $secrets);
    write_file("$dir/seeded.db", seeded_database());
    write_file("$dir/clean.db", clean_database());
    write_file("$dir/comments.db", comments_database());
    write_file("$dir/bare.db", bare_database());
    write_file("$dir/mibs/$TOOLS_MODULE", tools_mib());
    write_file("$dir/places.json", encode({ fixture => "$dir/mibs", powernet => $powernet, tripplite => $tripplite }));
    my @head = ("dbLoadDatabase(\"$root/dbd/snmp3Ioc.dbd\")", 'snmp3Ioc_registerRecordDeviceDriver(pdbbase)',
                "snmp3WorkerPath(\"$products/snmp3Worker\")", "snmp3ProbePath(\"$products/snmp3NativeProbe\")");
    my $session = "snmp3Session S 127.0.0.1:$agent version=2c communityFile=$secrets/community timeoutMs=300 retries=0";
    my %runs = ('agree' => ["$dir/mibs", "$dir/seeded.db", 'A:'],
                'draft-fixture' => ["$dir/mibs", "$dir/drafts/fixture.db", 'D:'],
                'draft-powernet' => [$powernet, "$dir/drafts/powernet.db", 'D:'],
                'draft-tripplite' => [$tripplite, "$dir/drafts/tripplite.db", 'D:'],
                'draft-system' => ['-', "$dir/drafts/system.db", 'D:'],
                'draft-tools' => ["$dir/mibs", "$dir/drafts/tools.db", 'D:']);
    for my $name (@IOCS) {
        my ($place, $database, $prefix) = $runs{$name}->@*;
        mkdir "$dir/ioc-$name" or die "cannot create $dir/ioc-$name: $!\n";
        write_file("$dir/ioc-$name/startup.cmd", join("\n", 'on error break', @head, $session, ($place eq '-' ? () : "snmp3Mib $place"),
                   "dbLoadRecords(\"$database\", \"P=$prefix,SESSION=S\")", 'iocInit') . "\n");
    }
    return 0;
}

# Commented-out records, with braces and quotes in comments and after fields, and one live record with a fault.
sub comments_database {
    return join('',
        "# record(longin, \"\$(P)Commented\") { field(INP, \"\@S $MODULE\::noSuchObject.0\") }\n",
        "#record(longin, \"\$(P)CommentedToo\") {\n#    field(INP, \"\@S NOPE-MIB::x.0\")\n#}\n",
        record('longin', 'Real', 'INP', "$MODULE\::fixtureInteger.0") =~ s/\}\n\z/    # a comment with a brace { and a \"quote\n}\n/r,
        record('longin', 'Faulty', 'INP', "$MODULE\::noSuchObject.0"));
}

# A database in the bare-word form of the database grammar, whose first record is on line 4: a name and a value
# without quotes, one record on one line with a good link, and a name that is a macro without quotes on line 9.
sub bare_database {
    return join('',
        "# The grammar takes a name or a value without quotes.\n",
        "# The first record is on line 4.\n",
        "#record(longin, Gone) {\n",
        "record(longin, Bare_Name) {\n    field(DTYP, snmp3)\n    field(INP, \"\@S $MODULE\::noSuchObject.0\")\n}\n",
        "record(longin, Bare_Good) { field(DTYP, snmp3) field(INP, \"\@S $MODULE\::fixtureInteger.0\") }\n",
        "record(longin, \$(P)Bare_Macro) {\n    field(DTYP, snmp3)\n    field(INP, \"\@S $MODULE\::noSuchObject.0\")\n}\n");
}

# A MIB whose text is not ASCII: units and a long description with a degree sign, as UTF-8.
sub tools_mib {
    my $degree = "\xC2\xB0";
    return join("\n", "$TOOLS_MODULE DEFINITIONS ::= BEGIN", '',
        'toolsRoot OBJECT IDENTIFIER ::= { iso 3 6 1 4 1 53864 5 }', '',
        'toolsTemperature OBJECT-TYPE', '    SYNTAX      INTEGER', "    UNITS       \"${degree}C\"", '    ACCESS      read-only',
        '    STATUS      mandatory', "    DESCRIPTION \"Temperature in ${degree}C of the power board, measured near the hot spot.\"",
        '    ::= { toolsRoot 1 }', '', 'END', '');
}

sub clean_database {
    return join('',
        record('longin', 'G_Integer', 'INP', "$MODULE\::fixtureInteger.0"),
        record('longout', 'G_IntegerSet', 'OUT', "$MODULE\::fixtureInteger.0"),
        record('lsi', 'G_Text', 'INP', "$MODULE\::fixtureText.0", 'SIZV, "256"'));
}

# Runs a tool of the products directory with the arguments, keeps its standard output, standard error and exit
# status as STEM.stdout, STEM.stderr and STEM.status. A tool that cannot be started is status 127 with the reason.
sub run_tool {
    my ($stem, $tool, @args) = @_;
    my $pid = fork();
    defined $pid or die "cannot fork: $!\n";
    if (!$pid) {
        open(STDOUT, '>', "$stem.stdout") or POSIX::_exit(126);
        open(STDERR, '>', "$stem.stderr") or POSIX::_exit(126);
        { exec { $tool } $tool, @args; }
        print STDERR "cannot start $tool: $!\n";
        POSIX::_exit(127);
    }
    waitpid($pid, 0);
    my $status = ($? & 127) ? 'signal ' . ($? & 127) : $? >> 8;
    write_file("$stem.status", "$status\n");
    return;
}

sub places {
    my ($dir) = @_;
    return decode_json(read_file("$dir/places.json"));
}

# Runs the checker on the seeded and on the clean database.
# usage: run-checker DIR PRODUCTS
sub command_run_checker {
    my ($dir, $products) = @_;
    my $place = places($dir);
    for my $name (qw(seeded clean comments bare)) {
        run_tool("$dir/checker-$name", "$products/snmp3MibCheck.pl", '--probe', "$products/snmp3NativeProbe",
                 '--mib', $place->{fixture}, "$dir/$name.db");
    }
    return 0;
}

# Runs the draft program for the three MIBs, and the checker on each draft.
# usage: run-drafts DIR PRODUCTS
sub command_run_drafts {
    my ($dir, $products) = @_;
    my $place = places($dir);
    mkdir "$dir/drafts" or die "cannot create $dir/drafts: $!\n";
    my %calls = (
        fixture => [$place->{fixture}, "$MODULE\::fixtureInteger", "$MODULE\::fixtureText"],
        powernet => [$place->{powernet}, '--index', '1..3', 'PowerNet-MIB::rPDU2OutletSwitchedStatusState',
                     'PowerNet-MIB::rPDU2OutletSwitchedControlCommand'],
        tripplite => [$place->{tripplite}, '--index', '1', 'TRIPPLITE-PRODUCTS::tlpUpsDevicePowerOnDelay',
                      'TRIPPLITE-PRODUCTS::tlpUpsDeviceTemperatureC'],
        system => ['-', 'SNMPv2-MIB::sysObjectID', 'SNMPv2-MIB::sysDescr', 'SNMPv2-MIB::sysContact'],
        tools => [$place->{fixture}, "$TOOLS_MODULE\::toolsTemperature"]);
    for my $name (qw(fixture powernet tripplite system tools)) {
        my ($mib, @rest) = $calls{$name}->@*;
        my @place = $mib eq '-' ? () : ('--mib', $mib);
        run_tool("$dir/draft-$name", "$products/snmp3MibDraft.pl", '--probe', "$products/snmp3NativeProbe", @place, @rest);
        write_file("$dir/drafts/$name.db", read_file("$dir/draft-$name.stdout"));
        run_tool("$dir/draftcheck-$name", "$products/snmp3MibCheck.pl", '--probe', "$products/snmp3NativeProbe",
                 @place, "$dir/drafts/$name.db");
    }
    return 0;
}

# Runs --describe and --resolve of the probe on the three MIBs, and --resolve of the probe of the unchanged tree.
# usage: run-probe DIR PRODUCTS BASELINE_PRODUCTS
sub command_run_probe {
    my ($dir, $products, $baseline) = @_;
    my $place = places($dir);
    my %names = (
        fixture => [$place->{fixture}, map { "$MODULE\::$_" } qw(fixtureInteger fixtureText)],
        powernet => [$place->{powernet}, map { "PowerNet-MIB::$_" } qw(rPDU2OutletSwitchedStatusState rPDU2OutletSwitchedControlCommand)],
        tripplite => [$place->{tripplite}, map { "TRIPPLITE-PRODUCTS::$_" } qw(tlpUpsDevicePowerOnDelay tlpUpsDeviceTemperatureC)]);
    my %instances = (fixture => ['0', '0'], powernet => ['1', '1'], tripplite => ['1', '1']);
    for my $name (qw(fixture powernet tripplite)) {
        my ($mib, @objects) = $names{$name}->@*;
        run_tool("$dir/probe-describe-$name", "$products/snmp3NativeProbe", '--describe', '-', $mib, @objects);
        my @indexed = map { "$objects[$_].$instances{$name}[$_]" } 0 .. $#objects;
        run_tool("$dir/probe-resolve-$name", "$products/snmp3NativeProbe", '--resolve', '-', $mib, @indexed);
        run_tool("$dir/probe-baseline-$name", "$baseline/snmp3NativeProbe", '--resolve', '-', $mib, @indexed);
    }
    return 0;
}

sub open_run {
    my ($dir) = @_;
    mkdir "$dir/steps" if !-d "$dir/steps";
    open(my $fifo, '>', "$dir/ioc.stdin") or die "cannot open $dir/ioc.stdin: $!\n";
    $fifo->autoflush(1);
    return { dir => $dir, fifo => $fifo };
}

sub next_number {
    my ($dir) = @_;
    my $path = "$dir/sequence";
    my $number = -s $path ? read_file($path) + 1 : 1;
    write_file($path, "$number\n");
    return $number;
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

# usage: put DIR RECORD VALUE
sub command_put {
    my ($dir, $record, $value) = @_;
    my $run = open_run($dir);
    shell($run, "dbpf \"$record\" \"$value\"");
    settle($run, $record);
    close($run->{fifo});
    return 0;
}

# usage: process DIR RECORD
sub command_process {
    my ($dir, $record) = @_;
    my $run = open_run($dir);
    shell($run, "dbpf \"$record.PROC\" 1");
    settle($run, $record);
    close($run->{fifo});
    return 0;
}

# Keeps the named fields as observe.json of the IOC directory.
# usage: observe DIR FIELD...
sub command_observe {
    my ($dir, @names) = @_;
    my $run = open_run($dir);
    my @values = fields($run, @names);
    my %fields;
    @fields{@names} = @values;
    close($run->{fifo});
    write_file("$dir/observe.json", encode({ utc => strftime('%Y-%m-%dT%H:%M:%SZ', gmtime), fields => \%fields }));
    return 0;
}

my (@checks);

sub check {
    my ($name, $actual, $expected) = @_;
    my $passed = (defined $actual && $actual eq $expected) ? 1 : 0;
    push @checks, { name => $name, passed => $passed ? JSON::PP::true : JSON::PP::false,
                    actual => $actual, expected => $expected };
    printf "%s %s actual=[%s] expected=[%s]\n", $passed ? 'PASS' : 'FAIL', $name,
        defined $actual ? $actual : 'undefined', $expected;
    return;
}

sub plain_lines {
    my ($path) = @_;
    my @lines = -e $path ? split(/\n/, read_file($path)) : ();
    s/\e\[[0-9;]*m//g for @lines;
    return @lines;
}

sub status_of {
    my ($stem) = @_;
    my $text = read_file("$stem.status");
    $text =~ s/\s+$//;
    return $text eq '' ? 'not run' : $text;
}

# Why a tool produced nothing: the status when it was not 0 or 1, and the first line of its standard error.
sub tool_fault {
    my ($stem) = @_;
    my $status = status_of($stem);
    my ($first) = plain_lines("$stem.stderr");
    return "status $status" . (defined $first ? ": $first" : '');
}

# The line each record of a checker run is reported at, as { RECORD => LINE }, with the macro of the name removed.
sub finding_lines {
    my ($stem) = @_;
    return { map { /^[^:]+:(\d+): (?:\$\(P\))?(\S+): / ? ($2 => $1) : () } plain_lines("$stem.stdout") };
}

# The findings of a checker run as { RECORD => CLASS }, with the macro of the record name removed.
sub findings {
    my ($stem) = @_;
    my %found;
    for my $line (plain_lines("$stem.stdout")) {
        next if $line !~ /^[^:]+:\d+: (\S+): (name|module|object|index|type|read-only|not-readable): /;
        my ($record, $class) = ($1, $2);
        $record =~ s/^\$\(P\)//;
        $found{$record} = exists $found{$record} ? "$found{$record},$class" : $class;
    }
    return \%found;
}

sub rejected_records {
    my ($dir) = @_;
    return { map { /^snmp3: record initialization rejected: (?:A:)?(\S+)$/ ? ($1 => 1) : () } plain_lines("$dir/ioc.stderr") };
}

sub ioc_started {
    my ($dir) = @_;
    return scalar grep { /iocRun: All initialization complete/ } plain_lines("$dir/ioc.stderr");
}

sub judge_checker {
    my ($dir) = @_;
    my $found = findings("$dir/checker-seeded");
    my $fault = $found->%* ? '' : tool_fault("$dir/checker-seeded");
    for my $pair (@FAULTS) {
        my ($class, $record) = $pair->@*;
        check("chk-$class", $found->{$record} // ($fault ne '' ? $fault : 'not named'), $class);
    }
    my @clean = plain_lines("$dir/checker-clean.stdout");
    check('chk-status', join(' ', status_of("$dir/checker-seeded"), status_of("$dir/checker-clean"), scalar(@clean) . ' lines'),
          '1 0 0 lines');
    my $comments = findings("$dir/checker-comments");
    my $status = status_of("$dir/checker-comments");
    my ($why) = plain_lines("$dir/checker-comments.stderr");
    check('chk-comments', join(' | ', $status, join(' ', map { "$_=$comments->{$_}" } sort keys $comments->%*))
                          . ($status =~ /^[01]$/ ? '' : ' | ' . ($why // 'no message')), '1 | Faulty=name');
    my $bare = findings("$dir/checker-bare");
    my $bare_lines = finding_lines("$dir/checker-bare");
    my $bare_status = status_of("$dir/checker-bare");
    my ($bare_why) = plain_lines("$dir/checker-bare.stderr");
    check('chk-bare', join(' | ', $bare_status, join(' ', map { "$_=$bare->{$_} line $bare_lines->{$_}" } sort keys $bare->%*))
                      . ($bare_status =~ /^[01]$/ ? '' : ' | ' . ($bare_why // 'no message')), '1 | Bare_Macro=name line 9 Bare_Name=name line 4');
    return;
}

sub judge_agreement {
    my ($dir) = @_;
    my $ioc = "$dir/ioc-agree";
    my $rejected = rejected_records($ioc);
    my $started = ioc_started($ioc);
    for my $pair (@FAULTS) {
        my ($class, $record) = $pair->@*;
        check("agree-$class", $started ? ($rejected->{$record} ? 'rejected' : 'accepted') : 'IOC did not start', 'rejected');
    }
    my $named = join(' ', sort keys findings("$dir/checker-seeded")->%*);
    my $refused = join(' ', sort keys $rejected->%*);
    check('agree-sets', $started ? "$named | $refused" : "$named | IOC did not start",
          join(' ', sort map { $_->[1] } @FAULTS) . ' | ' . join(' ', sort map { $_->[1] } @FAULTS));
    return;
}

# The records of a draft as "NAME:TYPE" in file order, with the macro of the name removed.
sub draft_records {
    my ($path) = @_;
    return map { /^record\((\w+), "\$\(P\)([^"]+)"\)/ ? "$2:$1" : () } plain_lines($path);
}

# The value of a field of a record of a draft, or undef.
sub draft_field {
    my ($path, $record, $field) = @_;
    my $text = read_file($path);
    return $text =~ /record\(\w+, "\$\(P\)\Q$record\E"\) \{(.*?)\n\}/s && $1 =~ /field\(\Q$field\E, "([^"]*)"\)/ ? $1 : undef;
}

sub judge_draft {
    my ($dir) = @_;
    my $fixture = "$dir/drafts/fixture.db";
    my @records = draft_records($fixture);
    my $link = sub { return draft_field($fixture, $_[0], $_[1]) // 'none' };
    check('draft-fixture-records', @records
          ? join(' | ', join(' ', @records), $link->('fixtureInteger', 'INP'), $link->('fixtureInteger_Set', 'OUT'),
                 $link->('fixtureText', 'INP'), $link->('fixtureInteger', 'DESC'), $link->('fixtureText', 'DESC'))
          : tool_fault("$dir/draft-fixture"),
          join(' | ', 'fixtureInteger:longin fixtureInteger_Set:longout fixtureText:lsi',
               "\@\$(SESSION) $MODULE\::fixtureInteger.0", "\@\$(SESSION) $MODULE\::fixtureInteger.0",
               "\@\$(SESSION) $MODULE\::fixtureText.0", 'Integer value of the fixture agent.',
               'Text value of the fixture agent.'));
    my $obs = -s "$dir/ioc-draft-fixture/observe.json" ? decode_json(read_file("$dir/ioc-draft-fixture/observe.json")) : undef;
    check('draft-fixture-value', defined $obs ? join(' ', map { $obs->{fields}{$_} // 'none' } 'D:fixtureInteger.SEVR', 'D:fixtureInteger')
                                              : (ioc_started("$dir/ioc-draft-fixture") ? 'no observation' : 'IOC did not start'),
          "NO_ALARM $WRITTEN");
    my $powernet = "$dir/drafts/powernet.db";
    my @expected;
    for my $n (1 .. 3) {
        push @expected, "rPDU2OutletSwitchedStatusState_$n:longin", "rPDU2OutletSwitchedControlCommand_$n:longin",
                        "rPDU2OutletSwitchedControlCommand_${n}_Set:longout";
    }
    my @power = draft_records($powernet);
    my @long = grep { my $d = draft_field($powernet, (split /:/)[0], 'DESC'); !defined $d || length($d) > 40 } @power;
    check('draft-powernet-records', @power ? join(' | ', join(' ', sort @power), scalar(@long) . ' descriptions out of range')
                                           : tool_fault("$dir/draft-powernet"),
          join(' | ', join(' ', sort @expected), '0 descriptions out of range'));
    my %seen;
    my @states = grep { !$seen{$_}++ } map { /^# states: (.*)$/ ? $1 : () } plain_lines($powernet);
    check('draft-powernet-states', @power ? join(' | ', @states) : tool_fault("$dir/draft-powernet"),
          join(' | ', '1 = off, 2 = on', '1 = immediateOn, 2 = immediateOff, 3 = immediateReboot, 4 = outletUnknown, '
               . '5 = delayedOn, 6 = delayedOff, 7 = delayedReboot, 8 = cancelPendingCommand'));
    my $trip = "$dir/drafts/tripplite.db";
    my $text = read_file($trip);
    my @units = map { draft_field($trip, $_, 'EGU') // 'none' } qw(tlpUpsDevicePowerOnDelay_1 tlpUpsDevicePowerOnDelay_1_Set);
    my $long_egu = draft_field($trip, 'tlpUpsDeviceTemperatureC_1', 'EGU');
    check('draft-tripplite-units', $text ne ''
          ? join(' | ', @units, (!defined $long_egu || length($long_egu) <= 15) ? 'egu fits' : 'egu too long',
                 index($text, '0.1 degrees Celsius') >= 0 ? 'units text kept' : 'units text lost')
          : tool_fault("$dir/draft-tripplite"),
          'Seconds | Seconds | egu fits | units text kept');
    my $tools = "$dir/drafts/tools.db";
    my $bytes = read_file($tools);
    my $valid = eval { my $copy = $bytes; utf8::decode($copy) or die; 1 } ? 'valid UTF-8' : 'not UTF-8';
    my $desc = draft_field($tools, 'toolsTemperature', 'DESC');
    my $egu = draft_field($tools, 'toolsTemperature', 'EGU');
    check('draft-nonascii', $bytes ne ''
          ? join(' | ', $valid, defined $desc && length($desc) <= 40 ? 'DESC fits' : 'DESC too long',
                 defined $egu && $egu eq "\xC2\xB0C" ? 'EGU kept' : 'EGU ' . (defined $egu ? 'changed' : 'absent'))
          : tool_fault("$dir/draft-tools"), 'valid UTF-8 | DESC fits | EGU kept');
    my @verdicts;
    for my $name (qw(fixture powernet tripplite system tools)) {
        my $fine = status_of("$dir/draftcheck-$name") eq '0' && !plain_lines("$dir/draftcheck-$name.stdout")
                   && ioc_started("$dir/ioc-draft-$name") && !rejected_any("$dir/ioc-draft-$name");
        push @verdicts, "$name " . ($fine ? 'accepted' : 'refused');
    }
    check('drafts-accepted', join(' ', @verdicts),
          'fixture accepted powernet accepted tripplite accepted system accepted tools accepted');
    return;
}

sub rejected_any {
    my ($dir) = @_;
    return scalar grep { /record initialization rejected/ } plain_lines("$dir/ioc.stderr");
}

# The facts --describe states about one object, as a short text.
sub described {
    my ($path, $text) = @_;
    my $json = eval { decode_json(read_file($path)) };
    return 'no output' if !defined $json;
    my ($entry) = grep { $_->{text} eq $text } ($json->{names} // [])->@*;
    return 'name absent' if !defined $entry;
    return "error $entry->{error}" if exists $entry->{error};
    return join(' ', map { defined $entry->{$_} ? $entry->{$_} : 'none' } qw(kind type access));
}

sub described_field {
    my ($path, $text, $field) = @_;
    my $json = eval { decode_json(read_file($path)) };
    return 'no output' if !defined $json;
    my ($entry) = grep { $_->{text} eq $text } ($json->{names} // [])->@*;
    return 'name absent' if !defined $entry || !exists $entry->{$field};
    my $value = $entry->{$field};
    return join(',', map { "$_->{label}=$_->{value}" } $value->@*) if ref $value eq 'ARRAY';
    return $value;
}

sub judge_probe {
    my ($dir) = @_;
    my $f = "$dir/probe-describe-fixture.stdout";
    check('describe-fixture', join(' | ', described($f, "$MODULE\::fixtureInteger"),
                                   described_field($f, "$MODULE\::fixtureInteger", 'description'),
                                   described_field($f, "$MODULE\::fixtureText", 'units')),
          'scalar integer read-write | Integer value of the fixture agent. | ');
    my $p = "$dir/probe-describe-powernet.stdout";
    check('describe-powernet', join(' | ', described($p, 'PowerNet-MIB::rPDU2OutletSwitchedStatusState'),
                                    described_field($p, 'PowerNet-MIB::rPDU2OutletSwitchedStatusState', 'enums')),
          'column integer read-only | off=1,on=2');
    my $t = "$dir/probe-describe-tripplite.stdout";
    check('describe-tripplite', join(' | ', described_field($t, 'TRIPPLITE-PRODUCTS::tlpUpsDevicePowerOnDelay', 'units'),
                                     described_field($t, 'TRIPPLITE-PRODUCTS::tlpUpsDeviceTemperatureC', 'units')),
          'Seconds | 0.1 degrees Celsius');
    my @same = map { read_file("$dir/probe-resolve-$_.stdout") eq read_file("$dir/probe-baseline-$_.stdout")
                     && read_file("$dir/probe-resolve-$_.stdout") ne '' ? "$_ equal" : "$_ different" } qw(fixture powernet tripplite);
    check('resolve-equal', join(' ', @same), 'fixture equal powernet equal tripplite equal');
    return;
}

# Judges the named cases from their retained files and writes results.json. A case whose directory is missing or
# whose driver did not finish makes the run incomplete.
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
        judge_checker($dir) if $case eq 'checker';
        judge_agreement($dir) if $case eq 'agreement';
        judge_draft($dir) if $case eq 'draft';
        judge_probe($dir) if $case eq 'probe';
    }
    return 2 if $incomplete;
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", encode({ scope => 'mib-tools', passed => $passed, checks => \@checks }));
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    my $usage = "usage: mib_tools.pl cases|port|prepare|run-checker|run-drafts|run-probe|put|process|observe|judge ARGUMENTS\n";
    defined $command or die $usage;
    return command_cases() if $command eq 'cases' && !@args;
    return command_port() if $command eq 'port' && !@args;
    return command_prepare(@args) if $command eq 'prepare' && @args == 8;
    return command_run_checker(@args) if $command eq 'run-checker' && @args == 2;
    return command_run_drafts(@args) if $command eq 'run-drafts' && @args == 2;
    return command_run_probe(@args) if $command eq 'run-probe' && @args == 3;
    return command_put(@args) if $command eq 'put' && @args == 3;
    return command_process(@args) if $command eq 'process' && @args == 2;
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
    print STDERR "mib_tools.pl: $error";
    $status = 2;
};
exit $status;
