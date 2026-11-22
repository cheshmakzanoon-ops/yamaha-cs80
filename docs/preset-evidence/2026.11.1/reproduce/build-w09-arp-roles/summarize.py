#!/usr/bin/env python3
"""Summarize the bounded factory arpeggiator runtime fixture."""
from pathlib import Path
import csv
import hashlib
import json
import subprocess

OUT = Path(__file__).resolve().parent
ROOT = OUT.parent
SOURCE = 'be460b0b93bdcb8dce5eb8925b18b72706b25e19'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

rows = list(csv.DictReader((OUT / 'results.csv').open()))
events = list(csv.DictReader((OUT / 'transitions.csv').open()))
assert len(rows) == 4
integer_fields = {'mode', 'octaves', 'factor', 'onsets', 'unique_notes', 'first_onset_frame',
                  'nonfinite', 'samples_over_full_scale', 'max_active_voices',
                  'final_active_voices', 'checks', 'failures'}
converted = []
for original in rows:
    row = {key: (int(value) if key in integer_fields else value if key in ('program', 'audio_sha256')
                 else float(value)) for key, value in original.items()}
    actual = [int(event['note']) for event in events if event['program'] == row['program'] and int(event['note']) >= 0]
    assert len(actual) == row['onsets']
    row['observed_note_sequence'] = actual
    row['source_notes'] = [60, 64, 67] if row['program'] == 'Arp Bells' else [48, 52, 55]
    row['source_velocities'] = [.45, .7, .9]
    if row['program'] == 'Arp Poly':
        expected = [48, 52, 55, 60, 64, 67]
    elif row['program'] == 'Arp Bells':
        expected = [60, 64, 67, 72, 76, 79, 76, 72, 67, 64]
    elif row['program'] == 'Arp Bass':
        expected = [48, 52, 55]
    else:
        expected = None
    if expected:
        assert actual == [expected[i % len(expected)] for i in range(len(actual))]
        row['expected_ordered_cycle'] = expected
    else:
        assert set(actual) == {48, 52, 55, 60, 64, 67}
        row['random_qualification'] = 'Actual sequence is recorded, but no exact random sequence is required by the fixture.'
    assert row['failures'] == row['nonfinite'] == row['samples_over_full_scale'] == row['final_active_voices'] == 0
    assert row['checks'] == 16
    converted.append(row)

inputs = ['build-audit/libcs80_core.a', 'build-audit/inventory-final.json', 'src/core/Engine.cpp',
          'src/core/Engine.h', 'src/core/Params.h', 'src/core/FactoryPresets.cpp',
          'src/core/FactoryPresets.h', 'src/core/CuratedLibrary.inc',
          'src/core/CuratedLibraryLevels.inc', 'src/core/curated/Modern.inc',
          'src/tools/TruthInputSha256.h']
input_hashes = {name: digest(ROOT / name) for name in inputs}
for name in inputs:
    if name.startswith('src/'):
        committed = subprocess.check_output(['git', 'show', f'{SOURCE}:{name}'], cwd=ROOT)
        assert hashlib.sha256(committed).hexdigest() == input_hashes[name]
assert input_hashes['build-audit/inventory-final.json'] == 'fe3ddc6ecdb9fa25e77170f1cd9f81e6f2fe0ef5aabd105506a0fa487eb286fa'
summary = {
    'assignment': 43,
    'status': 'PASS',
    'source_state': {'binary_publication_source': SOURCE,
                     'library_revision': '2026.11.1',
                     'qualification': 'Local core library built from the final calibrated data before publication; exact binary/input hashes identify the tested artifact. Not a CI-produced binary or external DAW test.',
                     'relevant_source_files_match_publication_commit': True},
    'conditions': {'sample_rate': 48000, 'captures': 4, 'seconds_per_capture': 9,
                   'total_audio_seconds': 36, 'held_chord_seconds': 6, 'release_seconds': 3,
                   'pressure': 0, 'midi_channel': 1, 'source_entry_order_indices': [2, 0, 1],
                   'source_entry_timing': 'All three note-ons occur before the first render sample.',
                   'pedal_down_seconds': 4.5, 'all_source_note_off_seconds': 6,
                   'pedal_up_seconds': 7.5, 'factory_parameters_unchanged': True,
                   'measurement': 'Observe all eight public VoiceInfo records after every host sample; hash actual stereo float32 audio in memory, then discard it.',
                   'predeclared_bounds': {'maximum_interval_error_seconds': .0005,
                                          'maximum_gate_fraction_error': .002}},
    'checks': {'passed': sum(row['checks'] for row in converted), 'failed': 0,
               'first_generated_gate_frame': 0,
               'source_releases_stop_generation_with_pedal_down': True,
               'pedal_up_does_not_restart_pattern': True,
               'source_velocity_and_channel_identity_preserved': True,
               'generated_gates_never_pedal_held': True,
               'maximum_simultaneous_generated_held_gates': 1},
    'aggregate': {'highest_peak_dbfs': max(row['peak_dbfs'] for row in converted),
                  'worst_note_interval_error_seconds': max(row['max_period_error_seconds'] for row in converted),
                  'worst_gate_fraction_error': max(row['max_gate_fraction_error'] for row in converted),
                  'latest_voice_retirement_seconds': max(row['retired_seconds'] for row in converted),
                  'loudest_final_half_second_rms_dbfs': max(row['final_half_second_rms_dbfs'] for row in converted)},
    'programs': converted,
    'limitations': ['Generated MIDI-note identities are measured via actual engine telemetry; this does not estimate acoustic fundamental frequencies or perceptual quality.',
                    'Free-running factory rates are exercised. No host-tempo automation or DAW integration is claimed.',
                    'Voice retirement does not imply digital silence: Arp Bells retains a modern delay tail.',
                    'One deterministic chord/performance per program is bounded evidence, not arbitrary-performance or full-polyphonic headroom qualification.',
                    'No WAV or raw PCM files were saved. Audio hashes use the native little-endian IEEE float32 stereo bytes from this Linux x86-64 run.'],
    'inputs_sha256': input_hashes,
    'artifacts_sha256': {name: digest(OUT / name) for name in ('probe.cpp', 'summarize.py', 'results.csv', 'transitions.csv', 'run.log')},
    'probe_executable_sha256': digest(OUT / 'probe'),
    'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0],
    'probe_compile_command': 'c++ -std=c++20 -O2 -I src build-w09-arp-roles/probe.cpp build-audit/libcs80_core.a -o build-w09-arp-roles/probe',
}
(OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps({'status': 'PASS', 'checks': summary['checks']['passed'], 'captures': 4,
                  'audio_seconds': 36, 'summary_sha256': digest(OUT / 'summary.json')}, indent=2))
