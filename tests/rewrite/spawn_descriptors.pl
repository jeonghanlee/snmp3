#!/usr/bin/env perl
# Judge of the spawn descriptor cell. test_spawn_descriptors.bash starts the production IOC with four inheritable pipe
# descriptors and records what it saw: markers.txt (the targets of the four descriptors in the starting shell, one per line),
# worker.pid (the process number of the worker that the runtime report named as ready, 0 when none), ioc-fds.txt and
# worker-fds.txt (the descriptors of the IOC and of the worker, one line each: the number and the target of its link) and
# ioc.returncode. This script writes results.json with one entry for each check and prints one line per check and the verdict.
# As in the former cell, only worker-ready is judged when no worker was ready.
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

# Reads a descriptor file into a hash from the number to the target.
sub read_descriptors {
    my ($path) = @_;
    my %targets;
    for my $line (read_lines($path)) {
        $line =~ /^(\d+)\t(.*)$/ or die "malformed descriptor line in $path: $line\n";
        $targets{$1 + 0} = $2;
    }
    return %targets;
}

sub main {
    my ($output) = @_;
    defined $output && -d $output or die "usage: spawn_descriptors.pl OUTPUT_DIRECTORY\n";
    my ($worker) = read_lines("$output/worker.pid");
    defined $worker && $worker =~ /^\d+$/ or die "malformed worker.pid\n";
    my @markers = read_lines("$output/markers.txt");
    @markers == 4 or die "the markers file does not hold four descriptors\n";
    my %ioc = read_descriptors("$output/ioc-fds.txt");
    my %child = read_descriptors("$output/worker-fds.txt");
    my ($returncode) = read_lines("$output/ioc.returncode");
    my @checks;
    my $check = sub {
        my ($name, $passed) = @_;
        push @checks, { name => $name, passed => $passed ? JSON::PP::true : JSON::PP::false };
        printf "%s %s\n", $passed ? 'PASS' : 'FAIL', $name;
        return;
    };
    $check->('worker-ready', $worker > 0);
    if ($worker > 0) {
        my %in_ioc = map { $_ => 1 } values %ioc;
        my %in_worker = map { $_ => 1 } values %child;
        $check->('markers-open-in-ioc', !grep { !$in_ioc{$_} } @markers);
        $check->('markers-absent-in-worker', !grep { $in_worker{$_} } @markers);
        $check->('worker-stdin-stdout-null',
                 ($child{0} // '') eq '/dev/null' && ($child{1} // '') eq '/dev/null');
        $check->('worker-fd3-socket', ($child{3} // '') =~ /^socket:/);
        $check->('worker-no-other-pipe', !grep { /^pipe:/ } values %child);
    }
    my $passed = (@checks && !grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    my %detail = (ioc_fds => [ sort { $a <=> $b } keys %ioc ], worker_fds => { map { $_ => $child{$_} } sort { $a <=> $b } keys %child });
    write_file("$output/results.json", JSON::PP->new->canonical->pretty->encode({
        passed => $passed, checks => \@checks, detail => \%detail,
        ioc_returncode => (defined $returncode && $returncode =~ /^\d+$/) ? $returncode + 0 : $returncode }));
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
    print STDERR "spawn_descriptors.pl: $error";
    $status = 2;
};
exit $status;
