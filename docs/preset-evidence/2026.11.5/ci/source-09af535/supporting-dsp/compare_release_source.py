#!/usr/bin/env python3
"""Record exact supporting/native-source relationships without relabeling CI."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

SUPPORTING = '09af5358d34a7ea431de7e8a42e35dae66e52159'
RELEASE = 'db6b5a76b3c5e38dd9444d0cf6065a14f6302814'
SUPPORTING_TREE = 'd18ddfb7766b622af346f55fe78a59d041adffc5'
RELEASE_TREE = 'fee5d145866135a4c9afb1c22afd375042e4e86f'
ALLOWED = {'docs/INSTALL_AND_QUICKSTART.md', 'docs/PRESET_GUIDE.md'}


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def tree(repo, revision):
    out = {}
    for record in git(repo, 'ls-tree', '-r', '-z', revision).split(b'\0'):
        if not record:
            continue
        meta, path = record.split(b'\t', 1)
        mode, kind, oid = meta.decode().split()
        name = path.decode('utf-8')
        if name in out:
            raise ValueError('Duplicate tracked path')
        out[name] = {'mode': mode, 'type': kind, 'git_object': oid}
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--repo', required=True, type=Path)
    p.add_argument('--out', required=True, type=Path)
    a = p.parse_args()
    if a.out.exists():
        raise ValueError('Refusing to overwrite evidence')
    assert git(a.repo, 'rev-parse', SUPPORTING + '^{tree}').decode().strip() == SUPPORTING_TREE
    assert git(a.repo, 'rev-parse', RELEASE + '^{tree}').decode().strip() == RELEASE_TREE
    before, after = tree(a.repo, SUPPORTING), tree(a.repo, RELEASE)
    assert set(before) == set(after)
    changed = {name for name in before if before[name] != after[name]}
    assert changed == ALLOWED, changed
    rows = []
    for name in sorted(changed):
        sides = []
        for revision, entries in [(SUPPORTING, before), (RELEASE, after)]:
            raw = git(a.repo, 'show', revision + ':' + name)
            sides.append({'source': revision, **entries[name], 'bytes': len(raw), 'sha256': hashlib.sha256(raw).hexdigest()})
        rows.append({'path': name, 'supporting': sides[0], 'native_release': sides[1]})
    prefixes = ('src/', 'tests/', 'scripts/', 'cmake/', 'installer/', 'assets/')
    exact = {'CMakeLists.txt', '.github/workflows/dsp-measurements.yml'}
    inputs = [{'path': name, **before[name]} for name in sorted(before)
              if name.startswith(prefixes) or name in exact]
    assert inputs and all(before[x['path']] == after[x['path']] for x in inputs)
    dependencies = [
        {'ctest': 'cs80_packaging_notice_tests', 'script': 'scripts/test-packaging-notices.py',
         'changed_inputs': sorted(ALLOWED),
         'reason': 'The fixture copies actual guide bytes, checks transformed links and verifies the packaged copies.'},
        {'ctest': 'cs80_macos_installer_architecture_tests', 'script': 'scripts/test-macos-installer-architectures.py',
         'changed_inputs': ['docs/INSTALL_AND_QUICKSTART.md'],
         'reason': 'The inert installer fixture copies the actual installation guide into its test source tree.'},
    ]
    for item in dependencies:
        raw = git(a.repo, 'show', SUPPORTING + ':' + item['script'])
        item['unchanged_script_sha256'] = hashlib.sha256(raw).hexdigest()
    report = {
        'schema': 1, 'supporting_execution_source': SUPPORTING,
        'supporting_tree': SUPPORTING_TREE, 'supporting_run_id': 45344888536, 'supporting_attempt': 1,
        'native_release_source': RELEASE, 'native_release_tree': RELEASE_TREE,
        'proof': 'Complete recursive tracked Git-entry comparison, including modes/types/object IDs; exact changed blobs additionally SHA-256 hashed.',
        'tracked_entries_per_tree': len(before), 'identical_tracked_entries': len(before) - len(changed),
        'changed_files': rows, 'audio_circuit_fixture_workflow_code_and_data_entries_identical': len(inputs),
        'unchanged_code_and_data_inventory': inputs,
        'whole_supporting_test_input_tree_identical': False,
        'documentation_dependent_packaging_tests': dependencies,
        'scope': 'All observed CI results remain attributed to 09af535. The two changed guides leave the audio/circuit/fixture/workflow implementations and data unchanged, but they are changed inputs to documentation-dependent packaging fixtures. The final-source native suite must qualify those fixtures and package the corrected guides.',
        'limitations': [
            'No supporting job or artifact is relabeled as a db6b5a7 execution.',
            'This is source equivalence, not a claim that independently built executables or provenance files are byte-identical.',
            'The actual 09af535 supporting workflow must still finish successfully; this comparison itself qualifies no run.',
            'The corrected documentation inputs require the separate native db6b5a7 execution and package-content checks.',
        ],
    }
    with a.out.open('x', encoding='utf-8') as f:
        json.dump(report, f, indent=2); f.write('\n')
    print(json.dumps({'changed_files': sorted(changed), 'identical_tracked_entries': report['identical_tracked_entries'],
                      'unchanged_code_and_data_entries': len(inputs), 'whole_supporting_test_input_tree_identical': False}))


if __name__ == '__main__':
    main()
