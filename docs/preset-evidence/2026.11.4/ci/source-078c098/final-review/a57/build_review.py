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
ledger = json.loads(ledger_path.read_text())
actual_count = len(ledger['workers'])
assert actual_count == 58, 'Refresh roster mapping before a later-stage audit'
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
    'audition_contract_only':{'wavs':138,'programs':74,'seconds':870,'current_artifact_qualification_pending':True},
    'limits':['Source routing, signal metrics and selected regression coverage do not prove universal subjective improvement or exact acoustic equivalence.','Bowed-only stationary formants do not repair brass/wind/plucked glides.','Sustained-body partial-decay literals and source literals replaced by full body mix must not count as audible changes.','A26/A28 lifecycle probe overlap is disclosed; their counts are not independent native release gates.','Direct-body sample counts and full-preset stereo sample counts have distinct scopes and are not added as one score.']
}
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
53: ('a53-review.md','Waiting for original Linux Test and actual uploaded package/test evidence.','In progress; no Linux package or completed native-suite qualification claimed.'),
54: ('a54-review.md','Qualified actual Intel Mac installation archive and original native test evidence.','251 registered, 249 passed, two formal skips, five nested Python skips; stale A56 handoff label is superseded by A55.'),
55: ('a55-review.md','Qualified eight original Mac validator lanes and distributed payload identity.','Eight invocations, zero validation retries, normal exit zero; whole release remains separate.'),
56: ('a56-review.md','Qualified two original Windows validator lanes and distributed payload; Linux validator portion pending.','Partial authored report: two of five assigned lanes and one of two payloads complete; whole assignment is not complete.'),
57: ('a57-review.md','Audited actual roster, report provenance, continuation manifest and numerical/source scope.','Current-scope report only; A58 exists and awaits original artifacts, A59/A60 unspawned; final ledger recheck pending.'),
58: (None,'Assigned actual final audition ZIP, complete audio-entry/sample and delivered-inventory qualification.','Actual spawned worker verified in live-agent list; waiting for original Linux producer, no completed artifact qualification claimed.')
}

retained_override={
    5:CONT/'a05/FINDINGS.md',6:CONT/'a06/summary.json',9:CONT/'a09/FINDINGS.md',10:CONT/'a10/FINDINGS.md',20:CONT/'a20/FINDINGS.md',
    48:CI/'supporting-dsp/FINDINGS.md',51:CI/'macos-arm64/a51-review.md',52:CI/'windows-x86_64/a52-review.md',
    54:CI/'macos-x86_64/FINDINGS.md',55:CI/'macos-validators/FINDINGS.md'
}
corroboration={
2:[CONT/'a06/summary.json',CONT/'a09/FINDINGS.md'],
3:[CONT/'a10/FINDINGS.md',CONT/'a32/summary.json',CONT/'reviews/a38-review.md',CONT/'reviews/a43-summary.json'],
4:[REPO/'docs/release/RELEASE_QUALIFICATION.md']
}
normalized=[]
for worker in ledger['workers']:
    number=worker['number']; name,outcome,scope=rows[number]
    row={**worker,'outcome':outcome,'scope_and_limits':scope,
         'existence_and_completion_provenance':'Root-maintained actual-spawn ledger and retained conversational continuity; historical spawn API receipts were not available to this audit.'}
    if number in (2,3,4):
        row.update({'report_evidence_class':'root_attested_conversational_completion','primary_report':None,
                    'transcript_retained_on_disk':False,'corroboration_not_authored_reports':[record(p) for p in corroboration[number]],
                    'attestation':'Root confirmed these were actual completed continuation agents and supplied their outcomes to A57. No original transcript is invented or recreated.'})
    elif number==56:
        row.update({'report_evidence_class':'partial_authored_report','primary_report':record(ROOT/'a56-review.md'),
                    'partial_records':[record(ROOT/'a56/windows-validator-observations.json'),record(ROOT/'a56/windows-vst3-payload.json')],
                    'partial_completion_basis':'Authored A56 report independently binds Windows source/producer/lanes/payload; Linux pending. Parser output alone remains unbound.'})
    elif number==57:
        row.update({'report_evidence_class':'current_audit_scratch_report','primary_report':{'path':str(ROOT/name)},
                    'followup_required':'Recheck A58 outcome and actual A59/A60 assignments and completion reports after they exist; regenerate final counts and manifest.'})
    elif number==58:
        row.update({'report_evidence_class':'actual_spawned_awaiting_artifacts','primary_report':None,
                    'existence_and_completion_provenance':'A58 task appears in the updated root ledger and the independently inspected live-agent list; it is running, not completed.'})
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
counts=Counter(w['status'] for w in normalized)
incomplete_count=len(normalized)-counts['completed']
draft={
    'schema':1,'status':'provisional_snapshot_before_A57_delivery_not_final_60_worker_ledger','as_of_utc':ASOF,
    'binary_source':SOURCE,'checkpoint_commit':head,'input_ledger':record(ledger_path),
    'requested_continuation_workers':60,'recorded_actual_continuation_workers':len(normalized),
    'completed_at_snapshot':counts['completed'],'incomplete_or_partial_at_snapshot':incomplete_count,
    'exact_status_counts':dict(counts),
    'unspawned_requested_workers':60-len(normalized),'unspawned_numbers':[59,60],
    'planned_workers_are_not_in_workers_array':True,
    'current_audit_delivery_note':'A57 is in progress in the captured root ledger. This report completes its current-scope audit; root may record that completion after delivery. Final 60-worker recheck remains pending.',
    'concurrency':'At most three worker agents alongside root, launched in batches; 60 requested agents does not mean 60 simultaneous workers.',
    'independent_live_snapshot':['/root','/root/a53_linux_package','/root/a57_ledger_evidence_audit','/root/a58_final_audition_verification'],
    'prior_live_snapshot':['/root','/root/a53_linux_package','/root/a56_windows_linux_validator_review','/root/a57_ledger_evidence_audit'],
    'excluded_prior_stage':{'path':'docs/preset-evidence/2026.11.4/work-log.json','rows':len(initial['workers']),'status_counts':dict(Counter(w['state'] for w in initial['workers'])),'counted_toward_this_continuation':0,'scope':'Entire prior 2026.11.4-stage roster excluded, including completed and planned/ready rows. A .4 revision label is not a worker count.'},
    'report_evidence_counts':dict(Counter(w['report_evidence_class'] for w in normalized)),
    'workers':normalized
}
(ROOT/'continuation-worker-ledger-normalized-a57.json').write_text(json.dumps(draft,indent=2)+'\n')

report = f'''# A57 — continuation ledger and evidence audit

**Current-scope audit passed with explicit pending publication work. This is not a completed 60-worker ledger or a final release qualification.**

Binary source: `{SOURCE}`. Documentation checkpoint: `{head}`. Audit snapshot: `{ASOF}`.

## Actual workers and evidence provenance

The root-maintained actual roster contains **{actual_count} distinct continuation workers** with sequential numbers 1–{actual_count} and unique task paths. At the captured snapshot, **{counts['completed']} are recorded completed and {incomplete_count} remain incomplete or partial: A53, A56, this A57 audit and A58**. A56 has delivered its Windows portion and awaits Linux; A58 is an actual spawned worker waiting for original audition artifacts. Workers 59–60 have not been spawned and are absent from the normalized worker array. This report completes A57's current-scope work; the parent can record that delivery, then request the final recheck after the remaining real workers have assignments and outcomes.

The initial independent live-agent snapshot contained root, A53, A56 and A57; the later snapshot contained root, A53, A57 and newly spawned A58. Historical spawn receipts are not available in this audit, so earlier actual-spawn/completion status is explicitly attributed to the root's retained continuation record, with authored reports and implementation evidence corroborating the work. No claim is made that a directory alone proves an agent existed.

**A02, A03 and A04 are root-attested conversational completions.** No original standalone report/transcript survives on disk. Their downstream corroboration is linked separately and is not presented as a file they authored: A02's normalization finding is supported by A06/A09; A03's library/formant/decay findings by A10/A32/A38/A43; A04's saved-session wording by the committed release contract and its history. The normalized ledger labels these evidence classes explicitly.

A10's retained report contains its complete original authored text plus a later **Raw-log provenance** addendum identifying the final versus earlier timing log. That single retained/scratch text difference is intentional and recorded, with both hashes. The addendum is not represented as original author text.

The earlier `docs/preset-evidence/2026.11.4/work-log.json` has **{len(initial['workers'])} prior-stage rows** with mixed completion/readiness states. The **entire prior-stage roster is excluded** from continuation totals. The `.4` revision label does not mean four workers. Concurrency was at most three worker agents alongside root, not 60 simultaneous workers.

## Manifest integrity and new retained inputs

The **committed 192-entry manifest at {head[:8]} passes**: every path exists, every byte count and SHA-256 agrees, and paths are unique. The initial inspection also found no unindexed files inside its continuation scope.

During this audit, the parent retained the original current inventory, current structural comparison and provenance under `continuation/final-inventory/`. The expanded **working-tree manifest has {len(working_manifest)} entries and passes** the same checks, with no missing, duplicate, mismatched or unindexed continuation files. The inventory and structural comparison are byte-identical to their original scratch inputs:

| Input | Bytes | SHA-256 |
|---|---:|---|
| Current inventory | 704,822 | `0545f9a6ede82c72f4bbecc0e032cbc08f4de257b496f6b763b1911d607f9611` |
| Current parameter coverage | 441,511 | `43b693d7990b0e8857867de3b29ed69b02fbc5cec83285f63977b6257825dad9` |

The retained provenance preserves export source `e32728b`; the relevant core, plugin, preset-capture and audition-script inputs are unchanged through `078c098`. This does not relabel the development inputs as an actual native audition ZIP. Historical `development/final-bank/inventory.json` remains unchanged and separate.

The sibling `ci/source-078c098` tree contains **{len(ci_files)} files at this snapshot**. Newly copied supporting-DSP, Intel-Mac, Mac-validator and Windows-job records are outside the continuation manifest's defined scope. They are not stale-manifest failures. Their pending Git retention and eventual final evidence index remain publication tasks. The two untracked Python bytecode cache files under scripts are generated cache files, not production source changes.

## Source and catalogue preservation

All **{len(changed_committed)} committed paths changed after binary source 078c098 are README/documentation/evidence/screenshots**. No committed or tracked working-tree production/test/workflow source change was found. `src/core`, `src/plugin`, `src/reference`, tests, scripts, CMake, resources and workflows are unchanged from the binary source through the checkpoint.

This audit independently reparsed the baseline and newly retained current inventory at binary32 precision. All **523 ordered identities, names, categories and tags are preserved**. There are **521 musical programs** and exactly two Init programs: **Init Patch, host 43**, and **Init Circuit, host 522**. Both preserve their old 198 values bit for bit and have neutral appended sizes of 1. The prior 198 core IDs remain a prefix of the current 200. All 521 musical programs change 7–52 prior non-volume controls compared with .3, and all 523 complete non-volume vectors are distinct. These are structural coverage claims; inactive literals and parameter distance do not establish perceptual improvement.

The 13 curated-family recipe reviews span **300 musical curated programs**. A24 separately reviews all **222 original addresses**, including one Init; the final curated Init is covered by the structural-preservation checks. This maps catalogue coverage without relabeling source reviews as listening coverage.

## Numerical and source-scope checks

| Claim | Audit result | Required scope |
|---|---|---|
| Tine regression | 486 checks / 0 failures; preceding model 486 / 162 failures | Direct model negative control; no acoustic-reference score |
| Bowed regression | 492 checks / 0 failures; preceding model 492 / 174 failures | Stationary authored body response, bowed model only |
| Configured affected reach | 17 tine programs / 28 ranks; 51 bowed programs / 91 ranks; disjoint union 68 | Configuration reach, not 68 complete listening tests |
| A06 direct parity | 10,137,600 samples | Binary32 bit comparison, retained zero-difference summary; no invented standalone log |
| A10 direct parity | 36,115,200 samples | Numeric float equality, not bitwise identity |
| A09 full-preset parity | 45 matching FNV1a64 audio hashes; 4,320,000 float samples | Selected full-strike/control cases, distinct from mono direct-model counts |
| A09 dynamics sweep | 90 unique cases across five presets | Three targeted tine presets plus two non-tine controls |
| All-521 spectral/envelope comparison | All 24 source-manifest hashes match historical 59d489d | Not a final-078 all-library matched rerender |
| Audition roster | Contract is 138 WAVs / 74 programs / 870 seconds | Actual current artifact acceptance still pending at this snapshot; no human-listening claim |

No totals above are added together to manufacture a combined acoustic-quality or listening-coverage score. A26/A28 lifecycle overlap is disclosed. A31's truncated JUnit stdout does not become an observed 284 internal-check count. Runtime measurements preserve shared-machine variance and measured added storage rather than claiming zero overhead.

## Review scope and limits to preserve

Family reviews cover existing recipe source/routing and selected measured fixtures. They do not establish that every preset was listened to, that every change is perceptually better, or that any named acoustic instrument is reproduced exactly. Deferred proposals remain deferred: A08's arbitrary texture cutoff table was not shipped; bowed-only fixed formants do not repair brass/wind/plucked glides; Kalimba, Xylophone and Steel Drum model approximations remain documented. Sustained-body partial decay is inactive, and full-body blending replaces some source literals, so those changes are not counted as audible improvements.

Native package and validator reports remain scoped. A51/A52/A54 qualify their own installation/native evidence; A55 qualifies eight Mac lanes. A56's authored partial report qualifies its two Windows lanes and one payload binding, but its five-lane/two-payload combined Windows/Linux assignment remains open. A53 still awaits Linux completion and A58 awaits the actual audition artifact. A54's old handoff reference to A56 for Mac validation is a historical assignment label; A55 is the actual Mac-validator reviewer.

## Remaining actions for the parent

1. Finish A53 and A56 from the original native run and actual artifacts. Preserve formal CTest skips separately from nested Python skips and validator-internal skips.
2. Finish A58's actual audition assignment; spawn real A59 and A60, assign concrete work, retain each outcome, and only then add them to the actual roster. A57's follow-up checks the final ledger rather than predicting future success.
3. Retain later authored reports and current CI evidence, then regenerate the final relevant evidence index. A single manifest must state its scope; sibling CI evidence cannot be inferred from continuation coverage.
4. Complete actual audition archive qualification, all-platform link/source mapping, final release review and root-owned publication. Preserve historical failures/cancellations and do not replace prior download links before required gates are met.

## Deliverables

- `continuation-worker-ledger-normalized-a57.json`: every actual worker's assignment, status, primary report, evidence class, outcome and limits; no planned workers in the array.
- `TEAM_REVIEW_2026_11_4.md`: proposed publication draft with the same explicit snapshot/pending state and compact primary-report map.
- `a57-review.json`: machine-readable audit summary.
- `a57/manifest-check.json`, `a57/source-preservation.json`, `a57/claim-checks.json`: exact inspected results and file identities.
- `a57/build_review.py`: repeatable read-only audit. It deliberately refuses a changed roster until future actual-worker mappings are reviewed.

All A57 writes are scratch audit artifacts. No repository content, Git state, external service, source, test, threshold, package or workflow was changed by A57.
'''
(ROOT/'a57-review.md').write_text(report)
draft['workers'][56]['primary_report']=record(ROOT/'a57-review.md')
(ROOT/'continuation-worker-ledger-normalized-a57.json').write_text(json.dumps(draft,indent=2)+'\n')

def rel_report(w):
    p=w.get('primary_report')
    if not p:
        return 'Root-attested conversation; corroboration in JSON' if w['number'] in (2,3,4) else 'Actual assignment; original artifact and authored report pending'
    rp=p.get('repository_path')
    if rp:
        return '['+Path(rp).name+']('+str(Path(rp).relative_to('docs'))+')'
    return '`'+str(Path(p['path']).relative_to(ROOT))+'` (scratch)'

team=f'''# Library 2026.11.4 — continuation team review

**PROPOSED DRAFT — {actual_count} actual workers at snapshot; final 60-worker roster and release gates remain open.**

Binary source `{SOURCE}`; documentation checkpoint `{head}`. As of `{ASOF}`, the root-maintained roster records {counts['completed']} completed workers and A53/A56/A57/A58 incomplete or partial. A56 has completed Windows and awaits Linux; A58 is a real running audition reviewer awaiting its original artifact. This draft is prepared by A57 at completion of its current-scope audit. Workers 59–60 have not yet been spawned and are not counted. Update the completion counts only from actual delivered outcomes.

The continuation uses batches of at most three worker agents alongside root. It is not a claim of 60 simultaneous agents. The prior 2026.11.4 development work-log is separate; none of its 53 rows is counted in this continuation.

## Evidence classes

The roster's actual-spawn and historical completion provenance is the root's retained conversation/continuity record. Most workers have retained authored reports. A02, A03 and A04 completed through conversation; their original report files/transcripts are unavailable. They are explicitly root-attested, with downstream corroboration linked separately in the normalized ledger. Corroboration is not a recreated authored report. A53 and A56 remain incomplete, and A58 awaits its original artifact.

A10's retained copy includes its original authored report plus a later raw-log-provenance addendum. The ledger records both hashes and does not attribute the added paragraph to the original worker report.

## Primary report map and outcomes

Paths below are repository-relative links for a future `docs/TEAM_REVIEW_2026_11_4.md`; scratch-only reports must be retained before final publication.

| Worker | Actual assignment | Outcome and scope | Primary report |
|---:|---|---|---|
'''
for w in normalized:
    assignment=re.sub(r'(all|exact|and)(\d)',r'\1 \2',w['assignment'])
    team+=f"| {w['number']:02d} | {assignment} | **{w['status'].replace('_',' ')}.** {w['outcome']} {w['scope_and_limits']} | {rel_report(w)} |\n"
team+='''
## Preserved claims and boundaries

All 523 existing identities remain, including two unchanged Init programs and 521 musical programs. The prior revoicing changes existing non-volume controls across those 521 programs. The continuation repairs configured tine/bowed behavior in 17 + 51 disjoint programs, without additional recipe-vector changes or new preset identities. The detailed full-preset dynamics experiment covers three tine targets and two controls; it is not 68 full-program listening tests.

The tine regression is 486 checks / zero failures with 162 failures on the preceding model. The bowed regression is 492 / zero with 174 preceding failures. A06 compares 10,137,600 binary32 sample bits; A10 compares 36,115,200 numeric float values; A09 records 45 matching audio hashes covering 4,320,000 stereo float values. Keep these scopes distinct. The historical all-521 matched spectral/envelope comparison belongs to 59d489d, not final binary source 078c098. The audition contract covers 138 WAVs and 74 programs, not the full catalogue or a human listening assessment.

Family audits establish source/routing coherence and selected measured behavior. They retain real model limits, deferred noise-spectrum proposals and inactive-control qualifications. They do not prove perfect or universal acoustic realism. See A38/A49 for the concise family-specific boundaries and A27/A30 for explicit factory RELOAD guidance.

## Manifest and source audit

The committed 192-entry continuation manifest at checkpoint 6426abaa matched every file's bytes and SHA-256. After the parent retained the original current inventory, current parameter coverage and provenance, the expanded 195-entry working manifest also passed with no omissions inside its scope. The sibling CI evidence tree is separate. All committed changes after binary source 078c098 are documentation, evidence or screenshots; no production/test/workflow drift was found.

## Pending finalization

Finish the original Linux native/package and Windows/Linux validator assignments; finish actual worker 58's audition archive review; spawn and complete actual workers 59–60; update this roster from their actual reports; retain final evidence; complete final link/source/publication verification. The final version must report its genuine completed count and required-gate outcomes rather than retaining this provisional text as a completed release claim.
'''
(ROOT/'TEAM_REVIEW_2026_11_4.md').write_text(team)
summary={
    'schema':1,'worker':57,'status':'current_scope_audit_complete_final_60_worker_followup_pending','as_of_utc':ASOF,
    'binary_source':SOURCE,'checkpoint_commit':head,'release_qualified':False,
    'counts_at_captured_roster':{'actual':actual_count,'completed':counts['completed'],'incomplete_or_partial':incomplete_count,'unspawned':60-actual_count},
    'own_delivery_note':draft['current_audit_delivery_note'],
    'manifest':manifest_result,'source':source_result,'claims':claim_result,
    'roster_evidence_classes':draft['report_evidence_counts'],
    'conversation_only_workers':[2,3,4],
    'resolved_during_audit':['Retained exact original current inventory/coverage inputs and preserved export-source provenance; expanded continuation manifest passes.'],
    'pending':['Actual Linux package/native completion','Combined A56 Windows/Linux validator completion','Actual A58 audition outcome and A59/A60 spawn and scoped reports','Final 60-worker ledger recheck','Actual audition qualification and final link/publication verification','Root-owned final evidence retention and scoped manifest refresh'],
    'artifacts':[record(ROOT/'a57-review.md'),record(ROOT/'TEAM_REVIEW_2026_11_4.md'),record(ROOT/'continuation-worker-ledger-normalized-a57.json'),record(OUT/'manifest-check.json'),record(OUT/'source-preservation.json'),record(OUT/'claim-checks.json')],
    'no_repository_writes':True,'no_git_mutations':True,'no_external_writes':True,'no_build_or_audio_rerun':True
}
(ROOT/'a57-review.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({'status':summary['status'],'counts':summary['counts_at_captured_roster'],'committed_manifest_entries':len(committed_manifest),'working_manifest_entries':len(working_manifest),'reports': [str(ROOT/n) for n in ['a57-review.md','a57-review.json','TEAM_REVIEW_2026_11_4.md','continuation-worker-ledger-normalized-a57.json']]},indent=2))
