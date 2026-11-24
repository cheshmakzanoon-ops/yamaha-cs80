#!/usr/bin/env python3
"""Run unchanged existing tests for the explicit .7 source; no native execution."""
from __future__ import annotations
import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=Path, required=True)
    parser.add_argument('--source', required=True)
    parser.add_argument('--revision', required=True)
    parser.add_argument('--out-dir', type=Path, required=True)
    args = parser.parse_args()
    evidence = args.repo / 'docs/preset-evidence/2026.11.5/verification'
    probe_path = evidence / 'a41-test-install-artifact.py'
    helper_path = evidence / 'a41-verify-install-artifact.py'
    probe = load('a30_reused_a41_tests', probe_path)
    # The historical suite's fixture constants are rebound only in memory.
    # Its eleven test bodies and all frozen files remain unchanged.
    probe.SOURCE, probe.REVISION = args.source, args.revision
    probe.legacy.REVISION = args.revision
    records = {}
    with tempfile.TemporaryDirectory(prefix='a30-source-tests-') as directory:
        scratch = Path(directory)
        for name in ('package-ci-artifacts.py', 'verify-ci-bundle.py',
                     'test-verify-ci-bundle.py', 'test-package-binary-identity.py'):
            relative = 'scripts/' + name
            raw = subprocess.check_output(['git', '-C', str(args.repo), 'show', args.source + ':' + relative])
            # The unchanged historical fixture builder loads these two files
            # from its repository; prove those bytes equal the explicit source.
            if name in ('package-ci-artifacts.py', 'verify-ci-bundle.py'):
                if (args.repo / relative).read_bytes() != raw:
                    raise ValueError('Historical fixture builder would load different source: ' + relative)
            (scratch / name).write_bytes(raw)
            records[relative] = {'bytes': len(raw), 'sha256': sha(raw)}
        bundle_tests = load('a30_source_bundle_tests', scratch / 'test-verify-ci-bundle.py')
        header_tests = load('a30_source_header_tests', scratch / 'test-package-binary-identity.py')
        suite = unittest.defaultTestLoader.loadTestsFromTestCase(probe.InstallArtifactTests)
        # Concrete contract edges absent from the A41 suite: Unix modes,
        # virtual symlink containment, and native executable/plugin role.
        focused = [bundle_tests.VerificationTests('test_wrong_mode'),
                   bundle_tests.VerificationTests('test_virtual_links'),
                   header_tests.IdentityTests('test_wrong_image_kind')]
        suite.addTests(focused)
        log = io.StringIO()
        result = unittest.TextTestRunner(stream=log, verbosity=2).run(suite)
    summary = {
        'scope': 'Unchanged existing synthetic tests; inert headers only. No real product, native CI, API authenticity or release qualification.',
        'source': args.source, 'revision': args.revision,
        'historical_suite_rebound_in_memory': ['SOURCE', 'REVISION', 'legacy.REVISION'],
        'historical_test_bodies_changed': False,
        'tests_run': result.testsRun, 'failures': len(result.failures), 'errors': len(result.errors),
        'a41_existing_methods': 11, 'focused_existing_methods': [case.id() for case in focused],
        'a41_positive_target_fixtures': sum(x['kind'] == 'accepted_inert_fixture' for x in probe.OBSERVATIONS),
        'a41_rejected_mutations': sum(x['kind'] == 'rejected_fixture' for x in probe.OBSERVATIONS),
        'a41_observations': probe.OBSERVATIONS,
        'source_test_files': records,
        'helper_sha256': sha(helper_path.read_bytes()), 'historical_probe_sha256': sha(probe_path.read_bytes()),
        'runner_sha256': sha(Path(__file__).read_bytes()),
    }
    args.out_dir.mkdir(parents=True, exist_ok=True)
    with (args.out_dir / 'existing-checks.log').open('x') as stream:
        stream.write(log.getvalue())
    with (args.out_dir / 'existing-checks.json').open('x') as stream:
        json.dump(summary, stream, indent=2)
        stream.write('\n')
    print(log.getvalue(), end='')
    return 0 if result.wasSuccessful() else 1


if __name__ == '__main__':
    raise SystemExit(main())
