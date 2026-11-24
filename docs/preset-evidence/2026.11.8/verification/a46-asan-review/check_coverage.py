#!/usr/bin/env python3
"""Independent read-only binary32 inventory scope crosscheck; no audio claims."""
import hashlib
import json
from pathlib import Path
import struct

ROOT = Path('/tmp/cs80-614c8ce056b7-notes')
OUT = Path(__file__).resolve().parent
paths = [ROOT / 'coverage-recovery/development/published-4-inventory.json',
         ROOT / 'coverage-recovery/verification/a12/inventory.json',
         ROOT / 'a53/recovery-independent-inventory.json']
raw = [path.read_bytes() for path in paths]
old, previous, current = [json.loads(data) for data in raw]
reported = json.loads((ROOT / 'coverage-recovery/fresh-coverage.json').read_bytes())


def require(ok, message):
    if not ok:
        raise ValueError(message)


def f32(value):
    return struct.pack('<f', value)


require([doc['library_revision'] for doc in (old, previous, current)] == ['2026.11.4', '2026.11.7', '2026.11.8'], 'Revisions differ')
require(all(len(doc['programs']) == 523 for doc in (old, previous, current)), 'Program count differs')
require(previous['parameter_ids'] == current['parameter_ids'] and len(current['parameter_ids']) == 204, 'Current IDs differ')
require(len(old['parameter_ids']) == 200, 'Old ID count differs')
old_ids = set(old['parameter_ids']) - {'volume'}
base = {'ch1_v_level', 'ch2_v_level'}
vectors, mechanisms, old_changes, beyond, init = set(), set(), set(), set(), set()
ranks = 0
for a, b, c in zip(old['programs'], previous['programs'], current['programs']):
    require((a['index'], a['host_index'], a['name']) == (b['index'], b['host_index'], b['name'])
            == (c['index'], c['host_index'], c['name']), 'Program identity changed')
    require(len(a['values']) == 200 and len(b['values']) == len(c['values']) == 204, 'Vector length differs')
    index = c['host_index']
    va, vb, vc = [dict(zip(doc['parameter_ids'], row['values']))
                  for doc, row in ((old, a), (previous, b), (current, c))]
    if any(f32(vb[k]) != f32(vc[k]) for k in current['parameter_ids']):
        vectors.add(index)
    changed = {k for k in old_ids if f32(va[k]) != f32(vc[k])}
    if changed:
        old_changes.add(index)
    if changed - base:
        beyond.add(index)
    for rank in (1, 2):
        model, material, mix = [vc[f'body{rank}_{p}'] for p in ('model', 'material', 'mix')]
        if mix > 0 and (model in (1, 3) or model == 2 and material != 6):
            mechanisms.add(index)
            ranks += 1
    if c['category'] == 'Init':
        init.add(index)
require(not (init & (vectors | mechanisms | old_changes)), 'Init changed or enters mechanism set')
counts = {'programs': 523, 'musical': 523 - len(init), 'init': len(init),
          'changed_vectors_since_7': len(vectors), 'piano_ep_mechanism_programs': len(mechanisms),
          'piano_ep_mechanism_ranks': ranks, 'overlap': len(vectors & mechanisms),
          'directly_affected_unique_programs': len(vectors | mechanisms),
          'changed_old_nonmaster_controls_since_4': len(old_changes),
          'changed_other_old_controls_beyond_base_vca_since_4': len(beyond),
          'base_vca_only_among_old_nonmaster_controls_since_4': len(old_changes - beyond)}
require(counts == reported['counts'], 'Independent counts differ from fresh coverage report')
records = [{'file': str(path), 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
            'git_blob': hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()}
           for path, data in zip(paths, raw)]
result = {'scope': 'Independent binary32 parameter comparison and declared mechanism coverage only; no audio/native qualification.',
          'source': reported['source_commit'], 'counts': counts, 'inputs': records,
          'changed_vectors': sorted(vectors), 'mechanism_programs': sorted(mechanisms),
          'init_indices': sorted(init), 'release_qualified': False}
with (OUT / 'coverage-crosscheck.json').open('x') as stream:
    json.dump(result, stream, indent=2)
    stream.write('\n')
print(json.dumps(counts))
