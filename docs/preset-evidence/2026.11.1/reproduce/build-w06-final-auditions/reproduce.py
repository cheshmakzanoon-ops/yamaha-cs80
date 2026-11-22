#!/usr/bin/env python3
"""Run the archived CI block with one explicit local executable substitution."""
from pathlib import Path
import argparse
import os
import subprocess
import sys

TASK = Path(__file__).resolve().parent
SOURCE = '99f5ebd6b71283a56810d801df89c49ef62f3512'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--capture', type=Path, required=True)
    parser.add_argument('--workdir', type=Path, required=True,
                        help='New directory inside a checkout of the recorded source commit.')
    args = parser.parse_args()
    capture = args.capture.resolve(strict=True)
    workdir = args.workdir.resolve()
    workdir.mkdir(parents=True, exist_ok=False)
    actual = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=workdir, text=True).strip()
    if actual != SOURCE:
        raise SystemExit('Check out the recorded source commit before reproducing this exact artifact.')
    original = (TASK / 'workflow-audition-original.py').read_text()
    old = "capture = Path('build/cs80_preset_capture')"
    assert original.count(old) == 1
    local = original.replace(old, 'capture = Path(' + repr(str(capture)) + ')')
    script = workdir / 'workflow-audition-local.py'
    script.write_text(local)
    env = dict(os.environ, GITHUB_SHA=SOURCE,
               GITHUB_REPOSITORY='cheshmakzanoon-ops/yamaha-cs80',
               GITHUB_RUN_ID='local-assignment-32-reproduction', GITHUB_RUN_ATTEMPT='1')
    subprocess.run([sys.executable, str(script)], cwd=workdir, env=env, check=True)


if __name__ == '__main__':
    main()
