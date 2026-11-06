#!/usr/bin/env perl
use strict;
use warnings;
use Digest::SHA qw(sha256_hex);
use File::Basename qw(dirname);
use File::Copy qw(copy);
use File::Path qw(make_path);
use Getopt::Long qw(GetOptions);
use JSON::PP;
use Pod::Usage qw(pod2usage);
use POSIX ();

my $JSON = JSON::PP->new->canonical->pretty;
my ($source, $checker, $output, $help);

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

sub main {
    GetOptions('source=s' => \$source, 'checker=s' => \$checker, 'output=s' => \$output, 'help' => \$help)
        or pod2usage(2);
    pod2usage(-exitval => 0, -verbose => 1) if $help;
    defined($source) && defined($checker) && defined($output) && !-e $output or pod2usage(2);
    mkdir($output, 0700) or die "$output: $!\n";
    my $manifest = $JSON->decode(read_bytes("$source/manifest.json"));
    ref($manifest) eq 'HASH' or die "Invalid manifest\n";
    my @cases = (['positive', '', 0], ['application', 'application-runner', 1]);
    for my $unit (qw(PDU1 PDU2)) {
        push @cases, ["put-$unit", "$unit:write-refused", 1],
            ["value-$unit", "$unit:refused-write-value-unchanged", 1],
            ["monitor-value-$unit", "$unit:monitor-valid-value", 1];
    }
    push @cases, ['unrelated-put-error', 'PDU1:write-refused', 1],
        ['lost-online', 'PDU1:monitor-online-loss-recovery', 1],
        ['lost-alarm', 'PDU1:monitor-alarm-loss-recovery', 1],
        ['healthy-offline', 'PDU2:monitor-remains-online', 1],
        ['missing-monitor', '', 2], ['bad-hash', '', 2];
    write_bytes("$output/plan.json", $JSON->encode(\@cases));
    my @results;
    for my $case (@cases) {
        my ($name, $expected, $status) = @$case;
        my $dir = "$output/$name";
        mkdir($dir, 0700) or die "$dir: $!\n";
        my %members;
        for my $member (sort keys %$manifest) {
            my $original = read_bytes("$source/$member");
            sha256_hex($original) eq $manifest->{$member}{sha256} or die "Changed original: $member\n";
            next if $name eq 'missing-monitor' && $member eq 'monitor-PDU1.stdout';
            my $parent = dirname("$dir/$member");
            make_path($parent) if !-d $parent;
            -d $parent or die "$parent: could not create directory\n";
            my $data = $original;
            if ($name eq 'unrelated-put-error' && $member eq 'put-PDU1.stderr') {
                $data = "ERROR: unavailable\n";
            } elsif ($name eq 'application' && $member eq 'application/result.json') {
                my $doc = $JSON->decode($data); $doc->{passed} = JSON::PP::false; $data = $JSON->encode($doc);
            } elsif ($name =~ /^put-(PDU[12])$/ && $member eq 'terminal.json') {
                my $unit = $1;
                my $doc = $JSON->decode($data);
                for my $command (@{$doc->{commands}}) { $command->{native_status} = 0 if $command->{name} eq "put-$unit"; }
                $data = $JSON->encode($doc);
            } elsif ($name =~ /^value-(PDU[12])$/ && $member eq "after-put-$1.stdout") {
                $data =~ s/(ap8932\.phases\.ch01\.current\.value\s+double\s+=\s+)12\.3/${1}99/g or die "No value mutation\n";
            } elsif ($name =~ /^monitor-value-(PDU[12])$/ && $member eq "monitor-$1.stdout") {
                $data =~ s/(ap8932\.phases\.ch01\.current\.value\s+double\s+=\s+)12\.3/${1}99/g or die "No monitor mutation\n";
            } elsif ($name eq 'lost-online' && $member eq 'monitor-PDU1.stdout') {
                $data =~ s/(ap8932\.online\.value\s+double\s+=\s+)0\b/${1}1/g or die "No online mutation\n";
            } elsif ($name eq 'lost-alarm' && $member eq 'monitor-PDU1.stdout') {
                $data =~ s/(ap8932\.phases\.ch01\.current\.alarm\.severity\s+int32_t\s+=\s+)3\b/${1}0/g or die "No alarm mutation\n";
            } elsif ($name eq 'healthy-offline' && $member eq 'monitor-PDU2.stdout') {
                $data =~ s/(ap8932\.online\.value\s+double\s+=\s+)1\b/${1}0/g or die "No healthy mutation\n";
            } elsif ($name eq 'bad-hash' && $member eq 'monitor-PDU1.stdout') { $data .= "\ncorrupted\n"; }
            write_bytes("$dir/$member", $data);
            $members{$member} = {sha256 => sha256_hex($name eq 'bad-hash' ? $original : $data), bytes => length($data)};
        }
        write_bytes("$dir/manifest.json", $JSON->encode(\%members));
        my $pid = fork(); defined($pid) or die "fork: $!\n";
        if (!$pid) {
            open(STDOUT, '>', "$output/$name.stdout") or POSIX::_exit(126);
            open(STDERR, '>', "$output/$name.stderr") or POSIX::_exit(126);
            { no warnings 'exec'; exec {$^X} $^X, $checker, '--evaluate', $dir, '--result', "$output/$name.result.json"; }
            POSIX::_exit(127);
        }
        waitpid($pid, 0) == $pid or die "waitpid: $!\n";
        my $native = $?;
        my @failed;
        if (-f "$output/$name.result.json") {
            my $result = $JSON->decode(read_bytes("$output/$name.result.json"));
            @failed = map { $_->{name} } grep { !$_->{passed} } @{$result->{checks}};
        }
        my $correct = ($native >> 8) == $status && !($native & 127) &&
            (!$expected || grep { $_ eq $expected } @failed);
        push @results, {name => $name, expected_assertion => $expected, expected_status => $status,
            native_status => $native >> 8, signal => $native & 127, failed_assertions => \@failed,
            passed => $correct ? JSON::PP::true : JSON::PP::false};
        print(($correct ? 'PASS' : 'FAIL') . " $name\n");
    }
    for my $member (keys %$manifest) {
        sha256_hex(read_bytes("$source/$member")) eq $manifest->{$member}{sha256} or die "Original changed\n";
    }
    write_bytes("$output/result.json", $JSON->encode({checks => \@results,
        checker_sha256 => sha256_hex(read_bytes($checker)), proof_sha256 => sha256_hex(read_bytes($0)),
        original_manifest_sha256 => sha256_hex(read_bytes("$source/manifest.json"))}));
    return scalar(grep { !$_->{passed} } @results) ? 1 : 0;
}

my $status;
eval { $status = main(); defined($status) or die "Missing status\n"; 1 }
    or do { print STDERR "prove-pva: $@"; $status = 2; };
exit $status;

__END__

=head1 NAME

test_apcpdu_pva_controls.pl - Exercise the actual PVA evaluator on real capture copies

=head1 SYNOPSIS

perl tests/rewrite/test_apcpdu_pva_controls.pl --source CAPTURE --checker SCRIPT --output NEW_DIRECTORY

=head1 OPTIONS

--source names a frozen capture. --checker is tests/rewrite/test_apcpdu_pva.pl.
--output names a new control directory. --help displays usage.
One unchanged replay must pass, eleven independently mutated observations must
fail at their named assertions, and missing/corrupt input must exit incomplete.
These are evaluator controls, not additional product executions.

=head1 EXIT STATUS

0 means every expected outcome was observed, 1 means a control failed, and
2 means setup or tool error. Local target: Debian 13, Perl 5.40.1.

=cut
