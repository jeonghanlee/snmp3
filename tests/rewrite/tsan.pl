#!/usr/bin/env perl
# Judge of the cell tsan. test_tsan.bash runs the test programs of the module under the thread sanitizer with one log
# directory per run and writes runs.tsv, one line per run: the name of the run and its exit status. This script reads
# the reports in the logs and writes results.json.
#
# A report is classified by the frames of its access and lock stacks. A frame inside the code of the module or of its
# tests (a path with /snmp3App/ or /tests/rewrite/) makes the report a failure. A report with no such frame is listed when tsan.supp
# names it with its reason, and otherwise it is unlisted, which also fails: EPICS Base and Net-SNMP are not
# instrumented, a report that stays inside them is classified once, by a person, and recorded in tsan.supp. The
# thread sanitizer's own suppressions are not used, because a suppression by module name would also hide a
# lock-order inversion of the module that shows a frame of libCom.
# Language level: Perl 5.26 (postfix dereference is the newest construct used).
use strict;
use warnings;
use JSON::PP;

my $MODULE_FRAME = qr{/snmp3App/|/tests/rewrite/};

sub read_file {
    my ($path) = @_;
    open(my $fh, '<', $path) or die "cannot read $path: $!\n";
    local $/;
    my $text = <$fh>;
    close($fh);
    return defined $text ? $text : '';
}

sub write_file {
    my ($path, $text) = @_;
    open(my $fh, '>', $path) or die "cannot write $path: $!\n";
    print {$fh} $text or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

# The list of reports that were classified as not the module's: one entry per line, kind, a pattern that the
# summary line of the report must match, and the reason, separated by a bar. Lines that start with # are comments.
sub load_known {
    my ($path) = @_;
    my @known;
    open(my $fh, '<', $path) or die "cannot read $path: $!\n";
    while (my $line = <$fh>) {
        $line =~ s/[\r\n]+\z//;
        next if $line =~ /^\s*(#|$)/;
        my ($kind, $pattern, $reason) = split /\s*\|\s*/, $line, 3;
        defined $reason or die "tsan.supp line has no reason: $line\n";
        push @known, { kind => $kind, pattern => qr/$pattern/, reason => $reason };
    }
    close($fh);
    return @known;
}

# Splits a log file into its reports. A report runs from the line WARNING: ThreadSanitizer to the line of equal signs.
sub reports_of {
    my ($text) = @_;
    my @reports;
    my $current;
    for my $line (split /\n/, $text) {
        if ($line =~ /^WARNING: ThreadSanitizer: (\S+(?: \S+)?)/ && !$current) {
            $current = { kind => $1, lines => [$line] };
            $current->{kind} =~ s/\s+\(.*//;
            next;
        }
        next unless $current;
        if ($line =~ /^={10,}$/) { push @reports, $current; undef $current; next; }
        push $current->{lines}->@*, $line;
    }
    push @reports, $current if $current;
    return @reports;
}

# The frames of the stacks that show what happened: the accesses of a data race, the acquisitions of a lock-order
# cycle and the call that took the lock. The stacks that only show where a thread, a mutex or a block of memory was
# created are not among them: a thread that the test program started names the program, not the code that raced.
sub access_frames {
    my ($report) = @_;
    my (@frames, $counting);
    for my $line ($report->{lines}->@*) {
        if ($line =~ /^  (\S.*)$/) {
            my $header = $1;
            $counting = $header =~ /^(?:Write|Read|Atomic|Previous|Mutex M\d+ (?:acquired|previously acquired)|Mutex M\d+ \(.*\) (?:acquired|previously acquired))/ ? 1 : 0;
            next;
        }
        push @frames, $line if $counting && $line =~ /^\s+#\d+ /;
    }
    return @frames;
}

sub classify {
    my ($report, @known) = @_;
    my @frames = access_frames($report);
    my ($summary) = grep { /^SUMMARY: / } $report->{lines}->@*;
    $summary = '' unless defined $summary;
    return ('module', 'a frame in the code of the module or of its tests')
        if grep { $_ =~ $MODULE_FRAME } @frames, $summary;
    for my $entry (@known) {
        next unless $report->{kind} eq $entry->{kind} && $summary =~ $entry->{pattern};
        return ('listed', $entry->{reason});
    }
    return ('unlisted', 'no frame in the module, and tsan.supp does not name it');
}

sub command_judge {
    my ($output, $known_path) = @_;
    my @known = load_known($known_path);
    my (@checks, @listed, @failing);
    my $runs = read_file("$output/runs.tsv");
    for my $line (split /\n/, $runs) {
        my ($name, $status) = split /\t/, $line;
        push @checks, { name => "run-$name-exit", passed => ($status eq '0' ? JSON::PP::true : JSON::PP::false),
                        actual => $status, expected => '0' };
    }
    # The negative controls: the cell fails when the thread sanitizer did not report the race a control made.
    if (-s "$output/controls.tsv") {
        for my $line (split /\n/, read_file("$output/controls.tsv")) {
            my ($name, $status) = split /\t/, $line;
            push @checks, { name => "control-$name-reported", passed => ($status eq '0' ? JSON::PP::true : JSON::PP::false),
                            actual => $status eq '0' ? 'reported' : 'not reported', expected => 'reported' };
        }
    }
    my %count = (module => 0, listed => 0, unlisted => 0);
    for my $log (sort glob("$output/logs/*/tsan.*")) {
        my ($run) = $log =~ m{/logs/([^/]+)/};
        for my $report (reports_of(read_file($log))) {
            my ($class, $reason) = classify($report, @known);
            $count{$class}++;
            my ($summary) = grep { /^SUMMARY: / } $report->{lines}->@*;
            my %row = (run => $run, kind => $report->{kind}, class => $class, reason => $reason,
                       summary => defined $summary ? $summary : '');
            push @failing, \%row if $class ne 'listed';
            push @listed, \%row if $class eq 'listed';
        }
    }
    push @checks, { name => 'reports-with-a-frame-in-the-module', passed => $count{module} == 0 ? JSON::PP::true : JSON::PP::false,
                    actual => $count{module}, expected => 0 };
    push @checks, { name => 'reports-unlisted', passed => $count{unlisted} == 0 ? JSON::PP::true : JSON::PP::false,
                    actual => $count{unlisted}, expected => 0 };
    my $passed = (!grep { !$_->{passed} } @checks) ? JSON::PP::true : JSON::PP::false;
    write_file("$output/results.json", JSON::PP->new->canonical->pretty->encode(
        { scope => 'tsan', passed => $passed, checks => \@checks, listed => \@listed, failing => \@failing }));
    for my $check (@checks) {
        printf "%s %s actual=[%s] expected=[%s]\n", $check->{passed} ? 'PASS' : 'FAIL', $check->{name},
            $check->{actual}, $check->{expected};
    }
    printf "listed reports: %d (%s)\n", scalar @listed, join('; ', map { "$_->{run}: $_->{kind}" } @listed) if @listed;
    printf "%s: %s/results.json\n", $passed ? 'PASS' : 'FAIL', $output;
    return $passed ? 0 : 1;
}

# Replaces one text of a file of a tree copy by another, for a negative control: the text must occur exactly once.
sub command_apply {
    my ($file, $old, $new) = @_;
    my $text = read_file($file);
    my $count = () = $text =~ /\Q$old\E/g;
    $count == 1 or die "the text to replace occurs $count times in $file\n";
    $text =~ s/\Q$old\E/$new/;
    write_file($file, $text);
    return 0;
}

# Succeeds when a report of the logs has a frame of the module in an access or lock stack, and the frames of that
# stack show the given text: the negative control names the function in which it took a lock out.
sub command_frames {
    my ($output, $pattern) = @_;
    my $matched = 0;
    for my $log (sort glob("$output/logs/*/tsan.*")) {
        for my $report (reports_of(read_file($log))) {
            my @frames = access_frames($report);
            next unless grep { $_ =~ $MODULE_FRAME } @frames;
            $matched++ if grep { index($_, $pattern) >= 0 } @frames;
        }
    }
    printf "%d report(s) with a frame of the module that names %s\n", $matched, $pattern;
    return $matched ? 0 : 1;
}

sub main {
    my ($command, @args) = @_;
    defined $command or die "usage: tsan.pl judge|apply|frames ARGUMENTS\n";
    return command_judge(@args) if $command eq 'judge' && @args == 2;
    return command_apply(@args) if $command eq 'apply' && @args == 3;
    return command_frames(@args) if $command eq 'frames' && @args == 2;
    die "usage: tsan.pl judge OUTPUT KNOWN_LIST | apply FILE OLD NEW | frames OUTPUT TEXT\n";
}

my $status;
eval {
    $status = main(@ARGV);
    defined $status or die "internal error: main returned no exit status\n";
    1;
} or do {
    my $error = $@;
    print STDERR "tsan.pl: $error";
    $status = 2;
};
exit $status;
