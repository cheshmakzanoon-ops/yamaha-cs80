#!/usr/bin/env python3
"""Read-only bank/metadata audit; writes evidence only inside this directory.

The frozen .6 comparator is run on its real .6 input without revision overrides.
Exact .6/.7 equality then carries its result forward; a separate current-value
classification independently checks the 493/28 distinction against .4 bytes.
"""
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

OUT = Path(__file__).resolve().parent
ROOT = OUT.parents[4]
EVIDENCE = ROOT / 'docs/preset-evidence'
SOURCE6 = '18711f93a39aa49f3d6f9a41784be456224facbe'
SOURCE4 = '8685e9ed39d6347b67401da7590b7e1e8e95472d'
SHA4 = '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
SHA6 = '02a38edcc47a1befd9f044d3a1203e7ed3c464987951562b0d81e88f86c66d50'
EXCLUDED = {'volume', 'output_level', 'mix', 'ch1_vcf_level', 'ch2_vcf_level',
            'ch1_v_level', 'ch2_v_level', 'ch1_pan', 'ch2_pan'}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def git_bytes(commit, path):
    return subprocess.check_output(['git', 'show', f'{commit}:{path}'], cwd=ROOT)


def write(name, obj):
    (OUT / name).write_text(json.dumps(obj, indent=2, ensure_ascii=False, allow_nan=False) + '\n')


def main():
    executable = Path(sys.argv[1] if len(sys.argv) > 1 else '/tmp/cs80-build-1cee5a/cs80_preset_capture').resolve()
    inventory = OUT / 'inventory.json'
    command = [str(executable), '--inventory', str(inventory), '--library', 'all']
    subprocess.run(command, check=True, cwd=ROOT)
    script = ROOT / 'scripts/compare-preset-response-2026-11-6.py'
    assert script.read_bytes() == git_bytes(SOURCE6, str(script.relative_to(ROOT)))
    spec = importlib.util.spec_from_file_location('frozen_response6', script)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    oldpath = EVIDENCE / '2026.11.5/baseline/inventory.json'
    basepath = EVIDENCE / '2026.11.7/development/baseline-inventory.json'
    old, oldsha = mod.load_inventory(oldpath)
    base, basesha = mod.load_inventory(basepath)
    current, currentsha = mod.load_inventory(inventory)
    assert oldsha == SHA4 == mod.BASELINE_INVENTORY_SHA256
    assert oldpath.read_bytes() == git_bytes(SOURCE4, 'docs/preset-evidence/2026.11.4/continuation/final-inventory/inventory.json')
    assert basesha == SHA6
    assert basepath.read_bytes() == git_bytes(SOURCE6, 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json')
    assert mod.REFINEMENT_REVISION == base['library_revision'] == '2026.11.6'
    assert current['library_revision'] == '2026.11.7'
    mod.validate_inventory(current, 204, 'Fresh .7')
    assert {k: v for k, v in base.items() if k != 'library_revision'} == {
        k: v for k, v in current.items() if k != 'library_revision'}
    # The byte stream itself differs by exactly this revision substitution.
    assert basepath.read_bytes().replace(b'"library_revision":"2026.11.6"',
        b'"library_revision":"2026.11.7"', 1) == inventory.read_bytes()
    coverage = mod.compare(old, base)
    write('cumulative-coverage.json', {
        'scope': 'Published .4 to current .7, through authenticated exact .6/.7 identity except revision',
        'comparator_revision_override': False,
        'comparator_sha256': sha(script.read_bytes()),
        'before_sha256': oldsha, 'intermediate_6_sha256': basesha,
        'current_7_sha256': currentsha,
        'new_7_preset_parameter_edits': 0,
        **coverage})

    # Execute the existing metadata emitter against actual current and .6 headers.
    emitter = EVIDENCE / '2026.11.6/development/a08-parameter-contract.cpp'
    with tempfile.TemporaryDirectory(prefix='cs80-a12-metadata-') as td:
        td = Path(td)
        (td / 'Params.h').write_bytes(git_bytes(SOURCE6, 'src/core/Params.h'))
        outputs = []
        for label, include in [('baseline', td), ('current', ROOT / 'src/core')]:
            binary = td / label
            subprocess.run(['c++', '-std=c++20', '-O2', '-I', str(include), str(emitter), '-o', str(binary)], check=True)
            outputs.append(subprocess.check_output([str(binary)]))
    assert outputs[0] == outputs[1]
    rows = [line.split('\t') for line in outputs[1].decode().splitlines()]
    assert len(rows) == 204 and all(len(row) == 9 + 257 for row in rows)
    assert [row[1] for row in rows] == current['parameter_ids']
    metadata_rows = [dict(index=int(row[0]), id=row[1], name=row[2], unit=row[3], group=row[4],
        min_bits=row[5], max_bits=row[6], default_bits=row[7], skew_bits=row[8],
        normalization_257_pairs_sha256=sha('\t'.join(row[9:]).encode())) for row in rows]

    # The actual host layout, helper definitions and policy parameter source are
    # unchanged. This is source/metadata evidence, not fresh JUCE host execution.
    same_sources = {}
    for p in sorted((ROOT / 'src/plugin').rglob('*')):
        if not p.is_file():
            continue
        rel = p.relative_to(ROOT).as_posix()
        raw = p.read_bytes()
        assert raw == git_bytes(SOURCE6, rel), rel
        same_sources[rel] = sha(raw)
    for rel in ('src/core/Params.h', 'src/core/MemoryPanels.h', 'src/core/SourceFactoryTones.h',
                'src/core/InstrumentBodyVariants.h', 'src/core/InstrumentBodyMaterials.h', 'src/core/Engine.cpp'):
        raw = (ROOT / rel).read_bytes()
        assert raw == git_bytes(SOURCE6, rel), rel
        same_sources[rel] = sha(raw)
    body_now = (ROOT / 'src/core/InstrumentBody.h').read_bytes()
    body_old = git_bytes(SOURCE6, 'src/core/InstrumentBody.h')
    assert body_now.split(b'class InstrumentBody')[0] == body_old.split(b'class InstrumentBody')[0]
    factory = (ROOT / 'src/core/FactoryPresets.h').read_bytes()
    assert factory == git_bytes(SOURCE6, 'src/core/FactoryPresets.h').replace(b'2026.11.6', b'2026.11.7')
    header = (ROOT / 'src/plugin/PluginProcessor.h').read_text()
    assert 'memoryControlCount = 26;' in header
    assert 'hostParameterCount = cs80::P_numParams + 2 + 4 * memoryControlCount;' in header
    assert len(rows) + 2 + 4 * 26 == 310
    body_ids = [row[1] for row in rows if row[1].startswith(('body1_', 'body2_'))]
    assert len(body_ids) == 14
    write('metadata-contract.json', {
        'passed': True, 'baseline_commit': SOURCE6, 'core_parameter_count': 204,
        'host_parameter_count': 310, 'body_parameter_count': 14,
        'core_names_units_groups_ranges_defaults_skews_exact': True,
        'normalization_probes_per_parameter': 257,
        'normalization_probe_pair_count': 204 * 257,
        'full_baseline_and_current_emitter_sha256': sha(outputs[0]),
        'actual_host_layout_source_unchanged': True, 'fresh_native_host_execution': False,
        'same_source_sha256': same_sources, 'parameters': metadata_rows})

    # Recalculate the conservative .4/current distinction without changing the
    # existing .6 classification tool or pretending its revision check accepts .7.
    bits = lambda v: struct.pack('<f', v)
    records, levels, mechanism_records = [], [], []
    program_sets, rank_counts = {}, Counter()
    for before, after in zip(old['programs'], current['programs']):
        v = dict(zip(current['parameter_ids'], after['values']))
        prior = dict(zip(old['parameter_ids'], before['values']))
        changed = [k for k in old['parameter_ids'] if bits(prior[k]) != bits(v[k])]
        direct = [k for k in changed if k not in EXCLUDED]
        rank_info, mechanisms = [], set()
        for rank in (1, 2):
            model, mix, variant, material = [v[f'body{rank}_{k}'] for k in ('model', 'mix', 'variant', 'material')]
            gain = 1 - v['mix'] if rank == 1 else v['mix']
            route = gain > 0 and (v[f'ch{rank}_v_level'] > 0 or v[f'ch{rank}_init_level'] > 0 or v[f'ch{rank}_after_level'] > 0)
            rank_info.append(dict(rank=rank, balance_gain=gain,
                changed_additive_vca_baseline=bits(prior[f'ch{rank}_v_level']) != bits(v[f'ch{rank}_v_level']),
                old_v_level=prior[f'ch{rank}_v_level'], v_level=v[f'ch{rank}_v_level'],
                init_level=v[f'ch{rank}_init_level'], after_level=v[f'ch{rank}_after_level']))
            if not (model and mix > 0 and gain > 0):
                continue
            assert route and v['volume'] > 0 and v['output_level'] > 0
            eligible = ['body']
            if model == 1:
                eligible.append('piano_partial_decay')
            if model != 15 or variant in (3, 4):
                eligible.append('noise_rate_normalization')
            if 9 <= model <= 14 or variant in (3, 4):
                eligible.append('sustained_air_and_flutter_subset')
            if 4 <= model <= 8 and material == 0:
                eligible.append('generic_string_formants')
            if model == 15 and variant in (3, 4):
                eligible.append('low_choir_harmonics')
            for item in eligible:
                rank_counts[item] += 1
                program_sets.setdefault(item, set()).add(after['host_index'])
            mechanisms.update(eligible)
        record = dict(host_index=after['host_index'], name=after['name'], category=after['category'],
            changed_published_controls=changed, beyond_conservative_levels_pan=direct, ranks=rank_info)
        records.append(record)
        if after['category'] != 'Init':
            assert any(k != 'volume' for k in changed)
            if not direct:
                levels.append(record)
        else:
            assert not changed and after['values'][200:] == [0, 0, 0, 0]
        mechanism_records.append(dict(host_index=after['host_index'], name=after['name'],
            category=after['category'], eligible_mechanisms=sorted(mechanisms)))
    assert len(levels) == 28
    assert all(set(p['changed_published_controls']) <= {'volume', 'ch1_v_level', 'ch2_v_level'} for p in levels)
    assert all(any(r['changed_additive_vca_baseline'] and r['balance_gain'] > 0 and r['init_level'] > 0 for r in p['ranks']) for p in levels)
    old_audit = json.loads((EVIDENCE / '2026.11.6/verification/a56/development-inventory-audit.json').read_bytes())
    assert {p['host_index'] for p in levels} == {p['host_index'] for p in old_audit['conservative_classification']['programs']}
    write('current-value-classification.json', {
        'passed': True, 'before_sha256': oldsha, 'current_sha256': currentsha,
        'cumulative_musical_programs_changed': 521, 'beyond_conservative_levels_pan': 493,
        'additive_vca_baseline_only': 28, 'all_28_have_active_velocity_route': True,
        'new_7_preset_parameter_edits': 0, 'excluded_ids': sorted(EXCLUDED),
        'interpretation': 'Cumulative structural control changes; direct means outside the declared level/pan exclusion. No claim that every changed control is audible. The 28 rank baseline changes alter an additive VCA response with velocity, rather than a post-touch master scalar.',
        'families': dict(Counter(p['category'] for p in current['programs'])), 'programs': records})
    mechanism_keys = ['piano_partial_decay', 'noise_rate_normalization', 'generic_string_formants', 'low_choir_harmonics']
    union = set().union(*(program_sets[k] for k in mechanism_keys))
    assert [len(program_sets[k]) for k in mechanism_keys] == [10, 256, 54, 7]
    assert len(union) == 256
    mechanism_summary = {k: dict(program_count=len(indices), rank_count=rank_counts[k], host_indices=sorted(indices))
                         for k, indices in sorted(program_sets.items())}
    write('mechanism-coverage.json', {
        'inventory_sha256': currentsha, 'source_header_sha256': sha(body_now),
        'definition': 'Saved rank with nonzero model/body mix/rank balance, and possible nonzero VCA route. Structural exposure, not an audio-difference or improvement assertion.',
        'conditions': {'piano_partial_decay': 'Piano model; upper principal partials.',
            'noise_rate_normalization': 'Excitation noise path, excluding Standard organ; rate change away from 96000 Hz internal.',
            'generic_string_formants': 'Standard clav/harpsichord/nylon/steel/bass; changed current pitch. Existing banjo/koto/harp paths excluded.',
            'low_choir_harmonics': 'Ah/Oo variant; captured note and body size request more than 24 harmonics.'},
        'summary': mechanism_summary, 'union_program_count': len(union),
        'musical_programs_outside_union': 521 - len(union), 'programs': mechanism_records})
    write('receipt.json', {
        'schema': 1, 'passed': True, 'scope': 'Local .7 snapshot; no archive, perceptual, or native-host qualification',
        'capture_command': command, 'capture_executable_sha256': sha(executable.read_bytes()),
        'source_commit_of_checkout': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source_dirty': True, 'source_header_sha256': sha(body_now),
        'factory_header_sha256': sha(factory),
        'published_4': dict(commit=SOURCE4, inventory_sha256=oldsha, authenticated_git_blob='docs/preset-evidence/2026.11.4/continuation/final-inventory/inventory.json'),
        'baseline_6': dict(commit=SOURCE6, inventory_sha256=basesha, authenticated_git_blob='docs/preset-evidence/2026.11.6/verification/a15/inventory.json'),
        'current_7_inventory_sha256': currentsha,
        'all_523_complete_program_records_exact': True, 'inventory_bytes_exact_except_revision': True,
        'names_categories_order_host_indices_tags_descriptions_unchanged': True,
        'core_parameter_values_exact': 523 * 204, 'core_parameter_count': 204,
        'host_parameter_count': 310, 'body_parameter_count': 14,
        'new_presets': 0, 'new_ids': 0, 'changed_ids_or_ranges': 0, 'new_7_recipe_value_edits': 0,
        'original_program_count': 222, 'curated_program_count': 301,
        'musical_programs_cumulatively_changed_from_published_4': 521,
        'beyond_conservative_levels_pan': 493, 'additive_vca_baseline_only': 28,
        'preserved_init': ['Init Patch', 'Init Circuit'],
        'new_mechanism_union_program_count': 256,
        'frozen_6_comparator_sha256': sha(script.read_bytes()), 'frozen_comparator_modified': False,
        'revision_override_used': False,
        'files': {p.name: dict(bytes=p.stat().st_size, sha256=sha(p.read_bytes())) for p in sorted(OUT.iterdir())
                  if p.is_file() and p.name != 'receipt.json'}})
    print(json.dumps({'passed': True, 'identity_programs': 523, 'exact_values': 523 * 204,
        'host_parameters': 310, 'cumulative_changed': 521, 'direct_conservative': 493,
        'additive_vca': 28, 'mechanism_union': 256, 'current_sha256': currentsha}))


if __name__ == '__main__':
    main()
