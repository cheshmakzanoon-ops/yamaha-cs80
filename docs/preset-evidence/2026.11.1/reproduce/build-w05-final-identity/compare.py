#!/usr/bin/env python3
"""Compare complete compiled records; inventory decimals are checked against float bits."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess

OUT = Path(__file__).resolve().parent
ROOT = OUT.parent
BASE = "7f55a95a7534b389927defe46866456560b3a878"

def sha(data):
    return hashlib.sha256(data).hexdigest()

def file_sha(path):
    return sha(path.read_bytes())

def read_bank(path):
    raw = path.read_bytes()
    pos = 0
    def uint64():
        nonlocal pos
        value = struct.unpack_from('<Q', raw, pos)[0]
        pos += 8
        return value
    def string():
        nonlocal pos
        length = uint64()
        value = raw[pos:pos+length].decode('utf-8')
        pos += length
        return value
    count, parameters = uint64(), uint64()
    records = []
    for _ in range(count):
        p = {'name': string(), 'category': string(), 'description': string(),
             'library': uint64(), 'tags': [string() for _ in range(uint64())]}
        p['bits'] = list(struct.unpack_from(f'<{parameters}I', raw, pos))
        p['values'] = list(struct.unpack_from(f'<{parameters}f', raw, pos))
        pos += parameters * 4
        records.append(p)
    assert pos == len(raw), (path, pos, len(raw))
    return parameters, records

inventories = {label: json.loads((OUT / f'{label}-inventory.json').read_text())
               for label in ('baseline', 'final')}
contracts = {label: json.loads((OUT / f'{label}-contract.json').read_text())
             for label in ('baseline', 'final')}
assert contracts['baseline'] == contracts['final'] == {
    'legacy_count': 222, 'curated_count': 301, 'host_count': 523,
    'core_parameters': 190, 'memory_controls': 26,
    'host_parameter_count_by_layout': 296, 'midi_cases': 49152, 'index_cases': 830}

banks = {}
for label in ('baseline', 'final'):
    banks[label] = {}
    for family in ('legacy', 'curated', 'host'):
        parameter_count, records = read_bank(OUT / label / f'{family}.bin')
        assert parameter_count == 190
        banks[label][family] = records
    assert banks[label]['host'] == banks[label]['legacy'] + banks[label]['curated']
    assert len(inventories[label]['programs']) == 301
    assert len(inventories[label]['parameter_ids']) == 190
    for index, (p, inv) in enumerate(zip(banks[label]['curated'], inventories[label]['programs'])):
        assert inv['index'] == index and inv['host_index'] == index + 222
        assert p['library'] == 1
        for field in ('name', 'category', 'description', 'tags'):
            assert p[field] == inv[field], (label, p['name'], field)
        decimal_roundtrip_bits = list(struct.unpack('<190I', struct.pack('<190f', *inv['values'])))
        assert p['bits'] == decimal_roundtrip_bits, (label, p['name'], 'float roundtrip')

assert inventories['baseline']['schema'] == inventories['final']['schema'] == 1
assert inventories['baseline']['library_revision'] == '2026.11'
assert inventories['final']['library_revision'] == '2026.11.1'
assert inventories['baseline']['parameter_ids'] == inventories['final']['parameter_ids']
ids = inventories['final']['parameter_ids']
assert len(ids) == len(set(ids)) == 190
assert banks['baseline']['legacy'] == banks['final']['legacy']

changed = []
descriptions = []
full_unchanged_names = []
for i, (before, after) in enumerate(zip(banks['baseline']['curated'], banks['final']['curated'])):
    for field in ('name', 'category', 'library', 'tags'):
        assert before[field] == after[field], (i, field)
    changes = [dict(parameter=parameter, before=before['values'][j], after=after['values'][j],
                    before_float32_bits=f'{before["bits"][j]:08x}',
                    after_float32_bits=f'{after["bits"][j]:08x}')
               for j, parameter in enumerate(ids) if before['bits'][j] != after['bits'][j]]
    if changes:
        changed.append(dict(index=i, host_index=i+222, name=before['name'], changes=changes))
    if before['description'] != after['description']:
        descriptions.append(dict(index=i, name=before['name'], before=before['description'], after=after['description']))
    if before == after:
        full_unchanged_names.append(before['name'])

expected_changed = {'Noir Swell Brass', 'Brass Ensemble 16', 'Neo-Noir Pad', 'Ether Pad',
                    'Arctic Pad', 'Synthwave Chords', 'Flute', 'Breathy Flute', 'Harp',
                    'Kalimba', 'Sine Bass', 'Upright Pluck'}
assert {p['name'] for p in changed} == expected_changed
assert len(changed) == 12
all_changes = [c for p in changed for c in p['changes']]
assert len(all_changes) == 42
assert sum(c['parameter'] != 'volume' for c in all_changes) == 38
assert {p['name'] for p in changed if any(c['parameter'] == 'volume' for c in p['changes'])} == {
    'Ether Pad', 'Kalimba', 'Sine Bass', 'Upright Pluck'}
assert {p['name'] for p in descriptions} == {
    'Brass Ensemble 16', 'Neo-Noir Pad', 'Arctic Pad', 'Synthwave Chords', 'UFO Landing'}
assert 'Init Circuit' in full_unchanged_names
assert len(full_unchanged_names) == 288  # UFO Landing is description-only.

binary_comparisons = {}
for family in ('legacy', 'midi', 'index'):
    b, f = OUT / 'baseline' / f'{family}.bin', OUT / 'final' / f'{family}.bin'
    assert b.read_bytes() == f.read_bytes()
    binary_comparisons[family] = {'byte_identical': True, 'bytes': b.stat().st_size,
                                'baseline_sha256': file_sha(b), 'final_sha256': file_sha(f)}

source_contracts = {}
for name in ('src/core/Params.h', 'src/core/FactoryPresets.cpp', 'src/core/CuratedLibrary.inc',
             'src/core/Engine.cpp', 'src/core/EngineWscHeld.cpp',
             'src/plugin/PluginProcessor.h', 'src/plugin/PluginProcessor.cpp',
             'src/plugin/PluginProcessorLibrary.cpp'):
    base = subprocess.check_output(['git', 'show', BASE + ':' + name], cwd=ROOT)
    current = (ROOT / name).read_bytes()
    assert base == current, name
    source_contracts[name] = {'byte_identical': True, 'sha256': sha(current)}
schema = int(re.search(r'stateSchemaVersion\s*=\s*(\d+)',
                      (ROOT / 'src/plugin/PluginProcessor.h').read_text()).group(1))
assert schema == 3

summary = {
    'assignment': 27, 'status': 'PASS', 'baseline_commit': BASE,
    'scope': 'Final calibrated 2026.11.1 bank versus frozen 2026.11 baseline; complete compiled binary records and inventory round-trip verified.',
    'baseline_revision': inventories['baseline']['library_revision'],
    'final_revision': inventories['final']['library_revision'],
    'contract': contracts['final'], 'state_schema_version': schema,
    'parameter_ids_order_unchanged': True,
    'catalogue_names_categories_tags_library_order_host_indices_unchanged': True,
    'legacy_complete_records_unchanged': True,
    'curated_changed_vector_count': len(changed),
    'curated_unchanged_vector_count': 301-len(changed),
    'curated_complete_record_unchanged_count': len(full_unchanged_names),
    'complete_record_qualification': '289 vectors unchanged. Of these, UFO Landing has a description-only correction, so 288 complete records including metadata remain byte-identical.',
    'changed_value_count': len(all_changes), 'nonvolume_changed_value_count': 38,
    'master_volume_changed_value_count': 4,
    'init_complete_record_unchanged': True,
    'inventory_float32_roundtrip_matches_compiled_records': True,
    'host_list_exactly_legacy_then_curated': True,
    'changed_programs': changed, 'description_changes': descriptions,
    'binary_comparisons': binary_comparisons,
    'source_contracts': source_contracts,
    'inputs': {name: {'sha256': file_sha(OUT/name), 'bytes': (OUT/name).stat().st_size}
              for name in ('baseline-inventory.json', 'final-inventory.json', 'baseline-core.a',
                           'final-core.a', 'identity.cpp', 'compare.py')},
    'limits': [
        'This verifies compiled factory data and source compatibility contracts, not host execution or acoustic realism.',
        'The host parameter total is derived from the unchanged layout, not a native plug-in reflection run.',
        'Binary record format uses little-endian u64 lengths/counts, UTF-8 strings, and exact float32 bits.',
        'MIDI comparison covers MSB 0..2, LSB 0..127, and Program 0..127 (49152 addresses), including unassigned addresses.',
        'Index comparison covers curated indices -1..302 and host indices -1..524 (830 lookups).']
}
(OUT/'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
print(json.dumps({k: summary[k] for k in ('status', 'curated_changed_vector_count',
    'curated_unchanged_vector_count', 'curated_complete_record_unchanged_count',
    'changed_value_count', 'nonvolume_changed_value_count', 'master_volume_changed_value_count')}))
print('Legacy full-record SHA256:', binary_comparisons['legacy']['final_sha256'])
print('MIDI map SHA256:', binary_comparisons['midi']['final_sha256'])
