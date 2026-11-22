#!/usr/bin/env python3
"""Summarize the fixed factory phrase probe; never rewrites captures."""
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
SOURCE = 'be460b0b93bdcb8dce5eb8925b18b72706b25e19'
EXPECTED_INVENTORY = 'fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows(name):
    with (HERE / name).open(newline='') as stream:
        values = list(csv.DictReader(stream))
    result = []
    text = {'program', 'lane', 'kind', 'audio_float32_le_fnv1a64'}
    for row in values:
        result.append({key: value if key in text else float(value) for key, value in row.items()})
    return result


def main():
    log = (HERE / 'run.log').read_text()
    match = re.search(r'Phrase gates: (\d+) checks, (\d+) failures;12 captures,86 seconds;', log)
    assert match and match[1] == '100' and match[2] == '0' and 'FAIL:' not in log
    capture, ring, glide = rows('captures.csv'), rows('ring-results.csv'), rows('glide-results.csv')
    assert len(capture) == 12 and len(ring) == 2 and len(glide) == 8
    assert sum(row['seconds'] for row in capture) == 86
    for row in capture:
        assert row['samples'] == row['seconds'] * 48000 * 2
        assert row['nonfinite'] == row['over_full_scale'] == row['final_active'] == 0
        assert -60 < row['rms_dbfs'] < 0 and row['peak_dbfs'] < 0
    assert digest(ROOT / 'build-audit/inventory-final.json') == EXPECTED_INVENTORY
    assert subprocess.run(['git', 'diff', '--quiet', SOURCE, '--', 'src/core'], cwd=ROOT).returncode == 0
    input_names = ['src/core/Engine.h', 'src/core/Engine.cpp', 'src/core/Params.h',
                   'src/core/FactoryPresets.h', 'src/core/FactoryPresets.cpp',
                   'src/core/CuratedLibrary.inc', 'src/core/CuratedLibraryLevels.inc',
                   'src/core/curated/Leads.inc', 'src/core/curated/Fx.inc', 'src/core/curated/Bells.inc',
                   'build-audit/libcs80_core.a', 'build-audit/inventory-final.json']
    artifact_names = ['probe.cpp', 'probe', 'summarize.py', 'README.md', 'run.log', 'captures.csv',
                      'ring-results.csv', 'ring-points.csv', 'glide-results.csv', 'glide-points.csv',
                      'initial-4s/probe.cpp', 'initial-4s/run.log', 'initial-4s/captures.csv',
                      'initial-4s/ring-results.csv', 'initial-4s/glide-results.csv']
    summary = {
        'assignment': 42, 'status': 'PASS', 'binary_source_commit': SOURCE,
        'observed_checkout_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'core_source_matches_binary_source_commit': True,
        'library_revision': '2026.11.1', 'sample_rate': 48000, 'velocity': 0.8, 'pressure': 0,
        'midi_channel': 1, 'factory_parameters_changed': False,
        'runtime_checks': 100, 'runtime_failures': 0, 'captures': 12, 'generated_seconds': 86,
        'total_generated_seconds_including_initial_observation': 140,
        'highest_peak_dbfs': max(row['peak_dbfs'] for row in capture),
        'lowest_whole_capture_rms_dbfs': min(row['rms_dbfs'] for row in capture),
        'all_captures_finite_below_full_scale_and_voices_retired': True,
        'ring_method': {
            'programs': ['UFO Landing', 'Ring Sweep Bell'], 'seconds_per_capture': 8,
            'lanes': ['continuous', 'overlap', 'pedal', 'released'],
            'events': [
                {'seconds': 0, 'action': 'All lanes note-on MIDI60.'},
                {'seconds': 0.4, 'action': 'Pedal lane CC64=1.'},
                {'seconds': 0.5, 'action': 'Pedal lane releases key60; note remains pedal held.'},
                {'seconds': 0.6, 'action': 'Overlap and pedal lanes note-on67.'},
                {'seconds': 0.9, 'action': 'Overlap and pedal lanes note-off67.'},
                {'seconds': 1.0, 'action': 'Overlap/released lanes note-off60; pedal lane CC64=0. Continuous lane still holds60.'},
                {'seconds': 1.2, 'action': 'Overlap/pedal/released lanes start fresh note72.'},
                {'seconds': 1.6, 'action': 'All remaining held notes release.'}
            ],
            'same_phrase_comparison_seconds': [0.6, 1.0], 'new_phrase_comparison_seconds': [1.2, 1.6],
            'results': ring,
            'interpretation': 'Overlap and pedal-held notes preserve the continuous shared ring envelope/carrier histories exactly. Pedal-up closes the gate; the subsequent new note matches a directly released/new-phrase control exactly and differs materially from the uninterrupted sweep.'
        },
        'glide_method': {
            'programs': ['Glide Lead', 'Glissando Lead'], 'seconds_per_capture': 5.5,
            'lanes': ['overlap', 'separated'],
            'events': [
                {'seconds': 0, 'action': 'Both lanes note-on60.'},
                {'seconds': 0.45, 'action': 'Separated lane note-off60.'},
                {'seconds': 0.5, 'action': 'Both lanes note-on72.'},
                {'seconds': 0.75, 'action': 'Overlap lane note-off60.'},
                {'seconds': 2.25, 'action': 'Separated lane note-off72.'},
                {'seconds': 2.3, 'action': 'Both lanes note-on55.'},
                {'seconds': 2.55, 'action': 'Overlap lane note-off72.'},
                {'seconds': 4.3, 'action': 'Both lanes note-off55.'}
            ],
            'measurement_windows_seconds': [[0.5, 2.2], [2.3, 4.2]],
            'pitch_basis': 'Nominal rank-I oscillator-Hz telemetry, converted in float64 using69+12*log2(hz/440).',
            'results': glide,
            'interpretation': 'Continuous glide traverses fractional pitches smoothly; glissando traverses the integer semitone grid in single-semitone jumps. Matching note histories have identical current-note oscillator trajectories whether the preceding note overlaps or has been released.'
        },
        'capture_metrics': capture,
        'initial_short_capture': {
            'path': 'initial-4s/', 'runtime_checks': 100, 'failed_checks': 6, 'generated_seconds': 54,
            'reason': 'Six ring-lane voices remained active at the premature four-second cutoff. All ring gate and glide assertions already passed. Authored2.0/3.2-second amplitude releases make a longer observation appropriate; ring captures were extended to8seconds without changing factory parameters or assertions.',
            'other_diagnostic_change': 'Pitch conversion now explicitly promotes oscillator frequency and reference to double before logarithms; this avoids float-intermediate rounding in the semitone-grid measurements.'
        },
        'reproduce_compile': 'c++ -std=c++20 -O2 -I ../src probe.cpp ../build-audit/libcs80_core.a -pthread -o probe',
        'reproduce_run': './probe > run.log 2>&1 && python summarize.py',
        'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
        'input_sha256': {name: digest(ROOT / name) for name in input_names},
        'artifact_sha256': {name: digest(HERE / name) for name in artifact_names},
        'limitations': [
            'The ring equality claims concern shared envelope/carrier telemetry, not complete audio equality between different note arrangements.',
            'Pitch-grid and glide claims concern actual oscillator-control telemetry. No independent acoustic-frequency or physical-instrument calibration is claimed.',
            'CC64 is the implemented MIDI sustain extension; this does not characterize the original hardware footswitch circuit.',
            'Zero active voices is not a claim of digital silence. Full-capture RMS is not a release-tail-only measurement.',
            'Only these four programs and event histories are covered; this is not universal controller or DAW qualification.',
            'Raw PCM/WAVs were not written. FNV-1a fingerprints summarize generated float32 sample streams; they are not cryptographic authenticity checks.'
        ]
    }
    (HERE / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS:100 runtime checks;12 captures;86 seconds; all input/output hashes recorded.')


if __name__ == '__main__':
    main()
