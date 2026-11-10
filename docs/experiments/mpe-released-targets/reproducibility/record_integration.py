#!/usr/bin/env python3
"""Record the expressly reviewed screenshot-only integration, not new tests.

Keeps the historical baseline/run/source-pins record unchanged. Source and
neighbor-test input identities must still match the frozen qualification.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parents[2]
PARENT = '4e01dcda8f0e6a721c492ef5e2ae4d952e7230c5'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    obj = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(obj)
    return obj


def git(*args):
    return subprocess.check_output(['git', '-C', str(REPO), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--screenshot-parent', required=True, choices=[PARENT])
    args = parser.parse_args()
    verifier = module('mpe_bundle_verify', ROOT/'verify.py')
    archive = module('mpe_bundle_archive', ROOT/'reproducibility/archive.py')
    sealer = module('mpe_bundle_seal', ROOT/'reproducibility/seal.py')
    pins = verifier.data(ROOT/'evidence/source-pins.json')
    parent = args.screenshot_parent
    parent_parent = git('rev-parse', parent+'^').decode().strip()
    verifier.require(git('rev-parse', parent_parent+'^{tree}') == git('rev-parse', archive.BASE+'^{tree}'), 'Screenshot child is not based on the same baseline tree')
    tree = archive.tree(REPO, parent)
    original = {n:r for n,r in pins['preserved_tree_entries'].items() if n.startswith('docs/screenshots/')}
    screenshots = {}
    for name, entry in sorted(tree.items()):
        if not name.startswith('docs/screenshots/'):
            continue
        content = git('show', parent+':'+name)
        screenshots[name] = dict(entry, bytes=len(content), sha256=hashlib.sha256(content).hexdigest())
    delta = sorted(n for n in set(original)|set(screenshots) if original.get(n) != screenshots.get(n))
    parent_delta = sorted(git('diff', '--name-only', parent_parent, parent).decode().splitlines())
    record = {
        'schema': 1, 'historical_baseline': archive.BASE,
        'final_source_test_freeze': pins['final_neighbor_test_freeze'],
        'integration_head_at_recording': git('rev-parse','HEAD').decode().strip(),
        'screenshot_parent': parent, 'screenshot_parent_parent': parent_parent,
        'snapshot_parent_parent_tree_equals_baseline': True,
        'changed_screenshot_paths': delta, 'screenshot_only_parent_delta': parent_delta,
        'screenshot_tree': screenshots, 'runtime_repeated': False,
        'scope': 'Only current-checkout screenshot preservation uses this parent. Original qualification baseline and all source/test/build hashes remain unchanged. Screenshots do not depict or qualify this repair.'}
    verifier.inspect_tree(REPO, pins, record)
    target = ROOT/'evidence/integration-preservation.json'
    verifier.require(not target.exists(), 'Integration record already exists; inspect rather than overwrite it')
    target.write_text(json.dumps(record, indent=2, sort_keys=True) + '\n')
    (ROOT/'INTEGRATION.md').write_text(f'''# Screenshot-only publication integration

The qualification evidence retains baseline `{archive.BASE}` and final
source/test freeze `{pins['final_neighbor_test_freeze']}` unchanged.

Publication preserves screenshot child `{parent}`, whose parent
`{parent_parent}` has the same whole repository tree as the qualification
baseline. The child changes exactly 24 screenshot/provenance paths and no
source, test, CMake or other paths. All 70 screenshot paths, modes and bytes
in the integrated checkout match that child; the full inventory is in
`evidence/integration-preservation.json`.

Every qualified source/test/CMake hash and every preserved non-screenshot
entry still matches the original qualification. This integration repeats no
runtime, GUI, screenshot-rendering or performance tests. The inherited images
depict the earlier MPE work, not this released-target repair. Historical
run pins and screenshot preservation at the time of qualification were not
rewritten. The verifier substitutes only this explicit screenshot subtree
when checking the present integrated checkout.

Recorded from integration HEAD `{record['integration_head_at_recording']}`.
This is a local integration identity, not a remote publication or CI claim.
''')
    sealer.seal(ROOT)
    print(json.dumps(verifier.verify(ROOT, REPO), sort_keys=True))


if __name__ == '__main__':
    main()
