from pathlib import Path
import hashlib,json,shutil,subprocess
root=Path('/workspace/scratch/aa102d573ba3')
repo=root/'cs80-packaging-canonical-fixture'
out=repo/'docs/evidence/installer-binary-payload-20261102'
def git(*args):
    return subprocess.check_output(['git','-C',str(repo),*args],text=True).strip()
assert not git('status','--porcelain')
head=git('rev-parse','HEAD')
expected=['CMakeLists.txt','tests/UiReadinessTests.h','tests/release/qualify_installer_reparse_fixture_mutations.py','tests/release/test_installer_binary_payload.py','tests/release/test_installer_reparse_fixture_identity.py']
changed=git('diff','--name-only','9bce169',head).splitlines()
assert [x for x in changed if not x.startswith('docs/')]==expected
for subtree in ['src','scripts','.github','tests/parity']:
    assert git('rev-parse',head+':'+subtree)==git('rev-parse','9bce169:'+subtree)
for p in expected:
    reference='4be3112' if p=='tests/UiReadinessTests.h' else '3fda883'
    assert git('rev-parse',head+':'+p)==git('rev-parse',reference+':'+p)
    assert (repo/p).read_bytes()==subprocess.check_output(['git','-C',str(repo),'show',head+':'+p])
review='docs/evidence/installer-binary-payload-20261102/independent-canonical-3fda883'
assert git('rev-parse',head+':'+review)==git('rev-parse','4599fff:'+review)
verified={}
for name in ['canonical-fixture-repair','independent-canonical-3fda883']:
    base=out/name
    manifest=json.loads((base/'SHA256.json').read_text())
    rows=manifest if isinstance(manifest,list) else manifest.get('files',manifest)
    if isinstance(rows,dict):
        entries=[dict(path=p,**(v if isinstance(v,dict) else {'sha256':v})) for p,v in rows.items()]
    else: entries=rows
    for r in entries:
        p=base/r['path'];data=p.read_bytes()
        assert hashlib.sha256(data).hexdigest()==r['sha256']
        if 'bytes' in r: assert len(data)==r['bytes']
    verified[name]=len(entries)
log=root/'cs80-packaging-canonical-evidence/composed-2f6c755-ctest-six.log'
assert '100% tests passed, 0 tests failed out of 6' in log.read_text()
shutil.copy2(log,out/log.name)
report={'source_code_tested':'2f6c755b2438cf14b25562b3c178cbdf8b7e3dd6',
        'review_docs_composed_head':head,'base_local':'9bce169b919216da3cd1e50c8c7395ca2a2b1f3f',
        'publication_expected_remote':'dcd5f7332f8ca67c2a66eef73f3adf9ab2bd08f4',
        'target_branch':'work/cs80-distribution-notices-sara-20261102',
        'production_src_tree':git('rev-parse',head+':src'),
        'unchanged_subtrees':['src','scripts','.github','tests/parity'],
        'non_document_changed_paths':[{ 'path':p,'git_blob':git('rev-parse',head+':'+p),
            'sha256':hashlib.sha256((repo/p).read_bytes()).hexdigest()} for p in expected],
        'ui_source_mapping':[{'original':o,'composed':git('rev-parse',c)} for o,c in [
            ('b3e26bab1dc9b426197358f370177bf4f61a6189','db463cf'),
            ('3fea56f82607f2df6fea7c4f57502445664705bb','3e37f57'),
            ('4be311228740cd8a77c6278949be5ce7dc9d40f8','2f6c755')]],
        'independent_review_original':'4599fffe8755a065d35afbb3b9bd067b86d902bc',
        'independent_review_composed':head,
        'archive_payloads_verified':verified,'composed_ctest_targets_passed':6,
        'ctest_log_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),
        'native_composed_execution':False,'native_alias_diagnosis':'inference pending native rerun',
        'notice_reparse_branch_excluded':'23e8a96cc3ed2540e625de866bc344e07979282d',
        'release_acceptance':False,'numerical_acceptance':False,
        'local_chain':git('rev-list','--reverse','9bce169..'+head).splitlines()}
(out/'COMPOSED_REPAIR_20261102.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
