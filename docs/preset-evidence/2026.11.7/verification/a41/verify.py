#!/usr/bin/env python3
"""A41: authenticate retained evidence and account for every saved selector.

Run from any directory. Writes only this verification directory and its small
temporary executable directory. It does not render PCM, rebuild the product,
modify a recipe, or rerun any of the cited audio suites.
"""
import csv
import hashlib
import io
import json
import math
import pathlib
import re
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[4]
TEMP = pathlib.Path('/tmp/cs80-a41-1cee5a')
SOUND = 'c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
FINAL = 'ee0dabcacb4cdff93f8803844f60348e0ceda6ed'
BASELINE = '18711f93a39aa49f3d6f9a41784be456224facbe'
INV = 'docs/preset-evidence/2026.11.7/verification/a12/inventory.json'
OLD_INV = 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'
CHECKS = []


def check(condition, description):
    CHECKS.append({'passed': bool(condition), 'description': description})


def sha(data):
    return hashlib.sha256(data).hexdigest()


def run(args):
    return subprocess.run(args, cwd=ROOT, capture_output=True, check=True)


def blob(revision, path):
    return run(['git', 'show', f'{revision}:{path}']).stdout


def write_json(name, value):
    (HERE / name).write_text(json.dumps(value, indent=2) + '\n')


current_bytes = (ROOT / INV).read_bytes()
baseline_bytes = blob(BASELINE, OLD_INV)
inventory = json.loads(current_bytes)
baseline = json.loads(baseline_bytes)
check(sha(current_bytes) == '55c08568074a6560ef10c8c525733a40aec42d9b8c6d20644bf4984cdd0d3e0d',
      'A12 current inventory has its authenticated digest')
check(sha(baseline_bytes) == '02a38edcc47a1befd9f044d3a1203e7ed3c464987951562b0d81e88f86c66d50',
      'baseline inventory is the authenticated .6 Git blob')
check(current_bytes.replace(b'2026.11.7', b'2026.11.6') == baseline_bytes,
      'complete inventory bytes equal .6 after changing only revision text')
check(len(inventory['programs']) == 523 and len(inventory['parameter_ids']) == 204,
      '523 complete identities and 204 core values per program')
check(inventory['programs'] == baseline['programs'],
      'all names, categories, addresses, descriptions, tags and 106692 values retained')

TEMP.mkdir(parents=True, exist_ok=True)
executable = TEMP / 'metadata_probe'
command = ['c++', '-std=c++20', '-O2', '-I', str(ROOT / 'src'),
           str(HERE / 'metadata_probe.cpp'), '-o', str(executable)]
compiled = run(command)
(HERE / 'compile.log').write_bytes(compiled.stdout + compiled.stderr)
metadata = json.loads(run([str(executable)]).stdout)
write_json('metadata.json', metadata)
models = {row['id']: row['name'] for row in metadata['models']}
materials = {row['id']: row for row in metadata['materials']}
variants = {row['id']: row for row in metadata['variants']}
accepted_materials = {1: 16, 2: 16, 3: 16, 4: 16, 5: 19, 6: 2, 7: 7, 8: 7, 9: 6}
accepted_variants = {1: 18, 2: 17, 3: 15, 4: 15}
for selector, row in materials.items():
    expected = [selector if selector and accepted_materials[selector] == model else 0
                for model in range(20)]
    check(row['effective_by_model'] == expected,
          f'material {selector}: compiled compatibility matches every published model')
for selector, row in variants.items():
    expected = [selector if selector and accepted_variants[selector] == model else 0
                for model in range(20)]
    check(row['effective_by_model'] == expected,
          f'variant {selector}: compiled compatibility matches every published model')
check(set(accepted_materials.values()).isdisjoint(set(accepted_variants.values())),
      'no published model accepts both a nonzero Material and a nonzero Variant')
check(all(row['attack'] == row['decay'] == row['release'] == 1
          for row in metadata['articulation'] if row['model'] == 15),
      'Organ base retains neutral outer articulation for every probed Ah/Oo-compatible case')

feet_labels = ['16', '8', '5 1/3', '4', '2 2/3', '2']
feet_ratios = [0.5, 1.0, 1.5, 2.0, 3.0, 4.0]
recipe_origin = {
    32: ('Dark Choir', 'velvet'), 95: ('Choir Pad', 'golden'),
    98: ('Dark Choir', 'midnight'), 184: ('Robot Voice', 'hollow'),
    156: ('Dulcimer', 'distant'),
}
expected_specials = {32, 95, 98, 184, 284, 285, 392, 393, 394, 400,
                     460, 463, 464, 465, 468, 472, 477, 487}
comparison_ids = {156, 354, 402}
specials, comparisons, all_ranks = [], [], []
for program in inventory['programs']:
    values = dict(zip(inventory['parameter_ids'], program['values']))
    ranks = []
    for rank in (1, 2):
        model = int(values[f'body{rank}_model'])
        variant = int(values[f'body{rank}_variant'])
        material = int(values[f'body{rank}_material'])
        effective_variant = variants[variant]['effective_by_model'][model]
        effective_material = materials[material]['effective_by_model'][model]
        mix = values[f'body{rank}_mix']
        balance = 1 - values['mix'] if rank == 1 else values['mix']
        vca = any(values[f'ch{rank}_{key}'] > 0
                  for key in ('v_level', 'init_level', 'after_level'))
        active = bool(model and mix > 0 and balance > 0 and vca)
        selected = effective_material > 0 or effective_variant > 0
        feet = int(values[f'ch{rank}_feet'])
        vocal = model == 15 and effective_variant in (3, 4)
        standard_string = 4 <= model <= 8 and effective_material not in (7, 8, 9)
        budgets = {}
        if vocal:
            for note in (36, 48, 60, 72):
                detune = values['detune'] / 100 if rank == 2 else 0
                hz = values['tuning_ref'] * 2 ** ((note - 69 + values['master_tune'] + detune) / 12)
                hz *= feet_ratios[feet]
                budgets[str(note)] = math.ceil(max(24, min(48, 4400 / (values[f'body{rank}_size'] * hz))))
        record = {
            'host_index': program['host_index'], 'program': program['name'], 'rank': rank,
            'model_id': model, 'model_name': models[model],
            'variant_id': variant, 'variant_name': variants[variant]['name'],
            'effective_variant_id': effective_variant,
            'material_id': material, 'material_name': materials[material]['name'],
            'effective_material_id': effective_material,
            'body_mix': mix, 'rank_balance': balance, 'possible_vca_route': vca,
            'active_body': active, 'selected_specialization': selected,
            'footage_selector': feet, 'footage_feet': feet_labels[feet],
            'body_brightness': values[f'body{rank}_brightness'],
            'body_decay': values[f'body{rank}_decay'], 'body_size': values[f'body{rank}_size'],
            'vca_attack_seconds_saved': values[f'ch{rank}_v_a'],
            'vca_decay_seconds_saved': values[f'ch{rank}_v_d'],
            'vca_sustain_saved': values[f'ch{rank}_v_s'],
            'vca_release_seconds_saved': values[f'ch{rank}_v_r'],
            'pressure_brightness': values[f'ch{rank}_after_brill'],
            'pressure_level': values[f'ch{rank}_after_level'],
            'new_7_piano_loss': active and model == 1,
            'new_7_noise_normalization_off_96000_internal': active and (model != 15 or vocal),
            'new_7_generic_string_formants': active and standard_string,
            'new_7_low_choir_if_budget_above_24': active and vocal,
            'nominal_choir_mode_counts_by_played_midi': budgets,
        }
        all_ranks.append(record)
        ranks.append(record)
    chosen = any(row['selected_specialization'] for row in ranks)
    if chosen or program['host_index'] in comparison_ids:
        base_recipe, finish = recipe_origin.get(program['host_index'], (program['name'], None))
        result = {
            'program': program, 'base_recipe': base_recipe, 'original_finish': finish,
            'rank_detune_cents': values['detune'],
            'rank_balance_II': values['mix'], 'ranks': ranks,
        }
        (specials if chosen else comparisons).append(result)

check({row['program']['host_index'] for row in specials} == expected_specials,
      'all and only the 18 expected existing identities select a specialization')
check(all(row['effective_material_id'] == row['material_id'] and
          row['effective_variant_id'] == row['variant_id'] for row in all_ranks),
      'no saved nonzero selector is ignored by compatibility across all 1046 ranks')
specialized_ranks = [row for row in all_ranks if row['selected_specialization']]
check(len(specialized_ranks) == 34 and all(row['active_body'] for row in specialized_ranks),
      'all 34 specialized ranks have positive body mix, rank balance and possible VCA route')
check(len([row for row in specialized_ranks if row['effective_material_id']]) == 18,
      'nine materials occur on exactly 18 active ranks')
check(len([row for row in specialized_ranks if row['effective_variant_id'] in (1, 2)]) == 4,
      'two struck-bar/shell variants occur on exactly four active ranks')
check(len([row for row in specialized_ranks if row['effective_variant_id'] in (3, 4)]) == 12,
      'Ah/Oo occur on exactly twelve active ranks')
check(not any(row['new_7_piano_loss'] or row['new_7_generic_string_formants'] for row in specialized_ranks),
      'none of the specialized ranks enters new .7 piano-loss or generic-string-formant paths')
check({row['program']['name'] for row in comparisons} == {'Dulcimer', 'Distant Keys 113', 'Dulcimer Pluck'},
      'three separately named dulcimer identities accounted without conflating their recipes')
write_json('recipes.json', {'specialized_programs': specials, 'standard_steel_comparators': comparisons})
write_json('all-rank-selectors.json', all_ranks)

rows = [rank for program in specials + comparisons for rank in program['ranks']]
fields = [key for key in rows[0] if key != 'nominal_choir_mode_counts_by_played_midi']
with (HERE / 'rank-inventory.csv').open('w', newline='') as stream:
    writer = csv.DictWriter(stream, fieldnames=fields, extrasaction='ignore')
    writer.writeheader()
    writer.writerows(rows)

source_files = [
    'src/core/InstrumentBody.h', 'src/core/InstrumentBodyMaterials.h',
    'src/core/InstrumentBodyVariants.h', 'src/core/BodyArticulation.h',
    'src/core/Engine.cpp', 'src/core/Engine.h', 'src/core/Params.h',
    'src/core/FactoryPresets.cpp', 'src/core/FactoryPresets.h',
    'src/core/OriginalCircuitLibrary.inc', 'src/core/CuratedLibrary.inc',
    'src/core/curated/Keys.inc', 'src/core/curated/Plucked.inc',
    'src/core/curated/Bells.inc', 'src/core/curated/Pads.inc', 'src/core/curated/Fx.inc',
    'tests/MaterialBodyTests.cpp', 'tests/InstrumentBodyVariantTests.cpp',
    'tests/PluckedMaterialTests.cpp', 'tests/BellMaterialPresetTests.cpp',
    'tests/PluckedMaterialPresetTests.cpp', 'tests/StruckBodyDynamicsTests.cpp',
    'tests/VocalBodyTests.cpp', 'tests/PianoCourseGeometryTests.cpp',
    'tests/PianoPartialDecayTests.cpp', 'tests/LowChoirSpectrumTests.cpp',
]
source_receipts = []
for path in source_files:
    data = (ROOT / path).read_bytes()
    sound_bytes, final_bytes = blob(SOUND, path), blob(FINAL, path)
    check(data == sound_bytes == final_bytes, f'{path}: exact frozen sound and final-source bytes')
    source_receipts.append({'path': path, 'sha256': sha(data), 'bytes': len(data),
                            'matches_sound_and_final': data == sound_bytes == final_bytes})
retained_paths = [
    'src/core/InstrumentBodyMaterials.h', 'src/core/InstrumentBodyVariants.h',
    'src/core/BodyArticulation.h', 'src/core/curated/Keys.inc',
    'src/core/curated/Plucked.inc', 'src/core/curated/Bells.inc',
    'src/core/curated/Pads.inc', 'src/core/curated/Fx.inc',
]
for path in retained_paths:
    check((ROOT / path).read_bytes() == blob(BASELINE, path), f'{path}: retained exactly from .6')


def function_text(source, name):
    match = re.search(r'\b(?:double|void)\s+' + name + r'\([^)]*\)\s*(?:const\s*)?noexcept\s*\{', source)
    if match is None:
        raise ValueError('Missing function definition: ' + name)
    start = source.index('{', match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end].encode()


body_path = 'src/core/InstrumentBody.h'
body_now = (ROOT / body_path).read_text()
body_old = blob(BASELINE, body_path).decode()
retained_functions = []
for name in ('makeMaterialModes', 'makeVariantBarModes', 'pluckedMaterialResponse',
             'pluckedMaterialWeight', 'stringLoss', 'configureExcitation',
             'hitSeconds', 'makeTineModes', 'makeBarModes'):
    now, old = function_text(body_now, name), function_text(body_old, name)
    check(now == old, name + ': exact .6 profile/excitation/loss function retained')
    retained_functions.append({'name': name, 'sha256': sha(now), 'bytes': len(now),
                                'exactly_retained_from_baseline6': now == old})

reused = [
    '2026.11.7/reviews/a12.md', '2026.11.7/reviews/a15.md', '2026.11.7/reviews/a17.md',
    '2026.11.7/reviews/a35.md', '2026.11.7/reviews/a36.md',
    '2026.11.7/reviews/a07.md', '2026.11.7/reviews/a21.md',
    '2026.11.7/verification/a12/receipt.json',
    '2026.11.7/verification/a15/summary.json', '2026.11.7/verification/a15/binding.json',
    '2026.11.7/verification/a17/results.json', '2026.11.7/verification/a17/binding.json',
    '2026.11.7/verification/a35/review.json', '2026.11.7/verification/a36/provenance.json',
    '2026.11.7/verification/a27/receipt.json',
    '2026.11.7/verification/a27/cs80_material_body_tests.log',
    '2026.11.7/verification/a27/cs80_instrument_body_variant_tests.log',
    '2026.11.7/verification/a27/cs80_instrument_body_engine_tests.log',
    '2026.11.7/verification/a15/material-preset-tests.log',
    '2026.11.7/verification/a07/current.log', '2026.11.7/verification/a07/vocal-regression.log',
    '2026.11.6/reviews/a07.md', '2026.11.6/reviews/a09.md',
    '2026.11.6/reviews/a10.md', '2026.11.6/reviews/a11.md',
    '2026.11.6/verification/a09/plucked-material-tests.log',
    '2026.11.6/verification/a09/measured-summary.json',
    '2026.11.6/development/a10/material-preset-tests.log',
    '2026.11.6/development/a11/preset-tests.log',
]
evidence_receipts = []
for relative in reused:
    path = 'docs/preset-evidence/' + relative
    data = (ROOT / path).read_bytes()
    evidence_receipts.append({'path': path, 'sha256': sha(data), 'bytes': len(data),
                             'reused_not_rerun_by_a41': True})
a27 = json.loads((ROOT / 'docs/preset-evidence/2026.11.7/verification/a27/receipt.json').read_text())
for test in a27['existing_tests']:
    path = ROOT / 'docs/preset-evidence/2026.11.7/verification/a27' / (test['name'] + '.log')
    check(test['exit_code'] == 0 and sha(path.read_bytes()) == test['stdout_sha256'],
          test['name'] + ': retained successful log matches source-bound A27 receipt')

write_json('checks.json', CHECKS)
receipt = {
    'scope': 'Source/selector/articulation accounting plus explicitly reused PCM evidence; no new audio captures',
    'review': {'path': 'docs/preset-evidence/2026.11.7/reviews/a41.md',
               'sha256': sha((ROOT / 'docs/preset-evidence/2026.11.7/reviews/a41.md').read_bytes())},
    'sound_source': SOUND, 'final_source': FINAL,
    'final_tree': run(['git', 'rev-parse', FINAL + '^{tree}']).stdout.decode().strip(),
    'head_at_run': run(['git', 'rev-parse', 'HEAD']).stdout.decode().strip(),
    'baseline': BASELINE,
    'inventory': {'path': INV, 'sha256': sha(current_bytes), 'baseline_sha256': sha(baseline_bytes),
                  'programs': len(inventory['programs']), 'parameters': len(inventory['parameter_ids']),
                  'exact_values': 523 * 204, 'complete_records_equal': inventory['programs'] == baseline['programs']},
    'summary': {'materials': 9, 'variants': 4, 'specialized_programs': len(specials),
                'specialized_ranks': len(specialized_ranks), 'all_program_ranks_accounted': len(all_ranks),
                'ignored_nonzero_selectors': sum(row['material_id'] != row['effective_material_id'] or
                    row['variant_id'] != row['effective_variant_id'] for row in all_ranks),
                'metadata_compatibility_cases': 20 * (10 + 5),
                'articulation_probe_cases': len(metadata['articulation']),
                'new_presets': 0, 'new_pcm_renders': 0},
    'probe': {'command': command, 'executable_sha256': sha(executable.read_bytes()),
              'compiler_version': run(['c++', '--version']).stdout.decode().splitlines()[0]},
    'sources': source_receipts, 'retained_body_functions': retained_functions,
    'reused_evidence': evidence_receipts,
    'checks': len(CHECKS), 'failures': sum(not row['passed'] for row in CHECKS),
    'pass_within_stated_scope': all(row['passed'] for row in CHECKS),
    'limitations': ['No new PCM matrix or listening assessment',
                    'Mode-ratio and decay checks are modeled-signal measurements, not acoustic validation',
                    'No nonlinear coupling/contact/complete-instrument or sample-recording equivalence claim',
                    'No packaged-binary or native-host qualification'],
}
receipt['outputs'] = {path.name: {'bytes': path.stat().st_size, 'sha256': sha(path.read_bytes())}
                      for path in HERE.iterdir() if path.is_file() and path.name != 'receipt.json'}
write_json('receipt.json', receipt)
print(json.dumps({'checks': receipt['checks'], 'failures': receipt['failures'], **receipt['summary']}, indent=2))
if not receipt['pass_within_stated_scope']:
    raise SystemExit(1)
