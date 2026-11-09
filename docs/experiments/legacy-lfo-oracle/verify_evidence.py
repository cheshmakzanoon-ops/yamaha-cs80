#!/usr/bin/env python3
"""Verify the frozen local evidence, not execute or rebuild its test binaries."""
from pathlib import Path
import argparse
import hashlib
import json
import re
import sys

BUNDLE = Path(__file__).resolve().parent
ROOT = BUNDLE.parents[2]
EVIDENCE = BUNDLE / 'evidence'
PROFILES = ('gcc-generic', 'gcc-native', 'clang-generic', 'clang-native')
FAULTS = {
    'coefficient': 1941582,
    'extra-rng': 647194,
    'skipped-wrap-draw': 254455,
    'phase-reset': 1582342,
    'missing-wrap': 1272275,
    'omitted-lfo-tick': 1836797,
    'revive-eligibility': 7396,
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8'))


def identity(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def check_file(path, expected):
    require(path.is_file() and not path.is_symlink(), f'Missing/nonregular file: {path}')
    actual = identity(path)
    require(all(actual[k] == expected[k] for k in actual), f'Identity mismatch: {path}')


def summary(path, failures):
    text = path.read_text()
    pattern = r'^(\d+) scenarios, (\d+) checks, (\d+) failures$'
    found = re.findall(pattern, text, re.MULTILINE)
    terminal = re.fullmatch(pattern, text.strip().splitlines()[-1]) if text.strip() else None
    require(len(found) == 1 and terminal is not None, f'Not one terminal summary: {path}')
    require(tuple(map(int, terminal.groups())) == (54, 1958109, failures),
            f'Incorrect exact result: {path}')
    return text


def passing_oracle(path, eligible):
    text = summary(path, 0)
    pattern = (r'^scenario rate=(\d+) wave=(\d+) mode=(original|wrap|scheduled) '
               r'frames=(\d+) eligible=(\d+) wraps=(\d+)/(\d+) draws=(\d+)/(\d+)$')
    rows = re.findall(pattern, text, re.MULTILINE)
    expected = {(48000, w, 'original') for w in range(6)}
    expected |= {(rate, w, mode) for rate in (44100, 48000, 96000, 192000)
                 for w in range(6) for mode in ('wrap', 'scheduled')}
    require(len(rows) == 54, f'Incomplete scenario rows: {path}')
    seen = set()
    for rate, wave, mode, frames, choice, w0, w1, d0, d1 in rows:
        rate, wave, frames, choice, w0, w1, d0, d1 = map(
            int, (rate, wave, frames, choice, w0, w1, d0, d1))
        key = (rate, wave, mode)
        require(key not in seen, f'Duplicate scenario: {path}: {key}')
        seen.add(key)
        frame_count = 512 if mode == 'original' else rate * (4 if mode == 'wrap' else 45) // (10 if mode == 'wrap' else 100)
        require(frames == frame_count and choice == eligible, f'Wrong frames/track: {path}: {key}')
        require((mode == 'original' and w0 == w1 == 0) or
                (mode != 'original' and min(w0, w1) >= 4), f'Insufficient wraps: {path}: {key}')
        draws = (w0 + 1, w1 + 1) if wave == 4 else ((2 * frames,) * 2 if wave == 5 else (0, 0))
        require((d0, d1) == draws, f'Wrong RNG count: {path}: {key}')
    require(seen == expected, f'Wrong scenario coverage: {path}')


def ctest(path, total):
    text = path.read_text()
    found = re.findall(r'^(\d+)% tests passed, (\d+) tests failed out of (\d+)$', text, re.MULTILINE)
    require(found == [('100', '0', str(total))], f'Incorrect CTest result: {path}')
    passed = re.findall(r'^\s*(\d+)/(\d+) Test\s+#\d+:\s+\S+\s+\.+\s+Passed\s+', text, re.MULTILINE)
    require(len(passed) == total and [int(n) for n, _ in passed] == list(range(1, total + 1))
            and all(int(n) == total for _, n in passed), f'Incomplete CTest rows: {path}')


def tree_identity(path):
    files = [(str(p.relative_to(path)), identity(p)) for p in sorted(path.rglob('*')) if p.is_file()]
    records = ''.join(f"{name}\t{item['bytes']}\t{item['sha256']}\n" for name, item in files)
    return {'files': len(files), 'bytes': sum(v['bytes'] for _, v in files),
            'sha256': hashlib.sha256(records.encode()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--external', action='store_true',
                        help='Also require excluded scratch binaries/trees at their recorded original locations.')
    args = parser.parse_args()
    manifest = read_json(BUNDLE / 'manifest.json')
    require(manifest['schema_version'] == 1, 'Unknown manifest schema')
    require(manifest['results'] == {
        'profile_scenarios': 54, 'profile_checks': 1958109, 'core_checks': 11508,
        'focused_ctests': 2, 'full_default_ctests': 142,
        'original_native_failures': 4, 'fault_failures': FAULTS}, 'Changed reported result contract')
    actual_paths = {str(p.relative_to(ROOT)) for p in BUNDLE.rglob('*') if p.is_file()}
    actual_paths.remove(str((BUNDLE / 'manifest.json').relative_to(ROOT)))
    actual_paths.add('docs/LEGACY_SUB_LFO_ORACLE.md')
    expected_paths = set(manifest['artifacts'])
    require(actual_paths == expected_paths, f'Unlisted/missing bundle files: {sorted(actual_paths ^ expected_paths)}')
    for name, expected in manifest['artifacts'].items():
        require(not Path(name).is_absolute() and '..' not in Path(name).parts, f'Unsafe path: {name}')
        check_file(ROOT / name, expected)

    pins = read_json(EVIDENCE / 'source-pins.json')
    require(pins['production_source_count'] == len(pins['production_files']) == 166, 'Wrong source file count')
    for group in ('production_files', 'test_and_build_files'):
        for name, expected in pins[group].items():
            check_file(ROOT / name, expected)
    actual_src = {str(p.relative_to(ROOT)) for p in (ROOT / 'src').rglob('*') if p.is_file()}
    require(actual_src == set(pins['production_files']), 'Production source inventory changed')

    runs = EVIDENCE / 'runs'
    for profile in PROFILES + ('sanitizer',):
        passing_oracle(runs / f'{profile}-oracle.log', 2 if profile.endswith('native') else 1)
        text = (runs / f'{profile}-core.log').read_text()
        require(text.strip().splitlines()[-1] == 'ALL 11508 CHECKS PASSED', f'Core summary: {profile}')
        require('FAIL:' not in text and text.count('ALL 11508 CHECKS PASSED') == 1, f'Core failure: {profile}')
        if profile != 'sanitizer':
            ctest(runs / f'{profile}-ctest.log', 2)
        build = read_json(EVIDENCE / 'builds' / f'{profile}.json')
        require(len(build['compile_commands']) == 5, f'Incomplete core/test commands: {profile}')
        for entry in build['compile_commands']:
            command = entry['command']
            require('-DCS80_DISABLE_AVX2_FILTER_BATCH=1' in command, f'AVX2 config: {profile}')
            if profile == 'sanitizer':
                require(all(flag in command for flag in ('-O1', '-g1', '-fsanitize=address,undefined,float-cast-overflow', '-D_GLIBCXX_ASSERTIONS')),
                        'Incomplete sanitizer instrumentation')
            else:
                require('-O3 -DNDEBUG' in command and ('-march=native' in command) == profile.endswith('native'),
                        f'Wrong profile flags: {profile}')
    ctest(runs / 'full-default-ctest-restarted.log', 142)
    require((runs / 'full-default-ctest-restarted.status').read_text().strip() == '0', 'Full CTest nonzero status')
    original = (runs / 'original-native-core.log').read_text()
    require(original.strip().splitlines()[-1] == '4 OF 11508 CHECKS FAILED' and original.count('FAIL:') == 4,
            'Original native negative control changed')
    expected_faults = {name: {'scenarios': 54, 'checks': 1958109, 'failures': count} for name, count in FAULTS.items()}
    for run, driver in (('mutations', 'mutation-driver.log'), ('mutations-final-restarted', 'mutation-final-restarted-driver.log')):
        require(read_json(runs / run / 'summary.json') == expected_faults, f'Fault summary: {run}')
        require(read_json(runs / driver) == expected_faults, f'Fault driver: {run}')
        for name, count in FAULTS.items():
            summary(runs / run / f'{name}.log', count)
    require((runs / 'mutation-final-restarted.status').read_text().strip() == '0', 'Final driver did not succeed')
    interrupted = (runs / 'full-default-ctest.log').read_text()
    partial_rows = re.findall(r'^\s*(\d+)/142 Test.*Passed', interrupted, re.MULTILINE)
    require('tests passed,' not in interrupted and list(map(int, partial_rows)) == list(range(1, 36)),
            'Interrupted full run scope changed')
    require(sorted(p.name for p in (runs / 'mutations-final').iterdir()) == ['coefficient-compile.command.json'],
            'Interrupted fault run scope changed')
    passing_oracle(EVIDENCE / 'review/native-focused.log', 2)
    review = read_json(EVIDENCE / 'review/mutation-review.json')
    for name in ('coefficient', 'revive-eligibility'):
        require(review['results'][name]['exit_code'] == 1 and review['results'][name]['failures'] == FAULTS[name],
                f'Independent fault review: {name}')
        summary(EVIDENCE / 'review' / f'independent-{name}.log', FAULTS[name])

    checker = read_json(EVIDENCE / 'review/checker-c95e1fa-review.json')
    cases = checker['parser_cases']
    require(cases['valid_accepted'] == len(cases['valid']) == 2 and
            cases['invalid_rejected'] == len(cases['invalid']) == 11,
            'Wrong independent parser-review counts')
    require(all(c['observed'] == 'accepted' for c in cases['valid']) and
            all(c['observed'] == 'RuntimeError rejection' for c in cases['invalid']),
            'Wrong independent parser-review outcomes')
    require(checker['driver_sha256'] == identity(BUNDLE / 'check_mutations.py')['sha256'],
            'Independent review names another checker')
    log_checks = checker['final_mutation_log_checks']
    require(log_checks['observed_driver_status'] == 0 and log_checks['summaries_equal'] is True,
            'Incomplete independent final-log review')
    for name, count in FAULTS.items():
        item = log_checks['logs'][name + '.log']
        require((item['scenarios'], item['checks'], item['failures']) == (54, 1958109, count)
                and item['accepted_by_hardened_parser'] is True and item['equals_previous_record'] is True,
                f'Wrong independent final-log review: {name}')
    comparisons = read_json(EVIDENCE / 'binary-comparisons.json')['comparisons']
    require(len(comparisons) == 2 and all(item['byte_for_byte_equal'] is True and
            item['current']['sha256'] == item['published_base']['sha256'] for item in comparisons),
            'Core binary comparison changed')

    diagnostic = (EVIDENCE / 'diagnosis/core-native.log').read_text()
    require(diagnostic.strip().splitlines()[-1] == '4 OF 11508 CHECKS FAILED' and
            diagnostic.count('FAIL:') == 4, 'Historical diagnostic failure count changed')
    first = re.findall(r'^FIRST wave=(\d) frame=(\d+) .*speedExpected=(\S+) speedActual=(\S+) ',
                       diagnostic, re.MULTILINE)
    require(first == [(str(w), '398', '0x1.89e99cp+2', '0x1.89e99ep+2') for w in range(4)],
            'Historical first-discrepancy evidence changed')
    require((EVIDENCE / 'diagnosis/core-outlined-native.log').read_text().strip().splitlines()[-1]
            == 'ALL 11508 CHECKS PASSED', 'Historical outlined-control result changed')
    for profile in ('generic', 'native'):
        rows = re.findall(r'^wave=(\d) first_fused=(-?\d+) first_split=(-?\d+)$',
                          (EVIDENCE / f'diagnosis/speed-{profile}.log').read_text(), re.MULTILINE)
        expected = [('398', '-1')] if profile == 'generic' else [('-1', '398')]
        require(rows == [(str(w), *expected[0]) for w in range(6)], 'Historical arithmetic profile changed')
    self_check = read_json(EVIDENCE / 'verifier-self-check.json')
    require(self_check['verifier_sha256'] == identity(BUNDLE / 'verify_evidence.py')['sha256'] and
            self_check['optimized_python'] is True and self_check['valid_terminal_summary_accepted'] == 1 and
            self_check['invalid_terminal_summaries_rejected'] == 8 and self_check['explicit_false_condition_rejected'] is True,
            'Wrong verifier self-check identity or counts')

    excluded = read_json(EVIDENCE / 'excluded-artifacts.json')
    files = {entry['path']: entry for entry in excluded['files']}
    for profile, sha in (('generic', 'f426f67fb902e07169a6aaada3b7383c3e7b61242be8248038ca9593295eaff2'),
                         ('native', '02ba607ec0cfb0aca83576f99adc0255e5d41c9c45ad3866bcadf4dec6c95e97')):
        matching = [v for k, v in files.items() if k.endswith(f'build-legacy-lfo-gcc-{profile}/libcs80_core.a') or
                    k.endswith(f'build-baseline-{profile}/libcs80_core.a')]
        require(len(matching) == 2 and all(v['sha256'] == sha for v in matching), 'Core library identity mismatch')
    if args.external:
        for entry in excluded['files']:
            check_file(Path(entry['path']), entry)
        for entry in excluded['trees']:
            actual = tree_identity(Path(entry['path']))
            require(all(actual[k] == entry[k] for k in actual), f"External tree mismatch: {entry['path']}")
    print(f"Verified {len(manifest['artifacts'])} bundle artifacts; 166 unchanged production source pins; "
          'four compiler profiles, complete-core sanitizers, 142/142 default CTests, '
          'original 4/11508 negative control, seven exact fault counts and interrupted-attempt scope.'
          + (' Excluded local artifacts also verified.' if args.external else ' Excluded artifacts were not opened.'))


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError, IndexError) as error:
        print(f'Evidence verification failed: {error}', file=sys.stderr)
        sys.exit(1)
