#!/usr/bin/env python3
"""Offline receipt replay, no DSP/CPU measurement. Fails closed if a recorded row changes."""
import hashlib,json,pathlib,re,runpy,sys
P=pathlib.Path(__file__).resolve().parent
def require(x,e):
 if not x:raise ValueError(e)
def main():
 m=json.loads((P/'manifest.json').read_text()); payload=set(str(f.relative_to(P)) for f in P.rglob('*') if f.is_file() and f!=P/'manifest.json' and '__pycache__' not in f.parts)
 require(payload==set(m), 'manifest coverage')
 for name,digest in m.items():require(hashlib.sha256((P/name).read_bytes()).hexdigest()==digest, name)
 E=P/'evidence'
 j=json.loads((E/'engine.json').read_text());rr=j['rows'];expect=[(s,48000,True) for s in range(7)]+[(0,44100,True),(1,96000,True),(0,48000,False)]
 arms=['stock','normal','witness','off'];tot={a:{k:0 for k in ['fast','stock','falseWitness','certificates']} for a in arms}
 require(len(rr)==40,'forty fixed rows')
 for n,(s,r,q) in enumerate(expect):
  sub=rr[n*4:n*4+4]
  require([x['arm'] for x in sub]==arms,'arm order')
  require(len({(x['sha256'],x['traceBytes'],x['nonzero'],x['quietBlocks'],x['playingBlocks']) for x in sub})==1,'trace/signal parity')
  for x in sub:
   require((x['scene'],x['rate'],x['filterKernel'])==(s,r,q),'scene provenance')
   require(x['callbacks']==256 and x['frames']==16384 and x['traceBytes']==3773296,'trace extent')
   require(x['falseWitness']==0 and x['quietBlocks']+x['playingBlocks']==256 and (s==5 or x['nonzero']>0),'trace validity')
   if x['arm']=='witness':
    if r!=48000 or s in (2,4,5):require(x['fast']==0,'negative/actual null occupancy')
    else:require(x['fast']>0,'actual positive')
   if x['arm']=='off':require(x['fast']==0,'forced off')
   for k in tot[x['arm']]:tot[x['arm']][k]+=x[k]
 setup=json.loads((E/'setup-counts.json').read_text())
 witness=[r for r in rr if r['arm']=='witness']
 require(len(setup)==len(witness)==10,'ten setup rows')
 for i,(a,r) in enumerate(zip(setup,witness)):
  require(a['matrix_row']==i and a['trace_sha256']==r['sha256'],'setup shadow hash')
  for k in ('scene','rate','filterKernel','frames','callbacks','fast','stock','certificates','falseWitness','nonzero','quietBlocks','playingBlocks'):require(a[k]==r[k],'setup preserved '+k)
  for k in ('attempts','numeric','resetRejected','foldRejected','knotTests','knotRejected','turnTests','turnRejected'):require(type(a[k])==int and a[k]>=0,'setup integer')
  require(a['attempts']>=a['numeric'] and a['numeric']==sum(a[k] for k in ('resetRejected','foldRejected','knotRejected','turnRejected','certificates')),'setup accounting')
  require(a['knotTests']<=8*a['numeric'] and a['fast']<=4*a['certificates'],'bound accounting')
 require([sum(x[k] for x in setup) for k in ('attempts','certificates','knotTests')]==[1789454,284276,5867705],'setup totals')
 require(tot==j['totals'] and tot['witness']=={'fast':1096251,'stock':2876325,'certificates':284276,'falseWitness':0},'stock witness totals')
 require(sum(x['traceBytes'] for x in rr if x['arm']=='normal')==37732960,'trace size/arm')
 for file,line in {
  'v2-direct.log':'pairs=114688 checks=573473 failures=0 skipped=40832 witnessDisagreements=0',
  'v2-witness.log':'pairs=114688 checks=573474 failures=0 skipped=40832 witnessDisagreements=0',
  'v2-bad.log':'pairs=114688 checks=573474 failures=306756 skipped=60532 witnessDisagreements=1932',
  'edges.log':'restored_scenarios=8192 pairs=98304 checks=491520 failures=0 admitted=21576',
  'edges-witness.log':'restored_scenarios=8192 pairs=98304 checks=491521 failures=0 admitted=21576',
  'native-fallback.log':'pairs=114688 checks=573485 failures=0 skipped=0 witnessDisagreements=0',
  'explicit-off.log':'pairs=114688 checks=573485 failures=0 skipped=0 witnessDisagreements=0',
  'sizes.log':'sizeof 5984 5984 align 8 8'}.items():require((E/file).read_text().splitlines()[-1]==line,file)
 for k,n in [('elapsed',4),('output',6)]:
  for a in ['stock','normal']:
   log=(E/f'{k}-{a}-trap.log').read_text()
   require(f'traps={n} whole_object_interrupt_mismatches=0' in log,'actual traps')
   require(k!='elapsed' or 'direct_public_continuation_mismatches=0' in log,'trap continuation')
 runpy.run_path(str(P/'entry-prefilter/verify.py'),run_name='__main__')
 print('Verified exact ten-scene stock receipts, positive and negative mechanism controls, fallback and trap evidence. No DSP or clock executed.')
if __name__=='__main__':main()
