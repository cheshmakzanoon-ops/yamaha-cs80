from pathlib import Path
import hashlib,json,re,subprocess,datetime
p=Path(__file__).resolve().parent
source='078c0981ffa1b5472342e0fb5ab8702bc3bd2889'
run=json.loads((p/'run.json').read_text());jobs=json.loads((p/'jobs.json').read_text())['jobs']
labels={137945816716:'synthetic',137945817147:'legacy',137945817176:'measure',137945817253:'sanitizers',137945817476:'source'}
rows=[]
for j in jobs:
 key=labels[j['id']];r={k:j.get(k) for k in ['id','name','head_sha','run_id','run_attempt','status','conclusion','started_at','completed_at','html_url']}
 r['steps']=[{k:s.get(k) for k in ['number','name','status','conclusion','started_at','completed_at']} for s in j['steps']]
 f=p/(key+'-job.log')
 if f.exists():
  b=f.read_bytes();log=b.decode();r['decoded_log']={'path':f.name,'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}
  loglines=log.splitlines()
  r['observed_exact_checkout']=any('git log -1 --format=%H' in line and i+1<len(loglines) and loglines[i+1].split()[-1]==source for i,line in enumerate(loglines))
  if key in ('synthetic','measure'):
   observed=re.findall(r'Ran (\d+) tests? in [0-9.]+s',log)
   r['python_unit_tests']={'ran':[int(x) for x in observed],'basis':'Original successful test step; retained Ran tests and OK lines. Separate from CTest counts.'}
  if key=='sanitizers':
   lines=[re.sub(r'^\d{4}-\d{2}-\d{2}T\S+\s','',x) for x in log.splitlines()]
   cases=[]
   for line in lines:
    m=re.search(r'\b(\d+)/(\d+) Test\s+#\d+:\s+(\S+)\s+\.+\s*(Passed|\*\*\*\w+)(.*)',line)
    if m:cases.append({'ordinal':int(m[1]),'registered':int(m[2]),'name':m[3],'result':m[4],'detail':m[5].strip()})
   cases=sorted({x['name']:x for x in cases}.values(),key=lambda x:x['ordinal'])
   r['ctest']={'observed_entries':len(cases),'registered':max([x['registered'] for x in cases],default=0),'passed':sum(x['result']=='Passed' for x in cases),'formal_skipped':sum(x['result']=='***Skipped' for x in cases),'failed':sum(x['result'] not in ('Passed','***Skipped') for x in cases),'formal_skip_names':[x['name'] for x in cases if x['result']=='***Skipped'],'cases':cases,'nested_python_skips':None,'nested_python_skip_note':'Successful Python output is not emitted by --output-on-failure; this run provides no reliable total of nested Python skips.'}
   r['sanitizer_diagnostic_lines']=[x for x in lines if re.search(r'ERROR: (AddressSanitizer|LeakSanitizer)|runtime error:|SUMMARY: (AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer)|AddressSanitizer:DEADLYSIGNAL',x)]
 rows.append(r)
artifacts=json.loads((p/'artifact-inspection.json').read_text()) if (p/'artifact-inspection.json').exists() else []
out={'schema':1,'reviewer':'actual continuation worker 48 of 60','source':source,'source_tree':subprocess.check_output(['git','rev-parse',source+'^{tree}'],cwd=p.parent/'yamaha-cs80',text=True).strip(),'reviewed_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'status':'completed' if all(j['status']=='completed' for j in jobs) else 'pending','conclusion':run['conclusion'],'run':{k:run.get(k) for k in ['id','name','head_sha','run_attempt','status','conclusion','created_at','run_started_at','updated_at','html_url']},'jobs':rows,'actual_artifact_inspections':artifacts,'scope_limits':['Supporting DSP workflow only; not native package or pluginval qualification.','Synthetic comparison and numerical/Fourier references are software fixtures, not physical CS-80 or acoustic-instrument evidence.','Compatibility artifacts retain reports/digests and the baseline corpus, not the raw comparison binaries. Successful original cmp steps provide their comparison evidence.','Sanitizer CTest counts are platform executions; Python nested subtest counts are not inferred from suppressed successful output.','Workflow success does not establish exact acoustic realism or perceptual quality.']}
(p/'supporting-ci.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'status':out['status'],'jobs':[(x['name'],x['conclusion']) for x in rows],'verified_archives':len(artifacts)}))
