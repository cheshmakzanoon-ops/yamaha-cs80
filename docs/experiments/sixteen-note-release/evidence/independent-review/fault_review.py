from pathlib import Path
import json,hashlib,runpy,subprocess,time
root=Path('/workspace/scratch/ee447c520988'); base=root/'sixteen-release-evidence/fault-controls'; run=base/'run-1'; ev=root/'sixteen-release-independent-review'; src=root/'sixteen-note-release'; driver=run/'reproducer.py'; m=runpy.run_path(str(driver)); summary=json.loads((run/'summary.json').read_text()); commands=json.loads((run/'commands.json').read_text()); by={v['label']:v for v in commands}; inputs=json.loads((run/'input-hashes.json').read_text())
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
old='Assign shared order to allocations executed in deferred/ARP context.';new='Assign shared order to deferred scanner allocations/promotions; ARP direct startVoice default-zero identity is unchanged.'
assert driver.read_text().replace(old,new)==(base/'check_fault_controls.py').read_text()
assert all(sha(src/f)==h for f,h in inputs['source_test_sha256'].items())
assert sha(run/'frozen/SixteenVoiceReleaseTests.cpp')==inputs['source_test_sha256']['tests/SixteenVoiceReleaseTests.cpp']
assert sha(Path(inputs['archive_path']))==inputs['archive_sha256']
parser=m['parser_self_test']();cases=[('unmutated-control',summary['control'],[])]+[(name,summary['mutations'][name],edits) for name,desc,edits in m['MUTATIONS']]+[(m['REDUNDANT'][0],summary['redundancy_control'],m['REDUNDANT'][2])]
review={'historical_driver_sha256':sha(driver),'current_driver_sha256':sha(base/'check_fault_controls.py'),'historical_current_driver_delta':'Only the descriptive scanner-versus-ARP string was corrected. Mutation definitions and executable logic are identical. An initial strict whole-driver hash check stopped before runs; this exact description-only delta was then verified.','summary_sha256':sha(run/'summary.json'),'parser_self_test':parser,'cases':{},'independent_runs':[],'notes':['The qualify-prior-deferred-candidates mutation removes the immediate flag check in nextReleaseOrder, affecting scanner allocations and promotions. ARP startVoice still receives default order zero.','The historical driver gate omits an explicit redundancy_control status conjunction. Its actual redundancy control is nevertheless validated separately here. Final archive verifier must enforce it explicitly.']}
for name,result,edits in cases:
 folder=run/name; private=folder/'src'; files=json.loads((folder/'source-hashes.json').read_text());assert all(sha(private/f)==h for f,h in files.items())
 expected={str(p.relative_to(run/'frozen/src')):p.read_bytes() for p in (run/'frozen/src').rglob('*') if p.is_file()}
 for rel,before,after in edits:
  assert expected[rel].decode().count(before)==1
  expected[rel]=expected[rel].decode().replace(before,after).encode()
 assert set(expected)==set(files) and all((private/f).read_bytes()==b for f,b in expected.items())
 for key,f in [('diff_sha256','mutation.diff'),('engine_object_sha256','Engine.o'),('test_object_sha256','tests.o'),('binary_sha256','probe'),('link_map_sha256','link.map')]: assert sha(folder/f)==result[key]
 for stage in ['engine-compile','test-compile','link','run']:
  row=by[name+'-'+stage]
  assert sha(run/row['stdout'])==row['stdout_sha256'] and sha(run/row['stderr'])==row['stderr_sha256']
  if stage!='run': assert row['exit_code']==0
 for kind,source,obj in [('engine',private/'core/Engine.cpp',folder/'Engine.o'),('test',run/'frozen/SixteenVoiceReleaseTests.cpp',folder/'tests.o')]:
  argv=by[name+'-'+kind+'-compile']['argv'];assert '-I'+str(private) in argv and argv[argv.index('-c')+1]==str(source) and argv[argv.index('-o')+1]==str(obj)
 link=by[name+'-link']['argv'];assert link.index(str(folder/'Engine.o'))<link.index(inputs['archive_path']) and link.index(str(folder/'tests.o'))<link.index(inputs['archive_path'])
 linkmap=(folder/'link.map').read_text();assert 'libcs80_core.a(Engine.cpp.o)' not in linkmap and 'LOAD '+str(folder/'Engine.o') in linkmap and 'LOAD '+str(folder/'tests.o') in linkmap
 row=by[name+'-run'];parsed=m['parse_summary']((run/row['stdout']).read_text(),(run/row['stderr']).read_text(),row['exit_code'],result['expected_exit']);assert parsed['checks']==result['checks'] and parsed['failures']==result['failures'] and parsed['exit_code']==result['exit_code']
 review['cases'][name]={'checks':parsed['checks'],'failures':parsed['failures'],'exit':parsed['exit_code'],'hashes_commands_private_headers_private_engine_link_verified':True}
assert summary['control']['status']=='passed' and summary['control']['exit_code']==0 and summary['redundancy_control']['status']=='undetected-redundant' and summary['redundancy_control']['exit_code']==0 and summary['redundancy_control']['failures']==0
assert len(summary['mutations'])==11 and all(v['status']=='detected' and v['exit_code']==1 for v in summary['mutations'].values())
assert summary['control']['binary_sha256']==json.loads((ev/'execution.json').read_text())['cs80_sixteen_voice_release_tests']['sha256']
for name in ['broadcast-two-engines','accept-unqualified-candidate']:
 binary=run/name/'probe';before=sha(binary);start=time.time();p=subprocess.run([str(binary)],capture_output=True,text=True,timeout=120);label='fault-'+name
 (ev/(label+'.stdout.log')).write_text(p.stdout);(ev/(label+'.stderr.log')).write_text(p.stderr);(ev/(label+'.exit')).write_text(str(p.returncode)+'\n')
 parsed=m['parse_summary'](p.stdout,p.stderr,p.returncode,1);assert parsed['checks']==1418 and parsed['failures']==summary['mutations'][name]['failures'] and before==sha(binary)==summary['mutations'][name]['binary_sha256']
 review['independent_runs'].append({'name':name,'argv':[str(binary)],'binary_sha256':before,'seconds':time.time()-start,**parsed,'stdout_sha256':sha(ev/(label+'.stdout.log')),'stderr_sha256':sha(ev/(label+'.stderr.log'))});print(name,parsed,flush=True)
review['source_test_unchanged_after']=all(sha(src/f)==h for f,h in inputs['source_test_sha256'].items());assert review['source_test_unchanged_after']
review['result']='Passed independent source/manifest review, all 13 case validations, parser self-tests, and two representative mutant reruns. No builds or source edits.'
(ev/'fault-review.json').write_text(json.dumps(review,indent=2)+'\n')
p=ev/'review.json';r=json.loads(p.read_text());r['fault_control_review']='fault-review.json';p.write_text(json.dumps(r,indent=2)+'\n')
print(review['result'])
