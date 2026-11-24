#!/usr/bin/env perl
# Judge of the component cell. test_r5_components.bash runs the programs and writes receipts.tsv, one line for each run: the
# name, the exit status, 1 when the bound of the cell ended the run, the bytes of the error output, the expected output ("-":
# not compared), the actual output and the SHA-256 of the program. This script writes results.json with one entry for each
# check (the name of the check is the name of the receipt of the former Python cell) and the digests of the sources, and prints
# one line per check and the verdict.
# A check passes when the program exits 0, was not ended by the bound, printed nothing to the error output and, for a digest,
# printed the expected value.
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use JSON::PP;

sub read_lines {
    my ($path) = @_;
    open(my $fh, '<', $path) or die "cannot read $path: $!\n";
    my @lines = <$fh>;
    close($fh);
    chomp @lines;
    return @lines;
}

sub write_file {
    my ($path, $text) = @_;
    open(my $fh, '>', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub main {
    my ($output) = @_;
    defined $output && -d $output or die "usage: r5_components.pl OUTPUT_DIRECTORY\n";
    my (@checks, %sources);
    for my $line (read_lines("$output/receipts.tsv")) {
        my ($name, $status, $forced, $errors, $expected, $actual, $product) = split /\t/, $line, -1;
        defined $product && $status =~ /^\d+$/ && $forced =~ /^[01]$/ && $errors =~ /^\d+$/
            or die "malformed receipt: $line\n";
        my $compared = $expected ne '-';
        my $passed = $status == 0 && !$forced && $errors == 0 && (!$compared || $actual eq $expected);
        push @checks, { name => $name, passed => $passed ? JSON::PP::true : JSON::PP::false,
                        exit_status => $status + 0, forced_cleanup => $forced ? JSON::PP::true : JSON::PP::false,
                        error_output_bytes => $errors + 0, expected => $compared ? $expected : undef,
                        actual => $compared ? $actual : undef, product_sha256 => $product };
        printf "%s %s exit=%d forced=%d stderr=%d%s\n", $passed ? 'PASS' : 'FAIL', $name, $status, $forced, $errors,
            $compared ? ($actual eq $expected ? ' digest=equal' : ' digest=DIFFERENT') : '';
    }
    @checks or die "no receipt\n";
    for my $line (read_lines("$output/sources.txt")) {
        $line =~ /^([0-9a-f]{64})\s+(\S.*)$/ or die "malformed source line: $line\n";
        $sources{$2} = $1;
    }
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    my $summary = { scope => 'Component executions only; IOC/worker/native traffic and R5 acceptance remain pending',
                    sources => \%sources, checks => \@checks, passed => $passed };
    write_file("$output/results.json", JSON::PP->new->canonical->pretty->encode($summary));
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
    print STDERR "r5_components.pl: $error";
    $status = 2;
};
exit $status;
