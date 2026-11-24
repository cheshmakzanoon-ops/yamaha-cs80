#!/usr/bin/env python3
"""Accept this actual ARM platform only; leave the cross-platform release open.

Run from the repository root after the three byte inspectors and A45 verifier.
No downloaded executable is loaded. API snapshots originate from authenticated
run/jobs/artifacts collection reads; selected single objects are retained too.
"""
from pathlib import Path
from datetime import datetime, timedelta
import hashlib
import importlib.util
import io
import json
import re
import subprocess
import tarfile
import tempfile
import xml.etree.ElementTree as ET
import zipfile

ROOT = Path.cwd()
HERE = Path(__file__).resolve().parent
GUI = HERE.parent / 'macos-gui/a45'
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
PRIOR = 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
RUN, JOB = 45366603800, 138349816955
ARCHIVES = Path('/workspace/scratch/b9c11dd92abc/native-artifacts')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def record(path):
    data = path.read_bytes()
    return {'path': str(path.relative_to(ROOT)), 'bytes': len(data), 'sha256': sha(data)}


def git_bytes(path, source=SOURCE):
    return subprocess.check_output(['git', 'show', source + ':' + path])


def load(name):
    return json.loads((HERE / name).read_bytes())


def dt(value):
    return datetime.fromisoformat(value.replace('Z', '+00:00'))


def main():
    run, job = load('run-api.json'), load('job-api.json')
    require(run['id'] == RUN and run['head_sha'] == SOURCE and run['run_attempt'] == 1, 'Run identity')
    require(job['id'] == JOB and job['run_id'] == RUN and job['head_sha'] == SOURCE
            and job['run_attempt'] == 1 and job['status'] == 'completed'
            and job['conclusion'] == 'success' and job['labels'] == ['macos-15'], 'Original ARM producer')
    steps = {step['name']: step for step in job['steps']}
    required = ['Run actions/checkout@v5', 'Record native platform', 'Configure',
                'Verify configured JUCE source', 'Build', 'Test', 'Fetch pluginval 1.0.4',
                'Register built AU', 'Validate VST3 with pluginval', 'Validate AU with pluginval',
                'Validate AU with pluginval GUI', 'Validate AU GUI fixed-seed regression',
                'Validate AU GUI regression seed 0x3c99748',
                'Package native macOS VST3, AU and Standalone',
                'Upload native macOS install bundle', 'Upload pluginval reports',
                'Upload test and JUCE source evidence']
    for name in required:
        require(steps[name]['status'] == 'completed' and steps[name]['conclusion'] == 'success', name)
    for previous, following in zip(required, required[1:]):
        require(steps[previous]['completed_at'] <= steps[following]['started_at'], 'Step order')

    original = (GUI / 'original-producer-job.log').read_bytes()
    lines = [re.sub(r'\x1b\[[0-9;]*m', '', re.sub(r'^\d{4}-\d\d-\d\dT\S+Z ?', '', line))
             for line in original.decode('utf-8-sig').splitlines()]
    text = '\n'.join(lines) + '\n'
    require('\n' + SOURCE + '\n' in text and '\narm64\n' in text, 'Checkout/native architecture')
    require('cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCS80_BUILD_PLUGIN=ON -DCS80_BUILD_TESTS=ON' in text,
            'Full native configuration')
    require('ctest --test-dir build --output-on-failure --no-tests=error --output-junit ' in text,
            'Unfiltered original CTest invocation')
    require('100% tests passed out of 273' in text and 'Total Test time (real)' in text,
            'Original complete CTest result')
    require('Process completed with exit code' not in text, 'Original nonzero process exit')

    uploads = []
    for kind, artifact_id, step_name in [
            ('install', 14910025415, 'Upload native macOS install bundle'),
            ('native', 14909506446, 'Upload test and JUCE source evidence'),
            ('validator', 14910405103, 'Upload pluginval reports')]:
        api = load(kind + '-api.json')
        raw = (ARCHIVES / f'{artifact_id}.zip').read_bytes()
        require(api['id'] == artifact_id and api['expired'] is False
                and api['size_in_bytes'] == len(raw) and api['digest'] == 'sha256:' + sha(raw)
                and api['workflow_run']['id'] == RUN and api['workflow_run']['head_sha'] == SOURCE,
                'Actual archive identity: ' + kind)
        success = f"Artifact {api['name']} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {artifact_id}"
        require(text.count(success) == 1 and text.count('SHA256 digest of uploaded artifact zip is ' + sha(raw)) == 1,
                'Original upload ID/hash/size: ' + kind)
        step = steps[step_name]
        # API step boundaries are truncated to whole seconds; artifact creation
        # timestamps may fall in the following second after finalization.
        require(dt(step['started_at']) <= dt(api['created_at']) <= dt(step['completed_at']) + timedelta(seconds=1),
                'Artifact creation outside original upload boundary: ' + kind)
        success_line = next(line for line in original.decode('utf-8-sig').splitlines() if success in line)
        when = dt(success_line.split(' ', 1)[0])
        require(dt(step['started_at']) <= when < dt(step['completed_at']) + timedelta(seconds=1),
                'Original success line outside API upload boundary')
        uploads.append({'kind': kind, 'artifact_id': artifact_id, 'name': api['name'],
                        'bytes': len(raw), 'sha256': sha(raw), 'created_at': api['created_at'],
                        'expires_at': api['expires_at'], 'upload_step': step_name,
                        'original_success_timestamp': success_line.split(' ', 1)[0],
                        'original_api_step': step})

    observation, archive_binding = load('native-observations.json'), load('parser-archive-binding.json')
    require(observation['all_requested_observations_green'] is True
            and archive_binding['status'] == 'parser_archive_and_member_input_binding_verified', 'Native observations/bytes')
    native = observation['native'][0]
    require(native['target'] == 'macos-arm64' and native['counts'] == {
        'registered': 273, 'passed': 271, 'failed': 0, 'formal_skipped': 2, 'disabled': 0, 'unknown': 0}, 'Native counts')
    require(native['nested_python_skips']['total'] == 5, 'Separate nested skip total')
    require({x['name'] for x in native['formal_skips']} == {
        'cs80_filter_batch_cap_tests', 'cs80_stereo_pra_exact_tests'}, 'Unexpected formal skip')
    require(all(x['message'] == 'SKIP_RETURN_CODE=77' for x in native['formal_skips']), 'Formal skip contract')

    baseline = ROOT / 'docs/preset-evidence/2026.11.5/ci/source-c76ad28/macos-arm64/attempt-1-failed/contents-test-evidence/ctest-results.xml'
    old_names = [x.attrib['name'] for x in ET.fromstring(baseline.read_bytes()).iter('testcase')]
    new_names = [x['name'] for x in native['testcases']]
    delta = subprocess.check_output(['git', 'diff', '--unified=0', PRIOR, SOURCE, '--', 'CMakeLists.txt'], text=True)
    require(not any(line.startswith('-') and not line.startswith('---') for line in delta.splitlines()),
            'Registration changed beyond additions; independently rederive roster')
    added = re.findall(r'^\+\s*add_test\(NAME\s+(\w+)', delta, re.M)
    require(len(old_names) == len(set(old_names)) == 266 and len(added) == len(set(added)) == 7,
            'Baseline/new registration inventory')
    require(len(new_names) == len(set(new_names)) == 273
            and set(new_names) == set(old_names) | set(added)
            and set(old_names).isdisjoint(added), 'Exact expected native roster')
    expected_tests = load('parser-inputs.json')['_external_reconciliation_not_enforced_by_parser']['all_new_tests_must_be_present_and_passed']
    results = {x['name']: x['result'] for x in native['testcases']}
    require(all(results[name] == 'passed' for name in expected_tests), 'Factory/new regression gate')
    require(results['cs80_ui_smoke'] == 'passed' and results['cs80_typed_value_unit_tests'] == 'passed', 'Native host/UI gates')
    last = (HERE / 'native-members/Testing/Temporary/LastTest.log').read_text()
    require('UI SMOKE PHASE: instrument body host identity and recall' in last
            and 'UI SMOKE TIMING: phase=instrument body host identity and recall' in last,
            'Expanded body-host phase not executed')
    fields = dict(line.split('=', 1) for line in (HERE / 'native-members/ci-juce-provenance.txt').read_text().splitlines() if '=' in line)
    for key, value in {'actual_sha':'c352e2489000ffeb9e397051377d75eef2efe641', 'actual_version':'8.0.11',
                       'source_clean':'true', 'verification':'PASS', 'workflow_commit':SOURCE,
                       'workflow_run':str(RUN), 'source_override':''}.items():
        require(fields[key] == value, 'JUCE provenance ' + key)

    install, payload = load('installation-byte-inspection.json'), load('payload-byte-binding.json')
    require(install['status'] == 'artifact_bytes_verified' and install['source'] == SOURCE
            and install['target'] == 'macos-arm64' and install['preset_library_revision'] == '2026.11.6'
            and set(install['formats']) == {'AU','VST3','Standalone'}, 'Installation bytes/context')
    require(len(install['binary_headers']) == 3 and all(b['header_check'] == 'PASS'
            and b['architecture'] == 'arm64' and b['mode'] == '0o755' for b in install['binary_headers']), 'Product headers')
    require(payload['status'] == 'archive_consistency_and_au_executable_binding_verified', 'AU archive binding')
    gui = json.loads((GUI / 'gui-qualification.json').read_bytes())
    require(gui['source_commit'] == SOURCE and gui['run_id'] == RUN and gui['run_attempt'] == 1
            and gui['job_id'] == JOB and gui['gui_validation_qualified'] is True
            and gui['validator_invocations'] == 5 and gui['validation_retries'] == 0
            and all(x['original_normal_exit'] == 0 for x in gui['outcomes']), 'Original validator execution binding')
    require(gui['original_job_log']['sha256'] == sha(original), 'A45 original log identity')
    au = next(x for x in install['binary_headers'] if x['path'].startswith('AU/'))
    require(au['sha256'] == gui['au_before_after_executable_sha256'], 'Packaged AU executable differs')
    require(gui['whole_au_directory_comparisons_passed'] is True
            and text.count('diff -qr "$AU_SOURCE" "$AU_DEST"') == 2, 'Complete AU directory comparisons')
    register_command = next(g for g in re.split(r'(?=^##\[group\]Run )', text, flags=re.M)
                            if 'codesign --force --sign - "$AU_SOURCE"' in g)
    require(register_command.index('codesign --force --sign - "$AU_SOURCE"')
            < register_command.index('cp -R "$AU_SOURCE" "$AU_DEST"')
            < register_command.index('diff -qr "$AU_SOURCE" "$AU_DEST"'), 'AU sign/copy/compare order')
    require('"$PWD/build/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3"' in text
            and 'python scripts/package-ci-artifacts.py --platform macos --build-dir build --out-dir packages' in text,
            'VST3 validated/packaged build path')

    guides = {'docs/INSTALL_AND_QUICKSTART.md':'INSTALL_AND_QUICKSTART.md',
              'docs/PRESET_GUIDE.md':'PRESET_GUIDE.md', 'docs/CI_DISTRIBUTION_STATUS.md':'DISTRIBUTION_STATUS.md'}
    guide_records = []
    with tempfile.TemporaryDirectory() as temp:
        tree = Path(temp)
        for name in ['scripts/package-ci-artifacts.py', *guides]:
            f = tree / name; f.parent.mkdir(parents=True, exist_ok=True); f.write_bytes(git_bytes(name))
        spec = importlib.util.spec_from_file_location('a47_source_packager', tree/'scripts/package-ci-artifacts.py')
        module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
        with zipfile.ZipFile(ARCHIVES/'14910025415.zip') as archive:
            inner = archive.read(install['inner_archive']['file'])
        with tarfile.open(fileobj=io.BytesIO(inner), mode='r:gz') as archive:
            for source, destination in guides.items():
                members = [x for x in archive.getmembers() if x.name.endswith('/'+destination)]
                require(len(members) == 1, 'Guide membership')
                actual = archive.extractfile(members[0]).read()
                expected_guide = module.packaged_markdown(source, SOURCE, guides)
                require(actual == expected_guide, 'Source-time bundled guide differs: ' + destination)
                guide_records.append({'source':source, 'member':members[0].name, 'bytes':len(actual), 'sha256':sha(actual)})

    skip_review = {
        'formal': [
            {'test':'cs80_filter_batch_cap_tests', 'basis':'Actual SKIP message: AVX2 unavailable or disabled; no SIMD qualification on ARM.'},
            {'test':'cs80_stereo_pra_exact_tests', 'basis':'Source explicitly returns 77 outside the admitted Linux x86-64 GCC 14.2/SSE2 specialization; Apple Clang/ARM is outside that predicate.'}],
        'nested': [
            {'suite':'cs80_package_binary_identity_tests','count':1,'basis':'Native Windows junction fixture is guarded by os.name == nt.'},
            {'suite':'cs80_packaging_notice_tests','count':1,'basis':'Actual verbose reason: Native Linux amd64, Debian packaging tools and cc required.'},
            {'suite':'cs80_windows_installer_preflight_tests','count':2,'basis':'Native directory-junction and native Inno Setup compilation fixtures require Windows.'},
            {'suite':'cs80_render_cli_tests','count':1,'basis':'Address-space-cap branch is conditional. Compact unittest output does not retain its exact reason; no claim that this skipped memory-cap case executed.'}]}
    result = {'schema':1, 'status':'platform_qualified', 'platform_qualified':True, 'release_qualified':False,
              'source_commit':SOURCE, 'source_tree':install['source_tree'], 'factory_revision':'2026.11.6',
              'run_id':RUN, 'run_attempt':1, 'job_id':JOB, 'target':'macos-arm64',
              'native_counts':native['counts'], 'nested_python_skips':5, 'skip_review':skip_review,
              'roster':{'method':'Preserve all 266 prior same-platform registrations; exact source CMake diff only adds seven. Prior outcomes are not reused.',
                        'prior_source':PRIOR, 'prior_roster':record(baseline), 'added':sorted(added),
                        'removed':[], 'all_expected_family_tests_passed':expected_tests,
                        'expanded_body_host_phase_executed':True},
              'validator_invocations':5, 'validation_retries':0, 'validator_original_normal_exits':[0]*5,
              'uploads':uploads, 'binary_headers':install['binary_headers'], 'inventory_entries':install['inventory_entries'],
              'packaged_guides':guide_records, 'juce':fields,
              'whole_au_directory_comparisons_passed':True, 'au_signing_before_copy_verified':True,
              'au_packaged_executable_sha256':au['sha256'],
              'vst3_binding':'Same source-controlled build path is validated and subsequently packaged in the original successful job, without intervening rebuild or mutation command. This is ordered path binding, not a separate before/after VST3 tree hash.',
              'original_ctest_normal_exit':0, 'ctest_exit_basis':'Authenticated successful original unfiltered CTest step plus complete matching JUnit/LastTest and original terminal result.',
              'inputs':{name:record(HERE/name) for name in ['run-api.json','jobs-api.json','job-api.json','artifacts-api.json',
                        'install-api.json','native-api.json','validator-api.json','installation-byte-inspection.json',
                        'payload-byte-binding.json','native-observations.json','parser-archive-binding.json']},
              'a45_gui_qualification':record(GUI/'gui-qualification.json'), 'original_job_log':record(GUI/'original-producer-job.log'),
              'source_files':{name:{'bytes':len(git_bytes(name)), 'sha256':sha(git_bytes(name))}
                              for name in ['CMakeLists.txt','.github/workflows/build.yml','scripts/package-ci-artifacts.py',
                                           'scripts/verify-ci-juce-source.cmake','src/core/FactoryPresets.h']},
              'verifier':record(Path(__file__)),
              'remaining_release_gates':['Other native platforms and all fifteen global validator lanes.',
                                         'Actual audition inventory/WAV acceptance and public download qualification.',
                                         'Final cross-platform aggregate and publication.'],
              'limits':['No downloaded Mac binary executed by this Linux inspection.',
                        'Standalone is compiled, header/inventory checked and covered by native processor/UI tests; no separate end-user launch assessment is claimed.',
                        'No publisher signing/notarization or measured acoustic equivalence is claimed.',
                        'Byte-inspection helpers retain their original limited false qualification flags.']}
    (HERE/'platform-qualification.json').write_text(json.dumps(result, indent=2)+'\n')
    print(json.dumps({k:result[k] for k in ['status','target','native_counts','nested_python_skips','validator_invocations','validation_retries','release_qualified']}))


if __name__ == '__main__':
    main()
