#!/usr/bin/env perl

# Writes the draft of a database for SNMP objects named through a MIB: for each object an input record, an output
# record when the object can be written, and one record for each index of a column. The record type, the direction,
# the description and the units come from the MIB, read by the native probe of the module. The draft is a starting
# text: the names of the records, what to keep and everything the MIB does not state stay with the author, and the
# finished database goes through snmp3MibCheck.pl.
#
# Language level: Perl 5.26, core modules only.

use strict;
use warnings;
use FindBin qw($Bin);
use Getopt::Long qw(GetOptionsFromArray);
use JSON::PP;
use POSIX ();
use Pod::Usage qw(pod2usage);

my $DESC_LENGTH = 40;      # the length of the DESC field of a record
my $EGU_LENGTH = 15;       # the longest units the EGU field of a record holds
my $LONG_STRING = 256;     # the size of the long string records the draft writes for text
my $OID_STRING = 128;      # the size for an object identifier: the longest one the module takes has 128 arcs
my $MAX_INDEXES = 4096;    # the most records one column may be drafted for

# The record types of a declared type, for reading and for writing, and the fields the type needs.
my %TYPE = (
    integer => ['longin', 'longout'], unsigned32 => ['int64in', 'int64out'], counter32 => ['int64in', 'int64out'],
    gauge32 => ['int64in', 'int64out'], timeticks => ['int64in', 'int64out'], counter64 => ['int64in', 'int64out'],
    octets => ['lsi', 'lso'], oid => ['lsi', 'lso'], ipAddress => ['lsi', 'lso'],
);
my %READABLE = map { $_ => 1 } ('read-only', 'read-write', 'read-create');
my %WRITABLE = map { $_ => 1 } ('read-write', 'write-only', 'read-create');

# Asks the probe about the objects and returns { name text => entry }.
sub describe_names {
    my ($probe, $system, $places, @names) = @_;
    pipe(my $reader, my $writer) or die "cannot create a pipe: $!\n";
    my $pid = fork();
    defined $pid or die "cannot fork: $!\n";
    if (!$pid) {
        close($reader);
        open(STDOUT, '>&', $writer) or POSIX::_exit(126);
        { exec { $probe } $probe, '--describe', $system, $places, @names; }
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

# The indexes of a range given as N or FIRST..LAST.
sub parse_range {
    my ($text) = @_;
    $text =~ /^(\d+)(?:\.\.(\d+))?$/ or die "the index range $text is not N or FIRST..LAST\n";
    my ($first, $last) = ($1, defined $2 ? $2 : $1);
    $first <= $last or die "the index range $text ends before it begins\n";
    $last - $first < $MAX_INDEXES or die "the index range $text holds more than $MAX_INDEXES indexes\n";
    return ($first .. $last);
}

# The number of bytes of a text as UTF-8, which is what the fields of a record hold.
sub bytes_of {
    my ($text) = @_;
    utf8::encode($text);
    return length $text;
}

# The description of an object as the DESC field takes it: one line, cut to the size of the field in bytes without
# splitting a character.
sub desc_text {
    my ($description, $fallback) = @_;
    my $text = defined $description ? $description : '';
    $text =~ s/\s+/ /g;
    $text =~ s/^ | $//g;
    $text = $fallback if $text eq '';
    chop $text while bytes_of($text) > $DESC_LENGTH;
    $text =~ s/ $//;
    $text =~ s/(["\\])/\\$1/g;
    return $text;
}

sub record_text {
    my ($type, $name, $link_field, $link, $desc, $units, $numeric_units, $size) = @_;
    my $text = "record($type, \"\$(P)$name\") {\n    field(DESC, \"$desc\")\n    field(DTYP, \"snmp3\")\n"
             . "    field($link_field, \"$link\")\n";
    $text .= "    field(EGU, \"$units\")\n" if $numeric_units && $units ne '' && bytes_of($units) <= $EGU_LENGTH;
    $text .= "    field(SIZV, \"$size\")\n" if $type eq 'lsi' || $type eq 'lso';
    $text .= "    field(OOPT, \"Every Time\")\n" if $type eq 'longout';
    return $text . "}\n";
}

# The records of one object at one instance (".0" or ".N"): the suffix of the record names and the instance.
sub object_records {
    my ($entry, $suffix, $instance) = @_;
    my ($module, $object) = split /::/, $entry->{text}, 2;
    my $type = $entry->{type};
    my $link = "\@\$(SESSION) $entry->{text}$instance";
    my $desc = desc_text($entry->{description}, $object);
    my $units = defined $entry->{units} ? $entry->{units} : '';
    my $numeric = $type ne 'octets' && $type ne 'oid' && $type ne 'ipAddress';
    my $size = $type eq 'oid' ? $OID_STRING : $LONG_STRING;
    my @text;
    push @text, "# $entry->{text}$instance: $entry->{access}, $type" . ($units ne '' ? ", units $units" : '') . "\n";
    my @states = sort { $a->{value} <=> $b->{value} } @{ $entry->{enums} // [] };
    push @text, '# states: ' . join(', ', map { "$_->{value} = $_->{label}" } @states) . "\n" if @states;
    if (!$TYPE{$type}) {
        push @text, "# the MIB declares no type this module holds; no record is written, give the type in the link\n";
        return @text;
    }
    push @text, record_text($TYPE{$type}[0], "$object$suffix", 'INP', $link, $desc, $units, $numeric, $size) if $READABLE{ $entry->{access} };
    push @text, record_text($TYPE{$type}[1], "$object${suffix}_Set", 'OUT', $link, $desc, $units, $numeric, $size) if $WRITABLE{ $entry->{access} };
    return @text;
}

sub main {
    my (@arguments) = @_;
    binmode(STDOUT, ':encoding(UTF-8)') or die "cannot set the output encoding: $!\n";
    my ($probe, $system, @places, $range, $help) = ("$Bin/snmp3NativeProbe", '-');
    GetOptionsFromArray(\@arguments, 'probe=s' => \$probe, 'mib=s' => \@places, 'system=s' => \$system,
                        'index=s' => \$range, 'help' => \$help)
        or pod2usage(-exitval => 2, -verbose => 0, -output => \*STDERR);
    pod2usage(-exitval => 0, -verbose => 1) if $help;
    pod2usage(-exitval => 2, -verbose => 0, -output => \*STDERR, -message => 'snmp3MibDraft: no object name given') if !@arguments;
    for my $name (@arguments) {
        $name =~ /^[A-Za-z][\w-]*::[A-Za-z][\w-]*$/ or die "$name is not MODULE::name without an index\n";
    }
    my @indexes = defined $range ? parse_range($range) : ();
    # The places go to the probe as the IOC hands them over: the mibs directory of the module, then the places given
    # with the first one last, because the library takes a module from the place registered last.
    my @ordered = (-d "$Bin/../../mibs" ? "$Bin/../../mibs" : (), reverse @places);
    my $answers = describe_names($probe, $system, @ordered ? join(':', @ordered) : '-', @arguments);
    my @text = ("# Draft of records for " . join(' ', @arguments) . ".\n",
                "# P is the prefix of the record names and SESSION the name of the snmp3Session; the names, the scan and\n",
                "# everything the MIB does not state are for the author to finish, and snmp3MibCheck.pl checks the result.\n\n");
    for my $name (@arguments) {
        my $entry = $answers->{$name} or die "the probe gave no answer for $name\n";
        die "$name: no MIB place holds its module\n" if ($entry->{error} // '') eq 'module';
        die "$name: the MIB does not hold the name\n" if ($entry->{error} // '') eq 'name';
        die "$name: it is no object with a value\n" if ($entry->{error} // '') eq 'object';
        die "$name: the probe answered with " . $entry->{error} . "\n" if exists $entry->{error};
        if ($entry->{kind} eq 'column') {
            die "$name is a column of a table: give its indexes with --index\n" if !@indexes;
            push @text, object_records($entry, "_$_", ".$_"), "\n" for @indexes;
        } else {
            push @text, object_records($entry, '', '.0'), "\n";
        }
    }
    print @text or die "cannot write the draft: $!\n";
    return 0;
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "snmp3MibDraft: $error";
    $status = 2;
};
exit $status;

__END__

=head1 NAME

snmp3MibDraft.pl - write the draft of a database for SNMP objects named through a MIB

=head1 SYNOPSIS

snmp3MibDraft.pl [--probe PATH] [--mib PLACE]... [--system DIR] [--index FIRST..LAST] MODULE::name...

=head1 DESCRIPTION

For each MODULE::name, given without an index, writes on the standard output the records that read the object and,
when its access allows, the records that write it. The native probe of the module reads the MIB: the declared type
chooses the record type, the access chooses the direction, the description becomes the C<DESC> field cut to its
size in bytes, and units that fit the C<EGU> field become that field; units that do not fit stay in the comment above the
records.

=over 4

=item *

An integer becomes C<longin> and C<longout>; an unsigned integer, a counter, a gauge, a time in ticks and a 64-bit
counter become C<int64in> and C<int64out>; a string, an object identifier and an address become C<lsi> and C<lso>.
A string record has the size C<SIZV> 256, and 128 for an object identifier, the longest one the module takes. A write record
carries the fields the IOC requires of it.

=item *

The labels of an enumerated integer are written as a comment above its records, C<# states: 1 = off, 2 = on>,
in the order of the values; the author turns them into the states of the finished records.

=item *

The records of a column of a table are written for each index of the range given with B<--index>; a scalar object has
one instance, C<.0>, and takes no index. A column of a table whose rows have more than one index, or an index that
is not a number, gets the one arc given with B<--index>, which names no row; the author completes those links.

=item *

An object that can be written has an input record named C<name> and an output record named C<name_Set>; a column
adds C<_N> for the index N after the name and before C<_Set>, as C<name_1> and C<name_1_Set> for the index 1.

=item *

The record names carry the macro C<$(P)> and the link names the session with C<$(SESSION)>.

=back

The exit status is 0 when the draft was written and 2 for a fault of the call, of the probe or of a name.

=head1 OPTIONS

=over 4

=item B<--probe> PATH

The native probe of the module; by default C<snmp3NativeProbe> beside this script.

=item B<--mib> PLACE

A directory or a file with MIB modules, as C<snmp3Mib> takes it in the startup script of an IOC. Given more than
once, the first place that holds a module is taken. The C<mibs> directory that the module installs, two levels above
the directory of this script, comes after the given places and before the system directories.

=item B<--system> DIR

The directory of the system MIBs; by default the directories built into the library of the probe.

=item B<--index> FIRST..LAST

The indexes a column is drafted for, a single number or a range, at most 4096 indexes.

=item B<--help>

Prints this text and exits with status 0.

=back

=cut
