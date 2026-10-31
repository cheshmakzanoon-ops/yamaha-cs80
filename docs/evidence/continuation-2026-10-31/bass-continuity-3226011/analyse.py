from pathlib import Path
import csv,json,hashlib,struct,difflib,math,shutil
root=Path('/workspace/scratch/2f805bffd21a/cs80-ui-publication')
raw=Path('/tmp/cs80-bass-continuity-3226011')
rel=Path('docs/evidence/continuation-2026-10-31/bass-continuity-3226011')
out=root/rel;out.mkdir(parents=True,exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def load(p):return list(csv.DictReader(p.open()))
def writecsv(p,fields,rows):
 with p.open('w',newline='') as f:
  w=csv.DictWriter(f,fields);w.writeheader();w.writerows(rows)
manifest=json.loads((raw/'manifest.json').read_text())
summary={'schema_version':1,'kind':'Diagnostic software evidence only; no hardware measurement; original phrase gate remains open','source_base':manifest['base'],'status':'No zero-time restart mechanism observed in these original physical-PANEL Bass fixtures; existing score fails unchanged','methods':{'phrase':'Original engine_audit events,64-frame quantization, original physical control coordinate and exact original float-step/chord-reference metric. Source performance policy,2x internal sampling.','state':'Capture after legitimate pending idle-time catchup, then immediately after assignment without elapsed render time. Compare all72 exposed continuation fields per rank, including oscillator residual tails, filter histories, both envelope values and both VCA nodes. Gate stages/control selectors are allowed to change and are excluded from the charge comparison.','neutrality':'Fresh uninstrumented core objects from the integrated3226011 compile are compared byte-for-byte against the instrumented Engine for every output sample. One-sample observation is also compared with the original64-frame rendering.','decomposition':'For diagnostics, write VCA output as y=.085*(c*(u+.0025)-q), c=LI*EG*2^EI, q=coupling. Delta y = .085*(c_previous*Delta u + (u_current+.0025)*Delta c - Delta q). Evaluate from recorded float nodes in double; report rounding residual and do not fit any coefficients. The direct pedestal term is .085*.0025*Delta c.','excerpt_selection':'Keep first16 active host frames; +/-8 frames around observed oscillator resets and EG stage transitions; +/-24 frames around each rank maximum slope before the63-host-sample FIR latency. Full raw traces and output binaries retain hashes/commands in the manifest.'},'instrumentation_equivalence':manifest['audio_instrumentation_equivalence'],'rows':[],'captured_fields_compared':0,'assignment_bit_mismatches':0,'raw_files':[]}
for name in ['probe.cpp','run.py','manifest.json']:
 shutil.copyfile(raw/name,out/name)
patch=''.join(difflib.unified_diff((root/'src/core/Engine.cpp').read_text().splitlines(True),(raw/'Engine-traced.cpp').read_text().splitlines(True),fromfile='a/src/core/Engine.cpp',tofile='b/src/core/Engine.cpp'))
(out/'trace-only.patch').write_text(patch)
shutil.copyfile(raw/'traced/metrics.csv',out/'metrics.csv')
for name in ['compile-probe.log','compile-traced-engine.log','link-control.log','link-traced.log','run-control.log','run-traced.log']:
 shutil.copyfile(raw/name,out/name)
old=root/'docs/evidence/continuation-2026-10-31/cold-idle-soa'
old_before={int(r['rate']):r for r in load(old/'click-causality-before.csv') if r['name']=='Bass Stack' and r['coordinate']=='original_physical_panel'}
old_after={int(r['rate']):r for r in load(old/'click-causality-after.csv') if r['name']=='Bass Stack' and r['coordinate']=='original_physical_panel'}
for metric in load(raw/'traced/metrics.csv'):
 rate=int(metric['rate']);event=int(metric['event_frame']);peak=int(metric['peak_frame'])
 row={'rate':rate,'metrics':{k:float(v) for k,v in metric.items()},'assignment':[],'internal':[]}
 states=load(raw/'traced'/f'assignment-{rate}.csv');by={}
 for r in states:by.setdefault((r['voice'],r['rank'],r['stage']),{})[r['field']]=float(r['value'])
 for rank in ['0','1']:
  a=by[('6',rank,'after_pending_catchup')];b=by[('6',rank,'after_assignment')];c=by[('6',rank,'before_pending_catchup')]
  mismatch=[k for k,v in a.items() if struct.pack('d',v)!=struct.pack('d',b[k])]
  summary['captured_fields_compared']+=len(a);summary['assignment_bit_mismatches']+=len(mismatch)
  row['assignment'].append({'rank':int(rank),'compared_fields':len(a),'bit_mismatches':mismatch,'maximum_numeric_difference':max(abs(v-b[k]) for k,v in a.items()),'pending_seconds_caught_up':a['osc_elapsed']-c['osc_elapsed'],'filter_charge_before_after':a['filter_envelope'],'amplitude_charge_before_after':a['amplitude_envelope'],'vca_li_before_after':a['vca_level'],'vca_coupling_before_after':a['vca_coupling']})
 shutil.copyfile(raw/'traced'/f'assignment-{rate}.csv',out/f'assignment-{rate}.csv')
 shutil.copyfile(raw/'traced'/f'reference-windows-{rate}.csv',out/f'reference-windows-{rate}.csv')
 taps=load(raw/'traced'/f'rank-taps-{rate}.csv');selected=set(range(event,event+17))
 derived=[]
 for rank in [0,1]:
  r=[{k:float(v) for k,v in t.items()} for t in taps if int(t['rank'])==rank]
  indices=[i for i in range(1,len(r)) if event<=r[i]['sample']<event+.008*rate-63]
  i=max(indices,key=lambda i:abs(r[i]['rank_output']-r[i-1]['rank_output']))
  v,u=r[i],r[i-1]
  c=v['li_node']*v['amp']*2**v['ei_octaves'];cp=u['li_node']*u['amp']*2**u['ei_octaves'];scale=.5*.17
  input_term=scale*cp*(v['vca_input']-u['vca_input']);control_term=scale*(v['vca_input']+.0025)*(c-cp);coupling_term=-scale*(v['coupling']-u['coupling']);pedestal_term=scale*.0025*(c-cp);actual=v['rank_output']-u['rank_output']
  point={'rank':rank,'sample':int(v['sample']),'internal_index':int(v['internal_index']),'time_since_event_ms':1000*(v['sample']-event+v['internal_index']/2)/rate,'output_peak_minus_internal_host_frames':peak-int(v['sample']),'rank_output_step':actual,'vca_input_step_contribution':input_term,'control_step_contribution':control_term,'coupling_step_contribution':coupling_term,'direct_pedestal_step_contribution':pedestal_term,'decomposition_rounding_residual':actual-input_term-control_term-coupling_term,'amp':v['amp'],'amp_stage':int(v['amp_stage']),'filter_eg':v['filter_eg'],'filter_stage':int(v['filter_stage']),'lp_cutoff_hz':v['lp_cutoff'],'oscillator_phase':v['phase'],'oscillator_hz':v['oscillator_hz'],'reset_count':int(v['reset_count'])}
  row['internal'].append(point);derived.append(point)
  selected.update(range(int(v['sample'])-24,int(v['sample'])+25))
  for j,v in enumerate(r):
   if j==0 or not event<=v['sample']<event+.008*rate:continue
   if any(v[k]!=r[j-1][k] for k in ['reset_count','amp_stage','filter_stage']):selected.update(range(int(v['sample'])-8,int(v['sample'])+9))
 writecsv(out/f'critical-internal-{rate}.csv',list(taps[0]),[r for r in taps if int(r['sample']) in selected])
 audio=load(raw/'traced'/f'output-{rate}.csv');a=next(r for r in audio if int(r['sample'])==event)
 row['first_rendered_event_sample_minus_omitted']={ch:float(a[ch])-float(a['omitted_'+ch]) for ch in ['left','right']}
 writecsv(out/f'output-window-{rate}.csv',list(audio[0]),[r for r in audio if event-64<=int(r['sample'])<=event+round(.010*rate)])
 b=old_before[rate];a=old_after[rate];pb=float(b['baseline_peak_jump']);pa=float(a['baseline_peak_jump']);rb=pb/float(b['baseline_score']);ra=pa/float(a['baseline_score'])
 row['archived_cold_clock_before_after']={'old_peak_jump':pb,'new_peak_jump':pa,'peak_percent_change':100*(pa/pb-1),'old_chord_reference':rb,'new_chord_reference':ra,'reference_percent_change':100*(ra/rb-1),'old_score':float(b['baseline_score']),'new_score':float(a['baseline_score'])}
 summary['rows'].append(row)
for p in sorted((raw/'traced').iterdir()):
 summary['raw_files'].append({'local_path':str(p),'sha256':sha(p),'bytes':p.stat().st_size})
summary['limitations']=['The existing <=1 phrase criterion remains failed; no acceptance value or musical source law was changed.','This diagnoses these two physical-PANEL source-policy Bass trajectories, not all presets or hardware behavior.','The original cold LI-node convention still initializes the static level on the first active sample. It is not a zero-time assignment reset in this trace; its fidelity remains the previously declared model simplification.','At the event sample, the newly rendered finite-time attack has a tiny FIR precursor relative to the omitted event; exact continuity concerns the zero-time state boundary, not identical audio after a new note.','The VCA decomposition uses recorded rounded nodes; its small residual is retained, not fitted away.','This diagnostic does not replace the separately registered capacitor-continuity, cold-clock, wake, noise or phrase gates.']
shutil.copyfile(Path(__file__),out/'analyse.py')
summary['analysis_sha256']=sha(out/'analyse.py')
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps({'fields':summary['captured_fields_compared'],'assignment_bit_mismatches':summary['assignment_bit_mismatches'],'files':len(list(out.iterdir())),'bytes':sum(p.stat().st_size for p in out.iterdir())},indent=2))
