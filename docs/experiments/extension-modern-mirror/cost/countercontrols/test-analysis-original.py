from pathlib import Path
import copy,importlib.util,json,math
root=Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('study',root/'study.py');s=importlib.util.module_from_spec(sp);sp.loader.exec_module(s)
plan=s.read(root/'plan.json');records=[]
def build():
 out=[]
 for slot in plan['slots']:
  x=s.expected(slot['scene'],slot['extra']);b=plan['batches'][slot['batch']];cpu=20_000_000
  if slot['extra']:cpu=22_500_000
  if b['kind']=='compare' and slot['label']=='B':cpu=15_000_000
  origin=(slot['id']+1)*1_000_000_000
  x['empty_clocks']=[{'cpu_begin_ns':origin+i*1000000,'cpu_end_ns':origin+i*1000000+100,'wall_begin_ns':origin+i*1000000,'wall_end_ns':origin+i*1000000+200} for i in range(3)]
  x['timing']={'cpu_begin_ns':origin+10000000,'cpu_end_ns':origin+10000000+cpu,'wall_begin_ns':origin+10000000,'wall_end_ns':origin+10000000+cpu+1000};out.append(x)
 return out
def check(name,fn):fn();records.append({'test':name,'pass':True})
x=build();r=s.analyze_data(plan,x);check('valid synthetic80 with known-work directions',lambda: s.need(r['control_pass'],'controls'))
check('five independent comparison batches per scene',lambda:s.need(all(v['independent_batches']==5 for v in r['summary'].values()),'batch count'))
check('signed arithmetic scale',lambda:s.need(abs(r['summary']['default']['median_hot_assumption_ms_per_audio_second']-(5_000_000/s.N)*750/1e6)<1e-12,'scale'))
for name,mut in [('missing slot',lambda z:z.pop()),('wrong main calls',lambda z:z[0].update(main_calls=s.N-1)),('fake extra work count',lambda z:next(v for v in z if v['extra']).update(shadow_calls=0)),('changed PCM',lambda z:z[0].update(pcm_hash=0)),('changed FP',lambda z:z[0]['after_fp'].update(mxcsr=0)),('backward clock',lambda z:z[0]['timing'].update(cpu_end_ns=1)),('zero clock',lambda z:z[0]['timing'].update(cpu_end_ns=z[0]['timing']['cpu_begin_ns'])),('wrong CPU',lambda z:z[0].update(cpu=1)),('missing empty probe',lambda z:z[0]['empty_clocks'].pop())]:
 z=copy.deepcopy(x);mut(z)
 try:s.analyze_data(plan,z);raise AssertionError('accepted '+name)
 except ValueError:records.append({'test':name,'pass':True})
for name,mut in [('sham false advantage',lambda z:[v['timing'].update(cpu_end_ns=v['timing']['cpu_begin_ns']+24_000_000) for i,v in enumerate(z) if plan['slots'][i]['batch']==0 and plan['slots'][i]['label']=='B']),('wrong positive sign',lambda z:[v['timing'].update(cpu_end_ns=v['timing']['cpu_begin_ns']+18_000_000) for v in z if v['extra']]),('clock overhead too large',lambda z:z[0]['empty_clocks'][0].update(cpu_end_ns=z[0]['empty_clocks'][0]['cpu_begin_ns']+150000))]:
 z=copy.deepcopy(x);mut(z);result=s.analyze_data(plan,z);assert not result['control_pass'],name;records.append({'test':name,'pass':True})
# A missing target call must invalidate the static loop proof pattern, not masquerade as an exact method.
for p in (root/'build').glob('*-loop-false.txt'):
 text=p.read_text();assert text.count('ModernLayerState::mirrorToEngine')==1;assert 'ModernLayerState::mirrorToEngine' not in text.replace('ModernLayerState::mirrorToEngine','omitted');records.append({'test':p.name+' actual direct call / omitted-call negative','pass':True})
(root/'untimed/analyzer-tests.json').write_text(json.dumps({'status':'PASS','tests':records,'measurement_clock_reads':0,'dsp_executions':0,'synthetic_only':True},indent=2)+'\n')
print('PASS',len(records),'software-only controls; no timings')
