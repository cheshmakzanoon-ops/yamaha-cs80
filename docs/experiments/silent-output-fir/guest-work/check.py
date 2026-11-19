#!/usr/bin/env python3
"""Replay all 15 raw selected dumps and compare every recorded public file fingerprint.
No synth, plugin, timers, or Callgrind are run during replay. Historical PCM values
are not in this compact package; reported word-level cross-runtime statistics are
retained original observations, not recomputed from hashes.
"""
import gzip,hashlib,json,sys,tempfile
from pathlib import Path
sys.dont_write_bytecode=True
from verify import parse
P=Path(__file__).resolve().parent
SCENES=('fresh-quiet','held-short','settled-release');ARMS={'A':('first4',4),'A2':('first4',4),'P':('all5',5),'S':('fifth',1),'C':('first4',4)}
def require(ok,m):
 if not ok:raise ValueError(m)
def main():
 for line in (P/'FROZEN.sha256').read_text().splitlines():
  sha,name=line.split(maxsplit=1);require(hashlib.sha256((P/name).read_bytes()).hexdigest()==sha,'frozen input changed: '+name)
 rec=json.loads(gzip.decompress((P/'recorded.json.gz').read_bytes()));dumps=json.loads(gzip.decompress((P/'selected-dumps.json.gz').read_bytes()));diag=rec['diagnostic']['scenes'];meta=rec['per_arm'];require(len(dumps)==15 and len(meta)==21,'incomplete run roster')
 expected=('0ed90e446f1faa6153b0e2f76f074b7da62201be49f76fc9183375e9ff20edf2','74f2507db4a7042a23a96b9a8c2249cc2f76d1f7f417ebb4838498d7f0578895');require(tuple(rec['input_sha256'][i] for i in ('stock','candidate'))==expected,'incorrect module pair')
 with tempfile.TemporaryDirectory() as tmp:
  out=Path(tmp)
  for scene in SCENES:
   cal={};warm=9216 if scene=='settled-release' else 32
   for a,(mode,n) in ARMS.items():
    name=f'cg-{scene}-{a}.1';f=out/name;f.write_text(dumps[name]); cal[a]=parse(f,n)
    require(json.loads(json.dumps(cal[a]))==diag[scene]['counts'][a], 'changed instruction/read/write count')
   for field in ('stub_plus_plugin','plugin_edge','route_inclusive'):
    v={a:tuple(x[field]) for a,x in cal.items()};require(v['A']==v['A2'] and all(x>0 for x in v['S']) and v['P']==tuple(x+y for x,y in zip(v['A'],v['S'])),'A/A or additive positive control failed')
   for runtime,arms in [('guest',ARMS),('native',{'s':('first4',4),'c':('first4',4)})]:
    reference=None
    for arm,(mode,n) in arms.items():
     ob=meta[f'{runtime}-{scene}-{arm}'];o=ob['outcome'];files=ob['files'];require(o['arm_window']==mode and o['counted_calls']==n and o['warm']==warm and o['genuine_calls']==warm+6,'incorrect selection or callback count');require(bool(o['under_valgrind'])==(runtime=='guest'),'runtime identity')
     require(o['module']==('/module/candidate.so' if arm in ('C','c') else '/module/stock.so'),'module binding');require(o['note_event_reads']==(16 if scene=='settled-release' else 8 if scene=='held-short' else 0),'incorrect note schedule');require((scene=='fresh-quiet' or o['peak']>1e-5) and (o['window_peak']>1e-5 if scene=='held-short' else o['window_peak']<=1e-6),'wrong quiet/played state')
     require(set(files)=={'pcm.bin','fp.bin','initial.state','postwarm.state','window.state','continuation.state'} and files['pcm.bin']['bytes']==(warm+6)*64*2*4 and files['fp.bin']['bytes']==(warm+6)*64,'missing public file')
     if reference is not None:require(reference==files,'within-runtime fingerprints differ');
     reference=files
   native=meta[f'native-{scene}-s']['files'];guest=meta[f'guest-{scene}-A']['files'];require(all(native[n]==guest[n] for n in native if n.endswith('.state')),'public component states differ across runtimes');pcmdiff=native['pcm.bin']!=guest['pcm.bin'];require(pcmdiff==(scene=='settled-release')==(diag[scene]['cross_runtime_pcm']['differing_words']>0),'cross-runtime failure lost or expanded')
   actual=[c-s for s,c in zip(cal['A']['plugin_edge'],cal['C']['plugin_edge'])];require(actual==diag[scene]['plugin_edge_delta_candidate_minus_stock'],'mismatched delta')
   require(all(abs(100*x/y-z)<1e-10 for x,y,z in zip(actual,cal['A']['plugin_edge'],diag[scene]['change_percent'])),'mismatched percent')
   print(scene,actual,'Ir/Dr/Dw; within-runtime public hashes match; cross-runtime PCM '+('DIFFERS' if pcmdiff else 'matches'))
 require(rec['original_strict_result']['exit']==1,'original long-tail failure lost')
if __name__=='__main__':main()
