#!/usr/bin/env python3
"""Corrected version-3 production-PIC study. Original terminal study remains closed."""
import argparse
import json
import os
import platform
import time
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET

from artifact_seal import seal_bundle, verify_bundle, restore_bundle
from execution import Commands
import host_contract as host
from linkage import verify_linkage
from production import Production, STRICT, CORE_TESTS, TARGETS, ROOT, FROZEN_INPUTS, save
from provenance import require, command_entries, frozen_files, verify_frozen, sha256
from profile_identity_v3 import make_contract, complete_training_attempt, live_profile_manifest, capture_unvalidated, validate_evidence


def core_driver_object(production):
    entries = command_entries(production.baseline_commands)
    matches = [path for path, row in entries.items()
               if row['source'] == str(production.repo / 'src/tools/source_factory_benchmark.cpp')]
    require(len(matches) == 1 and Path(matches[0]).is_file(), 'one built normal benchmark object required')
    return Path(matches[0])


def copy_file(source, destination):
    source, destination = Path(source), Path(destination)
    require(source.is_file() and not source.is_symlink() and not destination.exists(), 'fresh regular artifact copy required')
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    require(sha256(source) == sha256(destination), 'artifact copy mismatch')
    return destination


def seal_arm(prod, arm, qualification):
    root = prod.out / 'arms' / arm; root.mkdir(parents=True, exist_ok=False)
    files = {'engine': prod.engine, 'archive': prod.archive, 'benchmark': prod.benchmark,
             'standalone': prod.standalone, 'qualification': qualification,
             **{name: prod.build / name for name, _ in CORE_TESTS},
             **{name + '-map': path for name, path in prod.maps.items()}}
    sealed = {name: copy_file(path, root / name) for name, path in files.items()}
    bundle = root / 'CS-80 Reference.vst3'
    bundle_manifest = seal_bundle(prod.bundle, bundle)
    sealed['bundle'] = bundle
    manifest = frozen_files([path for name, path in sealed.items() if name != 'bundle'])
    save(root / 'file-manifest.json', manifest); save(root / 'bundle-manifest.json', bundle_manifest)
    return {'root': root, 'paths': sealed, 'files': manifest, 'bundle': bundle_manifest}


def bind_canonical_to_sealed(mapping):
    for canonical, sealed in mapping.items():
        require(Path(canonical).is_file() and Path(sealed).is_file(), 'canonical/sealed file missing')
        require(sha256(canonical) == sha256(sealed), 'canonical bytes differ from qualified sealed candidate')
    return frozen_files(mapping)


def canonical_candidate_mapping(prod, candidate):
    return {prod.engine: candidate['paths']['engine'], prod.archive: candidate['paths']['archive'],
            prod.benchmark: candidate['paths']['benchmark'], prod.standalone: candidate['paths']['standalone'],
            **{prod.build / target: candidate['paths'][target] for target, _ in CORE_TESTS},
            **{path: candidate['paths'][name + '-map'] for name, path in prod.maps.items()}}


def verify_arm(arm):
    verify_frozen(arm['files']); verify_bundle(arm['paths']['bundle'], arm['bundle'])


def inspect_arm(prod, name, arm):
    verify_arm(arm)
    p = arm['paths']; module = p['bundle'] / 'Contents/x86_64-linux/CS-80 Reference.so'
    members = prod.output(name + '-archive-members', ['ar', 't', p['archive']]).splitlines()
    member_file, _ = prod.run(name + '-archive-member-bytes', ['ar', 'p', p['archive'], 'Engine.cpp.o'])
    object_symbols = prod.output(name + '-engine-symbols', ['objdump', '-t', p['engine']])
    final_symbols = prod.output(name + '-vst3-symbols', ['objdump', '-t', module])
    report = verify_linkage(engine_bytes=p['engine'].read_bytes(), archive_member_bytes=member_file.read_bytes(),
        archive_members=members, archive_path=str(prod.archive), object_symbols=object_symbols,
        final_symbols=final_symbols, map_text=p['CS80Reference_VST3-map'].read_text(),
        cwd=str(prod.build), final_bytes=module.read_bytes())
    save(prod.out / (name + '-vst3-linkage.json'), report)
    standalone_symbols = prod.output(name + '-standalone-link-symbols', ['objdump', '-t', p['standalone']])
    standalone_report = verify_linkage(engine_bytes=p['engine'].read_bytes(), archive_member_bytes=member_file.read_bytes(),
        archive_members=members, archive_path=str(prod.archive), object_symbols=object_symbols,
        final_symbols=standalone_symbols, map_text=p['CS80Reference_Standalone-map'].read_text(),
        cwd=str(prod.build), final_bytes=p['standalone'].read_bytes())
    save(prod.out / (name + '-standalone-linkage.json'), standalone_report)
    for item in ('benchmark', 'qualification', 'standalone'):
        symbols = prod.output(name + '-' + item + '-symbols', ['nm', p[item]])
        require('__gcov' not in symbols and '__llvm_profile' not in symbols, 'assessment binary has profile runtime')
    exports = prod.output(name + '-vst3-exports', ['nm', '-D', '--defined-only', module])
    names = {line.split()[-1] for line in exports.splitlines() if line.split()}
    require({'GetPluginFactory', 'ModuleEntry', 'ModuleExit'} <= names, 'required VST3 exports missing')
    prod.output(name + '-vst3-elf', ['readelf', '-dWs', module])


def clean_capture_outputs(contract, build):
    for row in contract.values():
        directory = Path(row['output'])
        require(directory.parent == Path(build) and not directory.is_symlink(), 'owned direct build capture required')
        if directory.exists():
            host.capture_manifest(directory, row['kind'])
            shutil.rmtree(directory)
        require(not directory.exists(), 'capture output not fresh')


def validate_host_log(text, kind):
    expected = '12 native SUB scenarios, 0 failures' if kind == 'sub' else '12 native ring scenarios, 0 failures'
    require(kind in ('sub', 'ring'), 'unknown fixture footer')
    require(text.splitlines().count(expected) == 1 and 'FAIL' not in text and 'ERROR' not in text,
            'original full native host assertions did not finish cleanly')


def validate_pluginval(text):
    require('SUCCESS' in text.splitlines() and 'FAILED' not in text,
            'pluginval clean explicit SUCCESS missing or FAILED present')


def run_host_arm(prod, name, arm, contract, host_drivers):
    verify_arm(arm); verify_frozen(host_drivers)
    restore_bundle(arm['paths']['bundle'], prod.bundle, arm['bundle'], build_root=prod.build)
    clean_capture_outputs(contract, prod.build)
    root = prod.out / ('host-' + name); root.mkdir()
    for test_name, row in contract.items():
        verify_bundle(prod.bundle, arm['bundle']); verify_frozen(host_drivers)
        require(not Path(row['output']).exists(), 'selected output must begin absent')
        stdout, stderr = prod.commands.run(name + '-' + test_name, row['command'], cwd=prod.build, timeout=row['timeout'], pin=True)
        validate_host_log(stdout.read_text() + '\n' + stderr.read_text(), row['kind'])
        verify_bundle(prod.bundle, arm['bundle']); verify_frozen(host_drivers)
        manifest = host.capture_manifest(row['output'], row['kind'])
        destination = root / test_name
        shutil.copytree(row['output'], destination, copy_function=shutil.copy2)
        require(host.capture_manifest(destination, row['kind']) == manifest, 'preserved host captures changed')
    clean_capture_outputs(contract, prod.build)
    return root


def validate_junit(path, names):
    root = ET.parse(path).getroot(); cases = root.findall('.//testcase')
    require(len(cases) == 569 and {case.attrib['name'] for case in cases} == set(names), 'full JUnit inventory mismatch')
    require(all(case.find('failure') is None and case.find('error') is None and case.find('skipped') is None
                for case in cases), 'full release suite incomplete/failed/skipped')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--work', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--pluginval', type=Path, required=True)
    args = parser.parse_args()
    repo, work, out, pluginval = (path.resolve() for path in (args.repo, args.work, args.out, args.pluginval))
    work.mkdir(parents=True, exist_ok=False); out.mkdir(parents=True, exist_ok=False)
    commands = Commands(out / 'commands', total_seconds=18000, progress_seconds=5400)
    prod = Production(repo, work, out, commands)
    decision = {'decision': 'STOP_NO_RETRY', 'no_retry': True}
    profile_contract = None
    try:
        require(pluginval.is_file() and os.access(pluginval, os.X_OK), 'prepared official pluginval executable required')
        version = prod.output('pluginval-version', [pluginval, '--version'])
        require(version.strip() == 'pluginval - 1.0.4', 'pluginval version mismatch')
        pluginval_identity = frozen_files([pluginval])
        save(out / 'pluginval-identity.json', {'files': pluginval_identity, 'version': version})
        prod.verify_source('initial')
        commit = prod.output('source-commit', ['git', 'rev-parse', 'HEAD']).strip()
        research_paths = [path for directory in (ROOT, ROOT.parent / 'strict-pgo-research')
                          for path in directory.iterdir() if path.is_file()]
        research_paths += [repo / '.github/workflows' / name for name in
                          ('production-pic-pgo-study-v3.yml', 'production-pic-pgo-study-v2.yml', 'production-pic-pgo-study.yml', 'production-pic-pgo-preflight.yml', 'strict-pgo-research.yml')]
        research_manifest = frozen_files(research_paths)
        save(out / 'metadata.json', {'commit': commit, 'frozen_inputs': FROZEN_INPUTS,
            'platform': platform.platform(), 'machine': platform.machine(),
            'cpuinfo': Path('/proc/cpuinfo').read_text(),
            'initial_allowed_cpus': sorted(os.sched_getaffinity(0)), 'selected_cpu': commands.cpu,
            'runner_name': os.environ.get('RUNNER_NAME'), 'image': os.environ.get('ImageVersion'),
            'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
            'research_files': {str(Path(path).relative_to(repo)): value for path, value in research_manifest.items()}})
        prod.configure('baseline')
        prod.build_targets('build-baseline-selected', TARGETS)
        selected_inventory = json.loads(prod.output('selected-baseline-inventory', ['ctest', '--test-dir', prod.build, '--show-only=json-v1']))
        contract = host.validate_selected(selected_inventory, repo, prod.build)
        host_drivers = frozen_files([prod.build / 'cs80_vst3_sub_automation_tests', prod.build / 'cs80_vst3_ring_automation_tests'])
        save(out / 'common-host-driver-manifest.json', host_drivers)
        for path in host_drivers: copy_file(path, out / 'common-host-driver-bytes' / Path(path).name)
        qualifier_object = prod.build_qualification_driver()
        baseline_qualifier = work / 'baseline-qualification'
        prod.link_extra('link-baseline-qualification', qualifier_object, baseline_qualifier)
        baseline = seal_arm(prod, 'baseline', baseline_qualifier)
        prod.snapshot_ordinary()
        prod.ordinary.update(frozen_files([qualifier_object]))
        save(out / 'ordinary-objects.json', prod.ordinary)
        common = out / 'shared-object-bytes'; common.mkdir()
        for object_path in prod.ordinary:
            path = Path(object_path)
            selected = (path == qualifier_object or path == core_driver_object(prod)
                        or '/CMakeFiles/cs80_core.dir/' in str(path)
                        or any('/CMakeFiles/' + target + '.dir/' in str(path) for target, _ in CORE_TESTS))
            if selected:
                relative = path.relative_to(prod.build) if path.is_relative_to(prod.build) else Path(path.name)
                copy_file(path, common / relative)
        # No held-out test/audio observation has occurred. Generation is core-only.
        prod.configure('generate'); prod.unchanged_ordinary()
        require(not list(prod.profiles.iterdir()), 'fresh training profiles required')
        prod.remove_owned_outputs([prod.engine, prod.archive])
        prod.build_targets('build-instrumented-core-only', ['cs80_core'])
        prod.unchanged_ordinary()
        training = work / 'training'
        prod.link_extra('link-training-only', core_driver_object(prod), training, training=True)
        save(out / 'training-binary.json', frozen_files([training, prod.engine, prod.archive]))
        training_bytes = out / 'training-artifact-bytes'; training_bytes.mkdir()
        for source in (training, prod.engine, prod.archive): copy_file(source, training_bytes / source.name)
        require(not list(prod.profiles.iterdir()), 'unexpected pretraining profile')
        generation_rows = command_entries(json.loads((out / 'generate-compile_commands.json').read_text()))
        generation_row = generation_rows[str(prod.engine)]
        profile_contract = make_contract(generation_row['argv'], generation_row['directory'],
            generation_row['source'], str(prod.engine), str(prod.profiles),
            (training_bytes / 'Engine.cpp.o').read_bytes(), sha256(prod.engine))
        save(out / 'profile-contract.json', profile_contract)
        profile = complete_training_attempt(prod.profiles, out / 'profile-evidence', profile_contract,
            lambda: prod.run('training-once', STRICT.benchmark_args(training, out / 'training', commit, STRICT.TRAIN),
                             timeout=300, pin=True),
            lambda: STRICT.read_rows(out / 'training', STRICT.TRAIN))
        save(out / 'profile-manifest.json', profile)
        profile_evidence_files = frozen_files([out / 'profile-contract.json', out / 'profile-manifest.json',
            out / 'profile-evidence/inventory.json',
            *(out / 'profile-evidence/raw' / name for name in profile)])
        save(out / 'profile-evidence-file-manifest.json', profile_evidence_files)
        require(live_profile_manifest(prod.profiles, profile_contract) == profile, 'live profile differs from retained evidence')
        # Force fresh actual candidate output/map; preserve normal CMake post-link steps.
        prod.configure('use'); prod.unchanged_ordinary()
        require(live_profile_manifest(prod.profiles, profile_contract) == profile, 'profile changed before use build')
        prod.remove_owned_outputs([prod.engine, prod.archive, prod.module, prod.standalone, *prod.maps.values()])
        prod.build_targets('build-candidate-selected-fresh-link', TARGETS)
        prod.unchanged_ordinary()
        candidate_qualifier = work / 'candidate-qualification'
        prod.link_extra('link-candidate-qualification', qualifier_object, candidate_qualifier)
        candidate = seal_arm(prod, 'candidate', candidate_qualifier)
        verify_frozen(host_drivers)
        canonical = bind_canonical_to_sealed(canonical_candidate_mapping(prod, candidate))
        save(out / 'canonical-candidate-file-manifest.json', canonical)
        require(live_profile_manifest(prod.profiles, profile_contract) == profile, 'profile changed after use build')
        for name, arm in [('baseline', baseline), ('candidate', candidate)]: inspect_arm(prod, name, arm)
        selected_candidate = json.loads(prod.output('selected-candidate-inventory', ['ctest', '--test-dir', prod.build, '--show-only=json-v1']))
        require(host.validate_selected(selected_candidate, repo, prod.build) == contract, 'host command drift between built arms')
        save(out / 'selected-host-contract.json', contract)
        # Both arms and profile are now sealed. Only now begin held-out qualification.
        def frozen():
            verify_arm(baseline); verify_arm(candidate); prod.unchanged_ordinary()
            verify_frozen(canonical); verify_frozen(host_drivers); verify_frozen(research_manifest); verify_frozen(pluginval_identity)
            verify_frozen(profile_evidence_files)
            require(live_profile_manifest(prod.profiles, profile_contract) == profile, 'frozen profile changed')
            host.verify_definitions(prod.test_definitions, prod.build)
        qualification = {}
        for spec in (STRICT.QUAL_BASE, STRICT.QUAL):
            paths = {}
            for name, arm in [('baseline', baseline), ('candidate', candidate)]:
                frozen(); path = out / ('qualification-' + spec['suite'] + '-' + name); paths[name] = path
                prod.run(path.name, STRICT.benchmark_args(arm['paths']['qualification'], path, commit, spec), timeout=300, pin=True)
            STRICT.exact(STRICT.read_rows(paths['baseline'], spec), STRICT.read_rows(paths['candidate'], spec))
            qualification[spec['suite']] = STRICT.compare_transcripts(paths['baseline'], paths['candidate'])
        save(out / 'qualification.json', qualification)
        for target, timeout in CORE_TESTS:
            logs = []
            for name, arm in [('baseline', baseline), ('candidate', candidate)]:
                frozen(); argv = [arm['paths'][target]]
                if target == 'cs80_filter_batch_engine_tests': argv += ['--write-corpus', out / ('filter-corpus-' + name + '.bin')]
                stdout, stderr = prod.commands.run(target + '-' + name, argv, cwd=prod.build, timeout=timeout, pin=True)
                logs.append((stdout.read_bytes(), stderr.read_bytes()))
            require(logs[0] == logs[1], 'core regression outputs differ: ' + target)
        a, b = out / 'filter-corpus-baseline.bin', out / 'filter-corpus-candidate.bin'
        require(a.stat().st_size > 0 and a.stat().st_size == b.stat().st_size and sha256(a) == sha256(b), 'full core state corpus differs')
        save(out / 'filter-corpus.json', {'bytes': a.stat().st_size, 'sha256': sha256(a)})
        host_a = run_host_arm(prod, 'baseline', baseline, contract, host_drivers)
        host_b = run_host_arm(prod, 'candidate', candidate, contract, host_drivers)
        host_results = {name: host.compare_captures(host_a / name, host_b / name, row['kind']) for name, row in contract.items()}
        save(out / 'host-capture-comparison.json', host_results)
        frozen(); verify_bundle(prod.bundle, candidate['bundle'])
        def candidate_unchanged():
            frozen(); verify_frozen(canonical); verify_bundle(prod.bundle, candidate['bundle'])
        reference = None
        def cost_run(label, name):
            nonlocal reference
            candidate_unchanged(); arm = baseline if name == 'baseline' else candidate
            path = out / label
            prod.run(label, STRICT.benchmark_args(arm['paths']['benchmark'], path, commit, STRICT.COST, True), timeout=300, pin=True)
            rows = STRICT.read_rows(path, STRICT.COST, True)
            if reference is None: reference = rows
            else: STRICT.exact(reference, rows)
            return rows
        cost = STRICT.cost_phase(cost_run); save(out / 'compatibility-cost.json', cost)
        candidate_unchanged(); commands.remaining()
        if cost['decision'] != 'SCOPED_COST_PASS':
            decision = {**cost, 'release_qualified': False, 'stage': 'compatibility-cost'}
        else:
            commands.release_stage()
            prod.build_targets('build-all-normal-targets', ['all'])
            candidate_unchanged()
            full = json.loads(prod.output('full-built-test-inventory', ['ctest', '--test-dir', prod.build, '--show-only=json-v1']))
            names = host.validate_full_inventory(prod.config_inventory, full)
            host.validate_full_inventory(selected_candidate, full)
            host.verify_definitions(prod.test_definitions, prod.build)
            save(out / 'full-built-test-inventory.json', full)
            junit = out / 'full-ctest.xml'
            prod.run('full-normal-ctest', ['ctest', '--test-dir', prod.build, '--parallel', '1', '--output-on-failure', '--output-junit', junit])
            validate_junit(junit, names); candidate_unchanged()
            prod.run('normal-render-demo', [prod.build / 'cs80_render', '--out', out / 'cs80-demo.wav', '--seconds', '12'])
            prod.run('normal-ui-smoke', ['bash', repo / 'scripts/run-ui-smoke-linux.sh',
                prod.build / 'CS80Snapshots_artefacts/Release/CS80Snapshots', '--out', out / 'ui', '--self-test'],
                env={'JUCE_FONT_PATH': '/usr/share/fonts/truetype/dejavu'})
            candidate_unchanged()
            reports = out / 'pluginval-reports'; reports.mkdir()
            for kind in ('headless', 'gui', 'gui-seed'):
                argv = [pluginval, '--strictness-level', '10', '--validate-in-process', '--timeout-ms', '120000',
                        '--output-dir', reports, '--output-filename', kind]
                if kind == 'headless': argv += ['--skip-gui-tests']
                if kind == 'gui-seed': argv += ['--random-seed', '0x2da72db']
                argv += [prod.bundle]
                if kind != 'headless': argv = ['xvfb-run', '-a', '-s', '-screen 0 1920x1200x24', *argv]
                candidate_unchanged()
                stdout, stderr = prod.run('pluginval-' + kind, argv, timeout=240 if kind == 'gui-seed' else 600)
                validate_pluginval(stdout.read_text() + '\n' + stderr.read_text())
                candidate_unchanged()
            prod.run('normal-package-selftest', ['python3', repo / 'scripts/package-ci-artifacts.py', '--self-test'])
            prod.run('normal-package-linux', ['python3', repo / 'scripts/package-ci-artifacts.py', '--platform', 'linux',
                                            '--build-dir', prod.build, '--out-dir', out / 'packages'])
            candidate_unchanged(); prod.verify_source('final'); commands.remaining()
            decision = {'decision': 'PRODUCTION_PIC_QUALIFIED', 'release_qualified': True,
                        'cost': cost, 'full_tests': 569, 'no_retry': True,
                        'scope': 'Linux GNU13.3 production-PIC core CPU and exact experimental plugin qualification; no cross-platform or DAW CPU claim'}
    except BaseException as error:
        decision = {'decision': 'STOP_NO_RETRY', 'error': repr(error), 'release_qualified': False, 'no_retry': True}
    finally:
        if profile_contract is not None:
            try:
                final_profile = capture_unvalidated(prod.profiles, out / 'profile-final-evidence', profile_contract)
                if not final_profile['complete']:
                    raise ValueError('final raw profile evidence incomplete')
                if decision.get('release_qualified'):
                    require(validate_evidence(out / 'profile-final-evidence', profile_contract) == profile,
                            'final retained profile differs from qualified frozen profile')
            except BaseException as error:
                decision.setdefault('evidence_copy_errors', []).append(repr(error))
                decision['decision'] = 'STOP_NO_RETRY'; decision['release_qualified'] = False
        for source, name in [(prod.build / 'Testing/Temporary/LastTest.log', 'LastTest.log'),
                             (prod.build / 'Testing/Temporary/LastTestsFailed.log', 'LastTestsFailed.log'),
                             (prod.build / 'Testing/Temporary/CTestCostData.txt', 'CTestCostData.txt'),
                             (prod.build / 'ci-juce-provenance.txt', 'ci-juce-provenance.txt')]:
            if source.is_file():
                try: shutil.copy2(source, out / name)
                except OSError as error:
                    decision.setdefault('evidence_copy_errors', []).append(repr(error))
                    decision['decision'] = 'STOP_NO_RETRY'; decision['release_qualified'] = False
        decision['protocol_version'] = 3
        decision['prior_terminal_studies'] = [44072518871, 44077183905]
        decision['training_started'] = sum(row['label'] == 'training-once' for row in commands.ledger)
        decision['candidate_cost_started'] = sum(row['label'].startswith('cost-') and row['label'].endswith('-candidate') for row in commands.ledger)
        save(out / 'decision.json', decision)
        print(json.dumps(decision), flush=True)
    return 0 if decision['decision'] in ('PRODUCTION_PIC_QUALIFIED', 'TERMINAL_UNUSABLE_CALIBRATION', 'REJECT_GAIN_OR_REGRESSION_GATE') else 1

if __name__ == '__main__': raise SystemExit(main())
