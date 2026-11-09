import datetime, hashlib, json, os, pathlib, re, subprocess
root=pathlib.Path('/workspace/scratch/ee447c520988')
base=root/'mpe-onset-evidence'; build=root/'build-mpe-onset-gcc-generic'
ctest=root/'pitch-power-recovery/tools/cmake/data/bin/ctest'
out=base/'remaining-core-final'; out.mkdir(exist_ok=False)
original=(base/'full-default-ctest.log').read_text()
# Only explicit completed passing verdicts can be skipped. Started-only tests rerun.
completed={int(i):n for i,n in re.findall(r'^\s*\d+/143 Test\s+#(\d+): (\S+)\s+\.{3,}\s+Passed\s+',original,re.M)}
if len(completed)!=58: raise RuntimeError(f'Unexpected original completion count {len(completed)}')
prior=json.loads((base/'remaining-core-checked/progress.json').read_text())
for entry in prior['remaining']:
    if entry['passed']:
        path=base/'remaining-core-checked'/entry['log']
        if entry['returncode']!=0 or hashlib.sha256(path.read_bytes()).hexdigest()!=entry['sha256']:
            raise RuntimeError('Invalid completed continuation verdict')
        if entry['id'] in completed: raise RuntimeError('Repeated completed test')
        completed[entry['id']]=entry['name']
if len(completed)!=94: raise RuntimeError('Expected 94 passed tests before fixture correction')
registered=json.loads(subprocess.check_output([str(ctest),'--test-dir',str(build),'--show-only=json-v1'],text=True))['tests']
if len(registered)!=143: raise RuntimeError('Changed registered suite')
for i,n in completed.items():
    if registered[i-1]['name']!=n: raise RuntimeError('Original ID/name mismatch')
manifest={'source_commit':subprocess.check_output(['git','-C',str(root/'next-product-step'),'rev-parse','HEAD'],text=True).strip(), 'original_log_sha256':hashlib.sha256((base/'full-default-ctest.log').read_bytes()).hexdigest(), 'original_and_prior_completed':completed, 'prior_progress_sha256':hashlib.sha256((base/'remaining-core-checked/progress.json').read_bytes()).hexdigest(), 'prior_failed_test':95, 'fixture_change_commit':'fcaf4597f34b8ad0180bbbd1ac3600257b868700','registered_tests':[t['name'] for t in registered],'remaining':[]}
(out/'progress.json').write_text(json.dumps(manifest,indent=2)+'\n')
env=os.environ.copy(); env['PYTHONDONTWRITEBYTECODE']='1'
for i,t in enumerate(registered,1):
    if i in completed: continue
    name=t['name']; logfile=out/f'{i:03d}-{name}.log'
    argv=[str(ctest),'--test-dir',str(build),'-R','^'+re.escape(name)+'$','--output-on-failure']
    start=datetime.datetime.now(datetime.timezone.utc).isoformat()
    with logfile.open('w') as f: result=subprocess.run(argv,env=env,stdout=f,stderr=subprocess.STDOUT)
    text=logfile.read_text()
    passed=result.returncode==0 and '100% tests passed, 0 tests failed out of 1' in text and name in text
    manifest['remaining'].append({'id':i,'name':name,'argv':argv,'started_utc':start,'returncode':result.returncode,'passed':passed,'log':logfile.name,'sha256':hashlib.sha256(logfile.read_bytes()).hexdigest()})
    (out/'progress.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'{len(completed)+len(manifest["remaining"]):3d}/143 {name}: {passed}',flush=True)
    if not passed: raise SystemExit(1)
manifest['complete']=True
(out/'summary.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('All 143 tests passed across 58 original, 36 first-continuation, and 49 final-continuation verdicts; the earlier test95 failure is retained and its corrected fixture separately documented.',flush=True)
