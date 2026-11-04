#!/usr/bin/env python3
"""Mutation-check exact CPU skips against their published continuation contracts.

Builds run in a temporary consistent source tree; production files are never
modified. --overlay may select reviewed, unintegrated candidate source files.
Without an overlay, evaluate the selected files from --source.
"""
from pathlib import Path
import argparse
import difflib
import hashlib
import json
import re
import shutil
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--group',choices=['idle','stationary','lfo'],required=True)
    parser.add_argument('--overlay',type=Path)
    parser.add_argument('--definitions',type=Path,default=Path(__file__).with_name('exact-skip-mutations.json'))
    parser.add_argument('--registration-sha',required=True)
    parser.add_argument('--evidence',type=Path,required=True)
    parser.add_argument('--compiler',default='c++')
    args=parser.parse_args()
    root=args.source.resolve();overlay=(args.overlay or args.source).resolve();evidence=args.evidence.resolve()
    evidence.mkdir(parents=True,exist_ok=True)
    contracts={
        'idle':'tests/parity/tolerances-v2/idle-exact-skip.json',
        'stationary':'tests/parity/tolerances-v2/stationary-controls-exact.json',
        'lfo':'tests/parity/tolerances-v2/unused-lfo-readout-exact.json'}
    contract=(root/contracts[args.group]).read_bytes()
    committed=subprocess.check_output(['git','show',args.registration_sha+':'+contracts[args.group]],cwd=root)
    if contract.replace(b'\r\n',b'\n')!=committed:
        raise RuntimeError('Acceptance differs from the published registration')
    drivers={
        'idle':'tests/IdleExactSkipTests.cpp',
        'stationary':'tests/StationaryControlsExactTests.cpp',
        'lfo':'tests/UnusedLfoReadoutExactTests.cpp',
        'lfo_engine':'tests/UnusedLfoEngineOwnershipTests.cpp'}
    targets=['lfo','lfo_engine'] if args.group=='lfo' else [args.group]
    mutations=json.loads(args.definitions.read_text())[args.group]
    source_paths={
        'idle':['src/core/BbdChorus.h','src/core/PraWahCircuit.h'],
        'stationary':['src/core/Smoothing.h','src/core/ExpressionOptics.h'],
        'lfo':['src/core/ModulationLfo.h','src/core/Engine.cpp']}
    originals={p:(overlay/p).read_bytes() for p in source_paths[args.group]}
    results=[]
    with tempfile.TemporaryDirectory(prefix='cs80-exact-'+args.group+'-') as temporary:
        tree=Path(temporary)
        # Engine mutations compile a consistent full source snapshot, including
        # every header that determines the owning Engine object's layout.
        if args.group=='lfo':shutil.copytree(root/'src',tree/'src')
        for path,data in originals.items():
            dest=tree/path;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        def execute(name,target,expected=()):
            binary=tree/(target+'-gate')
            command=[args.compiler,'-std=c++20','-O2','-DNDEBUG','-Wall','-Wextra','-pedantic',
                     '-I',str(tree/'src'),'-I',str(root/'src/core'),'-I',str(root/'src'),str(root/drivers[target])]
            if target=='lfo_engine':command.extend([str(tree/'src/core/Engine.cpp'),str(tree/'src/core/FactoryPresets.cpp')])
            command.extend(['-o',str(binary)])
            built=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
            (evidence/(name+'-build.log')).write_text('COMMAND '+json.dumps(command)+'\n'+built.stdout+'\nEXIT '+str(built.returncode)+'\n')
            if built.returncode:raise RuntimeError(name+' did not compile; not a mutation kill')
            run=subprocess.run([str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
            (evidence/(name+'-run.log')).write_text(run.stdout+'\nEXIT '+str(run.returncode)+'\n')
            match=re.search(r': (\d+) checks, (\d+) failures',run.stdout)
            if not match:raise RuntimeError(name+' did not finish its assertion gate')
            result={'name':name,'target':target,'exit_code':run.returncode,'checks':int(match[1]),'failures':int(match[2]),
                    'expected_assertions':list(expected),'all_expected_assertions_observed':all(p in run.stdout for p in expected)}
            results.append(result);print(json.dumps(result),flush=True)
            if expected:
                if run.returncode!=1 or result['failures']==0 or not result['all_expected_assertions_observed']:
                    raise RuntimeError(name+' did not reject the intended defect')
            elif run.returncode or result['failures']:
                raise RuntimeError(name+' baseline failed')
        for target in targets:execute('candidate-'+target,target)
        for case in mutations:
            path=case['path'];before=originals[path].decode();after=before
            for old,new in case['replacements']:
                if after.count(old)!=1:raise RuntimeError(case['name']+' source anchor is not unique: '+old)
                after=after.replace(old,new,1)
            (tree/path).write_text(after)
            (evidence/(case['name']+'.patch')).write_text(''.join(difflib.unified_diff(
                before.splitlines(True),after.splitlines(True),fromfile='a/'+path,tofile='b/'+path)))
            execute(case['name'],case['target'],case['expected'])
            (tree/path).write_bytes(originals[path])
        for target in targets:execute('restored-'+target,target)
    for path,data in originals.items():
        if (overlay/path).read_bytes()!=data:raise RuntimeError('Original overlay was modified')
    record={'group':args.group,'registration_commit':args.registration_sha,
            'contract_sha256':hashlib.sha256(contract).hexdigest(),
            'source_sha256':{p:hashlib.sha256(b).hexdigest() for p,b in originals.items()},
            'driver_sha256':{p:hashlib.sha256((root/drivers[p]).read_bytes()).hexdigest() for p in targets},
            'compiler':args.compiler,'results':results,
            'scope':'Fresh independent software continuation and actual-source mutations; whole-engine compatibility and timings are separate.'}
    (evidence/'qualification.json').write_text(json.dumps(record,indent=2)+'\n')


if __name__=='__main__':main()
