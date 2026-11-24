#!/usr/bin/env python3
"""Reproduce A25's complete Pads identity/control audit; no audio-quality claim."""
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
BASE = ROOT / 'docs/preset-evidence/2026.11.5/baseline/inventory.json'
CAND = ROOT / 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]
old, new = [json.loads(p.read_text()) for p in (BASE, CAND)]
assert sha(BASE) == '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
ids = new['parameter_ids']
assert ids[:200] == old['parameter_ids']
identity = lambda p: (p['index'], p['host_index'], p['name'], p['category'])
assert [identity(p) for p in old['programs']] == [identity(p) for p in new['programs']]
rows = []
for before, after in zip(old['programs'], new['programs']):
    if after['category'] != 'Pads':
        continue
    changed = [ids[i] for i in range(200) if ids[i] != 'volume'
               and f32(before['values'][i]) != f32(after['values'][i])]
    assert changed, after['name']
    controls = dict(zip(ids, after['values']))
    assert controls['body1_material'] == controls['body2_material'] == 0
    active = [rank for rank in (1, 2)
              if controls[f'body{rank}_model'] and controls[f'body{rank}_mix'] > 0]
    rows.append({'host_index': after['host_index'], 'name': after['name'],
                 'prior_non_master_changes': changed, 'active_body_ranks': active,
                 'variants': [controls[f'body{rank}_variant'] for rank in (1,2)],
                 'volume': controls['volume']})
assert len(rows) == 63
assert len({tuple(f32(v) for i,v in enumerate(p['values']) if ids[i] != 'volume')
            for p in new['programs'] if p['category'] == 'Pads'}) == 63
named = {row['name']: row for row in rows}
assert named['Choir Pad']['variants'] == [3,3]
assert named['Dark Choir']['variants'] == [4,4]
for name in ('Cathedral Pad','Golden Pads 66'):
    assert abs(named[name]['volume'] - .650) < 1e-6
sources = subprocess.check_output(['git','ls-files','src/core'],cwd=ROOT).decode().splitlines()
source_hashes = {}
for path in sources:
    current = (ROOT/path).read_bytes()
    committed = subprocess.check_output(['git','show',f'{SOURCE}:{path}'],cwd=ROOT)
    assert current == committed, path
    source_hashes[path] = hashlib.sha256(current).hexdigest()
summary = {'source': SOURCE, 'scope': 'All Pads structural audit; numerical control changes do not establish perceptual quality.',
           'programs': len(rows), 'original_address_programs': sum(r['host_index']<222 for r in rows),
           'curated_programs': sum(r['host_index']>=222 for r in rows),
           'body_enabled_programs': sum(bool(r['active_body_ranks']) for r in rows),
           'circuit_only_programs': sum(not r['active_body_ranks'] for r in rows),
           'prior_non_master_change_range': [min(len(r['prior_non_master_changes']) for r in rows),max(len(r['prior_non_master_changes']) for r in rows)],
           'baseline_sha256': sha(BASE), 'candidate_inventory_sha256': sha(CAND),
           'program_rows': rows, 'core_source_sha256': source_hashes,
           'executables': {name: sha(ROOT/'build-preset'/name) for name in ('cs80_pad_poly_refinement_tests','cs80_vocal_body_tests')}}
(OUT/'inventory-audit.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({k:v for k,v in summary.items() if k not in ('program_rows','core_source_sha256','executables')},indent=2))
