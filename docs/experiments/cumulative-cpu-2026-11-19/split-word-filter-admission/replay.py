#!/usr/bin/env python3
"""Rebuild raw differential and/or Callgrind component comparisons in a fresh directory."""
import argparse
import filecmp
import hashlib
import json
import subprocess
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]
STOCK = '5499056a60ee5044275ba17eb7dddded4c96e90d'
OLD_HARNESS = Path('tools/packed-coefficient-admission-research/component.cpp')
HEADERS = ('Ig00156Batch.h','Ig00156Filter.h','Dsp.h','FastMath.h','Ig00156RootFallback.h',
           'AdsrRcReconstruction.inc','FilterEnvRcReconstruction.inc')
FLAGS = ['-std=gnu++20', '-O3', '-DNDEBUG', '-fPIC', '-ffp-contract=off']


def run(args, **kw):
    return subprocess.run(args, check=True, stdout=subprocess.PIPE, **kw).stdout


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def identical(a, b):
    if not filecmp.cmp(a, b, shallow=False):
        raise ValueError(f'{a.name} != {b.name}')
    return {'bytes': a.stat().st_size, 'sha256': digest(a)}


def counts(path):
    lines = path.read_text().splitlines()
    events = next(s for s in lines if s.startswith('events:')).split()[1:]
    total = next(s for s in lines if s.startswith('totals:')).split()[1:]
    parsed = dict(zip(events, map(int, total)))
    return [parsed[key] for key in ('Ir', 'Dr', 'Dw')]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('output', type=Path)
    parser.add_argument('--cxx', default='g++')
    parser.add_argument('--mode', choices=('oracle', 'work', 'mutations'), default='oracle')
    parser.add_argument('--valgrind', default='valgrind')
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    def baseline(path):
        return run(['git','-C',ROOT,'show',STOCK+':'+str(path)])
    # Freeze both arms' common headers and old harness, not just the old cpp.
    common = out/'common'
    for path in [OLD_HARNESS, *(Path('src/core')/name for name in HEADERS)]:
        dest = common/path
        dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(baseline(path))
    stock = out/'stock.cpp'
    stock.write_bytes(baseline('src/core/Ig00156Batch.cpp'))
    new = out/'candidate.cpp'
    new.write_bytes((ROOT/'src/core/Ig00156Batch.cpp').read_bytes())
    cmd = [args.cxx, *FLAGS, '-I'+str(common), '-I'+str(common/'src'), '-I'+str(common/'src/core')]
    report = {'basis': STOCK, 'compiler': run([args.cxx, '--version'], text=True).splitlines()[0],
              'flags': FLAGS, 'source_sha256': {k: digest(v) for k, v in [('stock',stock),('candidate',new)]}}
    if args.mode == 'mutations':
        negative = {}
        for name, old, replacement in [
                ('too-wide-static','_mm256_set1_epi32(highLimit-1)','_mm256_set1_epi32(highLimit)'),
                ('too-wide-dynamic','_mm256_set1_epi32(stateLimit-1)','_mm256_set1_epi32(stateLimit)'),
                ('no-ordered-fallback','if(!ordinary && !orderedEligible(stages,input))','if(!ordinary)')]:
            code = new.read_text()
            if code.count(old)!=1:
                raise ValueError('unexpected mutation anchor: '+name)
            src = out/(name+'.cpp')
            src.write_text(code.replace(old,replacement))
            exe = out/name
            run([*cmd, ROOT/'tests/Ig00156BatchTests.cpp',src,'-o',exe])
            result = subprocess.run([exe],capture_output=True,text=True)
            if result.returncode!=1 or 'FAIL' not in result.stderr:
                raise ValueError('relevant test failed to detect '+name)
            negative[name] = {'exit':result.returncode,'suite':result.stdout.strip(),
                              'first_mismatch':result.stderr.splitlines()[0]}
        report['detected'] = negative
        result = out/'receipt.json'
        result.write_text(json.dumps(report,indent=2)+'\n')
        print(result)
        return
    both = {}
    for arm, src in [('stock',stock), ('candidate',new)]:
        arm_dir = out / arm
        arm_dir.mkdir()
        if args.mode == 'oracle':
            records = {}
            for label, harness in [('static',common/OLD_HARNESS),('dynamic',HERE/'state-oracle.cpp')]:
                exe = arm_dir / label
                run([*cmd, '-Wno-subobject-linkage', f'-DBATCH_SOURCE="{src}"', harness, '-o', exe])
                records[label] = run([exe, arm_dir/(label+'.bin')], text=True).strip()
            exe = arm_dir / 'regression'
            run([*cmd, ROOT/'tests/Ig00156BatchTests.cpp', src, '-o', exe])
            records['permanent_suite'] = run([exe], text=True).strip()
            for name, flag in [('scalar-disabled','-DCS80_DISABLE_AVX2_FILTER_BATCH'),
                               ('nontrapping','-fno-trapping-math')]:
                run([*cmd, flag, '-x', 'c++', '-', '-c', '-o', arm_dir/(name+'.o')],input=src.read_bytes())
            both[arm] = records
        else:
            exe = arm_dir/'stage-work'
            run([*cmd, HERE/'stage-work.cpp', src, '-o', exe])
            rows = {}
            for scene,name in enumerate(('hp','lp','mixed','custom-large','custom-exact-ceiling')):
                row = {}
                for label, first, last in [('first',1,1),('repeat',1,1),('both',1,2),('second',2,2)]:
                    path = arm_dir/f'{scene}-{label}.cg'
                    args_ = [exe, str(scene), str(first), str(last)]
                    native = run(args_, text=True).strip()
                    guest = run([args.valgrind, '--tool=callgrind','--collect-atstart=no','--cache-sim=yes',
                                 '--quiet','--log-file='+str(arm_dir/'valgrind.log'),'--callgrind-out-file='+str(path),
                                 *args_], text=True).strip()
                    if native != guest:
                        raise ValueError(f'{arm} {name} {label} native != guest')
                    row[label] = {'Ir_Dr_Dw': counts(path), 'output':native}
                if row['first'] != row['repeat']:
                    raise ValueError(f'{arm} {name}: A/A mismatch')
                rows[name] = row
            both[arm] = rows
    if args.mode == 'oracle':
        if both['stock'] != both['candidate']:
            raise ValueError('observer counters or permanent regression differ')
        report['both_arms'] = both['stock']
        report['identical_streams'] = {f:identical(out/'stock'/f,out/'candidate'/f) for f in
                                       ('static.bin','dynamic.bin','scalar-disabled.o','nontrapping.o')}
    else:
        report['valgrind'] = run([args.valgrind, '--version'], text=True).strip()
        report['arms'] = both
        for scene,row in both['stock'].items():
            candidate = both['candidate'][scene]
            delta = {}
            for window,rec in row.items():
                if rec['output'] != candidate[window]['output']:
                    raise ValueError(f'{scene} {window}: stock != candidate')
                delta[window] = [y-x for x,y in zip(rec['Ir_Dr_Dw'],candidate[window]['Ir_Dr_Dw'])]
            if any(a+b!=c for a,b,c in zip(delta['first'],delta['second'],delta['both'])):
                raise ValueError(f'{scene}: marginal counts not additive')
    result = out/'receipt.json'
    result.write_text(json.dumps(report,indent=2)+'\n')
    print(result)


if __name__ == '__main__':
    main()
