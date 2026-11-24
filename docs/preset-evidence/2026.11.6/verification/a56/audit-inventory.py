#!/usr/bin/env python3
"""Independent inventory classification; does not qualify an archive or audio."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[5]
EVIDENCE = ROOT / 'docs/preset-evidence'
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
BASELINE = EVIDENCE / '2026.11.5/baseline/inventory.json'
PREVIOUS = EVIDENCE / '2026.11.5/inventory.json'
DEVELOPMENT = EVIDENCE / '2026.11.6/verification/a15/inventory.json'
EXCLUDED = {'volume', 'output_level', 'mix', 'ch1_vcf_level', 'ch2_vcf_level',
            'ch1_v_level', 'ch2_v_level', 'ch1_pan', 'ch2_pan'}
FAMILIES = {'Keys':45, 'Strings':59, 'Brass':59, 'Winds':14, 'Bass':50,
            'Bells':23, 'Plucked':17, 'Organ':15, 'Pads':63, 'Polysynth':25,
            'Leads':56, 'Sequences':27, 'FX':46, 'Hardware Studies':22, 'Init':2}
MATERIALS = {'Tubular Bells':(1,16), 'Gong':(2,16), 'Temple Bell':(3,16),
             'Agogo':(4,16), 'Music Box':(5,19), 'Kalimba':(6,2),
             'Banjo':(7,7), 'Koto':(8,7), 'Harp':(9,6)}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def read(path):
    return json.loads(path.read_text())

def bits(value):
    return struct.pack('<f', value)

def vector_bits(values):
    return b''.join(map(bits, values))

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--candidate', type=Path, default=DEVELOPMENT)
    parser.add_argument('--out', required=True, type=Path)
    args = parser.parse_args()
    before, previous, dev, current = map(read, (BASELINE, PREVIOUS, DEVELOPMENT, args.candidate))
    assert sha(BASELINE) == '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
    assert current['library_revision'] == '2026.11.6'
    assert len(current['programs']) == 523 and len(current['parameter_ids']) == 204
    assert current['parameter_ids'] == dev['parameter_ids']
    assert previous['parameter_ids'] == current['parameter_ids'][:202]
    assert before['parameter_ids'] == current['parameter_ids'][:200]
    assert dict(Counter(p['category'] for p in current['programs'])) == FAMILIES
    ids = current['parameter_ids']
    records, init, selected = [], [], []
    tag_changes = 0
    for old, prior, development, new in zip(before['programs'], previous['programs'], dev['programs'], current['programs']):
        for key in ('index', 'host_index', 'name', 'category'):
            assert old[key] == prior[key] == development[key] == new[key]
        assert len(new['values']) == 204
        assert vector_bits(new['values']) == vector_bits(development['values']), new['name']
        old_values = dict(zip(before['parameter_ids'], old['values']))
        values = dict(zip(ids, new['values']))
        changed = [key for key in before['parameter_ids'] if bits(old_values[key]) != bits(values[key])]
        beyond_levels = [key for key in changed if key not in EXCLUDED]
        tag_changes += old['tags'] != new['tags']
        if new['category'] == 'Init':
            assert vector_bits(old['values']) == vector_bits(new['values'][:200])
            assert vector_bits(prior['values']) == vector_bits(new['values'][:202])
            assert new['values'][200:] == [0,0,0,0]
            init.append({'name':new['name'], 'host_index':new['host_index'],
                         'published_200_prefix_equal':True, 'previous_202_prefix_equal':True,
                         'appended_selectors':new['values'][200:]})
        else:
            assert any(key != 'volume' for key in changed)
        expected_material, expected_model = MATERIALS.get(new['name'], (0, None))
        ranks = []
        for rank in (1,2):
            prefix = f'ch{rank}_'
            gain = (1-values['mix']) if rank == 1 else values['mix']
            model, mix, material = (values[f'body{rank}_{key}'] for key in ('model','mix','material'))
            assert material == expected_material
            if expected_material:
                assert model == expected_model and mix > 0 and gain > 0
                assert values[f'body{rank}_variant'] == 0
                assert values['source_performance'] == 1 and values['parity_mode'] == 0
                assert values['volume'] > 0 and values['output_level'] > 0
                assert values[prefix+'v_level'] + .25*values[prefix+'init_level'] > 0
            ranks.append({'rank':rank, 'body_model':model, 'body_mix':mix, 'material':material,
                          'rank_balance_gain':gain, 'pre_vca_body_coefficient':gain*mix,
                          'old_v_level':old_values[prefix+'v_level'], 'v_level':values[prefix+'v_level'],
                          'init_level':values[prefix+'init_level'], 'after_level':values[prefix+'after_level'],
                          'changed_baseline_with_active_velocity_route':
                              bits(old_values[prefix+'v_level']) != bits(values[prefix+'v_level'])
                              and gain > 0 and values[prefix+'init_level'] > 0})
        if expected_material:
            selected.append({'name':new['name'], 'host_index':new['host_index'], 'ranks':ranks})
        records.append({'name':new['name'], 'host_index':new['host_index'], 'category':new['category'],
                        'changed_published_controls':changed, 'beyond_conservative_levels_pan':beyond_levels,
                        'ranks':ranks})
    musical = [p for p in records if p['category'] != 'Init']
    levels_only = [p for p in musical if not p['beyond_conservative_levels_pan']]
    assert len(levels_only) == 28
    assert all(set(p['changed_published_controls']) <= {'volume','ch1_v_level','ch2_v_level'} for p in levels_only)
    assert all(any(r['changed_baseline_with_active_velocity_route'] for r in p['ranks']) for p in levels_only)
    binding = read(EVIDENCE/'2026.11.6/verification/a15/capture-binding.json')
    mismatched_source_files = []
    for path, digest in binding['core_source_hashes'].items():
        data = subprocess.check_output(['git','show',f'{SOURCE}:{path}'], cwd=ROOT)
        if hashlib.sha256(data).hexdigest() != digest:
            mismatched_source_files.append(path)
    assert not mismatched_source_files
    audio_path = EVIDENCE/'2026.11.5/verification/a33-matched-bank-audio.json'
    audio = read(audio_path)
    assert audio['inputs']['before_inventory_sha256'] == sha(BASELINE)
    audio_inventory = EVIDENCE/'2026.11.5/development/inventory-before-long-pad-headroom.json'
    assert audio['inputs']['after_inventory_sha256'] == sha(audio_inventory)
    historical_inventory_deltas = []
    for old, new in zip(read(audio_inventory)['programs'], previous['programs']):
        changed = [key for key, a, b in zip(previous['parameter_ids'], old['values'], new['values'])
                   if bits(a) != bits(b)]
        if changed:
            historical_inventory_deltas.append({'name':new['name'], 'changed_controls':changed})
    manifest = EVIDENCE/'2026.11.5/development/matched-capture-manifest.json'
    assert sha(manifest) == audio['inputs']['after_capture_manifest_sha256']
    assert read(manifest)['executable_sha256'] == audio['inputs']['after_executable_sha256']
    near_gain_only = [{k:p[k] for k in ('name','equal_rms_residual','averaged_spectral_hellinger','settings')}
                     for p in audio['programs'] if p['category'] != 'Init' and
                     p['equal_rms_residual'] is not None and p['equal_rms_residual'] <= .001]
    report = {'schema':1, 'structural_checks_passed':True,
              'archive_qualification_performed':False, 'fresh_audio_comparison_performed':False,
              'scope':'Inventory-only independent review; development data until separately bound to an actual CI artifact.',
              'inputs':{str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path):sha(path)
                        for path in (BASELINE, PREVIOUS, DEVELOPMENT, args.candidate, audio_path, audio_inventory, manifest)},
              'source_file_check':{'commit':SOURCE, 'a15_core_files_matching_committed_source':len(binding['core_source_hashes']),
                                   'executable_provenance_not_reestablished':True},
              'identities':523, 'development_values_equal_as_binary32':523,
              'parameter_counts':{'published':200, 'previous':202, 'current':204},
              'families':FAMILIES, 'tags_differ_from_published_count':tag_changes,
              'preserved_init':init, 'materials':selected,
              'conservative_classification':{'excluded_control_ids':sorted(EXCLUDED),
                  'musical_programs_with_other_changed_controls':len(musical)-len(levels_only),
                  'programs_with_only_rank_baseline_and_optional_master_changes':len(levels_only),
                  'all_28_have_changed_baseline_on_nonzero_rank_with_velocity_route':True,
                  'body_enabled_among_28':sum(any(r['body_model'] and r['body_mix']>0 and r['rank_balance_gain']>0 for r in p['ranks']) for p in levels_only),
                  'family_breakdown':dict(Counter(p['category'] for p in levels_only)),
                  'interpretation':'Rank v_level is additive with touch, not a post-touch scalar. This exclusion does not establish gain-only PCM or absent touch improvement.',
                  'programs':levels_only},
              'historical_audio':{'scope':audio['scope'], 'release_qualified':audio['release_qualified'],
                  'candidate_parameter_count':202, 'is_fcc3c8e_audio':False,
                  'source_binding':'Historical .5 local capture after 17 integration level corrections but before later long-pad headroom corrections; executable and inventories identified by retained hashes. Not current .5 inventory or .6 committed CI capture.',
                  'changes_from_historical_capture_inventory_to_current_5':historical_inventory_deltas,
                  'summary':audio['summary'], 'pressure_summary':audio['pressure_summary'],
                  'near_uniform_gain_under_single_gesture':near_gain_only,
                  'method':audio['methods'], 'audio_samples_independently_reanalyzed_here':False},
              'programs':records}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'structural_checks_passed':True, 'identities':523, 'beyond_conservative_levels_pan':493,
                      'rank_baseline_only':28, 'families':FAMILIES, 'material_programs':len(selected),
                      'tag_changes':tag_changes, 'archive_qualified':False}))

if __name__ == '__main__':
    main()
