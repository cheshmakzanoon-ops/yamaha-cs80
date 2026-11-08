#!/usr/bin/env python3
"""Compare complete native captures from independently built exact-main modules.

The caller supplies and attests the baseline module. This tool does not infer a
Git commit from a filename, relax numerical bounds, or accept a partial capture.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import subprocess
import time


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inspect(path, contract):
    data = path.read_bytes()
    offset = 0
    cases = 0
    parameter_counts = set()
    def take(fmt):
        nonlocal offset
        size = struct.calcsize(fmt)
        if offset + size > len(data):
            raise ValueError(f"Truncated capture at byte {offset}: {path}")
        result = struct.unpack_from(fmt, data, offset)
        offset += size
        return result
    for policy in range(3):
        for voices in contract['voice_counts']:
            for block in contract['block_sizes']:
                for kind in range(3):
                    if kind == 2 and policy == 0:
                        continue
                    for recall in contract['recalls']:
                        expected = (policy, voices, block, kind, recall['bank'], recall['sample'])
                        if take('<6i') != expected:
                            raise ValueError(f"Case identity mismatch at case {cases}: {path}")
                        samples, = take('<Q')
                        if samples != contract['audio_samples_per_case']:
                            raise ValueError('Wrong complete audio sample count')
                        if not all(math.isfinite(x) for x in take(f'<{samples}f')):
                            raise ValueError('Non-finite captured audio')
                        count, = take('<i')
                        if not 0 < count < 100000:
                            raise ValueError('Invalid parameter count')
                        parameter_counts.add(count)
                        ids = set()
                        for _ in range(count):
                            identity, value = take('<Id')
                            if identity in ids or not math.isfinite(value) or not 0 <= value <= 1:
                                raise ValueError('Invalid complete normalized parameter roster')
                            ids.add(identity)
                        cases += 1
    if cases != contract['capture_cases'] or offset != len(data):
        raise ValueError('Wrong case count or trailing capture bytes')
    return {'cases': cases, 'bytes': len(data), 'parameter_counts': sorted(parameter_counts),
            'sha256': digest(path)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('capture', 'baseline', 'candidate', 'contract', 'output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    if args.baseline.samefile(args.candidate):
        raise ValueError('A self-comparison is not an independent baseline gate')
    contract = json.loads(args.contract.read_text())
    if (contract['capture_cases'] != 336 or contract['audio_samples_per_case'] != 4096
            or contract['maximum_differing_bytes'] != 0
            or contract['comparison'] != 'complete_binary_byte_identity'):
        raise ValueError('The exact-main compatibility contract must remain strict')
    args.output.mkdir(parents=True, exist_ok=True)
    rows = []
    for name, module in (('baseline', args.baseline), ('candidate', args.candidate)):
        output = (args.output / (name + '.bin')).resolve()
        command = [str(args.capture.resolve()), str(module.resolve()), str(output)]
        started = time.monotonic()
        with (args.output / (name + '.log')).open('w') as log:
            result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, timeout=900)
        (args.output / (name + '.exit')).write_text(str(result.returncode) + '\n')
        row = {'name': name, 'command': command, 'module_sha256': digest(module),
               'capture_sha256': digest(args.capture), 'exit': result.returncode,
               'seconds': time.monotonic() - started}
        rows.append(row)
        (args.output / 'runs.json').write_text(json.dumps(rows, indent=2) + '\n')
        if result.returncode:
            raise RuntimeError(f'{name} capture failed: {result.returncode}')
        row['complete_capture'] = inspect(output, contract)
        print(name, row['complete_capture'], flush=True)
    a = (args.output / 'baseline.bin').read_bytes()
    b = (args.output / 'candidate.bin').read_bytes()
    equal = a == b
    report = {'contract_sha256': digest(args.contract), 'baseline_commit_required': contract['baseline_commit'],
              'runs': rows, 'complete_binary_byte_identical': equal,
              'differing_bytes': sum(x != y for x, y in zip(a, b)) + abs(len(a) - len(b)),
              'first_differing_byte': next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), None)}
    (args.output / 'results.json').write_text(json.dumps(report, indent=2) + '\n')
    print('complete binary byte identity:', equal, flush=True)
    return 0 if equal else 1


if __name__ == '__main__':
    raise SystemExit(main())
