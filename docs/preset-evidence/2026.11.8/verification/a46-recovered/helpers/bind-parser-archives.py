#!/usr/bin/env python3
"""Bind the pinned 2026.11.6 native-parser observations to actual ZIP/API bytes.

This is an input-integrity gate only. It does not accept native results, bind
an original workflow attempt, prove supplied API authenticity, or qualify a
platform/release. No downloaded executable is loaded or run.
"""
from __future__ import annotations
import argparse
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import zipfile

REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
HISTORICAL_PARSER_PATH = 'docs/preset-evidence/2026.11.4/ci/source-078c098/parse-native-evidence.py'
PARSER_PATH = 'docs/preset-evidence/2026.11.6/verification/a52/parse-native-evidence.py'
HISTORICAL_PARSER_SHA256 = 'f44d978bcb6d0754165c8d3d55320ac63919f22b15bcb37a54506ec00db3cc58'
PARSER_SHA256 = 'f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846'
PARSER_BYTES = 18241
ARTIFACTS = {
    'linux-x86_64': ('cs80-linux-test-evidence', 'cs80-pluginval-reports'),
    'windows-x86_64': ('cs80-windows-test-evidence', 'cs80-pluginval-windows'),
    'macos-arm64': ('cs80-macos-15-test-evidence', 'cs80-pluginval-macos-15'),
    'macos-x86_64': ('cs80-macos-15-intel-test-evidence', 'cs80-pluginval-macos-15-intel'),
}
MAX_ZIP = 256 * 1024 ** 2
MAX_MEMBER = 64 * 1024 ** 2


def require(ok, reason):
    if not ok:
        raise ValueError(reason)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def snapshot(path, limit):
    flags = os.O_RDONLY | getattr(os, 'O_NOFOLLOW', 0) | getattr(os, 'O_NONBLOCK', 0)
    fd = os.open(path, flags)
    with os.fdopen(fd, 'rb') as stream:
        require(stat.S_ISREG(os.fstat(stream.fileno()).st_mode), 'Nonregular input: ' + str(path))
        data = stream.read(limit + 1)
    require(0 < len(data) <= limit, 'Input outside size bound: ' + str(path))
    return data


def parse(data):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            require(key not in result, 'Duplicate JSON key: ' + key)
            result[key] = value
        return result
    def reject(value):
        raise ValueError('Nonfinite JSON constant: ' + value)
    return json.loads(data, object_pairs_hook=unique, parse_constant=reject)


def record(data, path):
    return {'path': str(path), 'bytes': len(data), 'sha256': digest(data)}


def match_record(expected, raw, label):
    require(isinstance(expected, dict), label + ': missing record')
    require(type(expected.get('bytes')) is int and expected['bytes'] == len(raw), label + ': byte count differs')
    require(expected.get('sha256') == digest(raw), label + ': SHA-256 differs')


def select(rows, target, label):
    require(isinstance(rows, list), label + ': expected observation list')
    matches = [row for row in rows if isinstance(row, dict) and row.get('target') == target]
    require(len(matches) == 1, label + ': missing or duplicate target')
    return matches[0]


def check_receipt(api, raw, name, source, run_id, repository):
    require(type(api.get('id')) is int and api['id'] > 0, 'Invalid artifact ID')
    route = 'https://api.github.com/repos/' + repository + '/actions/artifacts/' + str(api['id'])
    require(api.get('name') == name and api.get('url') == route and api.get('archive_download_url') == route + '/zip', 'Artifact name/route differs')
    require(type(api.get('size_in_bytes')) is int and api['size_in_bytes'] == len(raw), 'API ZIP byte count differs')
    require(api.get('digest') == 'sha256:' + digest(raw), 'API ZIP SHA-256 differs')
    workflow = api.get('workflow_run', {})
    require(workflow.get('id') == run_id and workflow.get('head_sha') == source, 'API source/run differs')
    return {'artifact_id': api['id'], 'name': name, 'bytes': len(raw), 'sha256': digest(raw)}


def member_bindings(raw, observation, kind):
    records = []
    if kind == 'native':
        require(set(observation.get('inputs', {})) == {'junit', 'lasttest'}, 'Native member input records absent')
        records.extend((name, item) for name, item in observation['inputs'].items())
        require(isinstance(observation.get('juce_provenance'), dict), 'Required JUCE provenance record absent')
        records.append(('juce_provenance', observation['juce_provenance']))
    else:
        version = observation.get('version', {})
        records.append(('validator_version', version.get('record')))
        require(isinstance(observation.get('lanes'), list) and observation['lanes'], 'Validator lane records absent')
        for lane in observation['lanes']:
            for name in ('console', 'report'):
                records.append((str(lane.get('lane')) + '/' + name, lane.get(name)))
            if 'core_log' in lane:
                records.append((str(lane.get('lane')) + '/core_log', lane['core_log']))
            for attempt in lane.get('preserved_failed_attempts', []):
                require(attempt.get('member') == attempt.get('path'), 'Preserved attempt member/path differs')
                records.append((str(lane.get('lane')) + '/prior_attempt', attempt))
    result = []
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        infos = archive.infolist()
        names = [item.filename for item in infos]
        require(len(names) == len(set(names)), 'Duplicate ZIP members')
        require(sum(item.file_size for item in infos) <= MAX_ZIP, 'Expanded ZIP exceeds bound')
        for label, expected in records:
            require(isinstance(expected, dict) and isinstance(expected.get('path'), str), label + ': member record absent')
            name = expected['path']
            require(name and not name.startswith('/') and '\\' not in name and ':' not in name
                    and all(part not in ('', '.', '..') for part in name.split('/'))
                    and PurePosixPath(name).as_posix() == name, label + ': unsafe member path')
            require(name in names, label + ': exact member absent from actual ZIP')
            info = archive.getinfo(name)
            require(not info.is_dir() and 0 <= info.file_size <= MAX_MEMBER and not info.flag_bits & 1, label + ': invalid ZIP member')
            data = archive.read(info)
            match_record(expected, data, label)
            result.append({'role': label, **record(data, name)})
    return result


def bind(observations, target, native_raw, native_api, validator_raw, validator_api, source, run_id, repository=REPOSITORY):
    require(target in ARTIFACTS, 'Unsupported target')
    require(re.fullmatch('[0-9a-f]{40}', source) is not None and type(run_id) is int and run_id > 0, 'Expected complete source/run context')
    require(observations.get('release_qualified') is False, 'Raw parser release scope changed')
    require(observations.get('parser', {}).get('sha256') == PARSER_SHA256
            and observations.get('parser', {}).get('bytes') == PARSER_BYTES, 'Observation parser identity differs')
    require(0 < len(native_raw) <= MAX_ZIP and 0 < len(validator_raw) <= MAX_ZIP, 'ZIP outside size bound')
    selected = [select(observations.get('native'), target, 'Native'), select(observations.get('validators'), target, 'Validator')]
    output = []
    for kind, observed, raw, api, name in zip(('native', 'validator'), selected, (native_raw, validator_raw), (native_api, validator_api), ARTIFACTS[target]):
        actual = check_receipt(api, raw, name, source, run_id, repository)
        match_record(observed.get('archive'), raw, kind + ' parser archive')
        members = member_bindings(raw, observed, kind)
        output.append({'kind': kind, 'parser_archive_record': observed['archive'], 'actual_zip_api_binding': actual, 'parsed_member_records_bound': members})
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--parser', type=Path, default=Path(__file__).with_name('parse-native-evidence.py'))
    parser.add_argument('--run-id', type=int, required=True)
    parser.add_argument('--attempt', type=int, required=True)
    parser.add_argument('--target', choices=ARTIFACTS, required=True)
    for name in ('observations', 'native-zip', 'native-api', 'validator-zip', 'validator-api', 'out'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    require(args.attempt > 0, 'Expected positive attempt context')
    inputs = {name: snapshot(getattr(args, name), MAX_ZIP if name.endswith('_zip') else 32 * 1024 ** 2)
              for name in ('observations', 'native_zip', 'native_api', 'validator_zip', 'validator_api')}
    source_parser = subprocess.check_output(['git', '-C', str(args.repo), 'show', args.source + ':' + HISTORICAL_PARSER_PATH])
    require(digest(source_parser) == HISTORICAL_PARSER_SHA256, 'Historical parser identity changed')
    current_parser = snapshot(args.parser, MAX_MEMBER)
    require(digest(current_parser) == PARSER_SHA256 and len(current_parser) == PARSER_BYTES, 'Current parser identity changed')
    bindings = bind(parse(inputs['observations']), args.target, inputs['native_zip'], parse(inputs['native_api']), inputs['validator_zip'], parse(inputs['validator_api']), args.source, args.run_id)
    result = {'schema': 1, 'status': 'parser_archive_and_member_input_binding_verified',
              'source': args.source, 'run_id': args.run_id, 'attempt_context': args.attempt, 'target': args.target,
              'parser_archive_binding_verified': True, 'native_results_accepted': False,
              'original_attempt_binding_verified': False, 'platform_qualified': False, 'release_qualified': False,
              'parser_source_path': PARSER_PATH, 'parser_sha256': PARSER_SHA256,
              'historical_source_parser_path': HISTORICAL_PARSER_PATH, 'historical_source_parser_sha256': HISTORICAL_PARSER_SHA256,
              'checker_sha256': digest(Path(__file__).read_bytes()), 'bindings': bindings,
              'inputs': {name: record(raw, getattr(args, name)) for name, raw in inputs.items()},
              'limits': ['Supplied API authenticity and exact producing attempt require the original external acquisition/job/upload gates.',
                         'This checks actual input identities, including parsed member bytes; it does not accept test counts, skips, lane results, exits, retries or payload equivalence.',
                         'Retain the raw parser release_qualified:false field unchanged.']}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open('x') as stream:
        json.dump(result, stream, indent=2, allow_nan=False)
        stream.write('\n')
    print(json.dumps({'status': result['status'], 'target': args.target, 'archives_bound': len(bindings), 'release_qualified': False}))


if __name__ == '__main__':
    main()
