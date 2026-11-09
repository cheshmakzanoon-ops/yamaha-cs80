#!/usr/bin/env python3
"""Private offline faults: never download, execute a validator, or edit the checkout."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
HELPER = 'scripts/download-pluginval-macos.py'
TEST = 'scripts/test-pluginval-macos-download.py'
WORKFLOW = '.github/workflows/build.yml'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    faults = [
        ('checksum-bypass', HELPER, 'if len(data) != EXPECTED_SIZE or digest != EXPECTED_SHA256:', 'if False:'),
        ('unbounded-read', HELPER, 'response.read(EXPECTED_SIZE + 1)', 'response.read()'),
        ('unsafe-redirect', HELPER, 'if not official_url(newurl):', 'if False:'),
        ('no-hop-counter', HELPER, 'if hops > self.max_redirections:', 'if False:'),
        ('retry-permanent', HELPER, 'if status != TRANSIENT:', 'if False:'),
        ('no-wall-timeout', HELPER, 'timeout=ATTEMPT_SECONDS)', 'timeout=None)'),
        ('extra-attempt', HELPER, 'ROUTE_ORDER = (0, 1, 0)', 'ROUTE_ORDER = (0, 1, 0, 1)'),
        ('tls-retry', HELPER, 'return FATAL, {"reason": "tls_verification"}', 'return TRANSIENT, {"reason": "tls_verification"}'),
        ('false-provenance', HELPER, 'record.update(verified=True,', 'record.update(verified=False,'),
        ('signed-url-leak', HELPER, 'host = urllib.parse.urlsplit(response.geturl()).hostname', 'host = response.geturl()'),
        ('weaken-validator', WORKFLOW, '--output-filename cs80-au-gui', '--output-filename changed-gate'),
        ('ignore-fetch-failure', WORKFLOW,
         '--evidence pluginval-download.json\n          unzip -q',
         '--evidence pluginval-download.json || true\n          unzip -q'),
        ('no-step-timeout', WORKFLOW, '- name: Fetch pluginval 1.0.4\n        timeout-minutes: 5', '- name: Fetch pluginval 1.0.4'),
        ('no-provenance-upload', WORKFLOW, 'name: cs80-pluginval-${{ matrix.runner }}\n          path: |\n            pluginval-download.json',
         'name: cs80-pluginval-${{ matrix.runner }}\n          path: |'),
    ]
    results = []
    with tempfile.TemporaryDirectory(prefix='cs80-macos-transfer-negative-') as directory:
        private = Path(directory)
        for name, relative, old, new in [('candidate', None, None, None), *faults]:
            case = private / name
            for path in (HELPER, TEST, WORKFLOW):
                destination = case / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(ROOT / path, destination)
            if relative:
                path = case / relative
                source = path.read_text()
                if source.count(old) != 1:
                    raise RuntimeError(f'{name}: source anchor changed')
                path.write_text(source.replace(old, new))
            result = subprocess.run([sys.executable, str(case / TEST), '-v'],
                                    capture_output=True, text=True, timeout=30)
            log = result.stdout + result.stderr
            (args.output / (name + '.log')).write_text(log)
            expected = 0 if name == 'candidate' else 1
            if result.returncode != expected or 'Ran 17 tests' not in log:
                raise RuntimeError(f'{name}: no expected normal test completion, exit={result.returncode}')
            results.append({'case': name, 'exit': result.returncode,
                            'expected_outcome': 'pass' if name == 'candidate' else 'detected'})
    (args.output / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'Candidate passed; {len(faults)} deliberately unsafe helper/workflow mutations detected.')


if __name__ == '__main__':
    main()
