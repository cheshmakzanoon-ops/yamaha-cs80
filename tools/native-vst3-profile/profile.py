#!/usr/bin/env python3
"""Build/collect a new Linux diagnostic, pack symbols, or replay the portable stock record."""
import argparse
from collections import Counter
import csv
import gzip
import hashlib
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.dont_write_bytecode = True
# Reuse the project's strict ELF64 executable-range, bounded-symbol and allocation reader.
sys.path.insert(0, str(ROOT / 'docs/experiments/shipping-core-profile'))
from elf_evidence import Elf, digest

SCENES = {'fresh-idle': (1, 16384), 'post-release': (1, 16384),
          'held8': (1, 4096), 'wake': (16, 512)}
ORDER = [('fresh-idle', 997), ('post-release', 3109), ('held8', 997), ('wake', 3109),
         ('wake', 997), ('held8', 3109), ('post-release', 997), ('fresh-idle', 3109),
         ('fresh-idle', 0), ('post-release', 0), ('held8', 0), ('wake', 0)]
DEFAULT = ROOT / 'docs/experiments/native-idle-profile/captures.json.gz'
RECORDED_SHA256 = '011efd7504577d5c78673099e36110e573d7f098bf315cff4a70f623b60aeae4'
UNKNOWN = '[outside bounded physical symbol]'
def check(ok, msg):
    if not ok:
        raise ValueError(msg)

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def build(args):
    out = args.output.resolve()
    check(not out.exists(), 'refusing to replace existing host')
    interfaces = ('pluginterfaces/base/coreiids.cpp', 'pluginterfaces/base/funknown.cpp',
                  'public.sdk/source/vst/vstinitiids.cpp')
    subprocess.run([args.cxx, '-O2', '-g', '-DNDEBUG', '-std=gnu++20',
                    '-I'+str(ROOT/'src'), '-I'+str(ROOT/'tests'), '-I'+str(args.sdk.resolve()),
                    HERE/'host.cpp', *(args.sdk.resolve()/p for p in interfaces),
                    '-ldl', '-pthread', '-o', out], check=True)

def collect(args):
    root, image, host = args.output.resolve(), args.module.resolve(), args.host.resolve()
    check(image.is_file() and host.is_file(), 'host and module must be regular files')
    check(args.cpu in os.sched_getaffinity(0), 'CPU not permitted')
    root.mkdir()  # one new directory; do not overwrite or silently resume
    start = (sha(image), sha(host))
    (root/'schedule.txt').write_text(''.join(f'{s} {p}\n' for s,p in ORDER))
    for scene, period in ORDER:
        with tempfile.TemporaryDirectory() as home:
            env = dict(os.environ, LC_ALL='C', HOME=home, XDG_CONFIG_HOME=home+'/config', XDG_DATA_HOME=home+'/data')
            dest = root/f'{scene}-{period}'
            subprocess.run(['taskset','-c',str(args.cpu),str(host),str(image),scene,str(period),str(dest)],
                           check=True, timeout=65, env=env)
    check(start == (sha(image), sha(host)), 'module or host changed during collection')

def bid(path):
    text = subprocess.check_output(['readelf', '-nW', str(path)], text=True)
    ids = re.findall(r'Build ID: ([0-9a-f]+)', text)
    check(len(ids) == 1, 'expected one GNU build ID')
    return ids[0]

def location(raw):
    if raw.startswith('??:'):
        return ''
    suffix = re.sub(r' \(discriminator \d+\)$', '', raw)
    if '/src/src/' in suffix:
        return 'src/'+suffix.rsplit('/src/src/',1)[1]
    if '/src/core/' in suffix:
        return 'src/core/'+suffix.rsplit('/src/core/',1)[1]
    if '/src/plugin/' in suffix:
        return 'src/plugin/'+suffix.rsplit('/src/plugin/',1)[1]
    # Never export user paths. Other compiler/SDK names are simply annotations.
    return 'external/'+suffix.rsplit('/',1)[-1]

def leaves(path, addresses):
    if not addresses:
        return {}
    text = subprocess.check_output(['addr2line','-e',str(path),'-a','-f','-C','-i'], text=True,
                                   input=''.join(f'{pc:x}\n' for pc in sorted(addresses)))
    chains = {}
    current = None
    for line in text.splitlines():
        if re.fullmatch(r'0x[0-9a-f]+', line):
            current=int(line,16); chains[current]=[]
        else:
            check(current is not None, 'unexpected addr2line output')
            chains[current].append(line)
    check(set(chains) == set(addresses), 'missing DWARF reply')
    return {k:(c[0],location(c[1])) for k,c in chains.items() if len(c)>=2 and len(c)%2==0}

def pack(args):
    check(not args.output.exists(), 'refusing to overwrite export')
    root=args.captures.resolve()
    expected=''.join(f'{s} {p}\n' for s,p in ORDER)
    check((root/'schedule.txt').read_text()==expected, 'unexpected schedule')
    plugin,host=Elf(args.module), Elf(args.host)
    build_id=bid(args.module)
    physic={k:e.physical_map()[0] for k,e in [('@plugin',plugin),('@host',host)]}
    executable={k:[(int(s[1],16),int(s[2],16)) for s in e.executable_ranges()]
                for k,e in [('@plugin',plugin),('@host',host)]}
    debug=args.debug_module or args.module
    proof={'stock_sha256':plugin.sha256,'plugin_build_id':build_id,'host_sha256':host.sha256}
    if args.debug_module:
        twin=Elf(debug)
        a=plugin.allocatable(); b=twin.allocatable()
        a.pop('.note.gnu.build-id',None); b.pop('.note.gnu.build-id',None)
        check(a==b, 'mapping module changes allocated sections or addresses')
        proof.update(debug_sha256=twin.sha256, symbol_twin_all_allocated_except_build_id=digest(a))
    keys=set()
    rough=[]
    for scene,period in ORDER:
        d=root/f'{scene}-{period}'
        meta=json.loads((d/'scene.json').read_text()); sample=json.loads((d/'sampler.json').read_text())
        check(sample['plugin_build_id']==build_id,'loaded image differs from supplied object')
        rows=[]
        with (d/'pcs.tsv').open() as f:
            reader=csv.DictReader(f,delimiter='\t')
            check(reader.fieldnames==['region','module','elf_vaddr','count'],'bad PC schema')
            for row in reader:
                mod=row['module']; pc=int(row['elf_vaddr'],16); key=(mod,pc)
                check(mod not in executable or any(a<=pc<b for a,b in executable[mod]),
                      'PC outside supplied module executable range')
                keys.add(key);rows.append((int(row['region']),key,int(row['count'])))
        rough.append((meta,sample,rows))
    names=sorted(keys); indexes={key:i for i,key in enumerate(names)}
    annotation=leaves(debug,{pc for mod,pc in names if mod=='@plugin'})
    table=[]
    for mod,pc in names:
        f=physic[mod](pc) if mod in physic else mod
        if f.startswith('[main ELF'):
            f=UNKNOWN
        leaf,src=annotation.get(pc,('','')) if mod=='@plugin' else ('','')
        table.append([mod,hex(pc),f,leaf if src else '',src])
    captures=[{'scene':meta,'sampler':sample,'pcs':[[r,indexes[key],n] for r,key,n in rows]}
              for meta,sample,rows in rough]
    export={'schema':'cs80-native-callback-pcs-v1','identity':proof,'symbols':table,'captures':captures}
    summary(export)
    raw=json.dumps(export,separators=(',',':')).encode()
    args.output.write_bytes(gzip.compress(raw,mtime=0))
    print(f'{len(table)} distinct tagged PCs, {len(captures)} captures; {sha(args.output)}')

def summary(bundle):
    check(bundle['schema']=='cs80-native-callback-pcs-v1','schema mismatch')
    ident=bundle['identity']; symbols=bundle['symbols']; captures=bundle['captures']
    check(re.fullmatch(r'[0-9a-f]{64}',ident['stock_sha256']) and re.fullmatch(r'[0-9a-f]+',ident['plugin_build_id']),
          'bad module identity')
    check(len(captures)==len(ORDER),'incomplete campaign')
    check(len({(s[0],s[1]) for s in symbols})==len(symbols) and
          all(len(s)==5 and '/' not in s[0] and re.fullmatch('0x[0-9a-f]+',s[1])
              and not s[4].startswith('/') for s in symbols),'malformed PC dictionary')
    common={};results=[];used=set()
    for (scene,period), cap in zip(ORDER,captures):
        c=cap['scene']; stat=cap['sampler']; entries=cap['pcs']
        check((c['scene'],c['period_us'])==(scene,period), 'invalid capture order')
        check(stat['plugin_build_id']==ident['plugin_build_id'] and stat['foreign_signals']==0 and stat['dropped']==0,
              'timer or module mismatch')
        check(all(type(stat[k]) is int and stat[k]>=0 for k in
                  ('recorded','outside','timer_overruns','dropped','foreign_signals')),'bad signal counters')
        reps, calls=SCENES[scene]
        check((c['placements'],c['region_calls'],c['region_frames'],c['warm_calls_per_instance'],c['rate'],c['block'])
              ==(reps,calls,64*calls,1024,48000,64),'invalid fixed scene geometry')
        check(all(type(c[k]) in (float,int) and math.isfinite(c[k]) and 0<=c[k] for k in
                  ('quiet_guard_peak','region_peak','region_rms','warm_held_peak'))
              and type(c['region_nonzero_samples']) is int and 0<=c['region_nonzero_samples']<=128*calls,
              'invalid audio diagnostics')
        check((c['warm_note_on_reads'],c['release_note_off_reads'],c['region_note_on_reads'])
              ==(8 if scene in ('held8','post-release') else 0,8 if scene=='post-release' else 0,128 if scene=='wake' else 0),
              'invalid event read census')
        check(c['release_wait_calls']==(8192 if scene=='post-release' else 0) and
              c['quiet_guard_frames']==(0 if scene=='held8' else 64*64*reps) and
              c['quiet_guard_peak']<=1e-6 and c['quiet_admission'] and c['audible_admission'], 'quiet guard mismatch')
        check(c['region_peak']<=1e-6 if scene in ('fresh-idle','post-release') else c['region_peak']>1e-5,
              'invalid observed audio guard')
        check(re.fullmatch('[0-9a-f]{16}',c['region_pcm_fnv1a64']), 'invalid PCM fingerprint')
        audio={k:v for k,v in c.items() if k!='period_us'}
        check(common.setdefault(scene,audio)==audio, 'sampling changed scene receipts or PCM fingerprint')
        counts=Counter(); totals=Counter(); physical=Counter(); leafs=Counter(); annotations=Counter(); seen=set()
        for r,idx,n in entries:
            check(type(n) is int and n>0 and r in (1,2,3) and type(idx) is int and 0<=idx<len(symbols)
                  and (r,idx) not in seen,'invalid or duplicate counted PC')
            seen.add((r,idx));used.add(idx);totals[r]+=n
            mod,_,f,leaf,loc=symbols[idx]
            if r==1:
                check(mod=='@host' and f=='cs80_profile_calibration','calibration outside known arithmetic loop')
                continue
            check(r!=3 or scene=='wake','onset PC outside wake')
            physical[f]+=n;leafs[leaf or f]+=n;annotations[loc or '[no source]']+=n
            if 'cs80::ReconstructedOscillator::' in f: group='oscillator family (physical)'
            elif f.startswith('cs80::Engine::updateSourceReadouts('): group='updateSourceReadouts (physical)'
            elif 'cs80::Ig00156BatchAccess::process(' in f: group='four-stage batch solver (physical)'
            elif 'cs80::OversamplingDecimator::output()' in leaf: group='decimator output (inline leaf)'
            elif mod=='@plugin': group='other plug-in PCs'
            else: group='outside plug-in'
            counts[group]+=n
        check(sum(totals.values())==stat['recorded'],'lost or extra counted PCs')
        if period==0:
            check(not entries and not stat['outside'] and not stat['timer_overruns'], 'control received timer samples')
        else:
            check(totals[1]>=10 and totals[2]+totals[3]>=100,'insufficient signal to rank broad regions')
        results.append({'scene':scene,'period_us':period,'calibration_pcs':totals[1],
                      'callback_pcs':totals[2]+totals[3],'first_wake_pcs':totals[3],
                      'outside_region_signals':stat['outside'],'timer_overruns':stat['timer_overruns'],
                      'exclusive_groups':dict(counts.most_common()),'physical_top':physical.most_common(10),
                      'leaf_top':leafs.most_common(10),'source_top':annotations.most_common(10)})
    check(used==set(range(len(symbols))),'unused PC dictionary entries')
    return results

def load(path):
    if path.resolve()==DEFAULT.resolve():
        check(sha(path)==RECORDED_SHA256, 'bundled evidence SHA-256 mismatch')
    return json.loads(gzip.decompress(path.read_bytes()))

def rebind(bundle, module, host, debug):
    proof=bundle['identity']
    check(sha(module)==proof['stock_sha256'] and sha(host)==proof['host_sha256'] and
          bid(module)==proof['plugin_build_id'], 'rebind needs the exact sampled binary and host')
    ep=Elf(module); eh=Elf(host); lookup={'@plugin':ep.physical_map()[0], '@host':eh.physical_map()[0]}
    if 'debug_sha256' in proof:
        check(debug and sha(debug)==proof['debug_sha256'],'rebind needs the exact mapping twin')
        a=ep.allocatable();b=Elf(debug).allocatable()
        a.pop('.note.gnu.build-id',None);b.pop('.note.gnu.build-id',None)
        check(a==b and digest(a)==proof['symbol_twin_all_allocated_except_build_id'], 'rebind code mismatch')
    else:
        check(debug is None, 'no separate mapping twin was used')
    syms=bundle['symbols']
    loc=leaves(debug or module,{int(x[1],16) for x in syms if x[0]=='@plugin'})
    ranges={k:[(int(s[1],16),int(s[2],16)) for s in e.executable_ranges()] for k,e in [('@plugin',ep),('@host',eh)]}
    for mod,vaddr,owner,fn,src in syms:
        pc=int(vaddr,16)
        if mod in ranges:
            check(any(a<=pc<b for a,b in ranges[mod]), 'rebind PC outside executable sections')
        phys=lookup[mod](pc) if mod in lookup else mod
        check((UNKNOWN if phys.startswith('[main ELF') else phys)==owner, 'physical symbol mismatch')
        leaf,mapped=loc[pc] if mod=='@plugin' else ('','')
        check((leaf if mapped else '',mapped)==(fn,src),'source annotation mismatch')

def replay(args):
    data=load(args.evidence);s=summary(data)
    if args.module or args.host or args.debug_module:
        check(args.module and args.host, 'provide --module and --host together')
        rebind(data,args.module,args.host,args.debug_module)
    print(json.dumps(s,indent=2) if args.json else '\n'.join(
        f"{r['scene']:12} {r['period_us']:4} us: {r['callback_pcs']:4} callback PCs, "
        f"{r['calibration_pcs']:2} calibration, {r['first_wake_pcs']:2} first-wake" for r in s))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);sp=p.add_subparsers(dest='action',required=True)
    b=sp.add_parser('build');b.add_argument('--sdk',type=Path,required=True);b.add_argument('--output',type=Path,required=True)
    b.add_argument('--cxx',default='c++');b.set_defaults(fn=build)
    c=sp.add_parser('collect');c.add_argument('--cpu',type=int,required=True);c.add_argument('--output',type=Path,required=True)
    c.add_argument('--module',type=Path,required=True);c.add_argument('--host',type=Path,required=True);c.set_defaults(fn=collect)
    t=sp.add_parser('pack');t.add_argument('--captures',type=Path,required=True);t.add_argument('--output',type=Path,required=True)
    t.add_argument('--module',type=Path,required=True);t.add_argument('--host',type=Path,required=True)
    t.add_argument('--debug-module',type=Path);t.set_defaults(fn=pack)
    r=sp.add_parser('replay');r.add_argument('--evidence',type=Path,default=DEFAULT);r.add_argument('--json',action='store_true')
    r.add_argument('--module',type=Path);r.add_argument('--host',type=Path);r.add_argument('--debug-module',type=Path)
    r.set_defaults(fn=replay);arg=p.parse_args()
    try:
        arg.fn(arg)
    except (ValueError,KeyError,AssertionError,subprocess.SubprocessError,OSError,IndexError) as e:
        p.exit(1,f'FAILED: {e}\n')
