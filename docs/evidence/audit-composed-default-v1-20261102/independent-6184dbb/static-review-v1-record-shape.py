import hashlib,json,subprocess
from pathlib import Path
root=Path('/workspace/scratch/aa102d573ba3/cs80-independent-composed-default-6184')
out=Path('/workspace/scratch/aa102d573ba3/cs80-independent-composed-default-6184-results')
def git(*a):return subprocess.check_output(['git','-C',str(root),*a])
def sha(b):return hashlib.sha256(b).hexdigest()
head=git('rev-parse','HEAD').decode().strip()
assert head=='6184dbbc6f8244adbe729a713e2c0f90b22a96c8'
contract=json.loads((root/'tests/release/full-engine-audit-composed-default-v1-namespace.json').read_bytes())
old=git('show',contract['source_infrastructure_commit']+':scripts/full_engine_audit_gate_v2.py')
assert sha(old)==contract['original_wrapper_sha256']
translated=old.decode()
for a,b in contract['translation_pairs']: translated=translated.replace(a,b)
current=(root/contract['wrapper_path']).read_bytes()
assert translated.encode()==current and sha(current)==contract['wrapper_sha256']
def tree(c):
 d={}
 for r in git('ls-tree','-rz',c).split(b'\0'):
  if not r:continue
  m,p=r.split(b'\t',1);d[p.decode()]=m.decode()
 return d
f=tree(contract['source_commit']); h=tree(head)
assert len(f)==2623
assert not [(p,v,h.get(p)) for p,v in f.items() if h.get(p)!=v]
assert git('rev-parse',head+':src').decode().strip()==contract['expected_src_tree']
checks=[]
for p in sorted((root/'docs/evidence/audit-composed-default-v1-20261102/qualified-46ec619').rglob('SHA256.json')):
 for name,want in json.loads(p.read_bytes()).items():
  actual=sha((p.parent/name).read_bytes()); assert want==actual,(name,want,actual)
  checks.append({'path':str((p.parent/name).relative_to(root)),'sha256':actual})
assert len(checks)==63
# All copied fixture/runner bytes are exact same preregistered namespace
# transformation; original behavior is not independently regenerated here.
copies={
 'tests/release/test_full_engine_audit_composed_default_v1.py':'tests/release/test_full_engine_audit_v2.py',
 'tests/release/test_full_engine_audit_composed_default_v1_completion_faults.py':'tests/release/test_full_engine_audit_v2_completion_faults.py',
 'tests/release/test_full_engine_audit_composed_default_v1_lock_timeout.py':'tests/release/test_full_engine_audit_v2_lock_timeout.py',
 'tests/release/qualify_full_engine_audit_composed_default_v1_mutations.py':'tests/release/qualify_full_engine_audit_v2_mutations.py',
}
copy_result=[]
for new,prior in copies.items():
 b=git('show',contract['source_infrastructure_commit']+':'+prior);s=b.decode()
 for a,z in contract['translation_pairs']:s=s.replace(a,z)
 c=(root/new).read_bytes()
 assert s.encode()==c, new
 copy_result.append({'path':new,'original_path':prior,'sha256':sha(c),'namespace_only':True})
report={'reviewed_head':head,'head_tree':git('rev-parse','HEAD^{tree}').decode().strip(),'source_F':contract['source_commit'],'source_T':contract['expected_src_tree'],'all_2623_original_F_paths_modes_blobs_unchanged':True,'wrapper_namespace_only':True,'wrapper_sha256':sha(current),'original_wrapper_sha256':sha(old),'copied_tests_and_runner':copy_result,'archive_sha256_checks':checks,'archived_file_count':len(checks),'no_source_edits':git('status','--porcelain').decode()==''}
(out/'static-review.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k not in ('archive_sha256_checks','copied_tests_and_runner')}))
