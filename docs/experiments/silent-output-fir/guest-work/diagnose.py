#!/usr/bin/env python3
"""Separate diagnostic after the stock long-release cross-runtime PCM guard failed.
Never converts the failure into a pass; verifies comparisons within and between runtimes separately.
"""
from pathlib import Path
import argparse,array,csv,hashlib,importlib.util,json,struct,sys
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('frozen',HERE/'verify.py');frozen=importlib.util.module_from_spec(spec);spec.loader.exec_module(frozen)
ARM=frozen.ARM;FILES=['pcm.bin','fp.bin','initial.state','postwarm.state','window.state','continuation.state']
SCENES=('fresh-quiet','held-short','settled-release'); sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()

def require(ok,msg):
 if not ok: raise ValueError(msg)

def cross_pcm(a,b,warm):
 raw_a=a.read_bytes();raw_b=b.read_bytes();require(len(raw_a)==len(raw_b),'incomplete PCM')
 wa=array.array('I',raw_a);wb=array.array('I',raw_b);fa=array.array('f',raw_a);fb=array.array('f',raw_b)
 first=None;n=window=sign=normal=0;peak=0.
 for i,(x,y) in enumerate(zip(wa,wb)):
  if x==y:continue
  n+=1;window+=warm*128<=i<(warm+5)*128;mag=max(abs(fa[i]),abs(fb[i]));peak=max(peak,abs(fa[i]-fb[i]));sign+=(x&0x7fffffff)==0 and (y&0x7fffffff)==0;normal+=(x&0x7f800000)!=0 or (y&0x7f800000)!=0
  if first is None:first={'float_index':i,'frame':i//2,'block':i//128,'channel':i%2,'native_bits':f'{x:08x}','guest_bits':f'{y:08x}'}
 return {'float_words':len(wa),'differing_words':n,'first_difference':first,'five_callback_window_differing_words':window,'signed_zero_only_differences':sign,'differing_pairs_with_at_least_one_normal':normal,'max_absolute_difference':peak}

def diagnose(p):
 out={}
 fp_pattern=struct.Struct('<iQII'+3*'IHHi'+'d');fp_labels=['result','silence','event_reads','event_count_reads']+[f'{s}.{n}' for s in ('before','after','probe_after') for n in ('mxcsr','x87_control','x87_status','errno')]+['probe']
 for scene in SCENES:
  warm=9216 if scene=='settled-release' else 32;census=warm+6
  parsed={a:frozen.parse(p/f'cg-{scene}-{a}.1',n) for a,(_,n) in ARM.items()};base=parsed['A']['plugin_edge'];cand=parsed['C']['plugin_edge'];delta=[j-i for i,j in zip(base,cand)]
  for k in ('stub_plus_plugin','plugin_edge','route_inclusive'):
   v={a:tuple(s[k]) for a,s in parsed.items()};require(v['A']==v['A2'],'A/A mismatch');require(v['P']==tuple(x+y for x,y in zip(v['A'],v['S'])) and all(x>0 for x in v['S']),'P != A + S');
  perarm={};metadata={}
  for arm in list(ARM)+['native-s','native-c']:
   translated=arm in ARM;d=(p/f'guest-{scene}-{arm}') if translated else p/f'native-{scene}-{arm[-1]}';m=json.loads((d/'outcome.json').read_text());hashes={f:sha(d/f) for f in FILES};ref=perarm.get('A' if translated else 'native-s'); require(ref is None or ref==hashes,'within-runtime byte mismatch: '+scene+'/'+arm);perarm[arm]=hashes
   require(m['warm']==warm and m['genuine_calls']==census and m['window']==5 and m['scene']==scene and m['under_valgrind']==int(translated),'wrong arm geometry');require((m['arm_window'],m['counted_calls'])==(ARM[arm] if translated else ('first4',4)),'wrong counted geometry');require(m['note_event_reads']==(16 if scene=='settled-release' else 8 if scene=='held-short' else 0),'event mismatch');require((scene=='fresh-quiet' or m['peak']>1e-5) and (m['window_peak']>1e-5 if scene=='held-short' else m['window_peak']<=1e-6),'amplitude guard');metadata[arm]={k:m[k] for k in ('cpu_avx2','cpu_avx512f','xcr0','peak','window_peak','note_event_reads','genuine_calls')}
  da=p/f'guest-{scene}-A';dn=p/f'native-{scene}-s';states_equal=all(perarm['A'][f]==perarm['native-s'][f] for f in FILES if f.endswith('.state'));require(states_equal,'public component state differs across runtimes')
  x=list(fp_pattern.iter_unpack((da/'fp.bin').read_bytes()));y=list(fp_pattern.iter_unpack((dn/'fp.bin').read_bytes()));require(len(x)==len(y)==census,'FP census');field_diffs={field:n for i,field in enumerate(fp_labels) if (n:=sum(t[i]!=u[i] for t,u in zip(x,y)))}
  pcm=cross_pcm(dn/'pcm.bin',da/'pcm.bin',warm)
  out[scene]={'units':['simulated_guest_instruction_references','simulated_guest_data_read_references','simulated_guest_data_write_references'],'counts':parsed,'plugin_edge_delta_candidate_minus_stock':delta,'change_percent':[x*100/y for x,y in zip(delta,base)],'calibration':'A==A2; P==A+S with positive S for Ir,Dr,Dw; delivered wrapper excluded','within_runtime_public_pcm_fp_state_sha256':{'native':perarm['native-s'],'guest':perarm['A']},'cross_runtime_component_snapshots_equal':states_equal,'cross_runtime_fp_fields_differ':field_diffs,'cross_runtime_pcm':pcm,'strict_cross_runtime_pcm_pass':pcm['differing_words']==0,'runtime_metadata':metadata}
 return {'status':'original full cross-runtime-PCM verifier FAILS settled release; these are diagnostic results, not a rescued pass','scenes':out,'clocks_or_cpu_measurement':False}
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__); ap.add_argument('directory',type=Path);args=ap.parse_args(); result=diagnose(args.directory.resolve()); print(json.dumps(result,sort_keys=True,indent=2))
