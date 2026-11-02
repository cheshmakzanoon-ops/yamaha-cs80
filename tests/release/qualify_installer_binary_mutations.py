#!/usr/bin/env python3
"""Actual helper-source controls with exact ordinary failures, never native claims."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
HELPER = 'scripts/verify_installer_binaries.py'
TEST = 'tests/release/test_installer_binary_payload.py'
MUTATIONS = [
    ('ignore_input_snapshot', 'require(captured == current,', 'require(True,',
     ['test_forged_manifest_cannot_certify_stale_payload']),
    ('allow_extra_inventory', 'require(set(wanted) == set(present),', 'require(set(wanted) <= set(present),',
     ['test_linux_unexpected_bundle_resource_rejected']),
    ('ignore_payload_bytes', "require(value['bytes'] == row['bytes'] and value['sha256'] == row['sha256'],", 'require(True,',
     ['test_linux_stale_or_changed_bytes_rejected', 'test_macos_changed_post_capture_signature_bytes_are_not_waived',
      'test_windows_stale_standalone_rejected']),
    ('ignore_mode', "require(value['mode'] == row['mode'],", 'require(True,',
     ['test_linux_executable_mode_loss_rejected']),
    ('ignore_link_target', "require(value['target'] == row['target'],", 'require(True,',
     ['test_internal_relative_link_target_change_is_rejected']),
    ('wrong_component_destination', "require(destination == expected['install_destination'],", 'require(True,',
     ['test_macos_wrong_install_location_rejected']),
    ('skip_au_verification', "    for key, expected in captured['components'].items():\n",
     "    for key, expected in captured['components'].items():\n        if key == 'au':\n            continue\n",
     ['test_macos_missing_component_rejected', 'test_macos_three_bundles_and_install_destinations',
      'test_macos_wrong_install_location_rejected']),
    ('claim_windows_vst3', "'verified_components': sorted(verified),",
     "'verified_components': (sorted(verified) + ['vst3'] if platform == 'windows' else sorted(verified)),",
     ['test_windows_only_actual_standalone_scope']),
    ('allow_empty_primary', 'require(primary.is_file() and not primary.is_symlink() and primary.stat().st_size > 0,',
     'require(True,', ['test_empty_primary_build_binary_is_rejected',
      'test_missing_primary_build_binary_is_not_a_resource_only_bundle',
      'test_missing_primary_build_binary_is_not_a_resource_only_bundle']),
    ('follow_expansion_root', '    expanded = independent_root(expanded)\n',
     '    expanded = Path(expanded).resolve()\n', ['test_linked_expansion_root_is_rejected']),
    ('follow_artifact_root', "    artifacts = component_path(independent_root(build), 'CS80Reference_artefacts/Release')\n",
     "    artifacts = Path(build).resolve() / 'CS80Reference_artefacts/Release'\n", ['test_linked_artifact_root_is_rejected']),
    ('allow_external_links', "require(not Path(target).is_absolute() and path.exists()\n                    and path.resolve().is_relative_to(boundary),",
     'require(True,', ['test_external_source_symlink_is_rejected']),
]


def classify(code, output, expected):
    failures = re.findall(r'^FAIL: (\w+) ', output, re.MULTILINE)
    errors = re.findall(r'^ERROR: (\w+) ', output, re.MULTILINE)
    inventory = re.findall(r'^Ran (\d+) tests in ', output, re.MULTILINE)
    summary = re.findall(r'^(?:OK(?: \(.*\))?|FAILED \(.*\))$', output, re.MULTILINE)
    wanted = f'FAILED (failures={len(expected)})' if expected else 'OK'
    qualified = (inventory == ['27'] and not errors and 'skipped=' not in output
                 and sorted(failures) == sorted(expected) and summary == [wanted]
                 and code == (1 if expected else 0))
    return {'qualified': qualified, 'ordinary_failures': failures, 'runtime_errors': errors,
            'test_inventory': inventory, 'expected_failures': expected}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if os.name == 'nt':
        raise ValueError('Complete POSIX/link mutation qualification requires native Linux/macOS; Windows cannot waive skips')
    args.out.mkdir(parents=True, exist_ok=False)
    original = (ROOT / HELPER).read_text()
    cases = [('positive', '', '', []), *MUTATIONS,
             ('runtime_error_control', "    require(platform in LAYOUT, 'Unknown installer platform')\n",
              "    raise RuntimeError('deliberate unqualified crash')\n", []),
             ('restored', '', '', [])]
    receipt = {'schema_version': 1, 'source_commit': subprocess.check_output(['git', '-C', str(ROOT), 'rev-parse', 'HEAD'], text=True).strip(),
               'files': {p: hashlib.sha256((ROOT / p).read_bytes()).hexdigest() for p in (HELPER, TEST)},
               'semantic_mutants': len(MUTATIONS), 'runtime_error_is_not_a_semantic_kill': True,
               'scope': 'Synthetic payload/helper identity controls only; no native Windows/macOS installer execution, compiler/source, signing or musical qualification',
               'cases': []}
    with tempfile.TemporaryDirectory(prefix='cs80-installer-binary-mutations-') as temp:
        scratch = Path(temp)
        for relative in (HELPER, TEST):
            target = scratch / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / relative, target)
        for name, before, after, expected in cases:
            if before and original.count(before) != 1:
                raise AssertionError('Mutation anchor is not unique: ' + name)
            (scratch / HELPER).write_text(original.replace(before, after) if before else original)
            shutil.rmtree(scratch / 'scripts/__pycache__', ignore_errors=True)
            result = subprocess.run([sys.executable, str(scratch / TEST)], stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, text=True, timeout=60, check=False)
            assessment = classify(result.returncode, result.stdout, expected)
            runtime = name == 'runtime_error_control'
            accepted = (not assessment['qualified'] and bool(assessment['runtime_errors'])
                        and assessment['test_inventory'] == ['27']) if runtime else assessment['qualified']
            log = args.out / (name + '.log'); log.write_text(result.stdout)
            receipt['cases'].append({'case': name, 'result': 'PASS' if accepted else 'FAIL',
                                     'expected': 'harness_refusal_not_semantic_kill' if runtime else 'semantic_rejection' if expected else 'pass',
                                     'exit': result.returncode, 'assessment': assessment,
                                     'log_sha256': hashlib.sha256(log.read_bytes()).hexdigest()})
            if not accepted:
                (args.out / 'incomplete-receipt.json').write_text(json.dumps(receipt, indent=2) + '\n')
                print(result.stdout, file=sys.stderr)
                raise AssertionError('Unqualified source control: ' + name + ': ' + str(assessment))
    (args.out / 'qualification.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
