#!/usr/bin/env python3
"""Isolated actual-source Engine/audit revision mutants; caller owns heavy lock."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--cxx',default='c++');p.add_argument('--group',choices=('all','engine','audit'),default='all')
    args=p.parse_args();args.out.mkdir(parents=True,exist_ok=False)
    root=Path(__file__).resolve().parents[2]
    tests={'engine':'PublicToneEngineRevisionTests.cpp','audit':'PublicToneAuditRevisionTests.cpp'}
    files=sorted(f for f in (root/'src').rglob('*') if f.is_file())+[root/'tests'/name for name in tests.values()]
    hashes={str(f.relative_to(root)):sha(f) for f in files}
    cases=[('positive-engine','engine',None,None,None),('positive-audit','audit',None,None,None),
      ('v1-only-compiler','engine','src/core/Engine.cpp',
       '? source_factory::compileTonePublicEstimate(rank, selector, tones[rank])',
       '? source_factory::compileTone(rank, selector, tones[rank])'),
      ('revision-cache-key-removed','engine','src/core/Engine.cpp',
       '|| revisions[rank] != frame.factoryToneRevision','/* actual revision cache key removed */'),
      ('revision-validation-removed','engine','src/core/Engine.cpp',
       'if (frame.factoryToneRevision != FactoryToneRevision::frozenV1\n        && frame.factoryToneRevision != FactoryToneRevision::publicAmendedV2)',
       'if (false) /* actual invalid revision guard removed */'),
      ('globalized-revision-cache','engine','src/core/Engine.cpp',
       'sourceToneRevisions = revisions;','sourceToneRevisions.fill(frame.factoryToneRevision);'),
      ('audit-propagation-removed','audit','src/tools/engine_audit.cpp',
       'frame.factoryToneRevision = auditFactoryToneRevision;',
       '/* actual candidate propagation removed */'),
      ('audit-flush-removed','audit','src/tools/engine_audit.cpp',
       'policy.flush();','/* actual buffered-write sync removed */')]
    if args.group!='all': cases=[case for case in cases if case[1]==args.group]
    rows=[];positive={}
    for name,kind,changed,old,new in cases:
        stage=args.out/name;stage.mkdir()
        for f in files:
            dest=stage/f.relative_to(root);dest.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(f,dest)
            if sha(dest)!=hashes[str(f.relative_to(root))]: raise RuntimeError('staging identity mismatch')
        if changed:
            target=stage/changed;s=target.read_text()
            if s.count(old)!=1: raise RuntimeError('actual mutation locator changed: '+name)
            target.write_text(s.replace(old,new))
        binary=stage/'test'
        command=[args.cxx,'-std=c++20','-O3','-DNDEBUG','-Isrc','src/core/Engine.cpp','src/core/FactoryPresets.cpp',
                 'tests/'+tests[kind],'-o',str(binary)]
        compiled=subprocess.run(command,cwd=stage,capture_output=True,text=True,timeout=300)
        (stage/'compile.log').write_text(compiled.stdout+compiled.stderr)
        row={'name':name,'kind':kind,'compile_command':command,'compile_exit':compiled.returncode,
             'compile_log_sha256':sha(stage/'compile.log')}
        if changed: row.update({'changed_source':changed,'changed_source_sha256':sha(stage/changed)})
        if compiled.returncode==0:
            run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=300)
            (stage/'run.log').write_text(run.stdout+run.stderr)
            match=re.search(r'Public tone (?:Engine|audit) revision: (\d+) checks, (\d+) failures;',run.stdout)
            checks,failures=map(int,match.groups()) if match else (0,-1)
            passed=bool(match) and (run.returncode==1 and failures>0 if changed else run.returncode==0 and failures==0)
            row.update({'run_exit':run.returncode,'checks':checks,'ordinary_failures':failures,
                        'qualification_pass':passed,'binary_sha256':sha(binary),'log_sha256':sha(stage/'run.log')})
            if not changed: positive[kind]=binary
        else: row['qualification_pass']=False
        rows.append(row);print(json.dumps(row),flush=True)
    for kind,binary in positive.items():
        run=subprocess.run([str(binary)],capture_output=True,text=True,timeout=300)
        log=args.out/('restored-'+kind+'.log');log.write_text(run.stdout+run.stderr)
        match=re.search(r'Public tone (?:Engine|audit) revision: (\d+) checks, (\d+) failures;',run.stdout)
        rows.append({'name':'restored-'+kind,'run_exit':run.returncode,
                     'qualification_pass':run.returncode==0 and bool(match) and int(match[2])==0,'log_sha256':sha(log)})
    unchanged=all(sha(root/name)==value for name,value in hashes.items())
    receipt={'schema_version':1,'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
             'source_sha256':hashes,'source_unchanged':unchanged,'runs':rows,
             'status':'pass' if unchanged and all(row['qualification_pass'] for row in rows) else 'fail',
             'boundary':'Software wiring/mutation evidence only; no original885 or default adoption.'}
    (args.out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0 if receipt['status']=='pass' else 1


if __name__=='__main__': raise SystemExit(main())
