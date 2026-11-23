"""Read-only repository audit; all generated outputs stay in this scratch workspace."""
from pathlib import Path
from collections import Counter
import csv
import hashlib
import json
import re
import struct
import subprocess
from datetime import datetime, timezone

ROOT = Path('/workspace/scratch/dde4ad85ed5b')
REPO = ROOT / 'yamaha-cs80'
CONT_REL = 'docs/preset-evidence/2026.11.4/continuation'
CI_REL = 'docs/preset-evidence/2026.11.4/ci/source-078c098'
CONT = REPO / CONT_REL
CI = REPO / CI_REL
OUT = ROOT / 'a57'
SOURCE = '078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
HISTORICAL = '59d489d1a1f0065ae2a0f42a08c74edc43e94159'
ASOF = datetime.now(timezone.utc).isoformat()

def git(*args):
    return subprocess.check_output(['git', *args], cwd=REPO).decode().strip()

def sha(b):
    return hashlib.sha256(b).hexdigest()

def record(p):
    p = Path(p)
    b = p.read_bytes()
    d = {'path': str(p), 'bytes': len(b), 'sha256': sha(b)}
    if p.is_relative_to(REPO):
        d['repository_path'] = str(p.relative_to(REPO))
        d['tracked_at_checkpoint'] = subprocess.run(
            ['git', 'ls-files', '--error-unmatch', d['repository_path']],
            cwd=REPO, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
    return d

def write(name, data):
    (OUT / name).write_text(json.dumps(data, indent=2) + '\n')

def inspect_manifest(data):
    counts = Counter(e['path'] for e in data)
    issues = []
    for e in data:
        p = CONT / e['path']
        if not p.is_file():
            issues.append({'path': e['path'], 'issue': 'missing'})
            continue
        b = p.read_bytes()
        if len(b) != e['bytes'] or sha(b) != e['sha256']:
            issues.append({'path': e['path'], 'issue': 'bytes_or_hash_mismatch'})
    return {'entries': len(data), 'unique_paths': len(counts),
            'duplicates': [k for k,v in counts.items() if v > 1],
            'issues': issues}

ledger_path = ROOT / 'continuation-worker-ledger-draft.json'
ledger_bytes=ledger_path.read_bytes()
ledger=json.loads(ledger_bytes)
(OUT/'input-roster-at-final-audit.json').write_bytes(ledger_bytes)
actual_count = len(ledger['workers'])
assert actual_count == 60, 'Final actual roster must contain exactly 60 workers'
assert [w['number'] for w in ledger['workers']] == list(range(1, actual_count+1))
assert len({w['task'] for w in ledger['workers']}) == actual_count
head = git('rev-parse', 'HEAD')
committed_manifest_bytes = subprocess.check_output(['git','show',f'HEAD:{CONT_REL}/evidence-files.json'],cwd=REPO)
committed_manifest = json.loads(committed_manifest_bytes)
working_manifest = json.loads((CONT/'evidence-files.json').read_text())
manifest_result = {
    'as_of_utc': ASOF, 'checkpoint_commit': head,
    'committed_checkpoint': {**inspect_manifest(committed_manifest), 'manifest_sha256': sha(committed_manifest_bytes)},
    'working_tree': {**inspect_manifest(working_manifest), 'manifest': record(CONT/'evidence-files.json')},
    'scope': 'Only files below continuation/. Sibling ci/source-078c098 evidence is outside this manifest; the manifest excludes itself.',
}
historical_manifest_commit = '6426abaa2578a72d9c4df489828037fc9db8eece'
historical_manifest_bytes = subprocess.check_output(['git','show',f'{historical_manifest_commit}:{CONT_REL}/evidence-files.json'],cwd=REPO)
manifest_result['earlier_192_entry_checkpoint'] = {**inspect_manifest(json.loads(historical_manifest_bytes)), 'commit':historical_manifest_commit, 'manifest_sha256':sha(historical_manifest_bytes)}
manifest_result['prepublication_snapshot_only'] = True
manifest_result['final_index_policy'] = 'This records prepublication manifest bytes at the named checkpoint. Root will build final indexes after copying reports. No final manifest hash or report-self hash is asserted.'
all_cont = {str(p.relative_to(CONT)) for p in CONT.rglob('*') if p.is_file()}
manifest_result['working_tree']['unindexed_files'] = sorted(all_cont - {x['path'] for x in working_manifest} - {'evidence-files.json'})
manifest_result['committed_checkpoint']['new_files_added_after_checkpoint'] = sorted({x['path'] for x in working_manifest} - {x['path'] for x in committed_manifest})
manifest_result['final_inventory_retention'] = []
for name in ['inventory.json', 'parameter-coverage.json']:
    p = CONT/'final-inventory'/name
    manifest_result['final_inventory_retention'].append({**record(p), 'identical_to_original_scratch_bytes': p.read_bytes()==(ROOT/'evidence/final'/name).read_bytes()})
manifest_result['final_inventory_provenance'] = record(CONT/'final-inventory/provenance.json')
assert not manifest_result['committed_checkpoint']['issues']
assert not manifest_result['working_tree']['issues']
assert not manifest_result['working_tree']['unindexed_files']
assert all(x['identical_to_original_scratch_bytes'] for x in manifest_result['final_inventory_retention'])
write('manifest-check.json',manifest_result)

changed_committed = git('diff','--name-only',SOURCE,'HEAD').splitlines()
changed_worktree = git('diff','--name-only','HEAD').splitlines()
untracked = git('ls-files','--others','--exclude-standard').splitlines()
ci_files = sorted(str(p.relative_to(REPO)) for p in CI.rglob('*') if p.is_file())
ci_group_counts = Counter('/'.join(p.split('/')[5:6]) for p in ci_files)
source_result = {
    'as_of_utc': ASOF, 'binary_source': SOURCE, 'checkpoint_commit': head,
    'committed_paths_changed_after_binary_source': len(changed_committed),
    'committed_non_documentation_changes': [p for p in changed_committed if p!='README.md' and not p.startswith('docs/')],
    'tracked_worktree_non_documentation_changes': [p for p in changed_worktree if p!='README.md' and not p.startswith('docs/')],
    'unchanged_source_trees': {},
    'final_inventory_export_to_binary_paths_identical': {},
    'ci_tree': {'file_count': len(ci_files), 'groups': dict(sorted(ci_group_counts.items())), 'uncommitted_files': [p for p in untracked if p.startswith(CI_REL+'/')], 'outside_continuation_manifest': True},
    'other_untracked_paths': [p for p in untracked if not p.startswith('docs/')],
    'scope': 'Source/tree and retained-file inspection. No builds, audio renders, native package requalification, source edits, Git mutations or external writes.'
}
for scope in ['src/core','src/plugin','src/reference','tests','scripts','CMakeLists.txt','resources','.github']:
    source_result['unchanged_source_trees'][scope] = not git('diff','--name-only',SOURCE,'HEAD','--',scope)
for scope in ['src/core','src/plugin','src/tools/preset_capture.cpp','scripts/capture-factory-auditions.py']:
    source_result['final_inventory_export_to_binary_paths_identical'][scope] = not git('diff','--name-only','e32728b31d97fc356cac291cb98935fdcd38f803',SOURCE,'--',scope)
assert not source_result['committed_non_documentation_changes']
assert not source_result['tracked_worktree_non_documentation_changes']
assert all(source_result['unchanged_source_trees'].values())
assert all(source_result['final_inventory_export_to_binary_paths_identical'].values())
write('source-preservation.json', source_result)

baseline_path = REPO/'docs/preset-evidence/2026.11.4/baseline/inventory.json'
current_path = CONT/'final-inventory/inventory.json'
baseline = json.loads(baseline_path.read_text())
current = json.loads(current_path.read_text())
f32 = lambda v: struct.pack('<f',v)
identity = lambda p: (p['index'],p['host_index'],p['name'],p['category'],p['tags'])
assert len(baseline['programs'])==len(current['programs'])==523
assert baseline['parameter_ids']==current['parameter_ids'][:198]
assert current['parameter_ids'][198:]==['body1_size','body2_size']
ids={name:i for i,name in enumerate(current['parameter_ids'])}
changed = []
inits = []
model_sets = {2:set(),9:set()}
model_ranks = Counter()
fingerprints = set()
identities_same = True
for b,p in zip(baseline['programs'],current['programs']):
    identities_same &= identity(b)==identity(p)
    changes=sum(f32(b['values'][i])!=f32(p['values'][i]) for i in range(198) if i!=ids['volume'])
    fingerprint=tuple(f32(v) for i,v in enumerate(p['values']) if i!=ids['volume'])
    fingerprints.add(fingerprint)
    if p['category']=='Init':
        inits.append({'name':p['name'],'host_index':p['host_index'], 'old_198_values_bit_identical': all(f32(a)==f32(c) for a,c in zip(b['values'],p['values'][:198])), 'appended_sizes':p['values'][198:]})
    else:
        changed.append(changes)
    for model in model_sets:
        for rank in (1,2):
            if p['values'][ids[f'body{rank}_model']]==model and p['values'][ids[f'body{rank}_mix']]>0:
                model_sets[model].add(p['host_index']);model_ranks[model]+=1
assert identities_same and len(inits)==2 and all(x['old_198_values_bit_identical'] for x in inits)
assert len(changed)==521 and min(changed)>0 and len(fingerprints)==523
assert not model_sets[2]&model_sets[9]

regressions={}
for key,rel in [('tine_before','a06/tine-dynamics-before.log'),('tine_after','a06/tine-dynamics.log'),('bowed_before','a10/baseline-bowed-tests.log'),('bowed_after','a10/bowed-tests.log')]:
    text=(CONT/rel).read_text()
    match=re.search(r'(\d+) checks, (\d+) failures\s*$',text)
    assert match
    regressions[key]={'checks':int(match[1]),'failures':int(match[2]),'file':record(CONT/rel)}
assert (regressions['tine_before']['checks'],regressions['tine_before']['failures'])==(486,162)
assert (regressions['tine_after']['checks'],regressions['tine_after']['failures'])==(486,0)
assert (regressions['bowed_before']['checks'],regressions['bowed_before']['failures'])==(492,174)
assert (regressions['bowed_after']['checks'],regressions['bowed_after']['failures'])==(492,0)

with (CONT/'a09/metrics.csv').open() as f: candidate=list(csv.DictReader(f))
with (CONT/'a09/baseline-parity-metrics.csv').open() as f: parity=list(csv.DictReader(f))
case=lambda x:(x['name'],x['note'],float(x['velocity']))
cmap={case(x):x for x in candidate}
assert len(candidate)==len(cmap)==90
assert len(parity)==45 and all(cmap[case(x)]['audio_fnv1a64']==x['audio_fnv1a64'] for x in parity)

audio_path=REPO/'docs/preset-evidence/2026.11.4/audio-comparison.json'
audio=json.loads(audio_path.read_text())
hash_matches={p:sha(subprocess.check_output(['git','show',f'{HISTORICAL}:{p}'],cwd=REPO))==h for p,h in audio['source_provenance']['after_source_sha256_verified'].items()}
assert len(hash_matches)==24 and all(hash_matches.values())
claim_result={
    'as_of_utc':ASOF,
    'inventory':{'identities':523,'musical_programs':521,'init_programs':inits,'ordered_identity_fields_unchanged':identities_same,'previous_core_ids':198,'current_core_ids':200,'musical_changed_existing_nonvolume_count_range':[min(changed),max(changed)],'duplicate_nonvolume_vectors':523-len(fingerprints),'inputs':[record(baseline_path),record(current_path)]},
    'configured_effect_reach':{'tine_programs':len(model_sets[2]),'tine_ranks':model_ranks[2],'bowed_programs':len(model_sets[9]),'bowed_ranks':model_ranks[9],'overlap':sorted(model_sets[2]&model_sets[9]),'union':len(model_sets[2]|model_sets[9]),'qualification':'Configured positive body mix, not 68 listening tests or equal benefit at every playing condition.'},
    'regressions':regressions,
    'sample_comparisons':[
        {'worker':6,'samples':(18*3+1)*3*5*3*4096,'comparison':'binary32 bit comparison','scope':'Full-strength tine and other 18 enabled models at specified velocities/rates/notes/colours; retained summary reports zero differences; no invented standalone stdout.'},
        {'worker':10,'samples':20*3*sum(int(1.6*2*r) for r in (44100,48000,96000)),'comparison':'numeric float inequality and maximum absolute difference, not bitwise','scope':'Fixed-pitch bowed and other models under seven-semitone slides; retained final log reports zero unequal values.'},
        {'worker':9,'cases':len(parity),'samples_per_case':96000,'samples':len(parity)*96000,'comparison':'matching FNV1a64 audio hashes','mismatches':0,'scope':'45 selected complete-preset endpoint/control cases; 90-case candidate dynamics sweep covers five complete presets.'}
    ],
    'historical_full_library_audio':{'source':HISTORICAL,'source_manifest_entries':len(hash_matches),'all_source_hashes_match':all(hash_matches.values()),'musical_programs':audio['musical_programs_using_pressure_where_required']['programs'],'spectral_changed':audio['musical_programs_using_pressure_where_required']['metric_group_changes']['spectral'],'envelope_changed':audio['musical_programs_using_pressure_where_required']['metric_group_changes']['envelope'],'scope':'Historical 59d489d comparison including pressure-one exceptions; no final-078 all-library matched rerender or human listening claim.'},
    'audition_scope':'Actual archive acceptance is bound below to A58; representative captures remain distinct from whole-catalogue inventory and human listening.',
    'limits':['Source routing, signal metrics and selected regression coverage do not prove universal subjective improvement or exact acoustic equivalence.','Bowed-only stationary formants do not repair brass/wind/plucked glides.','Sustained-body partial-decay literals and source literals replaced by full body mix must not count as audible changes.','A26/A28 lifecycle probe overlap is disclosed; their counts are not independent native release gates.','Direct-body sample counts and full-preset stereo sample counts have distinct scopes and are not added as one score.']
}
write('claim-checks.json',claim_result)

def inspect_retention(manifest_path, field, base):
    data=json.loads(manifest_path.read_text()); entries=data[field]; issues=[]
    for e in entries:
        p=Path(e['path']); p=p if p.is_absolute() else base/p
        if not p.is_file():
            issues.append({'path':str(p),'issue':'missing'});continue
        b=p.read_bytes()
        if len(b)!=e['bytes'] or sha(b)!=e['sha256']:
            issues.append({'path':str(p),'issue':'bytes_or_hash_mismatch'})
    result={'manifest':record(manifest_path),'entries':len(entries),'issues':issues,'scope':data.get('scope',data.get('retention_note',data.get('status')))}
    assert not issues, result
    return result

retention_result={
    'a53':inspect_retention(ROOT/'a53/retention-manifest.json','files',ROOT),
    'a58':inspect_retention(ROOT/'a58/retention-manifest.json','small_evidence_files',ROOT),
    'supporting_dsp':inspect_retention(CI/'supporting-dsp/retained-files.json','files',CI/'supporting-dsp'),
    'scope':'Validate exact manifest-listed bytes. These manifests have their own explicit coverage; they are not inferred complete indexes of all sibling evidence.'
}
write('retention-checks.json',retention_result)
author53=json.loads((ROOT/'a53-summary.json').read_text())
author56=json.loads((ROOT/'a56-review.json').read_text())
author58=json.loads((ROOT/'a58-review.json').read_text())
author59=json.loads((ROOT/'a59/a59-review.json').read_text())
author60=json.loads((ROOT/'a60-review.json').read_text())
assert author53['linux_scope_accepted'] and author53['native']['counts']['registered']==587
assert author56['complete_assigned_windows_linux_scope'] and author56['verified_original_validator_lanes']==5 and author56['verified_payload_bindings']==2
assert author58['accepted'] and not author58['blockers_in_this_scope']
assert not author59['complete_assigned_scope'] and not author60['final_release_review_complete'], 'A59/A60 advanced: read their new final reports before updating this audit'
native51=json.loads((ROOT/'a51-summary.json').read_text())
native52=json.loads((ROOT/'a52-review.json').read_text())
native54=json.loads((ROOT/'a54-review.json').read_text())
validators55=json.loads((ROOT/'a55-review.json').read_text())
native_rows=[
    {'platform':'Windows x86_64',**native52['native']['counts'],'nested_python_skips':native52['native']['nested_python_skips']['total'],'author':'A52'},
    {'platform':'macOS arm64',**native51['native_totals'],'nested_python_skips':native51['nested_python_skips']['total'],'author':'A51'},
    {'platform':'macOS x86_64',**native54['native_counts'],'nested_python_skips':native54['nested_python_skips_total'],'author':'A54'},
    {'platform':'Linux x86_64',**author53['native']['counts'],'nested_python_skips':author53['native']['nested_python_skips']['total'],'author':'A53'},
]
native_totals={key:sum(row[key] for row in native_rows) for key in ['registered','passed','failed','formal_skipped','disabled','unknown','nested_python_skips']}
assert native_totals=={'registered':1318,'passed':1311,'failed':0,'formal_skipped':7,'disabled':0,'unknown':0,'nested_python_skips':24}
accepted_scopes={
    'source':SOURCE,'run_id':45246863788,'run_attempt':1,
    'native_platforms':native_rows,'native_totals':native_totals,
    'validators':{'required_lanes':validators55['validator_totals']['required_lanes']+author56['required_original_validator_lanes'],'qualified_lanes':validators55['validator_totals']['successful_lanes']+author56['verified_original_validator_lanes'],'invocations':validators55['validator_totals']['validation_invocations']+author56['total_original_validator_invocations'],'validation_retries':validators55['validator_totals']['validation_retries']+author56['total_validation_retries'],'windows_linux_complete_payload_bindings':author56['verified_payload_bindings'],'scope':'Arithmetic crosscheck of authored, source-bound A55/A56 results; no new validator execution or requalification.'},
    'audition':{'artifact_id':author58['artifact']['id'] if 'id' in author58['artifact'] else author58['artifact'].get('artifact_id'), 'author_report':record(ROOT/'a58-review.json'),'observed':author58['observed'],'inventory_comparison':author58['inventory_comparison'],'scope':'Actual archive accepted by A58. Only 74 programs have selected WAV captures; 523 identities and 104600 values are inventoried. No human listening claim.'},
    'separate_supporting_sanitizer_scope':'A48 reports 199 passed / 200 registered, one formal skip. These are not native installation totals; suppressed successful stdout leaves nested Python skip count unknown.',
    'release_qualified_by_this_audit':False,
    'final_publication_reviews_pending':[59,60]
}
assert accepted_scopes['validators']['qualified_lanes']==13 and accepted_scopes['validators']['invocations']==13 and accepted_scopes['validators']['validation_retries']==0
write('accepted-scope-crosscheck.json',accepted_scopes)
claim_result['audition_actual']=accepted_scopes['audition']
write('claim-checks.json',claim_result)

# Each row's author report is kept separate from corroboration by other workers.
rows = {
1: ('a01-findings.md','Recovered prior release state and exact-source publication gates; identified stale saved-session wording.','Read-only continuity review; then-current CI observations are historical.'),
2: (None,'Identified per-strike tine L1 normalization that suppressed fundamental and velocity contrast.','Root-attested conversational completion; A06 implementation and A09 measurements corroborate the issue, not authorship.'),
3: (None,'Audited library reach, inert sustained partial decay, and onset-bound or pitch-following formants.','Root-attested conversational completion; A10, A32/A43 and A38 corroborate downstream work and limits.'),
4: (None,'Corrected release-contract saved-session wording: stored values do not freeze DSP; prior body controls restore and missing sizes default.','Root-attested conversational completion; committed contract/history corroborate the edit, not a recovered transcript.'),
5: ('a05/FINDINGS.md','Measured 90 baseline complete-preset cases; response stayed monotonic but tine dynamics were compressed.','Five presets, three notes, six velocities; no acoustic or listening score.'),
6: ('a06/summary.json','Implemented fixed-bound tine normalization; 486/486 checks pass versus 162 old failures; selected direct output bit parity preserved.','10,137,600 binary32 sample comparisons are a direct-model scope; full-preset checks are A09.'),
7: ('a07-review.md','Reviewed all 24 curated keyboard recipes; found no additional definite routing or articulation defect.','Named instrument models remain authored approximations; no listening proof.'),
8: ('a08-texture-design.md','Reviewed texture architecture and primary references; deferred unsupported per-family cutoff rewrite.','Proposed numerical cutoffs are unshipped estimates; no claimed production noise redesign.'),
9: ('a09/FINDINGS.md','Verified 90 corrected complete-preset cases and 45 matching parity hashes; tine body contrast expands about 3.9–4.0 dB.','Detailed dynamics target three tine presets plus two controls; configured reach is 17 presets.'),
10: ('a10/FINDINGS.md','Implemented stationary bowed-body resonance response; 492/492 checks pass versus 174 old failures.','36,115,200 numerically equal samples are not a bitwise assertion; response remains authored, bowed only.'),
11: ('a11-review.md','Reviewed all 14 curated winds; no additional definite recipe defect; identified inert sustained-body decay literals.','The bowed-only correction does not fix wind formant motion; no acoustic equivalence.'),
12: ('a12-review.md','Reviewed all 28 curated string recipes and texture balance; retained deliberate controller/articulation choices.','Sustained-body decay controls are inert; fixed formants handled by A10; no universal realism claim.'),
13: ('a13-review.md','Reviewed all 32 curated brass recipes; no additional definite routing, octave or articulation defect.','Single brass model and authored size are not calibrated trumpet/horn/tuba models; brass glide limit remains.'),
14: ('a14-review.md','Reviewed all 14 organ recipes and measured intended velocity neutrality.','Registration and selected capture checks do not establish measured acoustic organ identity.'),
15: ('a15-review.md','Reviewed all 16 plucked recipes and tested selected Kalimba behavior.','Kalimba retains EP-like excitation; generic plucked models are not calibrated acoustic references.'),
16: ('a16-review.md','Reviewed all 22 bell/mallet recipes; found no accidental route or articulation error.','Xylophone retains marimba mode profile; Steel Drum retains vibraphone-based approximation.'),
17: ('a17-review.md','Reviewed all 30 pads and measured Pressure Pad response.','Circuit-only recipe review; single-note pressure fixture does not replace library headroom gates.'),
18: ('a18-review.md','Reviewed all 24 polysynth recipes and targeted velocity, pressure, stab and pluck captures.','Circuit-only recipes; six selected captures, not broad listening validation.'),
19: ('a19-review.md','Reviewed all 24 bass recipes, measured short captures and Pedal Bass velocity identity.','Generic bass models and Fretless Glide formant limit remain; short fixtures do not prove every performance.'),
20: ('a20/FINDINGS.md','Measured 112 full-Engine timing runs in 56 pairs with eight active voices.','Shared-machine ratios vary; no unchanged-cost or DAW realtime guarantee.'),
21: ('a21-review.md','Reviewed all 30 leads and intentional pressure behavior; no additional definite route defect.','Lead names describe musical role; retained eight-voice mode is not newly monophonic.'),
22: ('a22-review.md','Reviewed all 24 FX and measured Wind entrance-description reversal.','Nature FX remain synthesized filtered-noise gestures; description correction follows A25.'),
23: ('a23-review.md','Reviewed all 18 Modern recipes, modulation and effects routes.','Source/inventory/math review; no new perceptual-equivalence evidence.'),
24: ('a24-review.md','Reviewed all 222 original addresses, inheritance and finish variants; confirmed stable identities.','Distinct non-volume vectors do not prove distinct perceived quality; inherited model limits remain.'),
25: ('a25-summary.md','Corrected Wind description and guide; original inherited description follows current recipe.','Only one source string changed; no numeric recipe, DSP, name, tag or order change.'),
26: ('a26-review.md','Reviewed state/lifecycle paths and scoped ASan/UBSan lifecycle probe: 240 cases, 983,100 sample comparisons.','No diagnostics in that header-only probe; not native host qualification.'),
27: ('a27-review.md','Verified LOAD/RELOAD current-factory behavior and saved-state recipe preservation.','Known VST3 processing-queue-only Program limitation remains; no arbitrary DAW certification.'),
28: ('a28-review.md','Reviewed numerical bounds, invalid controls and timebase with scoped sanitizer probe.','Lifecycle cases overlap A26; not a separate independent native release gate.'),
29: ('a29-review.md','Reviewed GCC/Clang/MSVC portability, array bounds, stack and fixed storage assumptions.','Static source review; actual native builds and hosts require separate evidence.'),
30: ('a30-summary.md','Updated install/preset guides for explicit current factory RELOAD and old-project behavior.','Prose-only change; bundled guide identity must follow actual package source.'),
31: ('a31-review.md','Independently read all-523 bank run and seven affected test executables; unchanged thresholds confirmed.','Eight local CTest executions; truncated internal stdout prevents invented assertion totals.'),
32: ('a32-review.md','Independently compared inventories: 523 identities, two unchanged Init programs, 521 musical non-volume changes.','Parameter changes are structural coverage, not universal audible benefit.'),
33: ('a33-review.md','Reviewed audition contract of 138 WAVs, 74 programs and 870 seconds.','Contract and source inspection only; actual current ZIP qualification remains separate.'),
34: ('a34-review.md','Reviewed verifier contracts and exercised existing self-tests.','Tools establish separate scopes; self-tests do not qualify actual native artifacts.'),
35: ('a35-review.md','Enumerated four native packages and all 13 pluginval lanes with producer/payload requirements.','Checklist of required evidence, not observed CI success.'),
36: ('a36-review.md','Bound all-521 matched spectral/envelope comparison to historical 59d489d.','Historical measured differences are neither final-078 matched renders nor listening evidence.'),
37: ('a37-review.md','Proposed explicit historical-versus-current labels and separate continuation ledger.','Historical records remain intact; wording proposals are not new verification runs.'),
38: ('a38-review.md','Reviewed family-specific acoustic/model limits and appropriate release claims.','No exact named-instrument reproduction, universal realism or indistinguishability established.'),
39: ('a39-review.md','Found earlier manifest omissions and missing final bowed timing raw log.','Historical finding resolved by retained log and expanded manifest; old failure remains documented.'),
40: ('a40-review.md','Corrected host-recall expected float after authentic screenshot failure.','Test expectation matches canonical JUCE roundtrip; no production sound fix.'),
41: ('a41-review.md','Pinned changed visible controls to prior fixture values for panel-layout goldens.','No golden PNG refresh, tolerance change, UI layout or production audio edit.'),
42: ('a42-review.md','Independently reviewed both UI fixture corrections and retained images.','Source review itself was not a native UI pass; A50 later qualifies actual run.'),
43: ('a43-review.md','Verified final-source recipe/identity/automation preservation and production-tree equivalence.','Source equivalence does not imply separately compiled binaries are byte-identical.'),
44: ('a44-review.md','Reviewed four-platform package/install instructions and explicit reload guidance.','Source contract; actual artifact acceptance belongs to platform workers.'),
45: ('a45-review.md','Prepared parser distinguishing CTest results, formal skips, nested Python skips and validator logs.','Parser is not an automatic release qualifier and does not infer producer binding.'),
46: ('a46-review.md','Qualified six supporting filter jobs with 24 passed CTest executions and 91 unchanged inputs.','Actually executed bcc86aa; exact input equivalence supports 078 without relabeling executed source.'),
47: ('a47-review.md','Recomputed numeric reach, dynamics, parity and runtime claims.','Independent arithmetic/source-scope audit; no rerender or listening claim.'),
48: ('a48-review.md','Qualified five original supporting DSP jobs and six archives; sanitizer 199 passed of 200 registered, one formal skip.','No reported sanitizer diagnostics; supporting workflow is separate from native packages.'),
49: ('a49-review.md','Reviewed final release prose against source/evidence and remaining acoustic limits.','Then-current download block was still prior qualified release; not a new publication verification.'),
50: ('a50-review.md','Qualified exact-source screenshot workflow and actual image archive, including unchanged goldens.','Original failed precursor run remains failed; no native audio or realism claim.'),
51: ('a51-review.md','Qualified actual Apple Silicon installation archive and original native test evidence.','251 registered, 249 passed, two formal skips, five nested Python skips; one platform scope.'),
52: ('a52-review.md','Qualified actual Windows installation archive and original native test evidence.','229 registered, 227 passed, two formal skips, eleven nested Python skips; validators separate.'),
53: ('a53-review.md','Qualified actual Linux installation archive and original native test evidence.','587 registered, 586 passed, one formal skip and three nested Python skips; validators and audition separately owned.'),
54: ('a54-review.md','Qualified actual Intel Mac installation archive and original native test evidence.','251 registered, 249 passed, two formal skips, five nested Python skips; stale A56 handoff label is superseded by A55.'),
55: ('a55-review.md','Qualified eight original Mac validator lanes and distributed payload identity.','Eight invocations, zero validation retries, normal exit zero; whole release remains separate.'),
56: ('a56-review.md','Qualified all five original Windows/Linux validator lanes and both complete distributed VST3 payload bindings.','Five original invocations, zero validation retries; original final process outcomes bound; overall release separately owned.'),
57: ('a57-review.md','Completed actual-60 roster, retained-evidence, source-preservation and claim-scope audit.','A59/A60 final deliveries and root status closure follow this report; no self-hash or unavailable final result is asserted.'),
58: ('a58-review.md','Accepted actual audition ZIP: 138 WAVs, 74 programs, 870 seconds; exact 523-by-200 inventory identity.','All 173 entries inspected, 172 checksums and 83,520,000 scalar samples checked; no listening or full-catalogue audio claim.'),
59: ('a59/a59-review.md','Completed preliminary three-platform download mapping and publication review; final all-platform candidate review is pending.','Actual worker with authored checkpoint; final mapping/publication delivery must be observed before status closure.'),
60: ('a60-review.md','Completed preliminary independent release review without a new blocker in then-completed scopes; final acceptance is pending.','Actual worker with authored checkpoint; current Linux/audition/aggregate/publication candidate follow-up remains subsequent.')
}

retained_override={
    5:CONT/'a05/FINDINGS.md',6:CONT/'a06/summary.json',9:CONT/'a09/FINDINGS.md',10:CONT/'a10/FINDINGS.md',20:CONT/'a20/FINDINGS.md',
    48:CI/'supporting-dsp/FINDINGS.md',51:CI/'macos-arm64/a51-review.md',52:CI/'windows-x86_64/a52-review.md',
    53:CI/'linux-x86_64/FINDINGS.md',54:CI/'macos-x86_64/FINDINGS.md',55:CI/'macos-validators/FINDINGS.md',
    56:CI/'windows-linux-validators/FINDINGS.md',58:CI/'audition/FINDINGS.md',
    59:CI/'final-review/a59-review.md',60:CI/'final-review/a60-review.md'
}
corroboration={
2:[CONT/'a06/summary.json',CONT/'a09/FINDINGS.md'],
3:[CONT/'a10/FINDINGS.md',CONT/'a32/summary.json',CONT/'reviews/a38-review.md',CONT/'reviews/a43-summary.json'],
4:[REPO/'docs/release/RELEASE_QUALIFICATION.md']
}
normalized=[]
for worker in ledger['workers']:
    number=worker['number']; name,outcome,scope=rows[number]
    row={**worker,'root_recorded_status_at_snapshot':worker['status'],'outcome':outcome,'scope_and_limits':scope,
         'existence_and_completion_provenance':'Root-maintained actual-spawn ledger and retained conversational continuity; historical spawn API receipts were not available to this audit.'}
    row['audit_observed_stage']='final_delivery_pending' if number in (59,60) else ('this_audit_delivery' if number==57 else 'assigned_scope_completed')
    row['planned_published_primary_path']=str((CI/'final-review/a57-review.md').relative_to(REPO)) if number==57 else (str(retained_override[number].relative_to(REPO)) if number in retained_override else (str((CONT/'reviews'/Path(name).name).relative_to(REPO)) if name else None))
    if number in (2,3,4):
        row.update({'report_evidence_class':'root_attested_conversational_completion','primary_report':None,
                    'transcript_retained_on_disk':False,'corroboration_not_authored_reports':[record(p) for p in corroboration[number]],
                    'attestation':'Root confirmed these were actual completed continuation agents and supplied their outcomes to A57. No original transcript is invented or recreated.'})
    elif number==57:
        row.update({'report_evidence_class':'current_final_audit_report','primary_report':{'path':str(ROOT/name),'self_hash_intentionally_omitted':True},
                    'subsequent_actions':'A59/A60 final delivery and root-owned final status closure/evidence-index generation. No claim about those unavailable final outcomes.'})
    elif number in (53,56,58,59,60):
        scratch=ROOT/name;retained=retained_override[number]
        row.update({'report_evidence_class':'authored_final_scope_report_pending_publication' if number in (53,56,58) else 'authored_preliminary_report_final_delivery_pending',
                    'primary_report':record(scratch),'published_path_exists_at_snapshot':retained.is_file(),
                    'snapshot_report_stage':'accepted_assigned_scope' if number in (53,56,58) else 'preliminary_checkpoint'})
        if retained.is_file():
            row['existing_published_copy']=record(retained)
            row['published_copy_matches_observed_report']=retained.read_bytes()==scratch.read_bytes()
        if number in (59,60):row['late_report_hash_policy']='Hash identifies the observed preliminary scratch checkpoint only. The planned published primary path is for the subsequent final delivery; it does not assert that unavailable final bytes match this checkpoint.'
    else:
        scratch=ROOT/name
        retained=retained_override.get(number,CONT/'reviews'/Path(name).name)
        if retained.is_file():
            row.update({'report_evidence_class':'retained_authored_report','primary_report':record(retained)})
            if scratch.is_file():
                row['scratch_report']=record(scratch)
                row['retained_matches_scratch_bytes']=retained.read_bytes()==scratch.read_bytes()
                if number==10 and not row['retained_matches_scratch_bytes']:
                    assert retained.read_text().startswith(scratch.read_text())
                    assert retained.read_text()[len(scratch.read_text()):].startswith('\n## Raw-log provenance\n')
                    row['report_evidence_class']='retained_authored_report_with_provenance_addendum'
                    row['retained_copy_note']='Retained copy preserves the entire original authored report and appends a Raw-log provenance section distinguishing final and earlier timing logs. Both copy identities are recorded; the addendum is not claimed as original author text.'
        else:
            assert scratch.is_file(),(number,str(scratch))
            row.update({'report_evidence_class':'scratch_authored_pending_report','primary_report':record(scratch)})
    normalized.append(row)

initial=json.loads((REPO/'docs/preset-evidence/2026.11.4/work-log.json').read_text())
counts=Counter(w['root_recorded_status_at_snapshot'] for w in normalized)
assert sum(w['audit_observed_stage']=='assigned_scope_completed' for w in normalized)==57
for w in normalized:
    w['status']='completed' if w['number']<=58 else 'final_review_in_progress'
    w['completion_basis']='This delivered A57 audit; root closure is subsequent' if w['number']==57 else ('Actual assigned scope report or explicit root-attested conversational completion' if w['number']<=58 else 'Actual authored preliminary checkpoint exists; final delivery not yet observed')
draft={
    'schema':2,'status':'all_60_actual_workers_58_delivered_scopes_A59_A60_final_delivery_subsequent','as_of_utc':ASOF,
    'binary_source':SOURCE,'checkpoint_commit':head,'input_ledger_snapshot':record(OUT/'input-roster-at-final-audit.json'),
    'requested_workers':60,'recorded_workers_so_far':60,'recorded_actual_continuation_workers':60,
    'completed_assigned_scopes_including_this_A57_delivery':58,'final_reviews_in_progress':[59,60],
    'unspawned_requested_workers':0,'unspawned_numbers':[],
    'root_recorded_completed_at_snapshot':counts['completed'],'root_exact_status_counts':dict(counts),
    'all_60_final_deliveries_complete_claimed':False,
    'closure_policy':'All 60 rows are actual agents. A59/A60 final delivery and root-owned status closure are subsequent. This audit does not predict their final acceptance or publication.',
    'self_hash_policy':'No A57 self hash or future final-index hash is embedded. Root generates final indexes after report copying and factual status closure. Late-report hashes identify observed preliminary checkpoints only.',
    'concurrency':'At most three worker agents alongside root, launched in batches; 60 agents does not mean 60 simultaneous workers.',
    'historical_spawn_provenance':'Root actual-spawn roster and retained conversation. Historical spawn API receipts are not reconstructed. Authored checkpoints corroborate A59/A60 work.',
    'excluded_prior_stage':{'path':'docs/preset-evidence/2026.11.4/work-log.json','rows':len(initial['workers']),'counted_toward_continuation':0,'scope':'Entire prior 2026.11.4-stage roster excluded, including completed and planned/ready rows.'},
    'report_evidence_counts':dict(Counter(w['report_evidence_class'] for w in normalized)),
    'workers':normalized
}
(ROOT/'continuation-worker-ledger-normalized-a57.json').write_text(json.dumps(draft,indent=2)+'\n')

report=f'''# A57 — final actual-60 roster and evidence audit

**All 60 continuation workers are actual recorded agents with unique assignments. A57's audit is complete. The ledger records 58 delivered scopes including this report; A59/A60 final delivery and root-owned final status closure are subsequent.**

Binary source: `{SOURCE}`. Inspected documentation checkpoint: `{head}`. Audit snapshot: `{ASOF}`. This supersedes the earlier A57 58-worker checkpoint. No original CI/artifact gate remains pending in this audit; final reviewer delivery, status closure and publication are later steps.

## Roster provenance and completion

The actual roster contains exactly 60 sequential numbers and 60 unique task paths. The entire **53-row prior-stage .4 work-log** is excluded. Follow-up turns reuse agents and do not create extra workers. Concurrency is at most three workers alongside root, not 60 simultaneous workers.

Author reports or explicit root-attested conversational completion establish 57 completed assigned scopes before this A57 delivery; this report is the 58th. A59/A60 are actual agents with authored preliminary reports, and their final reviews are in progress. The captured root ledger has {counts['completed']} literal `completed` rows; the normalized file preserves every original root status separately from the evidence-based current stage. Root will close final counts only after actual A59/A60 delivery.

**A02/A03/A04 remain root-attested conversational completions.** Their original standalone reports/transcripts are unavailable. A06/A09 corroborate A02's finding; A10/A32/A38/A43 corroborate A03; the committed release contract/history corroborate A04. Later corroboration is not represented as original author evidence. A10's retained report contains its original text plus a later raw-log-provenance addendum; both identities and the authorship distinction are recorded.

## Completed native, validator and actual-audition scopes

A53's final report accepts the actual Linux package and original native test evidence. A56 accepts all five original Windows/Linux validator lanes and both complete distributed VST3 payload bindings; its earlier Windows-only checkpoint remains historical. A58 accepts the actual audition archive. These outcomes replace their earlier waiting/partial stages without erasing them.

| Native target | Passed / registered | Formal CTest skips | Separate nested Python skips |
|---|---:|---:|---:|
| Windows x86_64 | 227 / 229 | 2 | 11 |
| macOS arm64 | 249 / 251 | 2 | 5 |
| macOS x86_64 | 249 / 251 | 2 | 5 |
| Linux x86_64 | 586 / 587 | 1 | 3 |
| **Total** | **1,311 / 1,318** | **7** | **24** |

All four native scopes have zero failed, disabled and unknown CTest results. Nested Python skips occur inside CTest executions and are not subtracted again. Linux's one formal skip is `cs80_stereo_pra_exact_tests` with return code 77. The supporting sanitizer run remains separate: **199 passed / 200 registered, one formal skip**; suppressed successful stdout leaves its nested Python skip count unknown.

A55's eight Mac lanes and A56's five Windows/Linux lanes total **13 qualified original strictness-10 lanes, 13 invocations and zero validation retries**, each with 523 programs. Original source/process/payload binding supports acceptance; a SUCCESS string alone is not the gate. Existing warnings and optional external SDK-validator notices remain recorded.

A58's actual ZIP has **173 regular files, 172 matching checksums, 138 WAVs / 74 programs / 870 seconds**, and **41,760,000 stereo frames / 83,520,000 scalar samples**. No nonfinite or above-full-scale samples were found. Its delivered inventory is byte-identical to the retained original export: all **523 identities, 200 parameter IDs and 104,600 binary32 values** match. This is actual archive acceptance, not just the earlier producer contract; it is not human listening or whole-catalogue WAV coverage.

## Retained bytes and source preservation

The committed **195-entry continuation manifest at {head[:8]} passes** every byte-count and SHA-256 check. The working manifest passes with no missing, duplicate, mismatched or unindexed files in its scope. The earlier **192-entry checkpoint at 6426abaa** also matches its recorded files. The retained current inventory/coverage preserve their original e32728b labels and leave historical development files intact.

The A53 **31-file** retention manifest, A58 **22-file** retention manifest, and supporting-DSP **{retention_result['supporting_dsp']['entries']}-file** index all pass their exact-byte checks. Sibling CI evidence is outside the continuation manifest. Root is retaining new final records and will create final scoped indexes after all reports and statuses are stable; no unavailable final index is claimed verified here.

All **{len(changed_committed)} committed paths changed after 078c098 through {head[:8]}** are README/documentation/evidence/screenshots. No committed or tracked working-tree production, test, script or workflow change was found. Core/plugin/reference, tests, scripts, CMake, resources and workflows are unchanged from the binary source. Relevant export inputs are unchanged e32728b to 078c098.

Independent binary32 inventory comparison confirms **523 preserved ordered identities** with **521 musical programs** and two Init programs: **Init Patch (43)** and **Init Circuit (522)**. Both preserve their prior 198 values and append neutral sizes of 1. The 198 prior core IDs remain a prefix of 200. All 521 musical programs change 7–52 previous non-volume controls relative to .3; all 523 complete non-volume vectors are distinct. The 13 curated-family reviews cover 300 musical recipes; A24 covers 222 original addresses including one Init; structural preservation covers the remaining curated Init.

## Numerical and claim boundaries

| Claim | Verified scope |
|---|---|
| Configured continuation reach | 17 tine / 28 ranks + 51 bowed / 91 ranks; disjoint union 68 |
| Tine regression | 486 checks / zero failures; preceding model 162 failures |
| Bowed regression | 492 checks / zero failures; preceding model 174 failures |
| A06 parity | 10,137,600 binary32 bit comparisons; zero differences in retained summary |
| A10 parity | 36,115,200 numerically equal float samples; **not a bitwise assertion** |
| A09 parity | 45 matching FNV1a64 hashes; 4,320,000 float samples |
| A09 dynamics | 90 cases, three tine targets plus two controls; about 3.9–4.0 dB greater soft-to-firm body contrast |
| All-521 spectral/envelope comparison | All 24 source hashes match **historical 59d489d**, not a final-078 matched rerender |

These scopes are not added into a combined realism or listening score. A26/A28 lifecycle overlap remains explicit; truncated A31 stdout does not become an observed final assertion total. Runtime variance and added storage remain disclosed. Recipe vectors stay unchanged during the continuation DSP fixes; the prior .4 stage performed the wider revoicing.

Family reviews establish source/routing coherence and selected measured behavior. A08's cutoff proposals remain unshipped; bowed-only formants do not fix wind/brass/plucked glides. Kalimba, Xylophone and Steel Drum model approximations remain. Inert sustained-body decay and source controls replaced by full body mix are not counted as audible improvements. No all-presets listening, perfect realism or exact acoustic-equivalence claim is made.

## Published primary paths and subsequent closure

The ledger/team draft uses the required paths under `docs/preset-evidence/2026.11.4/ci/source-078c098/`: A53 `linux-x86_64/FINDINGS.md`, A56 `windows-linux-validators/FINDINGS.md`, A58 `audition/FINDINGS.md`, and A57/A59/A60 `final-review/a57-review.md`, `a59-review.md`, `a60-review.md`. Planned publication paths are separate from currently observed report bytes. A59/A60 hashes identify their preliminary snapshots only; they are not predictions of final-report bytes.

A57 has no self hash. Final manifests must be generated after report copying and root's factual status closure, without embedding a future manifest digest into its indexed reports.

**No new blocking roster, retained-byte, numerical or source-preservation finding remains.** A59/A60 final delivery, final root status/count closure, publication and remote/link verification occur subsequently and are not claimed successful by this snapshot. The concrete 60-row draft is ready for that narrow closure.

A57 wrote only scratch artifacts and made no repository/Git/external mutation or native-test, validator or audio rerun.
'''
(ROOT/'a57-review.md').write_text(report)

def report_link(w):
    rp=w['planned_published_primary_path']
    if not rp:return 'Root-attested conversation; corroboration separately recorded'
    return '['+Path(rp).name+']('+str(Path(rp).relative_to('docs'))+')'

team=f'''# Library 2026.11.4 — 60-worker continuation review

**All 60 continuation workers are actual agents. This candidate records 58 delivered assigned scopes including A57; A59/A60 final review delivery and root status closure are subsequent.**

Binary source `{SOURCE}`. Inspected documentation checkpoint `{head}`. A57 audit snapshot `{ASOF}`.

The entire earlier 53-row .4 work-log is excluded. Workers ran in batches with at most three alongside root; follow-up turns do not add workers. A02/A03/A04 completed through root-attested conversation; their original files/transcripts are unavailable and later corroboration is not original author evidence. A10's retained report contains a disclosed later raw-log-provenance addendum.

The rows below link to intended published primary reports. A59/A60 currently have authored preliminary checkpoints and are performing final review; their links are reserved for their actual final deliveries. Root updates their final statuses/counts only after those deliveries.

## Actual worker map

| Worker | Actual assignment | Observed outcome and limits | Primary report |
|---:|---|---|---|
'''
for w in normalized:
    assignment=re.sub(r'(all|exact|and)(\d)',r'\1 \2',w['assignment'])
    stage='Final review in progress' if w['number'] in (59,60) else 'Delivered assigned scope'
    team+=f"| {w['number']:02d} | {assignment} | **{stage}.** {w['outcome']} {w['scope_and_limits']} | {report_link(w)} |\n"
team+='''
## Evidence and claim scope

Four native suites: **1,311 passed / 1,318 registered**, zero failures, **7 formal CTest skips**, and **24 separate nested Python skips**. Thirteen original pluginval lanes: **13 invocations, zero validation retries**, with original process/payload binding. Supporting sanitizer counts remain separate: 199 passed / 200 registered, one formal skip; its nested Python skip count is unknown because successful stdout was suppressed.

The actual audition contains **138 WAVs / 74 programs / 870 seconds**, with all 173 entries, 172 checksums and 83,520,000 scalar samples checked. Its 523-by-200 inventory equals the retained original export. This is not all-catalogue WAV coverage or human listening.

All **523 identities** remain, including two unchanged Init programs and **521 musical programs**. The prior .4 revoicing changes existing non-volume controls across those 521; continuation repairs configured tine/bowed behavior in **17 + 51 disjoint programs** without new identities or recipe-vector changes. Detailed dynamics cover three tine targets and two controls.

Tine regression: 486 checks, zero failures, 162 preceding failures. Bowed regression: 492 checks, zero failures, 174 preceding failures. A06 compares 10,137,600 binary32 sample bits; A10 compares 36,115,200 numeric floats; A09 records 45 matching hashes across 4,320,000 stereo float samples. These remain distinct. The all-521 matched spectral/envelope comparison belongs to historical **59d489d**, not a final-078 matched rerender.

Family reviews retain acoustic-model limits, inactive controls and deferred proposals. They do not establish perfect realism. Explicit factory LOAD/RELOAD remains necessary when an old project retains a previous recipe.

## Source, retention and final closure

The committed 195-entry continuation index and earlier 192-entry checkpoint pass their byte/hash checks. A53's 31-file and A58's 22-file retention manifests pass. Sibling CI/final-review evidence has its own scope; root generates final indexes after all records are copied. All committed post-078 changes through df9a8d2 are documentation/evidence/screenshots; no production/test/workflow drift was found.

A59/A60 final reports and root-owned factual status closure follow this audit. After those actual deliveries, root updates their rows/counts, retains final reports, creates non-self-referential indexes and completes publication/remote verification. No original CI/artifact gate remains pending; this section preserves the order of final review and publication without predicting the results.
'''
(ROOT/'TEAM_REVIEW_2026_11_4.md').write_text(team)
summary={
    'schema':2,'worker':57,'status':'final_actual_60_roster_and_evidence_audit_delivered_final_reviews_subsequent','as_of_utc':ASOF,
    'binary_source':SOURCE,'checkpoint_commit':head,'release_qualified':False,'new_blocking_findings':[],
    'counts':{'actual':60,'unspawned':0,'root_recorded_literal_completed':counts['completed'],'assigned_scopes_complete_before_this_delivery':57,'including_this_A57_delivery':58,'subsequent_final_deliveries_pending':[59,60]},
    'all_original_ci_artifact_scopes_have_delivered_acceptance':True,'no_unavailable_final_outcome_claimed':True,
    'manifest':manifest_result,'retention':retention_result,'source':source_result,'claims':claim_result,'accepted_scopes':accepted_scopes,
    'roster_evidence_classes':draft['report_evidence_counts'],'conversation_only_workers':[2,3,4],
    'primary_publication_paths':{str(w['number']):w['planned_published_primary_path'] for w in normalized if w['number']>=53},
    'subsequent_actions':['Actual final A59/A60 delivery','Root factual status/count closure','Final evidence indexing without self-hash cycles','Publication and remote/download verification'],
    'self_hash_policy':draft['self_hash_policy'],
    'deliverable_paths':[str(ROOT/n) for n in ['a57-review.md','a57-review.json','TEAM_REVIEW_2026_11_4.md','continuation-worker-ledger-normalized-a57.json']],
    'audited_input_snapshots':[record(OUT/'input-roster-at-final-audit.json'),record(ROOT/'a53-summary.json'),record(ROOT/'a56-review.json'),record(ROOT/'a58-review.json'),record(ROOT/'a59/a59-review.json'),record(ROOT/'a60-review.json')],
    'no_repository_writes':True,'no_git_mutations':True,'no_external_writes':True,'no_native_validator_or_audio_rerun':True
}
(ROOT/'a57-review.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({'status':summary['status'],'counts':summary['counts'],'manifest_entries':len(working_manifest),'native_totals':native_totals,'validator_totals':accepted_scopes['validators'],'report_paths':summary['deliverable_paths']},indent=2))
