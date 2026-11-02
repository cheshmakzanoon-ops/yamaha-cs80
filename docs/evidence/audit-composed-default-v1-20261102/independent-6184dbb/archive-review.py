import datetime,hashlib,json,re,shutil,subprocess
from pathlib import Path
root=Path('/workspace/scratch/aa102d573ba3/cs80-independent-composed-default-6184')
source=Path('/workspace/scratch/aa102d573ba3/cs80-independent-composed-default-6184-results')
dest=root/'docs/evidence/audit-composed-default-v1-20261102/independent-6184dbb'
def sha(b):return hashlib.sha256(b).hexdigest()
assert subprocess.check_output(['git','-C',str(root),'status','--porcelain'])==b''
assert not dest.exists()
source_report=json.loads((source/'source-mutants/qualification.json').read_bytes())
namespace_report=json.loads((source/'namespace-mutants/qualification.json').read_bytes())
assert source_report['semantic_mutants']==15 and len(source_report['cases'])==18
assert namespace_report['semantic_mutants']==6 and len(namespace_report['cases'])==9
log_checks=[]
for family,report in [('source-mutants',source_report),('namespace-mutants',namespace_report)]:
 for case in report['cases']:
  rel=f"{family}/{case['case']}.log" if family=='source-mutants' else f"{family}/{case['case']}/run.log"
  raw=(source/rel).read_bytes(); assert sha(raw)==case['log_sha256']
  if case['case']=='runtime_error_control': assert not case['assessment']['qualified']
  else: assert case['assessment']['qualified']
  log_checks.append({'path':rel,'sha256':sha(raw)})
assert len(log_checks)==27
for name,n in [('test_full_engine_audit_composed_default_v1.py',45),('test_full_engine_audit_composed_default_v1_registration.py',7),('test_full_engine_audit_composed_default_v1_lock_timeout.py',3),('test_full_engine_audit_composed_default_v1_completion_faults.py',2)]:
 text=(source/(name+'.log')).read_text(); assert re.findall(r'^Ran (\d+) tests in ',text,re.M)==[str(n)]; assert re.findall(r'^(?:OK|FAILED \(.*\))$',text,re.M)==['OK']
assert 'PASS: 10 audit coverage and production-identity mutation controls' in (source/'self-test.log').read_text()
assert '"preflight": "passed"' in (source/'preflight.log').read_text()
assert (source/'actionlint.log').read_bytes()==b''
static=json.loads((source/'static-review.json').read_bytes());assert static['archived_file_count']==61
# Retain complete raw outputs, including reviewer-side schema/count mistakes.
paths=[p.relative_to(source) for p in source.iterdir() if p.is_file() and p.name!='archive-review.py']
paths += [p.relative_to(source) for p in (source/'source-mutants').iterdir() if p.is_file()]
paths += [Path('namespace-mutants/qualification.json')]
for case in namespace_report['cases']:
 folder=Path('namespace-mutants')/case['case']; paths += [folder/'run.log',folder/'result.json']
 if case['mutation']: paths.append(folder/case['mutation']['path'])
paths.append(Path('archive-review.py'))
for relative in paths:
 p=dest/relative;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes((source/relative).read_bytes())
review={'reviewed_commit':'6184dbbc6f8244adbe729a713e2c0f90b22a96c8','implementation_commit':'46ec619096d5513724d8d40edc16779f1403976b','preregistration_commit':'598b8a52de3b5127edbb4724cb935fd9d9e467f0','reviewed_tree':static['head_tree'],'recorded_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'decision':'bounded capture-infrastructure review passed; no blocking findings','scope':'independent source inspection and replay of existing exact registered fixtures; no actual musical rendering','registered_R':'6eda5b0dee3b74e418d1c30517d4d7101fcb50f0','registered_R_tree':'c48bca5994c18e813a148f498e118fc63dd7d5af','source_F':static['source_F'],'source_T':static['source_T'],'base_tests':{'source_completion':45,'namespace_workflow':7,'lock_runtime':3,'partial_write_faults':2},'unchanged_V1_self_controls':10,'local_registered_R_F_T_preflight':'passed','source_guard_mutants':15,'namespace_mutants':6,'runtime_crashes_refused':2,'positive_and_restored':'passed both families','actionlint':'official 1.7.12 passed; optional external shellcheck/pyflakes disabled','source_F_git_paths_modes_blobs_unchanged':2623,'wrapper_and_four_copied_test_runner_files_namespace_only':True,'old_archive_inventory':{'hashed_payloads':61,'files_including_SHA256_json':62,'evidence_family_files_including_README_and_PREREGISTRATION':64},'old_archive_hashes_verified':61,'fresh_control_logs_verified':log_checks,'reviewer_harness_attempts_retained':['Initial static script looked for SHA256SUMS and checked zero archive files; not an archive pass.','Next script treated SHA256.json records as digest strings; failed before archive acceptance.','Next script assumed handoff 63 meant hashed payloads; actual complete manifest contains61. These are reviewer harness errors, not source-control kills.'],'source_edits_before_or_during_tests':False,'no_new_remote_read_claimed':True,'not_established':['actual885 capture','numerical PASS','later source tree coverage','CPU budget','native DAW or installer result','hardware parity','main activation or release'],'limitations':['Existing cooperative-worktree assumptions preserved; no hostile-writer or fsync/atomic-durability claim.','Original V1 output markers are only interpreted through the separate source-bound completion wrapper.','Actual remote checkout and Actions capture remain required for the registered F source.']}
(dest/'review.json').write_text(json.dumps(review,indent=2)+'\n')
(dest/'README.md').write_text('''# Independent composed-default V1 infrastructure review\n\nClean source `6184dbb` / implementation `46ec619`, reviewed independently on 2026-11-02. **No blocking findings within this capture-infrastructure scope.** No source or test code was changed during review.\n\nFresh replays pass 45 source/completion + 7 namespace/workflow + 3 lock/runtime + 2 partial-write fixtures and ten unchanged V1 self-controls. The local registered R/F/T preflight passes. All fifteen actual guard-source controls and six namespace/workflow controls fail with their exact prescribed ordinary assertion inventories; both deliberate crash substitutes are refused. Both families retain unmodified and restored passes. Official actionlint1.7.12 passes with only optional external linters disabled. All27 fresh control-log digests match their receipts.\n\nIndependently reproducing the committed namespace translation gives the exact wrapper and four copied test/runner files. Every2,623 original F Git path/mode/blob remains unchanged; source T is `0f8a26e733644ac9f5985164e0175d4143c5d917`. The previous archive has61 hashed payloads and62 files including its hash manifest (the whole family has64 including README and preregistration); all61 payload hashes and the complete inventory agree. Three reviewer-side archive-script assumptions are retained, corrected, and explicitly excluded from semantic control results.\n\nThe workflow is read-only, fully checks registered identities and source controls before build, retains the unabridged885-row three-rate command, and always uploads completed or partial evidence. Separate namespace markers do not replace the original V1 outputs. The unchanged audit executable returns normally after capture irrespective of numerical findings; those findings are reported, not waived.\n\nThis reviews only registered F `68c466d1dd27530dfef85479c7fdc59168320c36`, **not later source trees**. It does not establish actual885 rendering, numerical acceptance, CPU, native DAW/installer behavior, hardware parity, release eligibility, or new remote state. No formal PR approval was submitted. No hostile-writer, atomic/fsync-durability, or additional lifecycle guarantee is added beyond the inherited bounded contract.\n\nRaw logs, exact source mutation recipes, namespaced mutated files, static inspection script, and immutable receipts are adjacent. Each base suite and each source-control case held the shared heavy lock separately. The existing mutation harnesses were executed unchanged.\n''')
manifest={str(p.relative_to(dest)):{'sha256':sha(p.read_bytes()),'bytes':p.stat().st_size} for p in sorted(dest.rglob('*')) if p.is_file()}
(dest/'SHA256.json').write_text(json.dumps(manifest,indent=2)+'\n')
for n,v in manifest.items(): assert sha((dest/n).read_bytes())==v['sha256']
print(json.dumps({'decision':review['decision'],'archived_payload_files':len(manifest),'fresh_control_log_hashes':len(log_checks),'path':str(dest)}))
