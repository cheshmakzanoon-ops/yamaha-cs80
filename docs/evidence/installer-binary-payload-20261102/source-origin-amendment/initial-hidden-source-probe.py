import importlib.util,json,subprocess,sys
from pathlib import Path
root=Path('/workspace/scratch/aa102d573ba3/cs80-installer-binaries')
sys.path.insert(0,str(root/'tests/release'));sys.path.insert(0,str(root/'scripts'))
import test_distribution_docs as fixtures
import verify_distribution_docs as verifier
out=[]
for flag in ['--assume-unchanged','--skip-worktree']:
 case=fixtures.DistributionDocsTests('test_staged_metadata_is_exact_and_explicitly_limited');case.setUp()
 try:
  original=case.root/'LICENSE'; before=original.read_bytes();case.git('update-index',flag,'LICENSE');original.write_bytes(b'altered source notice hidden by index flag\n')
  status=case.git('status','--porcelain=v1','--untracked-files=all').strip()
  target=case.build/'hidden-source-payload';manifest=case.helper().stage_documents(case.root,case.license,target)
  verified=verifier.verify_documents(case.root,case.license,target,case.commit)
  assert (target/'LICENSE').read_bytes()!=before
  out.append({'flag':flag,'git_status':status,'actual_notice_matches_git_blob':False,'staging_admitted':True,'independent_verifier_admitted':True,'document_source_commit':manifest['source_commit'],'verified_document_count':verified['document_count']})
 finally:case.doCleanups()
print(json.dumps({'scope':'Actual synthetic repository hidden-index source-identity reproduction; no production package altered','observations':out},indent=2))
