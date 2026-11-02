#!/usr/bin/env python3
"""Byte-compare pristine/default Engine binaries; caller holds shared heavy lock."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

BASE_TREE='561f6bf37b8dcc46c54b5a35e328c08f0cbec399'


def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--baseline-revision',required=True);p.add_argument('--cxx',default='c++');args=p.parse_args()
    root=Path(__file__).resolve().parents[2];args.out.mkdir(parents=True,exist_ok=False)
    def git(*parts): return subprocess.check_output(['git',*parts],cwd=root)
    tree=git('rev-parse',args.baseline_revision+'^{tree}').decode().strip()
    if tree!=BASE_TREE: raise RuntimeError('baseline tree differs from preregistered pristine source')
    fixture=root/'tests/diagnostics/FactoryToneDefaultReplay.cpp'
    baseline=git('ls-tree','-r','--name-only',args.baseline_revision,'--','src').decode().splitlines()
    current=sorted(str(f.relative_to(root)) for f in (root/'src').rglob('*') if f.is_file())
    current_hashes={name:sha(root/name) for name in current};fixture_hash=sha(fixture)
    source_hashes={};runs=[];positive=None
    for name in ('pristine','candidate','default-v2-mutant'):
        stage=args.out/name;stage.mkdir();source_hashes[name]={}
        for relative in baseline if name=='pristine' else current:
            dest=stage/relative;dest.parent.mkdir(parents=True,exist_ok=True)
            data=git('show',args.baseline_revision+':'+relative) if name=='pristine' else (root/relative).read_bytes()
            dest.write_bytes(data);source_hashes[name][relative]=sha(dest)
            if name!='pristine' and sha(dest)!=current_hashes[relative]: raise RuntimeError('staged source changed')
        copied=stage/'fixture.cpp';shutil.copy2(fixture,copied)
        if sha(copied)!=fixture_hash: raise RuntimeError('fixture changed during stage')
        if name=='default-v2-mutant':
            header=stage/'src/core/Engine.h';text=header.read_text()
            old='FactoryToneRevision factoryToneRevision = FactoryToneRevision::frozenV1;'
            if text.count(old)!=1: raise RuntimeError('default mutation locator changed')
            header.write_text(text.replace(old,'FactoryToneRevision factoryToneRevision = FactoryToneRevision::publicAmendedV2;'))
            source_hashes[name]['src/core/Engine.h']=sha(header)
        binary=stage/'replay';pcm=stage/'pcm.bin'
        command=[args.cxx,'-std=c++20','-O3','-DNDEBUG','-Isrc','src/core/Engine.cpp','src/core/FactoryPresets.cpp','fixture.cpp','-o',str(binary)]
        build=subprocess.run(command,cwd=stage,capture_output=True,text=True,timeout=300)
        (stage/'compile.log').write_text(build.stdout+build.stderr)
        row={'name':name,'compile_exit':build.returncode,'compile_command':command}
        if build.returncode==0:
            run=subprocess.run([str(binary),str(pcm)],capture_output=True,text=True,timeout=300)
            (stage/'run.log').write_text(run.stdout+run.stderr)
            row.update({'run_exit':run.returncode,'run_log_sha256':sha(stage/'run.log'),'binary_sha256':sha(binary)})
            if run.returncode==0:
                bits=pcm.read_bytes();row.update({'pcm_bytes':len(bits),'pcm_sha256':sha(pcm)})
                if name=='pristine': positive=bits;row['qualification_pass']=True
                else: row['qualification_pass']=positive is not None and ((bits==positive) if name=='candidate' else (bits!=positive))
            else: row['qualification_pass']=False
        else: row['qualification_pass']=False
        runs.append(row);print(json.dumps(row),flush=True)
    unchanged=sha(fixture)==fixture_hash and all(sha(root/path)==value for path,value in current_hashes.items())
    receipt={'schema_version':1,'baseline_revision_historical':args.baseline_revision,'baseline_tree':tree,
             'candidate_revision':git('rev-parse','HEAD').decode().strip(),'fixture_sha256':fixture_hash,
             'source_sha256':source_hashes,'source_unchanged':unchanged,'runs':runs,
             'compiler':subprocess.check_output([args.cxx,'--version'],text=True),
             'status':'pass' if unchanged and all(r['qualification_pass'] for r in runs) else 'fail',
             'boundary':'798 bounded historical-default cases; not original885, integrated PR6 or CPU qualification.'}
    (args.out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    return 0 if receipt['status']=='pass' else 1


if __name__=='__main__': raise SystemExit(main())
