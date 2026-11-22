import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import subprocess

out = Path('preset-audition')
out.mkdir()
capture = Path('build/cs80_preset_capture')
source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip()
if source != os.environ['GITHUB_SHA']:
    raise SystemExit('Audition source does not match the workflow SHA')
(out / 'source-commit.txt').write_text(source + '\n', encoding='utf-8')

# Retain the original family examples and include every program in
# the character refinement. Slow sounds need time for their layers
# to develop; these are actual factory levels, not normalized demos.
original_presets = {
    'Brass Swell I', 'Solo Violin', 'Clarinet', 'Electric Piano I',
    'Flute Organ 8+4', 'Analog Bass', 'Nylon Pluck', 'Marimba',
    'Slow Pad', 'Arp Poly', 'Wind', 'Pressure Lead',
}
refined_presets = {
    'Flute', 'Breathy Flute', 'Harp', 'Kalimba', 'Sine Bass', 'Upright Pluck',
    'Noir Swell Brass', 'Brass Ensemble 16', 'Neo-Noir Pad',
    'Arctic Pad', 'Synthwave Chords', 'Ether Pad',
}
selections = [
    ('standard', '0', '3', '1', [
        'Brass Swell I', 'Solo Violin', 'Clarinet', 'Electric Piano I',
        'Flute Organ 8+4', 'Analog Bass', 'Nylon Pluck', 'Marimba',
        'Arp Poly', 'Wind',
    ]),
    ('pressure', '0.65', '3', '1', ['Pressure Lead']),
    ('refined', '0', '3', '1', [
        'Flute', 'Breathy Flute', 'Harp', 'Kalimba', 'Sine Bass', 'Upright Pluck',
    ]),
    ('slow', '0', '5', '2', [
        'Slow Pad', 'Noir Swell Brass', 'Brass Ensemble 16',
        'Neo-Noir Pad', 'Arctic Pad', 'Synthwave Chords', 'Ether Pad',
    ]),
]
requested = [name for _, _, _, _, names in selections for name in names]
expected = original_presets | refined_presets
if len(requested) != len(set(requested)) or set(requested) != expected:
    raise SystemExit('Audition selection must contain every original and refined program exactly once')
expected_wav_count = len(requested)
inventory_command = [str(capture), '--inventory', str(out / 'inventory.json')]
commands = []
for folder, pressure, hold, tail, names in selections:
    command = [str(capture), '--out', str(out / folder), '--wav',
               '--rate', '48000', '--velocity', '0.8',
               '--hold', hold, '--tail', tail, '--pressure', pressure]
    for name in names:
        command.extend(['--preset', name])
    commands.append(command)

provenance = {
    'schema': 1, 'source_commit': source,
    'repository': os.environ['GITHUB_REPOSITORY'],
    'run_id': os.environ['GITHUB_RUN_ID'],
    'run_attempt': os.environ['GITHUB_RUN_ATTEMPT'],
    'platform': platform.platform(),
    'capture_binary_sha256': hashlib.sha256(capture.read_bytes()).hexdigest(),
    'inventory_command': inventory_command, 'capture_commands': commands,
    'expected_wav_count': expected_wav_count,
    'selections': [
        {'folder': folder, 'pressure': float(pressure),
         'hold_seconds': float(hold), 'tail_seconds': float(tail), 'presets': names}
        for folder, pressure, hold, tail, names in selections
    ],
    'capture_status': 'started',
}
manifest = out / 'provenance.json'
manifest.write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
try:
    # Inventory is a separate CLI mode; combining it with --out is
    # rejected instead of silently omitting the requested WAVs.
    subprocess.run(inventory_command, check=True)
    for command in commands:
        subprocess.run(command, check=True)

    inventory = json.loads((out / 'inventory.json').read_text(encoding='utf-8'))
    inventory_names = {program['name'] for program in inventory['programs']}
    if len(inventory['programs']) != 301 or len(inventory_names) != 301 or not expected <= inventory_names:
        raise RuntimeError('Expected the complete 301-program inventory, including every audition')
    for folder, pressure, hold, tail, names in selections:
        with (out / folder / 'metrics.csv').open(newline='', encoding='utf-8') as stream:
            rows = list(csv.DictReader(stream))
        wavs = list((out / folder).glob('*.wav'))
        if len(rows) != len(names) or {row['name'] for row in rows} != set(names):
            raise RuntimeError('Audition metrics do not match the requested presets')
        if len(wavs) != len(names) or any(path.stat().st_size <= 58 for path in wavs):
            raise RuntimeError('Audition WAV set is incomplete')
        for row in rows:
            settings = {
                'sample_rate': 48000, 'velocity': 0.8, 'pressure': float(pressure),
                'hold_seconds': float(hold), 'tail_seconds': float(tail), 'rank': 0,
                'nonfinite': 0, 'samples_over_full_scale': 0,
            }
            if any(not math.isfinite(float(row[key])) or abs(float(row[key]) - value) > 1e-6
                   for key, value in settings.items()):
                raise RuntimeError('Audition capture settings or finite-output checks do not match')
    wav_count = len(list(out.rglob('*.wav')))
    if wav_count != expected_wav_count:
        raise RuntimeError('Audition WAV total does not match the selection')

    (out / 'README.txt').write_text(
        f'{expected_wav_count} actual curated factory programs from the source commit in provenance.json.\n'
        'Includes the original 12 examples and every program in the character refinement.\n'
        'Stereo 48 kHz IEEE float WAVs; velocity 0.8; category-specific MIDI notes are in metrics.csv.\n'
        'Standard/refined/pressure examples hold one note for 3 seconds, then a 1-second release excerpt.\n'
        'Slow examples hold one note for 5 seconds, then a 2-second release excerpt.\n'
        'No audio normalization or clipping is applied. Long releases may continue beyond the excerpt.\n'
        'Examples use zero pressure except Pressure Lead, which uses 0.65 because it is pressure-only.\n'
        'Arp Poly is a one-key arpeggiator example; these excerpts do not exercise chord playing.\n'
        'inventory.json contains all 301 curated programs and their complete parameter values.\n'
        'CSV metrics describe software output, not acoustic-instrument similarity or full-bank coverage.\n'
        'This artifact alone does not establish that every platform and pluginval gate passed.\n',
        encoding='utf-8')
    provenance['library_revision'] = inventory['library_revision']
    provenance['capture_status'] = 'complete'
    provenance['wav_count'] = wav_count
    manifest.write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
    checksums = [
        f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.relative_to(out).as_posix()}'
        for path in sorted(out.rglob('*')) if path.is_file()
    ]
    (out / 'SHA256SUMS').write_text('\n'.join(checksums) + '\n', encoding='utf-8')
except BaseException as error:
    provenance['capture_status'] = 'failed'
    provenance['error'] = str(error)
    manifest.write_text(json.dumps(provenance, indent=2) + '\n', encoding='utf-8')
    raise
