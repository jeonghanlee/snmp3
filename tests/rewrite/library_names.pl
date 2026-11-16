#!/usr/bin/env perl
# Judge of the library names cell: reads, for the tree it is given, the installed shared libraries of the module
# and the dynamic sections of its libraries and executables, and compares them with the names that the ABI number
# gives. Nothing is built or started; the tree is read as it stands.
#
# Table of checks. Expected names are literals: the ABI number 1 and the three library names.
#
#   check                    input                               state read                         expected
#   <name>-real-file         lib/<arch>/lib<name>.so.1           file type                          a regular file
#   <name>-soname            the same file                       SONAME of its dynamic section      lib<name>.so.1
#   <name>-link              lib/<arch>/lib<name>.so             link target                        lib<name>.so.1
#   <consumer>-needs         a library or executable             NEEDED entries naming libsnmp3*    the versioned names
#
# usage: library_names.pl TREE ARCH OUTPUT
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use JSON::PP;

my $ABI = 1;
my @LIBRARIES = qw(snmp3 snmp3Wire snmp3Native);
# Consumer, relative to the tree with ARCH filled in, and the module libraries it must name.
my @CONSUMERS = (
    ['snmp3Ioc',         'bin/ARCH/snmp3Ioc',            ['snmp3']],
    ['libsnmp3',         "lib/ARCH/libsnmp3.so.$ABI",    ['snmp3Wire']],
    ['snmp3Worker',      'bin/ARCH/snmp3Worker',         ['snmp3Native', 'snmp3Wire']],
    ['snmp3NativeProbe', 'bin/ARCH/snmp3NativeProbe',    ['snmp3Native']],
);

my @checks;

sub check {
    my ($name, $actual, $expected) = @_;
    my $passed = (defined $actual && $actual eq $expected) ? 1 : 0;
    push @checks, { name => $name, passed => $passed ? JSON::PP::true : JSON::PP::false,
                    actual => $actual, expected => $expected };
    printf "%s %s actual=[%s] expected=[%s]\n", $passed ? 'PASS' : 'FAIL', $name,
        defined $actual ? $actual : 'undefined', $expected;
    return;
}

# Entries of one kind in the dynamic section of a file; the reason when the file cannot be read.
sub dynamic {
    my ($path, $kind) = @_;
    return ("missing file") if !-e $path;
    my $pid = open(my $out, '-|');
    defined $pid or die "fork: $!\n";
    if (!$pid) {
        open(STDERR, '>', '/dev/null');
        exec { 'readelf' } 'readelf', '-d', $path;
        exit 127;
    }
    my @lines = <$out>;
    close($out);
    return ("readelf failed") if $? != 0;
    return map { /\($kind\).*\[(.*)\]/ ? $1 : () } @lines;
}

sub main {
    my ($tree, $arch, $output) = @_;
    defined $output or die "usage: library_names.pl TREE ARCH OUTPUT\n";
    for my $name (@LIBRARIES) {
        my $real = "$tree/lib/$arch/lib$name.so.$ABI";
        my $link = "$tree/lib/$arch/lib$name.so";
        check("$name-real-file", -l $real ? 'a link' : -f $real ? 'a regular file' : 'absent', 'a regular file');
        check("$name-soname", join(',', dynamic($real, 'SONAME')), "lib$name.so.$ABI");
        check("$name-link", -l $link ? readlink($link) : -e $link ? 'not a link' : 'absent', "lib$name.so.$ABI");
    }
    for my $consumer (@CONSUMERS) {
        my ($label, $relative, $needs) = @$consumer;
        (my $path = "$tree/$relative") =~ s/ARCH/$arch/;
        my @named = sort grep { /^libsnmp3|^missing file\z|^readelf failed\z/ } dynamic($path, 'NEEDED');
        check("$label-needs", join(',', @named), join(',', sort map { "lib$_.so.$ABI" } @$needs));
    }
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    open(my $fh, '>', "$output/results.json") or die "cannot write $output/results.json: $!\n";
    print {$fh} JSON::PP->new->canonical->pretty->encode({ scope => 'library names', passed => $passed, checks => \@checks })
        or die "cannot write $output/results.json: $!\n";
    close($fh) or die "cannot close $output/results.json: $!\n";
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "library_names.pl: $error";
    $status = 2;
};
exit $status;
