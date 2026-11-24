#!/usr/bin/env python3
"""Inspect retained original filter CI records and exact committed dependencies.

No product is built or run. This produces a supporting observation record only;
API authenticity comes from the separately documented connected acquisition.
"""
import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path, PurePosixPath
import posixpath
import re
import subprocess

REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'
WORKFLOW = '.github/workflows/filter-batch.yml'
TESTS = {'cs80_ig00156_batch_tests', 'cs80_filter_batch_engine_tests',
         'cs80_filter_batch_scheduling_tests', 'cs80_filter_batch_admission_tests'}
NAMES = {f'{compiler} / batch {mode}' for compiler in ('g++', 'clang++')
         for mode in ('DEFAULT', 'ON', 'OFF')}
STEPS = ('Configure core only', 'Build focused qualification tests', 'Exactness and scheduling gates')
COMMANDS = (
    '##[group]Run test ! -e build-filter-batch/CMakeCache.txt',
    '##[group]Run cmake --build build-filter-batch --parallel 2 --target cs80_ig00156_batch_tests cs80_filter_batch_engine_tests cs80_filter_batch_scheduling_tests',
    "##[group]Run ctest --test-dir build-filter-batch --output-on-failure -R '^cs80_(ig00156_batch_tests|filter_batch_engine_tests|filter_batch_scheduling_tests|filter_batch_admission_tests)$'",
)


def require(condition, message):
    if not condition:
        raise ValueError(message)


def file_record(path):
    raw = path.read_bytes()
    return {'path': path.name, 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()}


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def tree(repo, source):
    result = {}
    for item in git(repo, 'ls-tree', '-rz', source).split(b'\0'):
        if not item:
            continue
        metadata, name = item.split(b'\t', 1)
        mode, kind, digest = metadata.decode().split()
        result[name.decode()] = (mode, kind, digest)
    return result


def equivalent_inputs(repo, source, release):
    first, second = tree(repo, source), tree(repo, release)
    cmake = git(repo, 'show', source + ':CMakeLists.txt').decode()
    # The core-only configure also hashes provenance inputs for offline tools,
    # even though this workflow never builds those tool targets. Cover the
    # actual literal paths and all three reviewed foreach(pair) hash lists.
    configure_hash_inputs = set(re.findall(
        r'file\(SHA256 "\$\{CMAKE_CURRENT_SOURCE_DIR\}/([^"$]+)"', cmake))
    configure_hash_inputs.update(re.findall(r'"[A-Z]+\|([^"$]+)"', cmake))
    require(len(configure_hash_inputs) == 18, 'Review changed CMake configure hash inputs')
    require(cmake.count('file(SHA256 "${CMAKE_CURRENT_SOURCE_DIR}/${input}" hash)') == 3,
            'Review changed dynamic CMake configure hash loops')
    selected = {p for p in first if p.startswith('src/core/')}
    selected |= configure_hash_inputs
    selected |= {WORKFLOW, 'CMakeLists.txt', 'src/experimental/ContinuousVoiceCircuit.h',
                 'tests/Ig00156BatchTests.cpp', 'tests/FilterBatchEngineTests.cpp',
                 'tests/FilterBatchSchedulingTests.cpp', 'tests/test_filter_batch_admission.py',
                 'tests/Ig00156BatchAdmissionProbe.cpp'}
    raw_by_path = {}
    queue = sorted(selected)
    edges, unresolved = [], []
    for path in queue:
        require(path in first and first[path][1] == 'blob', 'Missing source input: ' + path)
        raw = git(repo, 'show', source + ':' + path)
        raw_by_path[path] = raw
        if not path.endswith(('.h', '.hpp', '.cpp', '.cc', '.cxx', '.inc')):
            continue
        for relative in re.findall(r'^\s*#\s*include\s*"([^"\r\n]+)"', raw.decode(), re.M):
            candidates = [posixpath.normpath(str(PurePosixPath(path).parent / relative)),
                          posixpath.normpath('src/' + relative), posixpath.normpath(relative)]
            target = next((candidate for candidate in candidates if candidate in first), None)
            if target is None:
                unresolved.append({'includer': path, 'include': relative})
                continue
            edges.append({'includer': path, 'include': relative, 'resolved': target})
            if target not in selected:
                selected.add(target)
                queue.append(target)
    require(not unresolved, 'Unresolved quoted repository includes: ' + repr(unresolved))
    rows = []
    for path in sorted(selected):
        require(path in second, 'Release omits relevant input: ' + path)
        a = raw_by_path[path]
        b = git(repo, 'show', release + ':' + path)
        same = a == b and first[path] == second[path]
        require(same, 'Relevant input differs: ' + path)
        rows.append({'path': path, 'bytes': len(a), 'sha256': hashlib.sha256(a).hexdigest(),
                     'git_blob': first[path][2], 'mode': first[path][0], 'same_bytes_and_mode': same})
    changed = git(repo, 'diff', '--name-only', source, release).decode().splitlines()
    return {'run_source': source, 'release_source': release,
            'scope': 'All src/core files, the continuous-circuit header, full CMake definition, all 18 repository files hashed during core-only CMake configure, filter workflow, four test entrypoints, the Python admission compiler probe, and their transitive quoted repository includes. No claim is made about other suites that consume the changed guides.',
            'run_source_tree': git(repo, 'rev-parse', source + '^{tree}').decode().strip(),
            'release_source_tree': git(repo, 'rev-parse', release + '^{tree}').decode().strip(),
            'input_count': len(rows), 'all_relevant_inputs_identical': True,
            'configure_hash_input_paths': sorted(configure_hash_inputs),
            'inputs': rows, 'quoted_include_edges': edges, 'unresolved_quoted_includes': unresolved,
            'complete_commit_diff_paths': changed,
            'guides_used_by_these_four_tests': False,
            'dependency_review': 'The Python admission harness compiles Ig00156Batch.cpp and Ig00156BatchAdmissionProbe.cpp. All four cs80_core translation units are covered by src/core. The exact test sources do not read packaged INSTALL/PRESET guides. CMake also hashes 18 explicitly recorded repository inputs while configuring offline targets that this workflow does not build. Standard libraries, compiler binaries and GitHub runner/action implementations remain attributes of the original execution, not a new execution at the release source.'}


def stamp(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))


def inspect_job(job, source, run_id, attempt, directory):
    require(job['run_id'] == run_id and job['run_attempt'] == attempt and job['head_sha'] == source,
            'Job source/run/attempt mismatch')
    require(job['status'] == 'completed' and job['conclusion'] == 'success', 'Original job did not pass')
    steps = []
    for name in STEPS:
        found = [s for s in job['steps'] if s['name'] == name]
        require(len(found) == 1, 'Missing or duplicate original step: ' + name)
        step = found[0]
        require(step['status'] == 'completed' and step['conclusion'] == 'success', 'Original step did not pass: ' + name)
        require(stamp(step['started_at']) <= stamp(step['completed_at']), 'Inverted step time')
        steps.append(step)
    require([s['number'] for s in steps] == sorted(s['number'] for s in steps), 'Original command ordering differs')
    path = directory / f"job-{job['id']}.log"
    raw = path.read_bytes()
    lines = re.sub(r'\x1b\[[0-9;]*[A-Za-z]', '', raw.decode('utf-8-sig')).splitlines()
    timed = []
    for line in lines:
        match = re.fullmatch(r'(\d{4}-\d\d-\d\dT\S+Z) (.*)', line)
        require(match is not None, 'Unrecognized original timestamped log line')
        timed.append((stamp(match[1]), match[2]))
    bodies = [body for _, body in timed]
    require(bodies.count(source) == 1, 'Original checkout SHA not independently observed once')
    commands = []
    for expected, step in zip(COMMANDS, steps):
        starts = [(when, body) for when, body in timed if body == expected]
        require(len(starts) == 1, 'Missing or repeated original command group')
        when, body = starts[0]
        require(stamp(step['started_at']) - timedelta(seconds=1) <= when <= stamp(step['completed_at']) + timedelta(seconds=1),
                'Command does not fall within its original API step')
        commands.append({'command_group': body, 'observed_at': when.isoformat(), 'api_step': step['name']})
    require(bodies.count('shell: /usr/bin/bash -e {0}') == 3, 'Unsuppressed original shell contract differs')
    compiler, mode = job['name'].split(' / batch ')
    require(bodies.count('  CXX: ' + compiler) == 1 and bodies.count('  BATCH_MODE: ' + mode) == 1,
            'Original matrix environment differs')
    identities = [body.removeprefix('-- The CXX compiler identification is ') for body in bodies
                  if body.startswith('-- The CXX compiler identification is ')]
    require(len(identities) == 1 and identities[0].startswith('GNU ' if compiler == 'g++' else 'Clang '),
            'Observed compiler family differs')
    configured = [body.removeprefix('CS80_ENABLE_AVX2_FILTER_BATCH:BOOL=') for body in bodies
                  if body.startswith('CS80_ENABLE_AVX2_FILTER_BATCH:BOOL=')]
    require(configured == ['OFF' if mode == 'OFF' else 'ON'], 'Observed CMake cache mode differs')
    fallback = '-- AVX2 filter batch request remains scalar: only GCC has a qualified detectable math contract'
    require(bodies.count(fallback) == (1 if compiler == 'clang++' and mode != 'OFF' else 0),
            'Observed explicit compiler fallback differs')
    case_lines = [body for body in bodies if re.match(r'\d+/\d+ Test\s+#\d+:', body)]
    require(len(case_lines) == 4, 'Actual CTest execution count differs')
    cases = []
    for body in case_lines:
        match = re.fullmatch(r'(\d+)/4 Test\s+#(\d+): (cs80_\w+)\s+\.+\s+Passed\s+([0-9.]+) sec', body)
        require(match is not None, 'CTest failed, skipped, or has unrecognized result: ' + body)
        cases.append({'ordinal': int(match[1]), 'registration': int(match[2]), 'name': match[3],
                      'status': 'passed', 'seconds': float(match[4])})
    require({c['name'] for c in cases} == TESTS and [c['ordinal'] for c in cases] == [1, 2, 3, 4],
            'Actual CTest roster/order does not agree')
    require(bodies.count('100% tests passed, 0 tests failed out of 4') == 1, 'Missing or repeated clean CTest terminal summary')
    require(not any(body.startswith('##[error]') for body in bodies), 'Original job log reports an error')
    return {'job_id': job['id'], 'name': job['name'], 'source': source, 'attempt': attempt,
            'status': job['status'], 'conclusion': job['conclusion'], 'html_url': job['html_url'],
            'started_at': job['started_at'], 'completed_at': job['completed_at'],
            'compiler_identification': identities[0], 'requested_mode': mode, 'cache_mode': configured[0],
            'source_declared_backend': 'GCC exact-math runtime dispatch; CPU-dependent AVX2 or scalar' if compiler == 'g++' and mode != 'OFF' else 'scalar',
            'backend_scope': 'The backend label describes cs80_core and its three linked C++ tests. The fourth CTest independently compiles admission probes and does not inherit the CMake OFF macro; its strict/unsafe/mixed compiler probes are a separate scope.',
            'observed_clang_fallback_message': bodies.count(fallback) == 1,
            'actual_accelerated_group_count': None,
            'nested_admission_profile_skips': None,
            'suppressed_stdout_scope': 'ctest --output-on-failure hides successful test stdout. Internal accelerated-group totals and admission-profile skips are unknown, not inferred as zero.',
            'gate_steps': steps, 'original_command_groups': commands,
            'ctest_cases': cases, 'ctest_counts': {'registered_in_filter_selection': 4, 'passed': 4, 'failed': 0, 'formal_skipped': 0},
            'original_exit_basis': 'Three original successful API steps and their timestamped direct commands under bash -e. The final direct CTest command is unsuppressed and its four real cases pass. No workflow continue-on-error or retry exists.',
            'log': file_record(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--release-source', required=True)
    parser.add_argument('--run-id', type=int, required=True)
    parser.add_argument('--attempt', type=int, required=True)
    parser.add_argument('--run-api', type=Path)
    parser.add_argument('--jobs-api', type=Path)
    parser.add_argument('--logs', type=Path)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--equivalence-only', action='store_true')
    args = parser.parse_args()
    require(all(re.fullmatch('[0-9a-f]{40}', x) for x in (args.source, args.release_source)), 'Full committed sources required')
    equivalence = equivalent_inputs(args.repo, args.source, args.release_source)
    if args.equivalence_only:
        report = equivalence
    else:
        require(all((args.run_api, args.jobs_api, args.logs)), 'Original run/jobs/logs inputs required')
        run = json.loads(args.run_api.read_text())
        jobs_api = json.loads(args.jobs_api.read_text())
        jobs = jobs_api['jobs']
        require(run['id'] == args.run_id and run['head_sha'] == args.source and run['run_attempt'] == args.attempt,
                'Original run identity differs')
        require(run['repository']['full_name'] == REPOSITORY and run['head_repository']['full_name'] == REPOSITORY,
                'Repository identity differs')
        require(run['path'] == WORKFLOW and run['event'] == 'push' and run['head_branch'] == 'main', 'Workflow/source event differs')
        require(run['status'] == 'completed' and run['conclusion'] == 'success', 'Original run did not pass')
        require(jobs_api['total_count'] == 6 and len(jobs) == 6 and {j['name'] for j in jobs} == NAMES,
                'Incomplete or duplicate matrix')
        workflow = git(args.repo, 'show', args.source + ':' + WORKFLOW).decode()
        require('continue-on-error' not in workflow and 'retry' not in workflow, 'Review changed source workflow exit/retry policy')
        rows = [inspect_job(j, args.source, args.run_id, args.attempt, args.logs) for j in jobs]
        report = {'schema': 1, 'reviewer': 'a47_filter_ci_qualification', 'release_qualified': False,
                  'supporting_filter_gates_passed': True, 'run_id': args.run_id, 'run_attempt': args.attempt,
                  'run_url': run['html_url'], 'run_source': args.source, 'release_source': args.release_source,
                  'workflow': WORKFLOW, 'run_conclusion': run['conclusion'],
                  'verified_at_utc': datetime.now(timezone.utc).isoformat(),
                  'run_api': file_record(args.run_api), 'jobs_api': file_record(args.jobs_api),
                  'equivalence': equivalence, 'jobs': rows,
                  'totals': {'original_jobs_passed': 6, 'ctest_executions': 24, 'ctest_passed': 24,
                             'ctest_failed': 0, 'formal_ctest_skipped': 0, 'unique_test_definitions': 4,
                             'internal_admission_profile_skips': None, 'actual_accelerated_group_count': None},
                  'scope': ['Original run-source executions are not relabeled as release-source executions.',
                            'Clang ON and DEFAULT explicitly select scalar fallback; OFF is scalar on either compiler.',
                            'Scalar/fallback labels apply to the configured core and three C++ tests, not to every independent probe compiled by the Python admission CTest.',
                            'GCC ON and DEFAULT admit a runtime-selected exact backend. Successful stdout does not expose an actual accelerated-group count.',
                            'These four core/Python gates establish neither a native plugin build, artifact payload identity, DAW compatibility nor acoustic equivalence.',
                            'This inspector reads supplied retained API text; authenticated acquisition provenance is documented separately.']}
    with args.out.open('x') as stream:
        json.dump(report, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'out': str(args.out), 'relevant_inputs': equivalence['input_count'],
                      'supporting_filter_gates_passed': report.get('supporting_filter_gates_passed', None)}))


if __name__ == '__main__':
    main()
