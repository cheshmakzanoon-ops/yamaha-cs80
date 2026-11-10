#!/usr/bin/env python3
import hashlib, json, pathlib, subprocess
H=pathlib.Path(__file__).resolve().parent; R=H.parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def equal(a,b):
    with a.open('rb') as x,b.open('rb') as y:
        while True:
            aa=x.read(1024*1024);bb=y.read(1024*1024)
            if aa!=bb:return False
            if not aa:return True
results={'source':str(H/'CompatibilityCapture-v2.cpp'),'source_sha256':sha(H/'CompatibilityCapture-v2.cpp'),'profiles':[]}
for profile in ['gcc-generic','gcc-native','clang-generic','clang-native']:
    b=H/'CompatibilityCapture-v2'/('baseline-'+profile);c=H/'CompatibilityCapture-v2'/('candidate-'+profile)
    rb=json.loads((b/'record.json').read_text());rc=json.loads((c/'record.json').read_text())
    flags=lambda rec:[x for x in rec['compile_command'][1:-4] if not x.startswith('-I')]
    q={'profile':profile,'source_matches_frozen':rb['source_sha256']==rc['source_sha256']==results['source_sha256'],
       'compiler_versions_match':rb['compiler_version']==rc['compiler_version'],'compile_flags_match':flags(rb)==flags(rc),
       'compile_exits':[rb['compile_exit'],rc['compile_exit']],'run_exits':[x['exit'] for r in [rb,rc] for x in r['runs']],
       'baseline_partition_equal':equal(b/'partition-0.bin',b/'partition-1.bin'),
       'candidate_partition_equal':equal(c/'partition-0.bin',c/'partition-1.bin'),
       'partition_0_baseline_candidate_equal':equal(b/'partition-0.bin',c/'partition-0.bin'),
       'partition_1_baseline_candidate_equal':equal(b/'partition-1.bin',c/'partition-1.bin'),
       'capture_bytes':(b/'partition-0.bin').stat().st_size,'capture_sha256':sha(b/'partition-0.bin'),
       'baseline_archive_sha256':rb['archive_sha256'],'candidate_archive_sha256':rc['archive_sha256']}
    q['passed']=all(q[k] for k in ['source_matches_frozen','compiler_versions_match','compile_flags_match','baseline_partition_equal','candidate_partition_equal','partition_0_baseline_candidate_equal','partition_1_baseline_candidate_equal']) and all(x==0 for x in q['compile_exits']+q['run_exits'])
    results['profiles'].append(q)
# Direct byte comparison of baseline header tree with the archive's recorded source tree.
base=R/'sixteen-note-release'; original=R/'sixteen-note-release-impl';candidate=R/'release-followup'
results['baseline_header_archive_source_tree_equal']=all(equal(f,original/f.relative_to(base)) for f in (base/'src/core').rglob('*') if f.is_file())
results['baseline_header_revision']=subprocess.check_output(['git','-C',str(base),'rev-parse','HEAD'],text=True).strip()
results['baseline_archive_source_revision']=subprocess.check_output(['git','-C',str(original),'rev-parse','HEAD'],text=True).strip()
results['candidate_core_matches_c0219bd']=subprocess.check_output(['git','-C',str(candidate),'diff','c0219bd','--','src/core'],text=True)==''
results['all_passed']=all(x['passed'] for x in results['profiles']) and results['baseline_header_archive_source_tree_equal'] and results['candidate_core_matches_c0219bd']
(H/'comparison.json').write_text(json.dumps(results,indent=2)+'\n')
print(json.dumps(results,indent=2))
raise SystemExit(0 if results['all_passed'] else 1)
