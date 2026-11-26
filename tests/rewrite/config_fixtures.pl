#!/usr/bin/env perl
# Input generator of the configuration cell. test_config.bash runs the capabilities of the native probe first and then calls
# this script once: it writes the configuration documents, secret files, helper links and IOC startup files of the cell into
# OUTPUT/fixtures and OUTPUT, and writes OUTPUT/cases.tsv, the ordered list of the runs that the cell makes after the capabilities
# run (one line each; see test_config.bash for the columns). It runs nothing and judges nothing.
# usage: config_fixtures.pl OUTPUT_DIRECTORY CAPABILITIES_JSON
# The documents are written with the same bytes as the former Python cell: the order of the keys of the seed is kept and the
# text is written as json.dumps writes it (", " and ": " as separators), because several cases replace a text of the seed.
# Language level: Perl 5.26.
use strict;
use warnings;
no warnings 'utf8';
use Cwd qw(abs_path);
use File::Basename qw(dirname);
use JSON::PP;
use POSIX ();

my $ARCH = 'linux-x86_64';
my $ROOT = abs_path(dirname(__FILE__) . '/../..');

# --- ordered JSON ---------------------------------------------------------------------------------------------------------
package OJ {
    sub new { return bless { k => [], v => {} }, shift; }
    sub set {
        my ($self, $key, $value) = @_;
        push @{ $self->{k} }, $key unless exists $self->{v}{$key};
        $self->{v}{$key} = $value;
        return $self;
    }
    sub get { return $_[0]{v}{ $_[1] }; }
    sub delete {
        my ($self, $key) = @_;
        return unless exists $self->{v}{$key};
        delete $self->{v}{$key};
        @{ $self->{k} } = grep { $_ ne $key } @{ $self->{k} };
        return;
    }
    sub keys { return @{ $_[0]{k} }; }
}
package JN { sub new { my ($c, $l) = @_; return bless \$l, $c; } }
package JB { sub new { my ($c, $b) = @_; return bless \$b, $c; } }
package main;

sub N { return JN->new("$_[0]"); }
sub T { return JB->new(1); }

sub parse_json {
    my ($text) = @_;
    my $position = 0;
    my $value = parse_value(\$text, \$position);
    return $value;
}

sub skip_space {
    my ($text, $position) = @_;
    $$position++ while $$position < length($$text) && substr($$text, $$position, 1) =~ /\s/;
    return;
}

sub parse_value {
    my ($text, $position) = @_;
    skip_space($text, $position);
    my $c = substr($$text, $$position, 1);
    if ($c eq '{') {
        my $object = OJ->new;
        $$position++;
        skip_space($text, $position);
        if (substr($$text, $$position, 1) eq '}') { $$position++; return $object; }
        while (1) {
            skip_space($text, $position);
            my $key = parse_string($text, $position);
            skip_space($text, $position);
            substr($$text, $$position++, 1) eq ':' or die "malformed object\n";
            $object->set($key, parse_value($text, $position));
            skip_space($text, $position);
            my $next = substr($$text, $$position++, 1);
            last if $next eq '}';
            $next eq ',' or die "malformed object\n";
        }
        return $object;
    }
    if ($c eq '[') {
        my @array;
        $$position++;
        skip_space($text, $position);
        if (substr($$text, $$position, 1) eq ']') { $$position++; return \@array; }
        while (1) {
            push @array, parse_value($text, $position);
            skip_space($text, $position);
            my $next = substr($$text, $$position++, 1);
            last if $next eq ']';
            $next eq ',' or die "malformed array\n";
        }
        return \@array;
    }
    return parse_string($text, $position) if $c eq '"';
    pos($$text) = $$position;
    if ($$text =~ /\G(-?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)/gc) { $$position = pos($$text); return N($1); }
    if ($$text =~ /\G(true|false)/gc) { $$position = pos($$text); return JB->new($1 eq 'true' ? 1 : 0); }
    die "unsupported JSON text at $$position\n";
}

sub parse_string {
    my ($text, $position) = @_;
    substr($$text, $$position++, 1) eq '"' or die "malformed string\n";
    my $out = '';
    while (1) {
        my $c = substr($$text, $$position++, 1);
        last if $c eq '"';
        if ($c eq '\\') {
            my $e = substr($$text, $$position++, 1);
            my %simple = ('n' => "\n", 'r' => "\r", 't' => "\t", 'b' => "\b", 'f' => "\f", '"' => '"', '\\' => '\\', '/' => '/');
            if (exists $simple{$e}) { $out .= $simple{$e}; next; }
            $e eq 'u' or die "malformed escape\n";
            my $code = hex(substr($$text, $$position, 4));
            $$position += 4;
            $out .= chr($code);
            next;
        }
        $out .= $c;
    }
    return $out;
}

# The text json.dumps writes: ", " and ": " as separators, every control character escaped; with $ascii also every character
# outside the printable ASCII range.
sub dump_string {
    my ($s, $ascii) = @_;
    my %short = ("\n" => '\n', "\r" => '\r', "\t" => '\t', "\b" => '\b', "\f" => '\f', '"' => '\"', '\\' => '\\\\');
    my $out = '"';
    for my $c (split //, $s) {
        my $o = ord($c);
        if (exists $short{$c}) { $out .= $short{$c}; }
        elsif ($o < 0x20 || ($ascii && ($o < 0x20 || $o > 0x7e))) {
            if ($o > 0xffff) {
                my $v = $o - 0x10000;
                $out .= sprintf('\u%04x\u%04x', 0xd800 | ($v >> 10), 0xdc00 | ($v & 0x3ff));
            } else {
                $out .= sprintf('\u%04x', $o);
            }
        } else { $out .= $c; }
    }
    return $out . '"';
}

sub dump_json {
    my ($v, $ascii) = @_;
    my $type = ref $v;
    return 'null' unless defined $v;
    return '{' . join(', ', map { dump_string($_, $ascii) . ': ' . dump_json($v->get($_), $ascii) } $v->keys) . '}' if $type eq 'OJ';
    return '[' . join(', ', map { dump_json($_, $ascii) } @$v) . ']' if $type eq 'ARRAY';
    return $$v if $type eq 'JN';
    return $$v ? 'true' : 'false' if $type eq 'JB';
    return dump_string($v, $ascii);
}

sub clone {
    my ($v) = @_;
    my $type = ref $v;
    if ($type eq 'OJ') {
        my $copy = OJ->new;
        $copy->set($_, clone($v->get($_))) for $v->keys;
        return $copy;
    }
    return [map { clone($_) } @$v] if $type eq 'ARRAY';
    return $v;
}

sub read_file {
    my ($path) = @_;
    open(my $fh, '<:raw', $path) or die "cannot read $path: $!\n";
    local $/;
    my $text = <$fh>;
    close($fh);
    return $text;
}

sub write_bytes {
    my ($path, $bytes) = @_;
    open(my $fh, '>:raw', $path) or die "cannot write $path: $!\n";
    print {$fh} $bytes or die "cannot write $path: $!\n";
    close($fh) or die "cannot close $path: $!\n";
    return;
}

sub utf8_bytes {
    my ($text) = @_;
    my $copy = $text;
    utf8::encode($copy);
    return $copy;
}

sub replace_first {
    my ($text, $old, $new) = @_;
    my $at = index($text, $old);
    $at >= 0 or die "text to replace not found: $old\n";
    substr($text, $at, length($old)) = $new;
    return $text;
}

sub main {
    my ($output, $capabilities_file) = @_;
    defined $capabilities_file && -d $output or die "usage: config_fixtures.pl OUTPUT_DIRECTORY CAPABILITIES_JSON\n";
    $output = abs_path($output);
    my $fixtures = "$output/fixtures";
    -d $fixtures or die "no fixtures directory\n";
    my $probe = "$ROOT/bin/$ARCH/snmp3NativeProbe";
    my $product = "$ROOT/bin/$ARCH/snmp3ConfigTest";
    my $binary = "$ROOT/bin/$ARCH/snmp3Ioc";
    my $worker = "$ROOT/bin/$ARCH/snmp3Worker";
    my $capabilities = decode_json(read_file($capabilities_file));
    my $auth = $capabilities->{authentication}[0]{name};
    my $priv = $capabilities->{privacy}[0]{name};

    my $seed = parse_json(read_file("$ROOT/tests/rewrite/config/seed.json"));
    for my $profile (@{ $seed->get('profiles') }) {
        $profile->set('authAlgorithm', $auth) if defined $profile->get('authAlgorithm');
        $profile->set('privAlgorithm', $priv) if defined $profile->get('privAlgorithm');
    }
    my @cases;
    # Checks recorded before the first document case: lifecycle (2), native capabilities (2), native cli (2).
    my $count = 6;
    my @sentinels;
    my @prehash;

    my $hash_before = sub {
        my ($path) = @_;
        open(my $fh, '-|', 'sha256sum', '--', $path) or die "cannot run sha256sum: $!\n";
        my $line = <$fh>;
        close($fh) or die "sha256sum failed for $path\n";
        push @prehash, $line;
        return;
    };

    my $write_case = sub {
        my ($name, $document, $raw) = @_;
        my $path = "$fixtures/$name.json";
        my $data = $raw ? $document : utf8_bytes(dump_json($document, 0));
        write_bytes($path, $data);
        return $path;
    };

    my $component = sub {
        my ($name, $mode, @arguments) = @_;
        push @cases, join("\t", 'component', $name, 0, 1, '-', 30, '-', $product, $mode, @arguments);
        $count += 3;
        return;
    };

    my $good;
    my $negative = sub {
        my ($name, $document, $options) = @_;
        $options ||= {};
        my $bad = $write_case->($name, $document, $options->{raw});
        my $bad_probe = $options->{bad_probe} // $probe;
        if (defined $options->{reason}) {
            $component->($name, 'negative-reason', $good, $probe, $bad, $bad_probe, $options->{reason});
        } else {
            $component->($name, 'negative', $good, $probe, $bad, $bad_probe);
        }
        return;
    };

    my $changed = sub {
        my ($name, $section, $index, $key, $value, $reason) = @_;
        my $document = clone($seed);
        my $target = $document->get($section)->[$index];
        if (defined $value) { $target->set($key, $value); } else { $target->delete($key); }
        $negative->($name, $document, { reason => $reason });
        return;
    };

    # secret files
    for my $name (qw(community auth priv)) {
        my $sentinel = 'FixtureSecret_' . join('', map { sprintf '%02x', int(rand(256)) } 1 .. 16);
        push @sentinels, $sentinel;
        write_bytes("$fixtures/$name.txt", "$sentinel\n");
        chmod 0600, "$fixtures/$name.txt";
    }
    write_bytes("$output/sentinels.txt", join('', map { "$_\n" } @sentinels));
    chmod 0600, "$output/sentinels.txt";

    $good = $write_case->('valid', $seed);
    my $replacement = clone($seed);
    $replacement->get('profiles')->[1]->set('timeoutMs', N(1500));
    $replacement->get('profiles')->[1]->set('checkRanges', JB->new(0));
    my $replacement_path = $write_case->('replacement', $replacement);
    my $operator_example = $write_case->('operator-example', parse_json(read_file("$ROOT/tests/rewrite/config/v2c.json")));
    $component->('V3.13_operator_JSON', 'accept', $operator_example, $probe);
    $component->('V3.2_4_5_8_9_positive', 'positive', $good, $probe, $replacement_path);
    $component->('V3.1_parser_limits', 'json-limits');
    push @cases, join("\t", 'stage', 'documents-start');

    my $strict = dump_json($seed, 1);
    my @texts = (
        ['json5-unquoted', replace_first($strict, '"schema"', 'schema')],
        ['json5-single-quote', replace_first($strict, '"schema"', "'schema'")],
        ['json5-trailing-comma', substr($strict, 0, -1) . ',}'],
        ['json-comment', '/* configuration */' . $strict],
        ['json-line-comment', "// configuration\n" . $strict],
        ['json-trailing-data', $strict . ' false'],
        ['json-truncated', substr($strict, 0, -1)],
        ['json-duplicate-root', replace_first($strict, '"schema": 1', '"schema": 1, "schema": 1')],
        ['json-duplicate-nested', replace_first($strict, '"id": "V1"', '"id": "V1", "id": "V1"')],
        ['json-escaped-duplicate', replace_first($strict, '"id": "V1"', '"id": "V1", "\u0069d": "V1"')],
        ['json-depth', ('[' x 33) . '0' . (']' x 33)],
        ['json-empty', ''],
    );
    $negative->($_->[0], $_->[1], { raw => 1 }) for @texts;
    $negative->('json-invalid-utf8', replace_first($strict, 'fixture-user', "\xff"), { raw => 1 });
    for my $entry (['overlong2', "\xc0\xaf"], ['overlong3', "\xe0\x80\xaf"], ['overlong4', "\xf0\x80\x80\xaf"],
                   ['surrogate', "\xed\xa0\x80"], ['above-max', "\xf4\x90\x80\x80"], ['leading-f5', "\xf5\x80\x80\x80"],
                   ['continuation', "\x80"], ['incomplete2', "\xc2"], ['incomplete3', "\xe0\xa0"], ['incomplete4', "\xf0\x90\x80"],
                   ['bad-continuation', "\xc2A"]) {
        $negative->('utf8-' . $entry->[0], replace_first($strict, 'fixture-user', $entry->[1]), { raw => 1 });
    }
    for my $entry (['low', '"\udc00"'], ['high', '"\ud800"'], ['high-followed-by-scalar', '"\ud800\u0041"']) {
        $negative->('unicode-' . $entry->[0], replace_first($strict, '"fixture-user"', $entry->[1]), { raw => 1 });
    }
    $negative->('json-size', ' ' x (1024 * 1024 + 1), { raw => 1 });
    for my $entry ([N(0), '0'], [N(2), '2'], ['1', '1'], [T(), 'True'], [N('1.0'), '1-0']) {
        my $document = clone($seed);
        $document->set('schema', $entry->[0]);
        $negative->('schema-' . $entry->[1], $document);
    }
    for my $section (qw(profiles endpoints bindings)) {
        my $document = clone($seed);
        $document->delete($section);
        $negative->("missing-$section", $document);
        $document = clone($seed);
        $document->set($section, OJ->new);
        $negative->("wrong-array-$section", $document);
        $document = clone($seed);
        push @{ $document->get($section) }, clone($document->get($section)->[0]);
        $negative->("duplicate-$section", $document);
        $changed->("unknown-$section", $section, 0, 'unused', N(1));
    }
    my $document = clone($seed);
    $document->set('unused', N(1));
    $negative->('unknown-root', $document);
    for my $field (['timeoutMs', 1, 60000], ['retries', 0, 10], ['maxVarbinds', 1, 1024]) {
        my ($key, $low, $high) = @$field;
        for my $entry ([N($low - 1), $low - 1], [N($high + 1), $high + 1], [N('1.5'), '1-5'], ['1', '1'], [T(), 'True']) {
            $changed->("$key-$entry->[1]", 'profiles', 1, $key, $entry->[0]);
        }
    }
    for my $entry ([N(0), '0'], ['true', 'true'], [N(1), '1']) {
        $changed->("checkRanges-$entry->[1]", 'profiles', 1, 'checkRanges', $entry->[0]);
    }
    for my $value ('', 'x' x 65, 'non ascii', "\x{e9}", 'bad/id', "bad\x00id") {
        $changed->("bad-id-$count", 'profiles', 0, 'id', $value);
    }
    $changed->('missing-profile', 'endpoints', 0, 'profile', 'absent');
    $changed->('missing-endpoint', 'bindings', 0, 'endpoint', 'absent');
    for my $value ('localhost', 'udp:127.0.0.1:161', '127.0.0.999', '') {
        $changed->("bad-address-$count", 'endpoints', 0, 'address', $value);
    }
    for my $entry ([N(0), '0'], [N(65536), '65536'], [N('1.5'), '1-5']) {
        $changed->("bad-port-$entry->[1]", 'endpoints', 0, 'port', $entry->[0]);
    }
    for my $value ('', '1', '1.40', '3.0', '1.3.', '1..3', '1.-1', '1.4294967296', 'name.oid', '.1.3.' . join('.', ('0') x 127)) {
        $changed->("bad-oid-$count", 'bindings', 0, 'oid', $value);
    }
    $changed->('bad-operation', 'bindings', 0, 'operation', 'walk');
    $changed->('bad-value-type', 'bindings', 0, 'valueType', 'unknown');
    for my $entry ([N(0), '0'], [N(2), '2'], [N('1.5'), '1-5']) {
        $changed->("bad-scalar-capacity-$entry->[1]", 'bindings', 0, 'capacity', $entry->[0]);
    }
    $changed->('bad-octet-capacity', 'bindings', 2, 'capacity', N(1024 * 1024 + 1));
    $changed->('group-on-set', 'bindings', 1, 'group', 'Batch', 'group member must be a get binding');
    for my $entry ([N(1), 'configuration string rejected'], [T(), 'configuration string rejected'],
                   ["bad\x00group", 'configuration string rejected'], ['', 'configuration identifier rejected'],
                   ['g' x 65, 'configuration identifier rejected'], ['bad/group', 'configuration identifier rejected'],
                   ['non ascii', 'configuration identifier rejected']) {
        $changed->("bad-group-$count", 'bindings', 0, 'group', $entry->[0], $entry->[1]);
    }
    $document = clone($seed);
    $document->get('bindings')->[0]->set('group', 'Batch');
    $document->get('bindings')->[2]->set('group', 'Batch');
    $negative->('group-two-endpoints', $document, { reason => 'group spans endpoints' });
    $document = clone($seed);
    $document->set('bindings', [map {
        my $binding = clone($seed->get('bindings')->[0]);
        $binding->set('id', "Member$_");
        $binding->set('group', 'Batch');
        $binding;
    } 0 .. 32]);
    $negative->('group-over-maxvarbinds', $document, { reason => 'group exceeds profile maxVarbinds' });
    $document = clone($seed);
    $document->set('bindings', [map {
        my $binding = clone($seed->get('bindings')->[2]);
        $binding->set('id', "Large$_");
        $binding->set('endpoint', 'Device');
        $binding->set('group', 'Batch');
        $binding->set('capacity', N(1024 * 1024));
        $binding;
    } 0 .. 1]);
    $negative->('group-over-frame', $document, { reason => 'group exceeds the request frame' });
    $changed->("v2-forbids-$_", 'profiles', 1, $_, 'unused')
        for qw(user securityLevel authAlgorithm authSecretFile privAlgorithm privSecretFile contextName securityEngineId contextEngineId);
    $changed->('v3-forbids-community', 'profiles', 2, 'communityFile', 'community.txt');
    $changed->('v1-missing-community', 'profiles', 0, 'communityFile', undef);
    $changed->('v3-missing-user', 'profiles', 2, 'user', undef);
    $changed->('v3-empty-user', 'profiles', 2, 'user', '');
    $changed->('v3-user-size', 'profiles', 2, 'user', 'x' x 33);
    $changed->('v3-context-size', 'profiles', 2, 'contextName', 'x' x 256);
    $changed->('bad-version', 'profiles', 1, 'version', '4');
    $changed->('bad-security-level', 'profiles', 2, 'securityLevel', 'privOnly');
    $changed->("noauth-forbids-$_", 'profiles', 2, $_, 'unused') for qw(authAlgorithm authSecretFile privAlgorithm privSecretFile);
    $changed->('auth-missing-algorithm', 'profiles', 3, 'authAlgorithm', undef);
    $changed->('auth-missing-secret', 'profiles', 3, 'authSecretFile', undef);
    $changed->('auth-forbids-privacy', 'profiles', 3, 'privAlgorithm', $priv);
    $changed->('priv-missing-algorithm', 'profiles', 4, 'privAlgorithm', undef);
    $changed->('priv-missing-secret', 'profiles', 4, 'privSecretFile', undef);
    $changed->('auth-unavailable', 'profiles', 3, 'authAlgorithm', 'FixtureUnavailable');
    $changed->('priv-unavailable', 'profiles', 4, 'privAlgorithm', 'FixtureUnavailable');
    for my $field (qw(securityEngineId contextEngineId)) {
        for my $value ('', '80000001', '80' x 33, '800000001', 'gg00000001', '00' x 5, 'ff' x 5, '0x') {
            $changed->("engine-$field-$count", 'profiles', 4, $field, $value);
        }
    }

    my $secret_case = sub {
        my ($name, %option) = @_;
        my $kind = $option{kind} // 'regular';
        my $mode = $option{mode} // 0600;
        my $index = $option{index} // 1;
        my $field = $option{field} // 'communityFile';
        my $path = "$fixtures/secret-$name";
        if ($kind eq 'symlink') { symlink("$fixtures/community.txt", $path) or die "symlink: $!\n"; }
        elsif ($kind eq 'directory') { mkdir($path) or die "mkdir: $!\n"; }
        elsif ($kind eq 'fifo') { POSIX::mkfifo($path, 0600) or die "mkfifo: $!\n"; }
        elsif ($kind eq 'missing') { }
        else {
            write_bytes($path, $option{data});
            $hash_before->($path);
            chmod $mode, $path;
        }
        $changed->("secret-$name", 'profiles', $index, $field, "secret-$name");
        return;
    };
    for my $mode (0644, 0640, 0604, 0610, 0601, 0) {
        $secret_case->('mode-' . sprintf('%o', $mode), data => 'fixture-community', mode => $mode);
    }
    $secret_case->($_, kind => $_) for qw(symlink directory fifo missing);
    for my $entry (['empty', ''], ['nul', "secret\x00value"], ['control', "secret\x01value"],
                   ['two-newlines', "fixture-secret\n\n"], ['community256', 'x' x 256], ['large-file', 'x' x 1027]) {
        $secret_case->($entry->[0], data => $entry->[1]);
    }
    $secret_case->('short-auth', data => 'x' x 7, index => 3, field => 'authSecretFile');
    $secret_case->('long-auth', data => 'x' x 1025, index => 3, field => 'authSecretFile');
    my $wrong_owner = "$fixtures/secret-owner";
    write_bytes($wrong_owner, 'fixture-community');
    $hash_before->($wrong_owner);
    chmod 0600, $wrong_owner;
    my $owner_note = "$output/not-run.txt";
    if (chown($> != 0 ? 0 : 1, -1, $wrong_owner)) {
        $changed->('secret-wrong-owner', 'profiles', 1, 'communityFile', 'secret-owner');
    } else {
        my $errno = $! + 0;
        $errno == 1 || $errno == 13 || $errno == 22 or die "chown failed: $!\n";
        write_bytes($owner_note, "V3.3_wrong_owner\tcannot create an independently owned regular fixture; chown errno=$errno\n");
    }
    push @cases, join("\t", 'stage', 'negative-documents');

    my @positive;
    for my $value (0x80, 0x7ff, 0x800, 0xd7ff, 0xe000, 0xffff, 0x10000, 0x10ffff) {
        my $doc = clone($seed);
        $doc->get('profiles')->[2]->set('user', chr($value));
        push @positive, [sprintf('utf8-valid-%x', $value), $doc];
    }
    $component->('unicode-valid-pair', 'accept',
                 $write_case->('unicode-valid-pair', replace_first($strict, '"fixture-user"', '"\ud834\udd1e"'), 1), $probe);
    $document = clone($seed);
    for my $field (['timeoutMs', 1, 60000], ['retries', 0, 10], ['maxVarbinds', 1, 1024]) {
        $document->get('profiles')->[0]->set($field->[0], N($field->[1]));
        $document->get('profiles')->[1]->set($field->[0], N($field->[2]));
    }
    $document->get('profiles')->[0]->set('id', 'i' x 64);
    $document->get('profiles')->[2]->set('user', 'u' x 32);
    $document->get('profiles')->[2]->set('contextName', 'c' x 255);
    $document->get('profiles')->[4]->set('securityEngineId', '0X800000000A');
    $document->get('profiles')->[4]->set('contextEngineId', '800000000b');
    $document->get('endpoints')->[0]->set('port', N(65535));
    $document->get('bindings')->[0]->set('oid', '.2.4294967295.' . join('.', ('0') x 126));
    $document->get('bindings')->[2]->set('capacity', N(1024 * 1024));
    push @positive, ['schema-boundaries', $document];
    $document = clone($seed);
    $document->set('bindings', [map {
        my $binding = clone($seed->get('bindings')->[0]);
        $binding->set('id', "Type_$_");
        $binding->set('valueType', $_);
        $binding->set('capacity', N($_ eq 'oid' ? 128 : 1));
        $binding;
    } qw(integer unsigned32 counter32 gauge32 timeticks counter64 octets oid ipAddress opaqueFloat opaqueDouble)]);
    push @positive, ['binding-types', $document];
    $document = clone($seed);
    my @members = map {
        my $binding = clone($seed->get('bindings')->[0]);
        $binding->set('id', "Member$_");
        $binding->set('group', 'Batch');
        $binding;
    } 0 .. 31;
    my $alone = clone($seed->get('bindings')->[2]);
    $alone->set('id', 'Alone');
    $alone->set('group', 'Single');
    $document->set('bindings', [@members, $alone]);
    push @positive, ['group-at-maxvarbinds', $document];
    my $empty = OJ->new;
    $empty->set('schema', N(1));
    $empty->set('profiles', []);
    $empty->set('endpoints', []);
    $empty->set('bindings', []);
    push @positive, ['empty-config', $empty];
    $document = clone($seed);
    for my $index (0 .. 4096 - scalar(@{ $document->get('profiles') }) - 1) {
        my $profile = OJ->new;
        $profile->set('id', "P$index");
        $profile->set('version', '1');
        $profile->set('communityFile', 'community.txt');
        push @{ $document->get('profiles') }, $profile;
    }
    push @positive, ['definitions4096', $document];
    my $excessive = clone($document);
    my $overflow = OJ->new;
    $overflow->set('id', 'overflow');
    $overflow->set('version', '1');
    $overflow->set('communityFile', 'community.txt');
    push @{ $excessive->get('profiles') }, $overflow;
    $negative->('definitions4097', $excessive);
    for my $entry (['community255', ('x' x 255) . "\r\n", 1, 'communityFile'], ['auth8', ('x' x 8) . "\n", 3, 'authSecretFile'],
                   ['auth1024', ('x' x 1024) . "\r\n", 3, 'authSecretFile']) {
        my ($name, $data, $index, $field) = @$entry;
        write_bytes("$fixtures/$name", $data);
        chmod 0600, "$fixtures/$name";
        my $doc = clone($seed);
        $doc->get('profiles')->[$index]->set($field, "$fixtures/$name");
        push @positive, [$name, $doc];
    }
    for my $entry (['authentication', 3, 'authAlgorithm'], ['privacy', 4, 'privAlgorithm']) {
        my ($category, $index, $field) = @$entry;
        for my $algorithm (@{ $capabilities->{$category} }) {
            my $doc = clone($seed);
            $doc->get('profiles')->[$index]->set($field, $algorithm->{name});
            push @positive, ["native-$category-$algorithm->{name}", $doc];
        }
    }
    for my $entry (@positive) {
        $component->($entry->[0], 'accept', $write_case->($entry->[0], $entry->[1]), $probe);
    }
    push @cases, join("\t", 'stage', 'positive-documents');

    my $fault_source = "$ROOT/tests/rewrite/helpers/probe_fault.py";
    write_bytes("$fixtures/production-probe.txt", $probe);
    for my $mode (qw(fail malformed oversized timeout closed-output schema duplicate)) {
        my $helper = "$fixtures/probe-$mode.py";
        symlink($fault_source, $helper) or die "symlink: $!\n";
        push @cases, join("\t", 'component', "helper-$mode", 0, 1, '-', 30, "bounded,reaped=$fixtures/probe-$mode.pid", $product,
                          'negative', $good, $probe, $good, $helper);
        $count += 3;
        $count += 2;
    }
    my $absent = "$fixtures/absent-helper";
    my $nonexecutable = "$fixtures/nonexecutable-helper";
    write_bytes($nonexecutable, 'not executable');
    chmod 0600, $nonexecutable;
    $component->('helper-missing', 'negative', $good, $probe, $good, $absent);
    $component->('helper-nonexecutable', 'negative', $good, $probe, $good, $nonexecutable);
    $component->('helper-relative', 'negative', $good, $probe, $good, 'relative-helper');
    for my $mode (qw(freeze bind)) {
        my $gate = "$fixtures/gate-$mode.py";
        symlink($fault_source, $gate) or die "symlink: $!\n";
        push @cases, join("\t", 'component', "V3.5_race-$mode", 0, 1, '-', 30, "reaped=$fixtures/gate-$mode.pid", $product,
                          "race-$mode", $good, $probe, $gate, "$fixtures/gate-$mode.pid", "$fixtures/gate-$mode.release");
        $count += 4;
    }
    push @cases, join("\t", 'stage', 'helpers');
    push @cases, join("\t", 'repeater', 'repeater');
    push @cases, join("\t", 'run', 'ioc-documented-startup', 0, 1, 'exit', 30, '-', $binary, "$ROOT/tests/rewrite/config/startup.cmd");

    my $prefix = qq{dbLoadDatabase("$ROOT/dbd/snmp3Ioc.dbd")\nsnmp3Ioc_registerRecordDeviceDriver(pdbbase)\nsnmp3WorkerPath("$worker")\n};
    my $load = qq{snmp3Load("$good", "$probe")\n};
    my $reload = qq{snmp3Load("$replacement_path", "$probe")\n};
    my $db = qq{dbLoadRecords("$ROOT/tests/rewrite/db/lifecycle.db", "P=Config_")\n};
    my $ioc = sub {
        my ($name, $body, $expected, $policy) = @_;
        $expected //= 0;
        $policy //= 'break';
        write_bytes("$output/$name.cmd", "on error $policy\n" . $prefix . $body);
        push @cases, join("\t", 'ioc', $name, $expected, 1, 'exit', 30, '-', $binary, "$output/$name.cmd");
        return;
    };
    $ioc->('ioc-startup', $load . "snmp3ConfigReport\n" . $db . "iocInit\n" . qq{dbpr("Config_PiniProbe", 2)\n} . "snmp3ConfigReport\n");
    $ioc->('ioc-replacement', $load . $reload . "snmp3ConfigReport\n");
    for my $entry (['build', $db . "iocBuild\n"], ['start', $db . "iocInit\n"], ['stop', $db . "iocInit\nsnmp3Stop\n"],
                   ['cold-stop', "snmp3Stop\n"]) {
        $ioc->("ioc-frozen-$entry->[0]", $load . $entry->[1] . $reload . "echo MUST_NOT_RUN\n", 1);
    }
    my $rejected_json = $write_case->('ioc-json5', replace_first($strict, '"schema"', 'schema'), 1);
    $ioc->('ioc-load-error', qq{snmp3Load("$rejected_json", "$probe")\n} . "echo MUST_NOT_RUN\n", 1);
    $ioc->('ioc-load-preservation', $load . "snmp3ConfigReport\n" . qq{snmp3Load("$rejected_json", "$probe")\n} .
           "snmp3ConfigReport\necho CONTINUED\n", 0, 'continue');
    my $invalid_unicode = $write_case->('ioc-invalid-utf8', replace_first($strict, 'fixture-user', "\xc0\xaf"), 1);
    my $unicode_load = qq{snmp3Load("$invalid_unicode", "$probe")\n};
    $ioc->('ioc-utf8-error', $load . "snmp3ConfigReport\n" . $unicode_load . "echo MUST_NOT_RUN\n", 1);
    $ioc->('ioc-utf8-preservation', $load . "snmp3ConfigReport\n" . $unicode_load . "snmp3ConfigReport\necho CONTINUED\n", 0, 'continue');
    $ioc->('ioc-missing-arguments', "snmp3Load\necho MUST_NOT_RUN\n", 1);
    $ioc->('ioc-help', "help snmp3Load\nhelp snmp3ConfigReport\nhelp snmp3Stop\nhelp snmp3Report\nhelp snmp3RuntimeReport\n");
    push @cases, join("\t", 'stage', 'ioc');

    write_bytes("$output/fixtures-pre.sha256", join('', @prehash));
    write_bytes("$output/operator-example.path", "$operator_example\n");
    write_bytes("$output/cases.tsv", join('', map { "$_\n" } @cases));
    return 0;
}

my $status = eval { main(@ARGV) };
if (!defined $status) {
    print STDERR "config_fixtures.pl: $@";
    exit 2;
}
exit $status;
