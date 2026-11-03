import ast,hashlib,json,re,subprocess,sys
from pathlib import Path
root=Path('/workspace/scratch/aa102d573ba3/cs80-independent-packaging-canonical-3fda');out=Path('/workspace/scratch/aa102d573ba3/cs80-independent-packaging-canonical-3fda-results')
def git(*a):return subprocess.check_output(['git','-C',str(root),*a])
def sha(b):return hashlib.sha256(b).hexdigest()
head=git('rev-parse','HEAD').decode().strip(); assert head=='3fda883d2d786895731ade5eb74b45ab4a80e5a2'
base='9bce169b919216da3cd1e50c8c7395ca2a2b1f3f'
fixture='tests/release/test_installer_binary_payload.py';helper='scripts/verify_installer_binaries.py';runner='tests/release/qualify_installer_binary_mutations.py';test='tests/release/test_installer_reparse_fixture_identity.py'
assert git('rev-parse',head+':src').decode().strip()==git('rev-parse',base+':src').decode().strip()=='7dd159d491012fd47b46c0aa651e86ea959ca289'
for p in [helper,runner]: assert git('show',base+':'+p)==(root/p).read_bytes()
old=ast.parse(git('show',base+':'+fixture));new=ast.parse((root/fixture).read_bytes())
def without_fixture(t):
 for c in t.body:
  if isinstance(c,ast.ClassDef) and c.name=='InstallerBinaryPayloadTests':c.body=[f for f in c.body if not (isinstance(f,ast.FunctionDef) and f.name=='simulated_directory_reparse')]
 return ast.dump(t,include_attributes=False)
assert without_fixture(old)==without_fixture(new), 'Unrelated original test behavior changed'
changes=git('diff','--name-only',base,head).decode().splitlines()
assert set(changes)=={'CMakeLists.txt','docs/evidence/installer-binary-payload-20261102/CANONICAL_FIXTURE_PREREG_20261102.md','tests/release/qualify_installer_reparse_fixture_mutations.py',fixture,test}
# Independently execute the exact preimplementation fixture, not just the
# new mutation's semantically equivalent lexical comparison replacement.
stage=out/'preimplementation';stage.mkdir()
for p in [fixture,helper,test]:
 data=git('show','100e0a8:'+p);dest=stage/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
result=subprocess.run([sys.executable,str(stage/test)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=60,check=False)
(out/'preimplementation-red.log').write_text(result.stdout)
expected=sorted(['test_noncanonical_artifact_retains_original_reparse_rejection','test_noncanonical_nested_bundle_reparse_is_rejected','test_selected_lexical_and_canonical_spellings_receive_attributes'])
assert result.returncode==1
assert re.findall(r'^Ran (\d+) tests in ',result.stdout,re.M)==['5']
assert sorted(re.findall(r'^FAIL: (\w+) ',result.stdout,re.M))==expected
assert not re.findall(r'^ERROR:',result.stdout,re.M)
assert re.findall(r'^(?:OK|FAILED \(.*\))$',result.stdout,re.M)==['FAILED (failures=3)']
# Author evidence is a docs-only child; verify its complete committed inventory.
archive_revision='d74b9e2222f24d727ef59cd1c62f04fbc3bdfe12'
archive='docs/evidence/installer-binary-payload-20261102/canonical-fixture-repair'
manifest=json.loads(git('show',archive_revision+':'+archive+'/SHA256.json'))
checks=[]
for name,record in manifest.items():
 data=git('show',archive_revision+':'+archive+'/'+name); assert sha(data)==record['sha256'] and len(data)==record['bytes'],name
 checks.append({'path':name,**record})
allpaths=git('ls-tree','-rz','--name-only',archive_revision,'--',archive).decode().strip('\0').split('\0')
assert set(allpaths)=={archive+'/'+n for n in manifest}|{archive+'/SHA256.json'}
assert len(manifest)==44
for p in changes:
 if not p.startswith('docs/'):assert git('show',archive_revision+':'+p)==(root/p).read_bytes()
report={'reviewed_commit':head,'base_commit':base,'source_tree':'7dd159d491012fd47b46c0aa651e86ea959ca289','five_changed_paths':changes,'all_original_test_AST_except_fixture_helper_unchanged':True,'production_verifier_blob':git('rev-parse',head+':'+helper).decode().strip(),'original13_control_runner_blob':git('rev-parse',head+':'+runner).decode().strip(),'preimplementation_revision':git('rev-parse','100e0a8').decode().strip(),'preimplementation_red':{'tests':5,'ordinary_failures':expected,'runtime_errors':0,'exit':result.returncode},'author_archive_revision':archive_revision,'author_archive_payloads_verified':checks,'clean_source_checkout':git('status','--porcelain')==b''}
(out/'static-review.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='author_archive_payloads_verified'}))
