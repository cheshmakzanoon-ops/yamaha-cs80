"""Bounded local diagnostic campaign. No native run occurs without --run-approved.
Inputs are matched sealed bundles and the separately reviewed materialized host.
"""
import argparse,hashlib,importlib.util,json,os,pathlib,resource,secrets,signal,stat,struct,subprocess,sys,time
from timing_study_decode import decode_timing,source_protocols,paired_summary,digest
ROOT=pathlib.Path(__file__).resolve().parent
U=lambda n:struct.pack('<Q',n)
T=lambda s:U(len(s.encode()))+s.encode()
H=lambda p:hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()

def request_bytes(bundle,arm,policy):
 bundle=pathlib.Path(bundle).resolve();module='Contents/x86_64-linux/CS-80 Reference.so'
 def root(p):
  s=p.stat();return T(str(p))+U(stat.S_IMODE(s.st_mode))+U(s.st_uid)+U(s.st_gid)
 def entry(p):
  s=p.lstat();directory=stat.S_ISDIR(s.st_mode)
  if not(directory or stat.S_ISREG(s.st_mode)) or (not directory and s.st_nlink!=1):raise ValueError('bundle must contain only regular singlelink files and directories')
  name='.' if p==bundle else p.relative_to(bundle).as_posix()
  return T(name)+U(directory)+U(stat.S_IMODE(s.st_mode))+U(s.st_uid)+U(s.st_gid)+U(0 if directory else s.st_size)+T('' if directory else H(p))
 paths=[bundle]+sorted(bundle.rglob('*'));assert len(paths)<=64
 return T('cs80-process-lifetime-request-v4')+T('process-lifetime')+T(arm)+T('abcdef019182faeb5265664943733830')+b''.join(U(x) for x in [policy,48000,0,16,64,2,2,2,2,24000,64,64,64*1024**2,4096])+root(bundle)+T(module)+U(len(paths))+b''.join(entry(p) for p in paths)+U(0)+U(0)

def schedule():
 result=[]
 for family,blocks in [('aa',2),('comparison',6)]:
  for block in range(blocks):
   for policy in range(3):
    for slot,label in enumerate('ABBA'):
     result.append({'family':family,'block':block,'policy':policy,'slot':slot,'label':label,'arm':'candidate' if family=='comparison' and label=='B' else 'stock'})
 return result

def run_one(exe,request,out,env):
 out.mkdir(mode=0o700);nonce=secrets.token_hex(32);argv=[str(exe),str(request),H(request),nonce,str(out/'receipt')]
 def limits():
  resource.setrlimit(resource.RLIMIT_CORE,(0,0));resource.setrlimit(resource.RLIMIT_FSIZE,(32*1024**2,32*1024**2))
 start=time.monotonic();reason=None
 with (out/'stdout').open('xb') as stdout,(out/'stderr').open('xb') as stderr:
  p=subprocess.Popen(argv,cwd=out,env=env,stdout=stdout,stderr=stderr,start_new_session=True,preexec_fn=limits)
  while p.poll() is None:
   if time.monotonic()-start>=25:reason='wall_deadline'
   if sum(f.stat().st_size for f in out.rglob('*') if f.is_file())>32*1024**2:reason='retained_cap'
   if reason:os.killpg(p.pid,signal.SIGKILL);break
   time.sleep(.01)
  code=p.wait(timeout=3)
 try:os.killpg(p.pid,0);empty=False
 except ProcessLookupError:empty=True
 if not empty:os.killpg(p.pid,signal.SIGKILL);reason=reason or 'remaining_process_group'
 record={'argv':argv,'nonce':nonce,'pid':p.pid,'returncode':code,'wall_seconds':time.monotonic()-start,'watchdog_reason':reason,'group_empty':empty,'environment':env,'files':{f.name:{'bytes':f.stat().st_size,'sha256':H(f)} for f in out.iterdir() if f.is_file()}}
 (out/'execution.json').write_text(json.dumps(record,indent=2)+'\n')
 if code!=0 or reason or not empty:raise RuntimeError('terminal timing process rejected: '+str(out))
 return record

def main():
 a=argparse.ArgumentParser();a.add_argument('--host',required=True);a.add_argument('--probe',required=True);a.add_argument('--stock',required=True);a.add_argument('--candidate',required=True);a.add_argument('--runtime-library-path',required=True);a.add_argument('--output',required=True);a.add_argument('--run-approved',action='store_true');args=a.parse_args()
 if not args.run_approved:raise SystemExit('Preparation only. Explicit reviewed run flag required.')
 host=pathlib.Path(args.host).resolve();out=pathlib.Path(args.output).resolve();out.mkdir(mode=0o700,parents=True,exist_ok=False);home=out/'home';home.mkdir(mode=0o700)
 env={'PATH':'/usr/bin:/bin','HOME':str(home),'LANG':'C','LC_ALL':'C','LD_LIBRARY_PATH':str(pathlib.Path(args.runtime_library_path).resolve())}
 spec=importlib.util.spec_from_file_location('isolated_timing_lifecycle',host/'process_lifetime_transaction.py');helper=importlib.util.module_from_spec(spec);spec.loader.exec_module(helper)
 bundle={'stock':pathlib.Path(args.stock).resolve(),'candidate':pathlib.Path(args.candidate).resolve()};module={k:v/'Contents/x86_64-linux/CS-80 Reference.so' for k,v in bundle.items()}
 frozen={'plan_sha256':H(ROOT/'timing_study_plan.json'),'overlay_manifest_sha256':H(host/'overlay-manifest.json'),'driver_sha256':H(host/'observer'),'probe_sha256':H(args.probe),'helper_sha256':H(host/'process_lifetime_transaction.py'),'runner_sha256':H(__file__),'decoder_sha256':H(ROOT/'timing_study_decode.py'),'module_sha256':{k:H(v) for k,v in module.items()},'schedule':schedule(),'environment':env,'production_qualified':False,'runtime_closure_complete':False}
 static_files={ROOT/'timing_study_plan.json':frozen['plan_sha256'],host/'overlay-manifest.json':frozen['overlay_manifest_sha256'],pathlib.Path(args.probe):frozen['probe_sha256'],host/'process_lifetime_transaction.py':frozen['helper_sha256'],pathlib.Path(__file__):frozen['runner_sha256'],ROOT/'timing_study_decode.py':frozen['decoder_sha256']}
 def verify_static():
  for path,wanted in static_files.items():
   if H(path)!=wanted:raise RuntimeError('frozen study code changed: '+str(path))
 verify_static()
 overlay=json.loads((host/'overlay-manifest.json').read_text())
 for name,wanted in overlay['materialized_sha256'].items():
  if H(host/name)!=wanted:raise RuntimeError('materialized host/helper drift: '+name)
 contracts={};requests={}
 for arm in bundle:
  for policy in range(3):
   key=(arm,policy);request=out/f'{arm}-{policy}.request';request.write_bytes(request_bytes(bundle[arm],arm,policy));request.chmod(0o600);requests[key]=request
   lines=subprocess.run([args.probe,str(request),H(request)],env=env,capture_output=True,text=True,check=True,timeout=10).stdout.splitlines();assert len(lines)==6
   protocols=source_protocols(bytes.fromhex(lines[2]))
   contracts[key]={'schema':helper.SCHEMA,'history_schema':helper.SOURCE_HISTORY,'arm':arm,'expected_manifest_digest':lines[0],'expected_evidence_digest':lines[1],'module_sha256':H(module[arm]),'case_identity_hex':lines[2],'protocol_sha256':lines[3:],'warmup_protocol_sha256':digest(protocols[4]),'measured_protocol_sha256':digest(protocols[5]),'controller_query_policy':2,'controller_query_result':1,'controller_query_pointer_present':False,'audio_input_topology':2,'parameter_binding_policy':2,'controller_state_policy':2,'controller_state_result':3,'selected_cid_hex':'abcdef019182faeb5265664943733830','controller_cid_hex':'abcdef011234abcd5265664943733830','module_root':str(bundle[arm]),'module_relative':'Contents/x86_64-linux/CS-80 Reference.so'}
   (out/f'{arm}-{policy}.contract.json').write_text(json.dumps(contracts[key],indent=2)+'\n')
 for policy in range(3):
  for field in ('case_identity_hex','protocol_sha256','warmup_protocol_sha256','measured_protocol_sha256'):
   if contracts[('stock',policy)][field]!=contracts[('candidate',policy)][field]:raise RuntimeError('cross-arm frozen workload mismatch: '+field)
 (out/'study-plan.json').write_bytes((ROOT/'timing_study_plan.json').read_bytes())
 frozen['requests']={f'{a}-{p}':H(v) for (a,p),v in requests.items()};(out/'frozen-plan.json').write_text(json.dumps(frozen,indent=2)+'\n')
 baseline={};completed=[]
 for index,item in enumerate(frozen['schedule']):
  verify_static();arm,policy=item['arm'],item['policy'];key=(arm,policy)
  if H(module[arm])!=frozen['module_sha256'][arm] or H(host/'observer')!=frozen['driver_sha256'] or H(requests[key])!=frozen['requests'][f'{arm}-{policy}']:raise RuntimeError('prelaunch immutable input changed')
  if sum(p.stat().st_size for p in out.rglob('*') if p.is_file())>1024**3-32*1024**2:raise RuntimeError('campaign retained bound')
  folder=out/f'{index:03d}-{item["family"]}-{policy}-{item["block"]}-{item["slot"]}-{arm}';execution=run_one(host/'observer',requests[key],folder,env)
  result=decode_timing((folder/'receipt').read_bytes(),contracts[key],execution['nonce'],execution['pid'],module[arm].stat(),helper);result.pop('payload')
  pcm=[s['pcm_sha256'] for s in result['timing_segments']]
  if policy not in baseline:baseline[policy]=pcm
  result['exact_cross_run_pcm_match']=pcm==baseline[policy];result.update(item)
  (folder/'decoded.json').write_text(json.dumps(result,indent=2)+'\n')
  if not result['exact_cross_run_pcm_match']:raise RuntimeError('matched complete-history PCM changed; retain results, stop campaign')
  if H(module[arm])!=frozen['module_sha256'][arm] or H(host/'observer')!=frozen['driver_sha256'] or H(requests[key])!=frozen['requests'][f'{arm}-{policy}']:raise RuntimeError('postrun module/observer/request changed')
  verify_static();completed.append(result);print(json.dumps({'index':index,**item,'cpu_ns_per_frame':result['measured_cpu_ns_per_frame']}),flush=True)
 summaries={}
 for policy in range(3):
  groups={}
  for family,count in [('aa',2),('comparison',6)]:
   groups[family]=[]
   for block in range(count):
    rows=[r for r in completed if r['policy']==policy and r['family']==family and r['block']==block];assert [r['slot'] for r in rows]==[0,1,2,3]
    groups[family].append({'cpu_per_frame':[r['measured_cpu_ns_per_frame'] for r in rows],'overhead_fraction':[r['measured_clock_overhead_fraction'] for r in rows]})
  summaries[policy]=paired_summary(groups['comparison'],groups['aa'])
 verify_static()
 (out/'summary.json').write_text(json.dumps(summaries,indent=2)+'\n')
if __name__=='__main__':main()
