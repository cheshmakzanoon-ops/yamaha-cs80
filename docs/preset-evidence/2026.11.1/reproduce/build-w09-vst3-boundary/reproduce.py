#!/usr/bin/env python3
"""Rebuild only this small native host; never rebuild or modify the plugin."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--mode', choices=['controller', 'queue-only'], default='controller')
parser.add_argument('--output', type=Path)
parser.add_argument('--module', type=Path)
args = parser.parse_args()
base = Path(__file__).resolve().parent
root = base.parent
out = (args.output or base / (args.mode + '-reproduction')).resolve()
out.mkdir(parents=True, exist_ok=True)
source = base / ('probe.cpp' if args.mode == 'controller' else 'initial-audio-queue-only/probe.cpp')
module = (args.module or root / 'build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so').resolve()
sdk = root / 'deps/juce/modules/juce_audio_processors_headless/format_types/VST3_SDK'
command = ['c++', '-std=c++17', '-O2', '-I', str(root / 'src'), '-I', str(sdk),
           '-I', str(base), str(source),
           str(sdk / 'pluginterfaces/base/coreiids.cpp'),
           str(sdk / 'pluginterfaces/base/funknown.cpp'),
           str(sdk / 'public.sdk/source/vst/vstinitiids.cpp'),
           str(root / 'build-audit/libcs80_core.a'), '-ldl', '-o', str(out / 'probe')]
with (out / 'compile.log').open('w') as log:
    compiled = subprocess.run(command, cwd=root, stdout=log, stderr=subprocess.STDOUT)
if compiled.returncode:
    raise SystemExit(compiled.returncode)
env = os.environ.copy()
env.pop('DISPLAY', None)
env['JUCE_FONT_PATH'] = '/usr/share/fonts/truetype/dejavu'
env['XDG_CONFIG_HOME'] = str(out / 'config')
env['CS80_FAVOURITES_FILE'] = str(out / 'favourites.txt')
Path(env['XDG_CONFIG_HOME']).mkdir(exist_ok=True)
invocation = [str(out / 'probe'), str(module), str(out)]
started = time.monotonic()
with (out / 'run.log').open('w') as log:
    result = subprocess.run(invocation, cwd=root, env=env, stdout=log,
                            stderr=subprocess.STDOUT, timeout=120)
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
record = {'mode': args.mode, 'compile_argv': command, 'run_argv': invocation,
          'raw_exit_code': result.returncode, 'elapsed_seconds': time.monotonic() - started,
          'module_sha256': sha(module), 'probe_sha256': sha(out / 'probe'),
          'source_sha256': sha(source), 'display_unset': True,
          'isolated_config': env['XDG_CONFIG_HOME']}
(out / 'run-result.json').write_text(json.dumps(record, indent=2) + '\n')
print(json.dumps(record, indent=2))
print((out / 'run.log').read_text().splitlines()[-1])
raise SystemExit(result.returncode)
