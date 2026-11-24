#!/usr/bin/env python3
"""Read-only independent continuity/accounting inspection; no audio generation."""
from pathlib import Path
import collections
import csv
import hashlib
import json
import math
import statistics
import struct
import subprocess

REPO = Path(__file__).resolve().parents[4]
E = REPO / 'docs/preset-evidence/2026.11.5'
BASE = '8685e9ed39d6347b67401da7590b7e1e8e95472d'
MID = '6094bce18c8b19da804c2fbc9a00e31969a7f579'
SOUND = '09af5358d34a7ea431de7e8a42e35dae66e52159'
REVIEW = 'db6b5a76b3c5e38dd9444d0cf6065a14f6302814'
INV = 'docs/preset-evidence/2026.11.5/inventory.json'
records = {}

def sha(b): return hashlib.sha256(b).hexdigest()
def git(*args): return subprocess.check_output(['git', *args], cwd=REPO)
def obj(rev, path): return git('show', rev + ':' + path)
def f32(x): return struct.pack('<f', x)
def retained(path):
    p = E / path
    b = p.read_bytes()
    records[path] = dict(bytes=len(b), sha256=sha(b))
    return json.loads(b)
def identity(p): return tuple(p[k] for k in ('index', 'host_index', 'name', 'category'))

before_bytes = (E / 'baseline/inventory.json').read_bytes()
mid_bytes = obj(MID, INV)
final_bytes = obj(REVIEW, INV)
assert sha(before_bytes) == '0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611'
assert sha(final_bytes) == 'fd91cc5dde92bf9f823114a5c07e609961d92cd69080b6b6791191e86ba2bfa1'
assert final_bytes == (E / 'inventory.json').read_bytes() == obj(SOUND, INV)
assert mid_bytes == (E / 'development/inventory-before-long-pad-headroom.json').read_bytes()
before, middle, final = map(json.loads, (before_bytes, mid_bytes, final_bytes))
assert len(before['programs']) == len(middle['programs']) == len(final['programs']) == 523
assert len(before['parameter_ids']) == 200
assert final['parameter_ids'] == before['parameter_ids'] + ['body1_variant', 'body2_variant']
assert final['parameter_ids'] == middle['parameter_ids']
ids = final['parameter_ids']
volume = ids.index('volume')
excluded = {ids.index(k) for k in ('volume', 'parity_mode', 'source_performance')}
init = {'Init Patch', 'Init Circuit'}
changed = []
tag_changed = []
body_enabled = []
old_new_counts = []
nonvolume_vectors = set()
pad_deltas = []
models = collections.Counter()
variants = collections.Counter()
for old, mid, new in zip(before['programs'], middle['programs'], final['programs']):
    assert identity(old) == identity(mid) == identity(new)
    assert len(old['values']) == 200 and len(new['values']) == len(mid['values']) == 202
    assert all(math.isfinite(v) for v in new['values'])
    oldv, midv, newv = [tuple(map(f32, p['values'])) for p in (old, mid, new)]
    sound_changes = [ids[i] for i in range(200) if i not in excluded and oldv[i] != newv[i]]
    if new['name'] in init:
        assert oldv == newv[:200] and newv[200:] == (f32(0), f32(0))
        assert {k:v for k,v in old.items() if k!='values'} == {k:v for k,v in new.items() if k!='values'}
    else:
        assert sound_changes
        changed.append(new['name'])
        old_new_counts.append(len(sound_changes))
    if old['tags'] != new['tags']: tag_changed.append(new['name'])
    fingerprint = tuple(v for i,v in enumerate(newv) if i != volume)
    assert fingerprint not in nonvolume_vectors
    nonvolume_vectors.add(fingerprint)
    assert {k:v for k,v in mid.items() if k!='values'} == {k:v for k,v in new.items() if k!='values'}
    for i,(a,b) in enumerate(zip(midv,newv)):
        if a != b:
            pad_deltas.append(dict(host_index=new['host_index'],name=new['name'],parameter=ids[i],before=mid['values'][i],after=new['values'][i]))
    active = False
    for rank in (1,2):
        model = new['values'][ids.index(f'body{rank}_model')]
        mix = new['values'][ids.index(f'body{rank}_mix')]
        if model != 0 and mix > 0:
            active = True
            models[int(model)] += 1
            variants[int(new['values'][ids.index(f'body{rank}_variant')])] += 1
    if active: body_enabled.append(new['name'])
assert len(changed) == 521 and len(body_enabled) == 272
assert len(tag_changed) == 14
assert [(x['host_index'],x['parameter']) for x in pad_deltas] == [(109,'volume'),(303,'volume')]

coverage = retained('parameter-coverage.json')
assert coverage['before']['inventory_sha256'] == sha(before_bytes)
assert coverage['after']['inventory_sha256'] == sha(final_bytes)
assert coverage['validation']['musical_programs_changed'] == len(changed)
assert coverage['validation']['updated_program_tags'] == len(tag_changed)
account = retained('development/long-pad-headroom/final-bank-audio-accounting.json')
for info in account['inputs'].values():
    b=(E/info['path']).read_bytes()
    assert len(b) == info['bytes'] and sha(b) == info['sha256']
    records[info['path']] = dict(bytes=len(b),sha256=sha(b))
a33 = retained('verification/a33-matched-bank-audio.json')
supp = retained('development/long-pad-headroom/final-audio-supplement.json')
delta = retained('development/long-pad-headroom/inventory-delta.json')
objects = retained('development/long-pad-headroom/core-object-equivalence.json')
assert delta['differences'] == pad_deltas
assert a33['inputs']['before_inventory_sha256'] == sha(before_bytes)
assert a33['inputs']['after_inventory_sha256'] == sha(mid_bytes)
assert a33['inputs']['analysis_script_sha256'] == sha((E/'verification/compare-matched-bank-audio.py').read_bytes())
assert supp['comparator_sha256'] == a33['inputs']['analysis_script_sha256']
assert supp['source_before'] == BASE and supp['source_after'] == SOUND
for f in retained('verification/a33-retained-files.json')['files']:
    b=(E/f['path']).read_bytes()
    assert len(b)==f['bytes'] and sha(b)==f['sha256']
old_main = {x['index']:x for x in a33['programs']}
assert set(old_main) == set(range(523))
replacements = {x['index']:x for x in supp['programs']}
assert set(replacements) == {109,303}
for i,r in replacements.items():
    old = old_main[i]
    for k in ('name','category','settings','frames','before_wav_sha256','before_pcm_sha256','before'):
        assert r[k] == old[k], (i,k)
    assert not r['exact_pcm_equal'] and not r['both_silent']
rows = [replacements.get(i,old_main[i]) for i in range(523)]
audible = [x for x in rows if not x['both_silent']]
assert sum(x['frames'] for x in rows) == 103153374
assert sum(not x['exact_pcm_equal'] for x in rows) == 519
assert [x['name'] for x in rows if x['both_silent']] == ['Electric Leads 136','Pressure Lead']
assert [x['name'] for x in rows if x['exact_pcm_equal'] and not x['both_silent']] == ['Init Patch','Init Circuit']
summary = dict(programs=len(rows),stereo_frames_per_version=sum(x['frames'] for x in rows),
    seconds_per_version=sum(x['frames'] for x in rows)/48000,
    exact_pcm_equal=[x['name'] for x in rows if x['exact_pcm_equal']],
    both_silent=[x['name'] for x in rows if x['both_silent']],
    changed_raw_pcm=sum(not x['exact_pcm_equal'] for x in rows),audible_comparisons=len(audible),
    equal_rms_residual_above_0_001=sum(x['equal_rms_residual']>.001 for x in audible),
    averaged_spectral_hellinger_above_0_01=sum(x['averaged_spectral_hellinger']>.01 for x in audible),
    highest_before_peak_dbfs=max(x['before']['peak_dbfs'] for x in rows),
    highest_after_peak_dbfs=max(x['after']['peak_dbfs'] for x in rows))
for key in ('rms_change_db','equal_rms_residual','averaged_spectral_hellinger'):
    values=[x[key] for x in audible]
    summary[key]=dict(min=min(values),median=statistics.median(values),max=max(values))
for k,v in summary.items(): assert account['main_summary'][k] == v,k
assert account['pressure_summary'] == a33['pressure_summary']
assert {p['name'] for p in a33['pressure_programs']} == {'Pressure Lead','Electric Leads 136'}
assert all(not p['exact_pcm_equal'] and not p['both_silent'] for p in a33['pressure_programs'])

# Hash only the four replacement inputs needed to close the composition chain.
# No new render or whole-bank recomparison is performed.
pad_wavs=[]
for row in supp['programs']:
    slug=''.join(c if c.isascii() and c.isalnum() else '-' for c in row['name'])
    filename=f"{row['index']:03d}-{slug}.wav"
    for phase, directory in [('before','baseline-headroom-subset'),('after','candidate-headroom-audio')]:
        p=REPO.parent/directory/filename
        b=p.read_bytes()
        assert sha(b) == row[phase+'_wav_sha256']
        assert b[:4]==b'RIFF' and b[8:16]==b'WAVEfmt '
        offset=12; chunks={}
        while offset<len(b):
            tag=b[offset:offset+4];n=struct.unpack_from('<I',b,offset+4)[0]
            chunks[tag]=b[offset+8:offset+8+n];offset+=8+n+(n%2)
        pcm=chunks[b'data']
        assert sha(pcm)==row[phase+'_pcm_sha256'] and len(pcm)//8==row['frames']
        pad_wavs.append(dict(path=str(p.relative_to(REPO.parent)),phase=phase,bytes=len(b),wav_sha256=sha(b),pcm_sha256=sha(pcm),frames=len(pcm)//8))

# Independently re-read existing archives and compare all object members.
archives={'before':Path('/tmp/cs80-a40/libcs80_core.a'),'after':REPO/'build-preset/libcs80_core.a'}
core_records=[]
for item in objects['members']:
    row={'member':item['member']}
    for phase,p in archives.items():
        h=sha(subprocess.check_output(['ar','p',str(p),item['member']]))
        assert h == item[phase+'_sha256']
        row[phase+'_sha256']=h
    row['identical']=row['before_sha256']==row['after_sha256']
    assert row['identical']==item['identical']
    core_records.append(row)
assert [x['member'] for x in core_records if not x['identical']] == ['FactoryPresets.cpp.o']
for phase,p in archives.items():
    members=subprocess.check_output(['ar','t',str(p)],text=True).splitlines()
    assert members == [x['member'] for x in objects['members']]
post_core=retained('development/core-after-long-pad-headroom.json')
assert sha(archives['after'].read_bytes()) == post_core['files']['build-preset/libcs80_core.a']['sha256']
assert sha((REPO/'build-preset/cs80_preset_capture').read_bytes()) == supp['after_executable_sha256']

# Inspect retained prefix streams, not new compilations or host executions.
abi=REPO.parent/'a21-host-abi'
old=(abi/'baseline.tsv').read_bytes();new=(abi/'current.tsv').read_bytes()
assert len(old.splitlines())==200 and len(new.splitlines())==202
assert old==b''.join(new.splitlines(keepends=True)[:200])
assert sha(old)=='2632fd6788f95e94627ba5d9b176a85e93c008e198b50b23a3151e96c4fed70d'
assert (abi/'baseline/Params.h').read_bytes()==obj(BASE,'src/core/Params.h')
assert (abi/'current/Params.h').read_bytes()==obj(REVIEW,'src/core/Params.h')
assert obj(MID,'src/core/Params.h')==obj(REVIEW,'src/core/Params.h')
assert obj(MID,'src/tools/preset_capture.cpp')==obj(REVIEW,'src/tools/preset_capture.cpp')
assert obj(BASE,'src/tools/preset_capture.cpp')==obj(REVIEW,'src/tools/preset_capture.cpp')
new_head=git('rev-parse','HEAD').decode().strip()
source_refs={rev:git('rev-parse',rev+'^{tree}').decode().strip() for rev in (BASE,MID,SOUND,REVIEW)}
result={
 'schema':1,'worker':56,'reviewed_source':REVIEW,'observed_head':new_head,'release_qualified':False,
 'scope':'Independent data, existing artifact integrity and composition review; no audio regeneration, native execution or listening assessment.',
 'source_trees':source_refs,
 'inventory':{'baseline_sha256':sha(before_bytes),'matched6094_sha256':sha(mid_bytes),'final_sha256':sha(final_bytes),'ordered_identities':523,'changed_existing_non_volume_non_policy_programs':len(changed),'unchanged_initialization_programs':sorted(init),'updated_tag_lists':tag_changed,'duplicate_non_volume_vectors':0,'body_enabled_programs':len(body_enabled),'circuit_only_programs':523-len(body_enabled),'active_models':dict(sorted(models.items())),'active_variants':dict(sorted(variants.items())),'changed_old_controls_min':min(old_new_counts),'changed_old_controls_median':statistics.median(old_new_counts),'changed_old_controls_max':max(old_new_counts)},
 'only_6094_to_final_control_changes':pad_deltas,
 'composed_main_summary_rederived':summary,
 'pressure_supplement_programs':a33['pressure_summary']['programs'],
 'musical_programs_with_changed_output_in_main_or_pressure_gesture':521,
 'replacement_input_wavs':pad_wavs,
 'core_archive_members_verified':core_records,
 'core_metadata_prefix':{'old_controls':200,'new_controls':202,'identical_rows':200,'normalization_points_per_control':257,'retained_stream_sha256':sha(old),'header_git_objects_match':True,'new_compilation_performed':False},
 'source_differences_6094_to_db6b':git('diff','--name-only',MID,REVIEW).decode().splitlines(),
 'source_differences_db6b_to_observed_head':git('diff','--name-only',REVIEW,new_head).decode().splitlines(),
 'retained_input_identities':records,
 'remaining_native_gates':['Authentic final source/run/attempt and complete native CTest/LastTest/JUCE qualification on all four targets.','cs80_parameter_state_index_tests: detached ValueTree mapping, not processor execution.','cs80_modern_recall_host_tests: actual processor prefix, variant host mapping, session/user-preset/A-B migration, all523 factory/MIDI recalls and next-note audio ownership.','cs80_typed_value_unit_tests: variant choice text and parser rejection.','cs80_ui_smoke: complete host/state/editor integration, with original successful process exit.','All13 original pluginval lanes and actual distributed-payload/source bindings.'],
 'limits':['Updated tags and descriptions are not frozen program identity fields.','Changed PCM/residual/spectrum and greater touch contrast are descriptive, not quality scores.','Two Init renders are exact only for the tested matched gestures; shared DSP changes can affect saved settings generally.','The historical A33 replay command needs its6094 inventory/executable, not current post-pad-trim paths.','Final native source must independently qualify current Python capture metadata assertions; this review does not erase their prior failure.']}
out=E/'verification/a56-accounting-review.json'
assert not out.exists()
out.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'passed':True,'source':REVIEW,'observed_head':new_head,'identities':523,'changed_musical_controls':len(changed),'changed_musical_output':521,'tag_updates':len(tag_changed),'body_enabled':len(body_enabled),'two_pad_inputs_verified':len(pad_wavs),'core_object_members':len(core_records),'composed_frames':summary['stereo_frames_per_version'],'native_qualified':False,'output':str(out)}))
