from pathlib import Path
import hashlib,json,subprocess,re,shutil
r=Path.cwd();e=r/'docs/experiments/release-floor-interaction';e.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert Path('build/matrix.exit').read_text().strip()=='0'
runs=json.loads(Path('build/matrix-runs.json').read_text());assert len(runs)==6 and all(x['exit']==0 for x in runs)
for name in ['build-command.sh','build.log','build.exit','run-matrix.py','matrix.log','matrix.exit','matrix-runs.json','capture-identities.json','runtime-inputs.json','provenance-before.json']:shutil.copyfile(r/'build'/name,e/name)
for run in runs:shutil.copyfile(r/run['log'],e/f"{run['rate']}-{run['mode']}.log")
base=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();unchanged={}
for name in subprocess.check_output(['git','ls-tree','-r','--name-only',base,'src','scripts','tests/parity/tolerances-v2','tests/Vst3AutomationTests.cpp'],text=True).splitlines():
 assert subprocess.check_output(['git','show',f'{base}:{name}'])==(r/name).read_bytes(),name
 unchanged[name]=sha(r/name)
assert len([p for p in unchanged if p.startswith('tests/parity/tolerances-v2/') and p.endswith('.json')])==50
(e/'unchanged-files.json').write_text(json.dumps(unchanged,indent=2)+'\n')
prov=json.loads(Path('build/provenance-before.json').read_text());assert all(sha(Path(p))==h for group in ['sdk','sources'] for p,h in prov[group].items())
for k in ['compiler','python']:assert sha(Path(prov[k]['path']).resolve())==prov[k]['sha256']
prov['timing']='After runtime; SDK/compiler/Python/source hashes unchanged since before direct host compilation'
(e/'provenance-after.json').write_text(json.dumps(prov,indent=2)+'\n')
inputs=json.loads(Path('build/runtime-inputs.json').read_text());assert all(sha(Path(p))==h for p,h in inputs.items());(e/'runtime-inputs-after.json').write_text(json.dumps(inputs,indent=2)+'\n')
metrics=[]
for run in runs:
 text=Path(run['log']).read_text();assert '12 native release_floor_interaction scenarios, 0 failures' in text
 for scenario,cap,source,err,effect,prefix,whole in re.findall(r'release_floor_interaction=release_floor_interaction_scenario(\d+)-(\d+)-(\d+) max_error=([^ ]+) effect=([^ ]+) prefix_exact=([^ ]+) whole_exact=([^\n]+)',text):metrics.append(dict(scenario=int(scenario),capacity=int(cap),source=int(source),rate=run['rate'],mode=run['mode'],max_error=float(err),effect=float(effect),prefix_exact=int(prefix),whole_exact=int(whole)))
assert len(metrics)==72
v=dict(base_commit=base,scope='Direct supported host only; no CMake execution, plugin build or CTest',build_exit=0,runs=6,full_cases=72,render_count=936,capture_identity_count=216,baseline_comparison=False,all_run_exits_zero=True,inputs_before_after_identical=True,metrics=metrics,old_contract_count=50,old_contracts_byte_unchanged=True,unchanged_scope='All src, prior scripts/contracts and shared renderer match base',source_hashes={p:sha(Path(p)) for p in prov['sources']},artifact_policy='Full logs, commands and capture hashes retained; raw PCM and executable binaries local ignored files, not distributed; not a complete replay bundle',limitations=['Static gate and rank floors only; below-floor PCM equality, conservative crossing-prefix3300 and positive tail[3400,4096). No dynamic rank, internal-state equality or exact crossing-index claim.','No hardware, complete-stage or calibrated timing claims.','No broad, malformed, GUI, DAW, CPU or negative tests.'])
(e/'verification.json').write_text(json.dumps(v,indent=2)+'\n')
print('Evidence prepared; final prose and hash freeze next.')
