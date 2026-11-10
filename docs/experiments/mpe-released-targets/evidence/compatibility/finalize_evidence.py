#!/usr/bin/env python3
import hashlib,json,pathlib,subprocess
H=pathlib.Path(__file__).resolve().parent;R=H.parents[1]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def cmd(*a):return subprocess.check_output(a,text=True)
provenance={}
for name,rev in [('baseline','sixteen-note-release'),('baseline_archive_source','sixteen-note-release-impl'),('candidate','release-followup')]:
    repo=R/rev
    inputs={str(f.relative_to(repo)):sha(f) for d in ['src/core','src/experimental'] for f in sorted((repo/d).rglob('*')) if f.is_file()}
    provenance[name]={'repository':str(repo),'head':cmd('git','-C',str(repo),'rev-parse','HEAD').strip(),'runtime_diff_from_frozen_commit':cmd('git','-C',str(repo),'diff','2ade213' if name.startswith('baseline') else 'c0219bd','--','src/core','src/experimental'),'input_hashes':inputs}
provenance['baseline_runtime_sources_match']=provenance['baseline']['input_hashes']==provenance['baseline_archive_source']['input_hashes']
(H/'runtime-provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
files={str(f.relative_to(H)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(H.rglob('*')) if f.is_file() and f.name not in ['integrity.json']}
(H/'integrity.json').write_text(json.dumps(files,indent=2)+'\n')
print('Artifacts:',len(files),'Runtime identity:',provenance['baseline_runtime_sources_match'])
