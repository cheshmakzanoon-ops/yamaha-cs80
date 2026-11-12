from pathlib import Path
import hashlib,json,subprocess,re,shutil,sys
r=Path.cwd(); e=r/'docs/experiments/key-release-automation';e.mkdir(parents=True,exist_ok=False)
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert Path('build/matrix.exit').read_text().strip()=='0'
runs=json.loads(Path('build/matrix-runs.json').read_text());comparison=json.loads(Path('build/pcm-comparison.json').read_text());assert len(runs)==12 and all(x['exit']==0 for x in runs)
for name in ['build-command.sh','configure.log','configure.exit','build.log','build.exit','run-key-release-matrix.py','matrix.log','matrix.exit','matrix-runs.json','pcm-comparison.json','runtime-inputs.json','provenance-before.json']:
 shutil.copyfile(r/'build'/name,e/name)
for run in runs:shutil.copyfile(r/run['log'],e/f"{run['kind']}-{run['rate']}-{run['mode']}.log")
base=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
oldfiles=subprocess.check_output(['git','ls-tree','-r','--name-only',base,'src','scripts','tests/parity/tolerances-v2'],text=True).splitlines()
unchanged={}
for name in oldfiles:
 if name=='src/plugin/HostAutomationBridge.h':continue
 previous=subprocess.check_output(['git','show',f'{base}:{name}']);current=(r/name).read_bytes();assert previous==current,name
 unchanged[name]=sha(r/name)
contracts=[p for p in unchanged if p.startswith('tests/parity/tolerances-v2/') and p.endswith('.json')];assert len(contracts)==49
(e/'unchanged-files.json').write_text(json.dumps(unchanged,indent=2)+'\n')
sourcefiles=list(json.loads(Path('build/provenance-before.json').read_text())['sources'])
inputs=json.loads(Path('build/runtime-inputs.json').read_text()); assert all(sha(Path(p))==h for p,h in inputs.items())
(e/'runtime-inputs-after.json').write_text(json.dumps(inputs,indent=2)+'\n')
prov=json.loads(Path('build/provenance-before.json').read_text())
assert all(sha(Path(p))==h for group in ['sdk','sources'] for p,h in prov[group].items())
assert sha(Path(prov['compiler']['path']))==prov['compiler']['sha256']
assert sha(Path(prov['python']['path']).resolve())==prov['python']['sha256']
prov['timing']='After runtime; recorded SDK/compiler/Python/source hashes unchanged since before compilation'
(e/'provenance-after.json').write_text(json.dumps(prov,indent=2)+'\n')
metrics=[]
for run in runs:
 if run['kind']!='candidate':continue
 text=Path(run['log']).read_text();assert '6 native key_release scenarios, 0 failures' in text
 for lane,capacity,source,error,effect,prefix in re.findall(r'key_release=key_release_lane(\d+)-(\d+)-(\d+) max_error=([^ ]+) effect=([^ ]+) prefix_exact=([^\n]+)',text):metrics.append(dict(lane=int(lane),capacity=int(capacity),source=int(source),rate=run['rate'],mode=run['mode'],max_error=float(error),effect=float(effect),prefix_exact=int(prefix)))
assert len(metrics)==36
verification=dict(base_commit=base,scope='Bounded direct native supported-input tests, not CTest',build=dict(configure_exit=0,build_exit=0,targets=['CS80Reference_VST3','cs80_vst3_key_release_automation_tests']),runs=12,full_cases=36,compatibility_cases=36,pcm_pairs=108,all_actual_pcm_pairs_exact=comparison['all_equal'],all_run_exits_zero=True,inputs_before_after_identical=True,metrics=metrics,old_contract_count=49,old_contracts_byte_unchanged=True,unchanged_scope='All src except single capability header and all prior scripts/contracts match base; see unchanged-files.json',source_hashes={p:sha(r/p) for p in sourcefiles},artifact_policy='Logs, command scripts and actual capture hashes retained here. Raw PCM and native binaries retained in ignored workspace build directories, not distributed; not a complete replay bundle.',limitations=['Global KEY RELEASE positive for all selected sources.', 'Positive witness only released tail frames[3100,4096); no full release duration, isolated envelope timing or physical calibration qualification.', 'No invented bypass control.', 'No hardware/DAW/GUI/AU/CPU/broad/negative tests.'])
(e/'verification.json').write_text(json.dumps(verification,indent=2)+'\n')
print('Evidence prepared; update doc verified section before final hash freeze.')
