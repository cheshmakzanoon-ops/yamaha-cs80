#!/usr/bin/env python3
"""Read-only saved-data audit; never compiles or invokes the native executable.

Usage: python independent-capture-audit.py [CAPTURE_DIRECTORY]
Prints JSON to stdout. Phase accumulation is exact integer arithmetic because
these saved float rate inputs are multiples of 2^-19 Hz. No production helpers
or saved reference phases are imported.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import sys

CAPTURES = (Path(sys.argv[1]) if len(sys.argv) > 1 else
            Path(__file__).resolve().parents[3] / 'build-lfo-random-variable-rate/captures')
QUANTUM_DENOMINATOR = 1 << 19
SEEDS = (0x6c8e9cf5, 0x3b9e5d21, 0xa1c4f00d)
RATES = (88200, 96000, 192000, 384000)

def values(path):
    return [v[0] for v in struct.iter_unpack('<f', path.read_bytes())]

def stream(seed):
    state = seed
    while True:
        state = (state ^ (state * 8192)) & 0xffffffff
        state ^= state // 131072
        state = (state ^ (state * 32)) & 0xffffffff
        yield (state // 256) / 8388608 - 1

def float32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]

cases = []
total = 0
input_hashes = {}
for sample_rate in RATES:
    rate_path = CAPTURES / f'rates-{sample_rate}.f32'
    rates = values(rate_path)
    assert len(rates) == sample_rate // 4
    input_hashes[rate_path.name] = hashlib.sha256(rate_path.read_bytes()).hexdigest()
    phases, cycles = [], []
    numerator = 0
    denominator = QUANTUM_DENOMINATOR * sample_rate
    minimum_clearance = 1.0
    for rate in rates:
        assert math.isfinite(rate) and 16 <= rate <= 20
        quantum = int(rate * QUANTUM_DENOMINATOR)
        assert quantum / QUANTUM_DENOMINATOR == rate
        numerator += quantum
        cycle, remainder = divmod(numerator, denominator)
        phases.append(remainder / denominator)
        cycles.append(cycle)
        minimum_clearance = min(minimum_clearance,
                                min(remainder, denominator - remainder) / denominator)
    assert cycles[-1] == 4 and minimum_clearance > 1e-10
    for waveform in (5, 6):
        previous_seed = None
        for seed in SEEDS:
            identity = f'{sample_rate}-wave{waveform}-seed{seed}'
            actual_path = CAPTURES / f'{identity}-actual.f32'
            reference_path = CAPTURES / f'{identity}-reference.f32'
            actual, saved_reference = values(actual_path), values(reference_path)
            assert len(actual) == len(saved_reference) == len(rates)
            generator = stream(seed)
            left = next(generator)
            right = next(generator) if waveform == 6 else left
            previous_cycle = 0
            worst_actual = worst_reference = 0.0
            for phase, cycle, observed, saved in zip(phases, cycles, actual, saved_reference):
                if cycle != previous_cycle:
                    assert cycle == previous_cycle + 1
                    previous_cycle = cycle
                    if waveform == 5:
                        left = next(generator)
                    else:
                        left, right = right, next(generator)
                # Exact rational phase is projected to double only for cosine.
                expected = float32(left if waveform == 5 else
                                  left + (right - left) * (1 - math.cos(math.pi * phase)) / 2)
                assert math.isfinite(observed) and abs(observed) <= 1
                assert math.isfinite(saved) and abs(saved) <= 1
                actual_error, reference_error = abs(observed - expected), abs(saved - expected)
                assert actual_error <= 1e-6 and reference_error <= 1e-6
                if waveform == 5:
                    assert (struct.pack('<f', observed) == struct.pack('<f', expected)
                            == struct.pack('<f', saved))
                worst_actual = max(worst_actual, actual_error)
                worst_reference = max(worst_reference, reference_error)
                total += 1
            if previous_seed is not None:
                assert previous_seed != actual
            previous_seed = actual
            cases.append({'case': identity, 'samples': len(actual), 'wraps': previous_cycle,
                          'max_actual_error': worst_actual,
                          'max_saved_reference_error': worst_reference,
                          'minimum_boundary_clearance': minimum_clearance,
                          'actual_sha256': hashlib.sha256(actual_path.read_bytes()).hexdigest(),
                          'reference_sha256': hashlib.sha256(reference_path.read_bytes()).hexdigest()})
assert len(cases) == 24 and total == 1140300
report = {'result': 'PASS', 'method': 'exact integer phase accumulation with 2^-19 Hz rate quantum; independent uint32 RNG arithmetic; double cosine then float32 projection',
          'cases': len(cases), 'compared_samples': total, 'rate_quantum': '2^-19 Hz',
          'max_actual_error': max(c['max_actual_error'] for c in cases),
          'max_saved_reference_error': max(c['max_saved_reference_error'] for c in cases),
          'minimum_boundary_clearance': min(c['minimum_boundary_clearance'] for c in cases),
          'wraps_per_case': 4, 'sample_hold_bit_exact': True,
          'rate_capture_sha256': input_hashes, 'case_results': cases,
          'limits': 'Saved-data audit only. Reset replay is verified by the reviewed native fixture and its completed log, not rerun here. No native execution, build, timing or randomness-quality claim.'}
print(json.dumps(report, indent=2))
