#!/usr/bin/env python3
"""Independent factual cross-checks for assignment 45; no audio is rendered."""
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
D = ROOT / 'docs/preset-evidence/2026.11.1'
checks = []

def check(label, actual, expected):
    checks.append({'claim': label, 'actual': actual, 'expected': expected,
                   'pass': actual == expected})

def read(path):
    return json.loads((ROOT / path).read_text())

def fbits(values):
    return b''.join(struct.pack('<f', x) for x in values)

manifest = read('docs/preset-evidence/2026.11.1/provenance.json')
tracked = set(subprocess.check_output(['git', 'ls-files'], cwd=ROOT, text=True).splitlines())
manifest_issues = []
for entry in manifest['files']:
    path = D / entry['path']
    raw = path.read_bytes()
    if hashlib.sha256(raw).hexdigest() != entry['stored_sha256']:
        manifest_issues.append(entry['path'] + ': stored SHA-256 mismatch')
    if len(raw) != entry['bytes']:
        manifest_issues.append(entry['path'] + ': size mismatch')
    if path.relative_to(ROOT).as_posix() not in tracked:
        manifest_issues.append(entry['path'] + ': not tracked')
check('Dossier file records', len(manifest['files']), 81)
check('Dossier stored hashes, sizes and Git tracking', manifest_issues, [])

old = read('build-baseline/inventory.json')
new = read('build-audit/inventory-final.json')
check('Circuit inventory entries', len(new['programs']), 301)
check('Circuit parameter IDs', len(new['parameter_ids']), 190)
check('Parameter-ID order preserved', new['parameter_ids'], old['parameter_ids'])
check('Init remains final program', new['programs'][-1]['name'], 'Init Circuit')
expected_categories = {'Brass': 33, 'Strings': 31, 'Pads': 32, 'Polysynth': 25,
                       'Keys': 25, 'Organ': 15, 'Winds': 14, 'Plucked': 17,
                       'Bass': 25, 'Leads': 32, 'Bells': 23, 'FX': 24,
                       'Sequences': 4, 'Init': 1}
check('Guide category counts', dict(Counter(p['category'] for p in new['programs'])), expected_categories)
changed_vectors, changed_records, changed_descriptions, changed_values = [], [], [], []
for before, after in zip(old['programs'], new['programs']):
    assert before['name'] == after['name']
    name = after['name']
    if fbits(before['values']) != fbits(after['values']):
        changed_vectors.append(name)
    if before != after:
        changed_records.append(name)
    if before['description'] != after['description']:
        changed_descriptions.append(name)
    for pid, a, b in zip(new['parameter_ids'], before['values'], after['values']):
        if fbits([a]) != fbits([b]):
            changed_values.append({'program': name, 'parameter': pid, 'before': a, 'after': b})
check('Changed Circuit value vectors', len(changed_vectors), 12)
check('Unchanged Circuit value vectors', 301 - len(changed_vectors), 289)
check('Unchanged complete Circuit records', 301 - len(changed_records), 288)
check('Changed descriptions', len(changed_descriptions), 5)
check('Changed voicing floats', sum(v['parameter'] != 'volume' for v in changed_values), 38)
check('Changed master levels', sum(v['parameter'] == 'volume' for v in changed_values), 4)
check('UFO Landing metadata only', 'UFO Landing' in changed_descriptions and 'UFO Landing' not in changed_vectors, True)

plucks = read('docs/preset-evidence/2026.11.1/plucks/measurements.json')
for name, expected in [('Harp', [8.85, 8.99]), ('Kalimba', [8.08, 8.21])]:
    values = [w['improvement_db'] for row in plucks['rows'] if row['name'] == name
              for w in row['windows'] if w['begin'] == .004 and w['end'] == .02]
    check(name + ' documented 4–20 ms improvement bounds', [round(min(values), 2), round(max(values), 2)], expected)
kalimba = next(row for row in plucks['long'] if row['name'] == 'Kalimba' and row['bank'] == 'candidate')
check('Kalimba documented early/late strike ratios', [round(kalimba['windows'][i]['strike_body_db'], 2) for i in (0, 1)], [-18.07, -51.51])

paths = [
    'README.md', 'docs/PRESET_GUIDE.md', 'docs/PRESET_REFINEMENT_2026_11_1.md',
    'docs/preset-evidence/2026.11.1/README.md', 'docs/preset-evidence/2026.11.1/provenance.json',
    'build-baseline/inventory.json', 'build-audit/inventory-final.json',
    'build-w05-native-recall/summary.json', 'build-w06-sample-rates/summary.json',
    'build-w06-final-auditions/summary.json', 'build-w06-performance-controls/summary.json',
    'build-w07-modern-role/summary.json', 'build-w08-long-sustain/summary.json',
    'build-w08-rapid-strikes/summary.json', 'build-w09-arp-roles/summary.json',
    'build-w09-phrase-gates/summary.json',
]
inputs = [{'path': p, 'sha256': hashlib.sha256((ROOT / p).read_bytes()).hexdigest()} for p in paths]
result = {
    'assignment': 45,
    'kind': 'factual editorial audit, not a new sound-quality or listening test',
    'status': 'PASS' if all(c['pass'] for c in checks) else 'FAIL',
    'created_utc': datetime.now(timezone.utc).isoformat(),
    'observed_checkout_head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
    'public_docs_modified': False,
    'blocking_factual_defects': [],
    'automatic_cross_checks': {'passed': sum(c['pass'] for c in checks), 'failed': sum(not c['pass'] for c in checks), 'checks': checks},
    'changed_programs': changed_vectors,
    'changed_descriptions': changed_descriptions,
    'changed_values': changed_values,
    'input_sha256': inputs,
    'manual_review': 'See recommended-wording.md for claim-to-evidence review, qualifications, and exact wording.',
    'limits': [
        'No acoustic reference, physical CS-80 measurement, listening panel or new audio execution was performed by this editorial review.',
        'Local summaries and committed dossier were checked; CI completion, release assets and download availability are not certified here.',
        'External VST3-boundary evidence was still pending at this review cut; native JUCE fixtures are described separately.',
        'The 81 stored dossier hashes and sizes are verified. Original hashes are provenance assertions unless the original is still available.',
        'Private raw audio may have been pruned after hashes and measurements were recorded; historical authoring snapshots are not rewritten.',
    ],
}
(OUT / 'summary.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps({'status': result['status'], 'passed': result['automatic_cross_checks']['passed'], 'failed': result['automatic_cross_checks']['failed'], 'dossier_records': len(manifest['files']), 'failed_claims': [c['claim'] for c in checks if not c['pass']]}))
if result['status'] != 'PASS':
    raise SystemExit(1)
