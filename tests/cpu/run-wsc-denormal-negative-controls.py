#!/usr/bin/env python3
"""Compile independent WSC controls and source mutants serially outside the repo.

Caller must hold the project heavy-work lock. Never changes source/oracle pins,
never accepts compile errors or signals as an expected ordinary assertion.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

FLAGS=['-O3','-DNDEBUG','-std=gnu++20','-frounding-math']


def changed(text,old,new):
    if text.count(old)!=1:
        raise ValueError('source mutation anchor is not unique: '+old)
    return text.replace(old,new)


def run(command,base):
    result=subprocess.run(command,text=True,stdout=subprocess.PIPE,stderr=subprocess.PIPE)
    base.with_suffix('.stdout').write_text(result.stdout)
    base.with_suffix('.stderr').write_text(result.stderr)
    base.with_suffix('.command.json').write_text(json.dumps(command,indent=2)+'\n')
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--cxx',default='g++')
    parser.add_argument('--objdump',default='objdump')
    args=parser.parse_args()
    source=args.source.resolve();out=args.out.resolve()
    if out.exists():
        raise ValueError('evidence directory must be new; never overwrite prior results')
    out.mkdir(parents=True)
    header=(source/'src/core/ReconstructedOscillator.h').read_text()
    oracle=source/'tests/reference/WscLoopOriginal.h'
    if hashlib.sha256(oracle.read_bytes()).hexdigest()!='9976f2e1a2de7ec2513c156cf70cc020911edfe486b70651c4425e97bdca4c4c':
        raise ValueError('immutable WSC oracle changed')
    backend=['CS80_WSC_FOUR_LANE_BACKEND_TEST=1']
    variants=[
      ('scalar-positive',header,[],0,None,False),
      ('backend-positive',header,backend,0,None,True),
      ('old-denormal-guard',changed(header,'if((mxcsr&(3u<<13))!=0u ||',
          'if((mxcsr&((3u<<13)|(1u<<15)|(1u<<6)))!=0u ||'),backend,1,
          'WSC denormal-mode actual-admission work gate failed',False),
      ('disabled-backend',changed(header,'if(!__builtin_cpu_supports("avx2")||',
          'if(true||!__builtin_cpu_supports("avx2")||'),backend,1,
          'WSC denormal-mode actual-admission work gate failed',False),
      ('forged-scalar-work',changed(header,'        advanceTickFourAvx2(lanes);',
          '        for(auto* lane:lanes)lane->advance(lane->dt,lane->endNodes);'),backend,0,
          None,True),
      ('skipped-vector-state',changed(header,'        advanceTickFourAvx2(lanes);',
          '        /* source mutant: skip physical vector state work */'),backend,1,
          'WSC exact bit mismatch budget exceeded',False),
      ('cross-vector-lane',changed(header,'freq[i]=-lane->state.heldFrequencyHz;',
          'freq[i]=-lanes[(i+1)%4]->state.heldFrequencyHz;'),backend,1,
          'WSC exact bit mismatch budget exceeded',False),
      ('unmasked-x87',header,backend+['CS80_WSC_FOUR_LANE_MUTATE_X87_GUARD=1'],1,
          'x87 unmasked-trap batch admitted or changed FP status',False),
    ]
    for name,assertion in [('MASK','lane.state.'),('CROSS_LANE','lane.state.'),
                           ('EVENT','events.absoluteSeconds'),('FP','fp.exceptions')]:
        variants.append(('original-'+name.lower(),header,['CS80_WSC_LANE_MUTATE_'+name+'=1'],
                         1,assertion,False))
    records=[]
    for name,body,defines,expected,assertion,inspect in variants:
        directory=out/name
        (directory/'src/core').mkdir(parents=True)
        (directory/'tests/reference').mkdir(parents=True)
        (directory/'src/core/ReconstructedOscillator.h').write_text(body)
        shutil.copyfile(oracle,directory/'tests/reference/WscLoopOriginal.h')
        shutil.copyfile(source/'tests/WscIndependentLanePreregTests.cpp',
                        directory/'tests/WscIndependentLanePreregTests.cpp')
        binary=directory/'test'
        command=[args.cxx,*FLAGS,'-I'+str(directory/'src'),'-I'+str(directory/'tests')]
        command.extend('-D'+value for value in defines)
        command.extend([str(directory/'tests/WscIndependentLanePreregTests.cpp'),'-o',str(binary)])
        compile_result=run(command,directory/'compile')
        if compile_result.returncode:
            raise RuntimeError(name+': compile failed, not an accepted negative')
        executed=run([str(binary)],directory/'run')
        if executed.returncode!=expected:
            raise RuntimeError(name+': unexpected exit '+str(executed.returncode))
        if assertion and assertion not in executed.stdout+executed.stderr:
            raise RuntimeError(name+': named assertion absent')
        record={'name':name,'compile_exit':0,'run_exit':executed.returncode,
                'expected_assertion':assertion,'source_sha256':hashlib.sha256(body.encode()).hexdigest(),
                'binary_sha256':hashlib.sha256(binary.read_bytes()).hexdigest()}
        if inspect:
            inspected=run([sys.executable,str(source/'tests/cpu/check-wsc-vector-work.py'),
                           str(binary),'--objdump',args.objdump],directory/'vector-work')
            expected_work=1 if name=='forged-scalar-work' else 0
            if inspected.returncode!=expected_work:
                raise RuntimeError(name+': vector work witness returned '+str(inspected.returncode))
            if expected_work and 'FAIL WSC real four-double vector arithmetic missing' not in inspected.stderr:
                raise RuntimeError(name+': missing ordinary real-vector assertion')
            record['vector_work_exit']=inspected.returncode
        records.append(record)
        print(name+': expected result observed',flush=True)
    summary={'compiler':subprocess.check_output([args.cxx,'--version'],text=True),
             'flags':FLAGS,'source':str(source),'oracle_sha256':hashlib.sha256(oracle.read_bytes()).hexdigest(),
             'variants':records,'passed':True,'performance_claim':False}
    (out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
    print('WSC denormal source controls: all '+str(len(records))+' variants passed',flush=True)

if __name__=='__main__':
    try:
        main()
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:
        print('WSC negative-control FAIL: '+str(error),file=sys.stderr)
        sys.exit(1)
