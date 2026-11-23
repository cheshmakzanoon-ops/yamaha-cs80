#!/usr/bin/env python3
"""Capture reproducible, unnormalized examples of the existing circuit library."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess

KEYS = [
    'Electric Piano I', 'Electric Piano II', 'Tine Piano', 'Clavichord I',
    'Clavichord II', 'Funky Clavichord', 'Harpsichord I', 'Harpsichord II',
    'Lute Harpsichord', 'Toy Piano', 'Honky Keys', 'Vibes Keys', 'Celesta',
    'Saw Pulse Piano', 'Soft Keys', 'Chorus Keys', 'Stage Keys', 'Velocity Keys',
    'Dulcimer', 'Reed Piano', 'Glass Keys', 'Tremolo Keys', 'Contour Keys', 'Pressure Keys',
]


def selections():
    groups = [
        dict(folder='keyboards', names=KEYS, velocity=.85, pressure=0, hold=4, tail=1),
        dict(folder='soft-keyboards', names=['Electric Piano I', 'Electric Piano II', 'Tine Piano',
             'Reed Piano', 'Stage Keys'], velocity=.3, pressure=0, hold=4, tail=1),
        dict(folder='families', names=['Brass Swell I', 'Sforzando Brass', 'Covered Horn', 'Solo Violin',
             'Solo Cello', 'Marcato Strings', 'Recorder', 'Clarinet', 'Flute Organ 8+4', 'Tremolo Organ',
             'Rubber Bass', 'Picked Bass', 'Nylon Pluck', 'Mandolin', 'Vibraphone', 'Marimba',
             'Tremolo Brass', 'Tremolo Pad', 'Tremolo Poly', 'Touch Lead', 'Aftertouch Vibrato Lead',
             'Glide Lead'], velocity=.85, pressure=0, hold=7, tail=2),
        dict(folder='original-addresses', names=['Tone I: E. Piano', 'Tone II: Bass', 'Analog Piano',
             'Bright Keys', 'Amber Keys 97', 'Slow Keys 106', 'Replicant Brass', 'Bass Stack'],
             velocity=.85, pressure=0, hold=4, tail=1),
        dict(folder='pressure-zero', names=['Aftertouch Vibrato Lead', 'Pressure Lead', 'Wah Lead'],
             velocity=.85, pressure=0, hold=5, tail=1),
        dict(folder='pressure-full', names=['Aftertouch Vibrato Lead', 'Pressure Lead', 'Wah Lead'],
             velocity=.85, pressure=1, hold=5, tail=1),
    ]
    for note in (48, 60, 84):
        groups.append(dict(folder=f'tine-register-{note}', names=['Tine Piano'], velocity=.85,
                           pressure=0, hold=3, tail=1, note=note))
    return groups


def capture_auditions(capture: Path, out: Path, source: str):
    out.mkdir(parents=True, exist_ok=False)
    groups = selections()
    expected_count = sum(len(group['names']) for group in groups)
    inventory_cmd = [str(capture), '--library', 'all', '--inventory', str(out / 'inventory.json')]
    commands = []
    for group in groups:
        command = [str(capture), '--library', 'all', '--out', str(out / group['folder']), '--wav',
                   '--rate', '48000', '--velocity', str(group['velocity']), '--pressure', str(group['pressure']),
                   '--hold', str(group['hold']), '--tail', str(group['tail'])]
        if 'note' in group:
            command += ['--note', str(group['note'])]
        for name in group['names']:
            command += ['--preset', name]
        commands.append(command)
    provenance = dict(schema=2, source_commit=source, repository=os.environ.get('GITHUB_REPOSITORY'),
                      run_id=os.environ.get('GITHUB_RUN_ID'), run_attempt=os.environ.get('GITHUB_RUN_ATTEMPT'),
                      platform=platform.platform(), capture_binary_sha256=hashlib.sha256(capture.read_bytes()).hexdigest(),
                      inventory_command=inventory_cmd, capture_commands=commands, selections=groups,
                      expected_wav_count=expected_count, capture_status='started')
    manifest = out / 'provenance.json'
    def save():
        manifest.write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
    (out / 'source-commit.txt').write_text(source + '\n', encoding='utf-8')
    save()
    try:
        subprocess.run(inventory_cmd, check=True)
        inventory = json.loads((out / 'inventory.json').read_text(encoding='utf-8'))
        programs = inventory['programs']
        if len(programs) != 523 or [p['host_index'] for p in programs] != list(range(523)):
            raise RuntimeError('Expected all 523 existing factory addresses in order')
        names = {p['name'] for p in programs}
        if len(names) != 523 or not {n for g in groups for n in g['names']} <= names:
            raise RuntimeError('Audition roster contains missing or ambiguous names')
        for group, command in zip(groups, commands):
            subprocess.run(command, check=True)
            directory = out / group['folder']
            with (directory / 'metrics.csv').open(newline='', encoding='utf-8') as f:
                rows = list(csv.DictReader(f))
            wavs = list(directory.glob('*.wav'))
            if len(rows) != len(group['names']) or {r['name'] for r in rows} != set(group['names']):
                raise RuntimeError('Capture rows do not match selected programs')
            if len(wavs) != len(rows) or any(p.stat().st_size <= 58 for p in wavs):
                raise RuntimeError('Missing or empty audition WAV')
            for row in rows:
                expected = dict(sample_rate=48000, velocity=group['velocity'], pressure=group['pressure'],
                                hold_seconds=group['hold'], tail_seconds=group['tail'], rank=0,
                                nonfinite=0, samples_over_full_scale=0)
                if 'note' in group:
                    expected['note'] = group['note']
                if any(not math.isfinite(float(row[k])) or abs(float(row[k]) - v) > 1e-6 for k, v in expected.items()):
                    raise RuntimeError('Capture settings or audio safety checks differ')
        if len(list(out.rglob('*.wav'))) != expected_count:
            raise RuntimeError('Audition count differs from manifest')
        provenance.update(capture_status='complete', library_revision=inventory['library_revision'],
                          wav_count=expected_count)
        save()
        (out / 'README.txt').write_text(
            f'{expected_count} unnormalized circuit-synth auditions from the recorded source commit.\n'
            '24 existing keyboards at firm velocity, five soft-strike comparisons, 22 refined instrument examples,\n'
            'eight revoiced original addresses, three paired pressure examples, and Tine Piano at MIDI notes 48, 60 and 84.\n'
            'Pressure Lead is intentionally silent in pressure-zero and sounds in pressure-full.\n'
            '48 kHz stereo float WAV. Each folder has exact settings and raw measurements in metrics.csv.\n'
            'The complete 523-program parameter inventory is included. No presets were added.\n'
            'Single-note examples do not establish acoustic similarity, chord behavior or host validation.\n'
            'Long releases can continue beyond the excerpt; no normalization or clipping was applied.\n', encoding='utf-8')
        (out / 'SHA256SUMS').write_text(''.join(
            f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.relative_to(out).as_posix()}\n'
            for p in sorted(out.rglob('*')) if p.is_file()), encoding='utf-8')
    except BaseException as error:
        provenance.update(capture_status='failed', error=str(error))
        save()
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture', required=True, type=Path)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
    if os.environ.get('GITHUB_SHA', source) != source:
        raise SystemExit('Audition checkout differs from workflow source')
    capture_auditions(args.capture.resolve(), args.out.resolve(), source)


if __name__ == '__main__':
    main()
