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
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SAMPLE_RATE = 48000

KEYS = [
    'Electric Piano I', 'Electric Piano II', 'Tine Piano', 'Clavichord I',
    'Clavichord II', 'Funky Clavichord', 'Harpsichord I', 'Harpsichord II',
    'Lute Harpsichord', 'Toy Piano', 'Honky Keys', 'Vibes Keys', 'Celesta',
    'Saw Pulse Piano', 'Soft Keys', 'Chorus Keys', 'Stage Keys', 'Velocity Keys',
    'Dulcimer', 'Reed Piano', 'Glass Keys', 'Tremolo Keys', 'Contour Keys', 'Pressure Keys',
]
PRESSURE_COMPARISONS = [
    'Aftertouch Vibrato Lead', 'Pressure Lead', 'Wah Lead',
    'Pressure Pad', 'Flute', 'Solo Violin', 'Pressure Keys',
]
SUSTAINED_C4 = [
    'Solo Violin', 'Flute', 'Clarinet', 'Oboe', 'English Horn',
    'Covered Horn', 'Muted Trumpet',
]
SUSTAINED_BASS_C4 = ['Solo Cello', 'Bass Clarinet', 'Bassoon']
MATERIAL_PROFILES = [
    'Tubular Bells', 'Gong', 'Temple Bell', 'Agogo', 'Music Box',
    'Banjo', 'Koto', 'Harp', 'Kalimba',
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
        dict(folder='pressure-zero', names=list(PRESSURE_COMPARISONS),
             velocity=.85, pressure=0, hold=5, tail=1),
        dict(folder='pressure-full', names=list(PRESSURE_COMPARISONS),
             velocity=.85, pressure=1, hold=5, tail=1),
        dict(folder='instrument-bodies', names=['Trombone Glide', 'Theatre Organ', 'Oboe', 'Bassoon',
             'Guitar I', 'Guitar II', 'Kalimba'], velocity=.85, pressure=0, hold=6, tail=2),
        dict(folder='textures-and-effects', names=['Evolving Pad', 'Breathing Pad', 'Thunder',
             'Rain', 'Surf', 'Wind'], velocity=.85, pressure=0, hold=7, tail=2),
        dict(folder='soft-kalimba', names=['Kalimba'], velocity=.3, pressure=0, hold=4, tail=1),
    ]
    for note in (48, 60, 84):
        groups.append(dict(folder=f'tine-register-{note}', names=['Tine Piano'], velocity=.85,
                           pressure=0, hold=3, tail=1, note=note))
    # Five held seconds expose the bass piano's prompt sound and aftersound.
    # These matched requests differ from the shorter keyboard overview clips.
    for note in (36, 60, 84):
        for label, velocity in (('soft', .3), ('firm', .85)):
            groups.append(dict(folder=f'piano-register-{note}-{label}',
                               names=['Analog Piano', 'Stage Keys'], velocity=velocity,
                               pressure=0, hold=5, tail=1, note=note))
    for label, velocity in (('soft', .3), ('firm', .85)):
        groups.append(dict(folder=f'sustained-effort-{label}-c4', names=list(SUSTAINED_C4),
                           velocity=velocity, pressure=0, hold=6, tail=1, note=60))
        # The main body is at 16 feet in these three existing presets. Key 72
        # therefore matches the C4 foundation of the 8-foot programs above.
        # Complete presets retain their own upper layers and other controls.
        groups.append(dict(folder=f'sustained-bass-effort-{label}-c4',
                           names=list(SUSTAINED_BASS_C4), velocity=velocity,
                           pressure=0, hold=6, tail=1, note=72))
    # Equal played keys examine corresponding clarinet fingering registers;
    # Bass Clarinet's 16-foot foundation sounds one octave below Clarinet.
    for note in (60, 64, 68, 72):
        for label, velocity in (('soft', .3), ('firm', .85)):
            groups.append(dict(folder=f'clarinet-register-{note}-{label}',
                               names=['Clarinet', 'Bass Clarinet'], velocity=velocity,
                               pressure=0, hold=3, tail=1, note=note))
    # Matched gestures expose material spectra, contact and independent decay.
    # These are the existing programs with their revised resonators, not new
    # factory addresses or post-normalized demonstrations.
    for note in (48, 60, 72):
        for label, velocity in (('soft', .25), ('firm', .85)):
            groups.append(dict(folder=f'mallet-material-{note}-{label}',
                               names=['Xylophone', 'Steel Drum', 'Marimba', 'Vibraphone'],
                               velocity=velocity, pressure=0, hold=3.5, tail=.8, note=note))
        groups.append(dict(folder=f'choir-vowels-{note}', names=['Choir Pad', 'Dark Choir'],
                           velocity=.85, pressure=0, hold=6, tail=1, note=note))
    groups.append(dict(folder='original-articulation',
                       names=['Frozen Leads 121', 'Soft Leads 128', 'Neon Sequences 174'],
                       velocity=.85, pressure=0, hold=5, tail=1, note=60))
    # Append matched excerpts for the revised bell, string and tine materials;
    # the preceding overview and comparison requests stay unchanged.
    for note in (48, 60, 72):
        for label, velocity in (('soft', .25), ('firm', .85)):
            groups.append(dict(folder=f'material-profile-{note}-{label}',
                               names=list(MATERIAL_PROFILES), velocity=velocity,
                               pressure=0, hold=3.5, tail=1, note=note))
    # Keep all 225 earlier requests intact. Stage Keys already has five-second
    # soft/firm holds at MIDI 36; append its missing 48s and Soft Keys at both.
    for note in (36, 48):
        for label, velocity in (('soft', .3), ('firm', .85)):
            groups.append(dict(folder=f'low-piano-tail-{note}-{label}',
                               names=['Soft Keys'] if note == 36 else ['Stage Keys', 'Soft Keys'],
                               velocity=velocity, pressure=0, hold=5, tail=1, note=note))
    # Complete the five existing vocal pads at main-body MIDI 36 and 48.
    # Choir Pad at 48 and Dark Choir at played 48/60 are already captured above.
    # The latter and its two original-address derivatives have 16-foot rank I;
    # played keys 48/60 put that foundation at sounding MIDI 36/48.
    for foundation in (36, 48):
        groups.append(dict(folder=f'low-choir-ah-foundation-{foundation}',
                           names=['Choir Pad', 'Golden Pads 52'] if foundation == 36 else ['Golden Pads 52'],
                           velocity=.85, pressure=0, hold=6, tail=1, note=foundation))
        groups.append(dict(folder=f'low-choir-oo-foundation-{foundation}',
                           names=['Blade Choir', 'Midnight Pads 55'],
                           velocity=.85, pressure=0, hold=6, tail=1, note=foundation + 12))
    return groups


def roster_summary(groups):
    """Validate render requests before creating directories or invoking children."""
    folders, requests, names = set(), set(), set()
    count = 0
    durations = []
    for group in groups:
        folder = group['folder']
        if not isinstance(folder, str) or re.fullmatch(r'[a-z0-9]+(?:-[a-z0-9]+)*', folder) is None:
            raise RuntimeError('Audition folder must be a simple relative name')
        if folder in folders:
            raise RuntimeError('Duplicate audition folder')
        folders.add(folder)
        selected = group['names']
        if not selected or any(not isinstance(name, str) or not name.strip() for name in selected):
            raise RuntimeError('Audition selections must contain named programs')
        if len(set(selected)) != len(selected):
            raise RuntimeError('Duplicate program within an audition folder')
        for key, low, high in (('velocity', 0, 1), ('pressure', 0, 1), ('hold', .05, 30), ('tail', 0, 30)):
            value = group[key]
            if not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
                raise RuntimeError('Invalid audition setting: ' + key)
        note = group.get('note')
        if note is not None and (type(note) is not int or not 0 <= note <= 127):
            raise RuntimeError('Invalid audition MIDI note')
        for name in selected:
            request = (name, note, group['velocity'], group['pressure'], group['hold'], group['tail'])
            if request in requests:
                raise RuntimeError('Duplicate audition render request')
            requests.add(request)
        names.update(selected)
        count += len(selected)
        durations.append(len(selected) * (group['hold'] + group['tail']))
    if not count:
        raise RuntimeError('Empty audition roster')
    return dict(wav_count=count, selected_program_count=len(names), audio_seconds=math.fsum(durations))


def validate_resolved_requests(groups, programs):
    """Reject aliases between category defaults and explicit MIDI notes."""
    by_name = {program['name']: program for program in programs}
    requests = set()
    for group in groups:
        for name in group['names']:
            category = by_name[name].get('category')
            if not isinstance(category, str) or not category:
                raise RuntimeError('Audition program has an invalid category')
            # These are preset_capture.cpp's category defaults. The explicit
            # note and the default key must identify the same render request.
            default_note = (48 if category == 'Bass' else 67 if category in ('Winds', 'Leads')
                            else 72 if category == 'Bells' else 60)
            note = group.get('note', default_note)
            request = (name, note, group['velocity'], group['pressure'], group['hold'], group['tail'])
            if request in requests:
                raise RuntimeError('Duplicate audition render request after resolving the default note')
            requests.add(request)


def verify_source(source: str, repo: Path | None = None):
    """Reject staged/unstaged tracked edits and a checkout that moved during capture."""
    repo = ROOT if repo is None else repo
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=repo, text=True).strip()
    if head != source:
        raise RuntimeError('Audition checkout changed from the recorded source commit')
    if os.environ.get('GITHUB_SHA', source) != source:
        raise RuntimeError('Audition checkout differs from workflow source')
    dirty = subprocess.check_output(
        ['git', 'status', '--porcelain=v1', '--untracked-files=no'], cwd=repo, text=True)
    if dirty:
        raise RuntimeError('Audition capture requires a clean tracked source tree; commit or restore tracked edits first')
    tree = subprocess.check_output(['git', 'rev-parse', 'HEAD^{tree}'], cwd=repo, text=True).strip()
    return dict(source_commit=head, source_tree=tree, tracked_source_clean=True)


def audition_readme(groups, summary, source, revision):
    lines = [
        f"{summary['wav_count']} unnormalized circuit-synth auditions of {summary['selected_program_count']} existing programs.",
        f"{summary['audio_seconds']:g} seconds total; library revision {revision}; source commit {source}.",
        '48 kHz stereo float WAV. Each folder has exact settings and raw measurements in metrics.csv.',
        'The complete 523-program parameter inventory is included. No presets were added.',
        '',
        'Listening folders:',
    ]
    for group in groups:
        count = len(group['names'])
        note = str(group['note']) if 'note' in group else 'category default (see metrics.csv)'
        lines.append(f"- {group['folder']}: {count} WAVs, {count * (group['hold'] + group['tail']):g} seconds; "
                     f"velocity {group['velocity']:g}, pressure {group['pressure']:g}, MIDI note {note}; "
                     f"hold {group['hold']:g}s, release excerpt {group['tail']:g}s.")
    lines += [
        '',
        'Compare firm keyboard and instrument-body examples with soft-keyboards and soft-kalimba.',
        'Pressure-zero and pressure-full use the same programs, strike velocities and durations.',
        'Pressure Lead is intentionally silent in pressure-zero and sounds in pressure-full.',
        'The instrument-bodies folder includes the revised slide-brass, tremulant, woodwind and plucked-string voices.',
        'Textures-and-effects includes evolving/breathing pads and the distinct thunder, rain, surf and wind layers.',
        'The Tine Piano register folders compare MIDI notes 48, 60 and 84.',
        'Piano-register folders compare Analog Piano and Stage Keys at MIDI 36, 60 and 84, each at velocities 0.3 and 0.85.',
        'Their five-second holds expose the bass attack and aftersound before a one-second key-release excerpt.',
        'Sustained-effort folders compare all six expressive body families at velocities 0.3 and 0.85 with pressure zero.',
        'The c4 folders share a C4 main-body foundation: the 8-foot presets use MIDI 60; Solo Cello, Bass Clarinet and Bassoon use MIDI 72 for their 16-foot bodies.',
        'These complete presets retain their own upper layers, filters and envelopes; same-foundation comparisons do not isolate resonant size alone.',
        'Clarinet-register folders compare played MIDI keys 60, 64, 68 and 72 at both velocities; Bass Clarinet sounds one octave below Clarinet at those equal keys.',
        'Material-profile folders compare Tubular Bells, Gong, Temple Bell, Agogo, Music Box, Banjo, Koto, Harp and Kalimba at MIDI 48, 60 and 72, each at velocities 0.25 and 0.85 with pressure zero.',
        'Their matched 3.5-second holds and one-second key-release excerpts expose each existing voice\'s strike, material spectrum and decay without normalization.',
        'Low-piano-tail folders extend the five-second held-tail comparisons to Soft Keys at MIDI 36 and 48 and Stage Keys at MIDI 48, each at velocities 0.3 and 0.85; the existing piano-register-36 folders supply the matched Stage Keys examples.',
        'These low notes expose the evolving upper-partial decay above the longer-lived piano foundation before the one-second key-release excerpt.',
        'Low-choir folders complete five existing vocal pads at main-body sounding MIDI 36 and 48: Choir Pad and Golden Pads 52 use 8-foot Ah bodies; Dark Choir, Blade Choir and Midnight Pads 55 use 16-foot Oo foundations with lighter 8-foot upper ranks.',
        'The Ah requests play MIDI 36/48 and the Oo requests play MIDI 48/60, all at velocity 0.85, pressure zero, six-second holds and one-second release excerpts. Choir Pad at played 48 and Dark Choir at played 48/60 remain in the existing choir-vowels folders and are not duplicated.',
        'These low-foundation examples expose the upper vocal bands at low pitch while retaining each complete preset\'s filters, upper layers and effects; they do not isolate vowels or establish acoustic similarity.',
        'The original 225 requests are unchanged; 13 low-register captures are appended. Existing sustained-effort folders already provide soft/firm wind comparisons.',
        'The capture CLI does not schedule pitch bends or select render quality. These fixed-note, 48 kHz clips do not verify bend behavior or noise consistency across quality modes; those require the dedicated DSP tests.',
        '',
        'Tracked sources are checked against the recorded commit before and after capture; the executable hash must also stay unchanged.',
        'Provenance records the executable SHA256, source tree, exact commands and CI run identity when available.',
        'Single-note examples do not establish acoustic similarity, chord behavior or host validation.',
        'Long releases can continue beyond the excerpt. The export applies no normalization or additional clipping.',
    ]
    return '\n'.join(lines) + '\n'


def capture_auditions(capture: Path, out: Path, source: str):
    groups = selections()
    summary = roster_summary(groups)
    initial_source = verify_source(source)
    binary_digest = hashlib.sha256(capture.read_bytes()).hexdigest()
    out.mkdir(parents=True, exist_ok=False)
    expected_count = summary['wav_count']
    inventory_cmd = [str(capture), '--library', 'all', '--inventory', str(out / 'inventory.json')]
    commands = []
    for group in groups:
        command = [str(capture), '--library', 'all', '--out', str(out / group['folder']), '--wav',
                   '--rate', str(SAMPLE_RATE), '--velocity', str(group['velocity']), '--pressure', str(group['pressure']),
                   '--hold', str(group['hold']), '--tail', str(group['tail'])]
        if 'note' in group:
            command += ['--note', str(group['note'])]
        for name in group['names']:
            command += ['--preset', name]
        commands.append(command)
    provenance = dict(schema=3, source_commit=source, source_tree=initial_source['source_tree'],
                      tracked_source_clean_before_capture=True, tracked_source_clean_after_capture=False,
                      repository=os.environ.get('GITHUB_REPOSITORY'),
                      run_id=os.environ.get('GITHUB_RUN_ID'), run_attempt=os.environ.get('GITHUB_RUN_ATTEMPT'),
                      platform=platform.platform(), capture_binary_sha256=binary_digest,
                      inventory_command=inventory_cmd, capture_commands=commands, selections=groups,
                      selected_program_count=summary['selected_program_count'],
                      expected_audio_seconds=summary['audio_seconds'],
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
        validate_resolved_requests(groups, programs)
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
                expected = dict(sample_rate=SAMPLE_RATE, velocity=group['velocity'], pressure=group['pressure'],
                                hold_seconds=group['hold'], tail_seconds=group['tail'], rank=0,
                                nonfinite=0, samples_over_full_scale=0)
                if 'note' in group:
                    expected['note'] = group['note']
                if any(not math.isfinite(float(row[k])) or abs(float(row[k]) - v) > 1e-6 for k, v in expected.items()):
                    raise RuntimeError('Capture settings or audio safety checks differ')
        if len(list(out.rglob('*.wav'))) != expected_count:
            raise RuntimeError('Audition count differs from manifest')
        if verify_source(source) != initial_source:
            raise RuntimeError('Audition source tree changed during capture')
        if hashlib.sha256(capture.read_bytes()).hexdigest() != binary_digest:
            raise RuntimeError('Capture executable changed during audition capture')
        provenance.update(capture_status='complete', library_revision=inventory['library_revision'],
                          tracked_source_clean_after_capture=True,
                          wav_count=expected_count, audio_seconds=summary['audio_seconds'])
        save()
        (out / 'README.txt').write_text(
            audition_readme(groups, summary, source, inventory['library_revision']), encoding='utf-8')
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
    source = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if os.environ.get('GITHUB_SHA', source) != source:
        raise SystemExit('Audition checkout differs from workflow source')
    capture_auditions(args.capture.resolve(), args.out.resolve(), source)


if __name__ == '__main__':
    main()
