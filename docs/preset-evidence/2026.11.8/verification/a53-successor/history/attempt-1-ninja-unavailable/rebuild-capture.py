#!/usr/bin/env python3
"""Execute and retain the exact clean, serial Release capture rebuild."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
REPO = Path('/tmp/cs80-614c8ce056b7')
BUILD = Path('/tmp/cs80-614c8ce056b7-build')
CMAKE = Path('/tmp/cs80-614c8ce056b7-deps/cmake/data/bin/cmake')
SOURCE = '1b5b3c70471e369481b1f813880a6bcc762a2f57'
TREE = 'b235b67a801d33e9572076b8dfcfbb89e71fac29'
RECEIPT = HERE / 'clean-build-receipt.json'


def stamp():
    return datetime.now(timezone.utc).isoformat()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def source_state():
    git = lambda *args: subprocess.check_output(['git', '-C', str(REPO), *args], text=True).strip()
    state = {'source': git('rev-parse', 'HEAD'), 'tree': git('rev-parse', 'HEAD^{tree}'),
             'tracked_status': git('status', '--porcelain=v1', '--untracked-files=no')}
    if state != {'source': SOURCE, 'tree': TREE, 'tracked_status': ''}:
        raise RuntimeError('Clean exact source/tree requirement failed: ' + repr(state))
    return state


def main():
    if RECEIPT.exists():
        raise RuntimeError('Build receipt already exists; preserve it and use a new explicitly reviewed run')
    record = {'schema': 1, 'status': 'started', 'execution_kind': 'local_recovery',
              'original_native_build_run_id': 45457535944, 'original_native_build_attempt': 1,
              'source_before': source_state(), 'started_at_utc': stamp(), 'steps': [],
              'scope': 'Clean Release core/capture rebuild only; no native plugin or CTest qualification.'}

    def save():
        RECEIPT.write_text(json.dumps(record, indent=2) + '\n')

    def run(name, command):
        path = HERE / (name + '.log')
        if path.exists():
            raise RuntimeError('Step log already exists: ' + str(path))
        step = {'name': name, 'command': command, 'cwd': str(REPO), 'started_at_utc': stamp()}
        record['steps'].append(step)
        save()
        print('Starting ' + name, flush=True)
        start = time.monotonic()
        with path.open('xb') as stream:
            result = subprocess.run(command, cwd=REPO, stdout=stream, stderr=subprocess.STDOUT, check=False)
        step.update(completed_at_utc=stamp(), elapsed_seconds=time.monotonic() - start,
                    original_returncode=result.returncode, log=str(path),
                    log_bytes=path.stat().st_size, log_sha256=digest(path))
        save()
        print(name + ' exited ' + str(result.returncode), flush=True)
        if result.returncode != 0:
            raise RuntimeError(name + ' failed with original return code ' + str(result.returncode))

    try:
        run('cmake-version', [str(CMAKE), '--version'])
        run('compiler-version', ['/usr/bin/g++', '--version'])
        run('configure', [str(CMAKE), '--fresh', '-S', str(REPO), '-B', str(BUILD), '-G', 'Ninja',
                          '-DCMAKE_BUILD_TYPE=Release', '-DCS80_BUILD_PLUGIN=OFF', '-DCS80_BUILD_TESTS=OFF'])
        run('clean', [str(CMAKE), '--build', str(BUILD), '--target', 'clean'])
        remaining = [str(path) for target in ('cs80_core', 'cs80_preset_capture')
                     for path in (BUILD / 'CMakeFiles' / (target + '.dir')).rglob('*.o')]
        remaining += [str(BUILD / name) for name in ('libcs80_core.a', 'cs80_preset_capture')
                      if (BUILD / name).exists()]
        record['after_clean'] = {'remaining_core_or_capture_objects_and_products': remaining,
                                 'observed_at_utc': stamp()}
        save()
        if remaining:
            raise RuntimeError('Clean left core/capture objects or products')
        run('build', [str(CMAKE), '--build', str(BUILD), '--target', 'cs80_preset_capture', '--parallel', '1'])
        record['source_after'] = source_state()
        record['outputs'] = {name: {'path': str(BUILD / name), 'bytes': (BUILD / name).stat().st_size,
                                  'sha256': digest(BUILD / name)}
                             for name in ('libcs80_core.a', 'cs80_preset_capture')}
        record.update(status='complete', completed_at_utc=stamp(), all_original_step_exits_zero=True)
        save()
        print('Clean serial Release capture rebuild complete', flush=True)
    except BaseException as error:
        record.update(status='failed', completed_at_utc=stamp(), error=str(error))
        save()
        raise


if __name__ == '__main__':
    try:
        main()
    except BaseException as error:
        print('Clean capture rebuild failed: ' + str(error), file=sys.stderr)
        raise SystemExit(1)
