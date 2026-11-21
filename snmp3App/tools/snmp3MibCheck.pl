#!/usr/bin/env perl

# Checks the records of EPICS databases that name an SNMP object through a MIB against the MIB files, before an
# IOC is started: for every such record whether the name resolves, whether the record type can hold the declared
# type of the object, and whether the direction of the record agrees with the access of the object. The names are
# resolved by the native probe of the module in one run, the way the IOC resolves them.
#
# Language level: Perl 5.26, core modules only.

use strict;
use warnings;
use FindBin qw($Bin);
use Getopt::Long qw(GetOptionsFromArray);
use JSON::PP;
use POSIX ();
use Pod::Usage qw(pod2usage);

# The value types a record kind takes, and the record kinds that read and write.
my %INTEGER = map { $_ => 1 } qw(integer unsigned32 counter32 gauge32 timeticks counter64);
my %NUMERIC = (%INTEGER, opaqueFloat => 1, opaqueDouble => 1);
my %TEXT = map { $_ => 1 } qw(octets oid ipAddress);
my %TAKES = (
    ai => \%NUMERIC, ao => \%NUMERIC,
    longin => \%INTEGER, longout => \%INTEGER, int64in => \%INTEGER, int64out => \%INTEGER,
    stringin => \%TEXT, lsi => \%TEXT, stringout => { octets => 1 }, lso => { octets => 1 },
);
my %INPUT = map { $_ => 1 } qw(ai longin int64in stringin lsi waveform);
my %OUTPUT = map { $_ => 1 } qw(ao longout int64out stringout lso);
my %READABLE = map { $_ => 1 } ('read-only', 'read-write', 'read-create');
my %WRITABLE = map { $_ => 1 } ('read-write', 'write-only', 'read-create');
my %FTVL_NUMERIC = map { $_ => 1 } qw(LONG ULONG INT64 UINT64 FLOAT DOUBLE);

# The text with every comment removed and every line break kept, so that line numbers stay: a comment runs from a #
# outside a string to the end of its line.
sub strip_comments {
    my ($text) = @_;
    my ($out, $quoted, $comment) = ('', 0, 0);
    for (my $index = 0; $index < length $text; $index++) {
        my $char = substr($text, $index, 1);
        if ($comment) {
            if ($char eq "\n") { $comment = 0; $out .= $char; }
            next;
        }
        if ($quoted) {
            if ($char eq '\\' && $index + 1 < length $text) { $out .= $char; $char = substr($text, ++$index, 1); }
            elsif ($char eq '"') { $quoted = 0; }
        } elsif ($char eq '"') {
            $quoted = 1;
        } elsif ($char eq '#') {
            $comment = 1;
            next;
        }
        $out .= $char;
    }
    return $out;
}

# A name or a value in the grammar of a database: a string in quotes, or a bare word, which may hold macro
# references. It captures twice, the string and the bare word, of which one is defined.
my $WORD = qr/(?:"((?:[^"\\]|\\.)*)"|((?:[\w\-+:.\[\]<>;]|\$\([^)]*\)|\$\{[^}]*\})+))/;

# The records of a database as { name, kind, line, fields }.
sub parse_records {
    my ($text) = @_;
    $text = strip_comments($text);
    my @records;
    my ($line, $counted) = (1, 0);
    while ($text =~ /\brecord\s*\(\s*(\w+)\s*,\s*$WORD\s*\)\s*\{/g) {
        my ($kind, $name) = ($1, defined $2 ? $2 : $3);
        my $start = $-[0];
        my $body_from = pos($text);
        my ($depth, $quoted, $index) = (1, 0, $body_from);
        my $body = '';
        while ($index < length $text && $depth > 0) {
            my $char = substr($text, $index, 1);
            if ($quoted) {
                if ($char eq '\\') { $body .= $char; $index++; $char = substr($text, $index, 1); }
                elsif ($char eq '"') { $quoted = 0; }
            } elsif ($char eq '"') {
                $quoted = 1;
            } elsif ($char eq '{') {
                $depth++;
            } elsif ($char eq '}') {
                $depth--;
            }
            $body .= $char if $depth > 0;
            $index++;
        }
        $depth == 0 or die "a record that is not closed: $name\n";
        pos($text) = $index;
        my %fields;
        while ($body =~ /\bfield\s*\(\s*(\w+)\s*,\s*$WORD\s*\)/g) {
            $fields{$1} = defined $2 ? $2 : $3;
        }
        # The lines are counted from the end of the last record, so that a database is read once.
        $line += () = substr($text, $counted, $start - $counted) =~ /\n/g;
        $counted = $start;
        push @records, { name => $name, kind => $kind, line => $line, fields => \%fields };
    }
    return @records;
}

# The link of a record that reads or writes an object through the module, as { session, object, type }, or undef
# when the record is not one of them or its link names no MIB object (a binding, a number or a macro).
sub mib_link {
    my ($record) = @_;
    my $fields = $record->{fields};
    return undef if !defined $fields->{DTYP} || $fields->{DTYP} ne 'snmp3';
    my $link = $INPUT{$record->{kind}} ? $fields->{INP} : $OUTPUT{$record->{kind}} ? $fields->{OUT} : undef;
    return undef if !defined $link || $link !~ /^\@/;
    my ($session, $object, @options) = split ' ', $link;
    return undef if !defined $object || index($object, '::') < 1 || index($object, '$(') >= 0;
    my ($type) = map { /^type=(\S+)$/ ? $1 : () } @options;
    return { object => $object, type => $type };
}

# Runs the probe once for all names and returns its answer as { name text => entry }.
sub resolve_names {
    my ($probe, $system, $places, @names) = @_;
    return {} if !@names;
    pipe(my $reader, my $writer) or die "cannot create a pipe: $!\n";
    my $pid = fork();
    defined $pid or die "cannot fork: $!\n";
    if (!$pid) {
        close($reader);
        open(STDOUT, '>&', $writer) or POSIX::_exit(126);
        { exec { $probe } $probe, '--resolve', $system, $places, @names; }
        print STDERR "cannot start $probe: $!\n";
        POSIX::_exit(127);
    }
    close($writer);
    my $text = do { local $/; <$reader> };
    close($reader);
    waitpid($pid, 0);
    my $status = $?;
    $status == 0 or die "the probe $probe failed with status " . (($status & 127) ? 'signal ' . ($status & 127) : $status >> 8) . "\n";
    my $answer = eval { decode_json($text) };
    defined $answer && ref $answer eq 'HASH' && ref $answer->{names} eq 'ARRAY' or die "the probe $probe printed no answer\n";
    return { map { $_->{text} => $_ } @{ $answer->{names} } };
}

# The fault of one record, or undef: the class and the text of the first rule it breaks, in the order the IOC
# applies them: the name, then the access, then the type.
sub fault {
    my ($record, $link, $entry) = @_;
    if (!defined $entry) { return ('name', "MIB name $link->{object} was not answered"); }
    if (exists $entry->{error}) {
        my %text = (name => 'is not resolved', module => 'names a module that no MIB place holds',
                    object => 'is no object with a value', index => 'carries no index of an instance');
        return ($entry->{error}, "MIB name $link->{object} " . ($text{ $entry->{error} } // "failed with $entry->{error}"));
    }
    my $input = $INPUT{ $record->{kind} } ? 1 : 0;
    if ($input && !$READABLE{ $entry->{access} }) {
        return ('not-readable', "an input record on an object whose access is $entry->{access}");
    }
    if (!$input && !$WRITABLE{ $entry->{access} }) {
        return ('read-only', "an output record on an object whose access is $entry->{access}");
    }
    my $type = defined $link->{type} ? $link->{type} : $entry->{type};
    return ('type', "the MIB gives no type and the link carries none") if !defined $type || $type eq '';
    if ($record->{kind} eq 'waveform') {
        my $ftvl = $record->{fields}{FTVL} // 'STRING';
        my $fits = ($type eq 'octets' || $type eq 'ipAddress') ? $ftvl eq 'UCHAR' : $type eq 'oid' ? $ftvl eq 'ULONG'
                 : ($NUMERIC{$type} && $FTVL_NUMERIC{$ftvl}) ? 1 : 0;
        return ('type', "a waveform of $ftvl cannot hold $type") if !$fits;
        return ();
    }
    my $takes = $TAKES{ $record->{kind} };
    return () if !defined $takes;
    return ('type', "a $record->{kind} record cannot hold $type") if !$takes->{$type};
    return ();
}

sub main {
    my (@arguments) = @_;
    my ($probe, $system, @places, $help) = ("$Bin/snmp3NativeProbe", '-');
    GetOptionsFromArray(\@arguments, 'probe=s' => \$probe, 'mib=s' => \@places, 'system=s' => \$system, 'help' => \$help)
        or pod2usage(-exitval => 2, -verbose => 0, -output => \*STDERR);
    pod2usage(-exitval => 0, -verbose => 1) if $help;
    pod2usage(-exitval => 2, -verbose => 0, -output => \*STDERR, -message => 'snmp3MibCheck: no database given') if !@arguments;
    my (@records, %seen);
    for my $path (@arguments) {
        open(my $fh, '<', $path) or die "cannot read $path: $!\n";
        my $text = do { local $/; <$fh> };
        close($fh);
        for my $record (parse_records($text)) {
            my $link = mib_link($record) or next;
            push @records, { path => $path, record => $record, link => $link };
            $seen{ $link->{object} } = 1;
        }
    }
    # The library takes a module from the place registered last, so the places go to the probe from the farthest to
    # the nearest, as the IOC hands them over: the mibs directory the module installs, then the places given with
    # the first one last.
    my @ordered = (-d "$Bin/../../mibs" ? "$Bin/../../mibs" : (), reverse @places);
    my $answers = resolve_names($probe, $system, @ordered ? join(':', @ordered) : '-', sort keys %seen);
    my $findings = 0;
    for my $item (@records) {
        my ($class, $text) = fault($item->{record}, $item->{link}, $answers->{ $item->{link}{object} });
        next if !defined $class;
        $findings++;
        printf "%s:%d: %s: %s: %s\n", $item->{path}, $item->{record}{line}, $item->{record}{name}, $class, $text;
    }
    printf STDERR "snmp3MibCheck: %d findings in %d records checked\n", $findings, scalar @records;
    return $findings ? 1 : 0;
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "snmp3MibCheck: $error";
    $status = 2;
};
exit $status;

__END__

=head1 NAME

snmp3MibCheck.pl - check the records of a database against the MIB files of the objects they name

=head1 SYNOPSIS

snmp3MibCheck.pl [--probe PATH] [--mib PLACE]... [--system DIR] DATABASE...

=head1 DESCRIPTION

Reads the records of each DATABASE whose device type is C<snmp3> and whose link names an object as
C<MODULE::name.index>, resolves all the names in one run of the native probe, and prints one line on the standard
output for each record that the IOC would reject because of its MIB name, its access or its type:

    FILE:LINE: RECORD: CLASS: TEXT

CLASS is C<name> (the MIB does not hold the name), C<module> (no place holds the MIB), C<object> (the name is no
object with a value), C<index> (the name carries no index of an instance), C<read-only> (an output record on an
object that cannot be written), C<not-readable> (an input record on an object that cannot be read) or C<type> (the
record type cannot hold the type the MIB declares, or the MIB declares none and the link carries none). A record
has at most one line, for the first rule it breaks, in the order the IOC applies them.

The record names are printed as they stand in the database, with their macros. A comment is skipped, and a name or
a value written without quotes, as the grammar of a database allows, is read. A link that is a binding, a
number or holds a macro is not checked, and neither is a record whose C<DTYP> is written as a macro. The rules of the IOC for the fields of a record, the grammar of the link,
C<SCAN> and the subscriptions are not taken over.

The summary of the run goes to the standard error. The exit status is 0 for a database without findings, 1 for
findings and 2 for a fault of the call or of the probe.

=head1 OPTIONS

=over 4

=item B<--probe> PATH

The native probe of the module; by default C<snmp3NativeProbe> beside this script.

=item B<--mib> PLACE

A directory or a file with MIB modules, as C<snmp3Mib> takes it in the startup script of an IOC. Given more than
once, the first place that holds a module is taken, as in a startup script. The C<mibs> directory that the module
installs, two levels above the directory of this script, comes after the given places and before the system
directories, as it does in an IOC.

=item B<--system> DIR

The directory of the system MIBs, as the second argument of C<snmp3Mib>; by default the directories built into the
library of the probe.

=item B<--help>

Prints this text and exits with status 0.

=back

=cut
