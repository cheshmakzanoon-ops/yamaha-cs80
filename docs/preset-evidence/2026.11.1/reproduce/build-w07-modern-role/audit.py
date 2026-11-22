from pathlib import Path
import hashlib, json, math, subprocess
ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
inventory_path=ROOT/'build-audit/inventory-final.json'
inventory=json.loads(inventory_path.read_text())
wave_names=['sine','triangle','saw up','saw down','square','sample and hold','smooth random']
dest_names={1:'pitch',2:'filter cutoff',3:'level CV',4:'rank pan',5:'pulse width'}
dest_scale={1:2.0,2:1.6,3:0.25,4:0.7,5:0.18}
fx_types={0:'off',1:'classic chorus',2:'classic six-stage phaser',3:'classic tape delay',4:'classic plate',5:'classic drive'}
fx_models={0:'classic type',1:'BBD ensemble',2:'spring',3:'plate',4:'tape echo',5:'drive'}
arp_modes={0:'up',1:'down',2:'up/down without repeated endpoints',3:'random',4:'input order'}
programs=[]; tag_mismatches=[]; checks=[]
for record in inventory['programs']:
 v=dict(zip(inventory['parameter_ids'],record['values']))
 features=[]
 for slot in range(1,4):
  if v[f'lfo{slot}_depth']>0 and v[f'lfo{slot}_dest']>0:
   dest=int(v[f'lfo{slot}_dest']);wave=int(v[f'lfo{slot}_wave'])
   ranks=[]
   for rank,weight in [(1,1-v['mix']),(2,v['mix'])]:
    prefix=f'ch{rank}_'
    source=v[prefix+'saw']>0 or v[prefix+'pulse']>0 or v[prefix+'noise']>0
    if weight>0 and (dest!=2 or (source and v[prefix+'vcf_level']>0)) and (dest!=5 or v[prefix+'pulse']>0):ranks.append(rank)
   checks.append({'program':record['name'],'check':f'LFO{slot} has audible source/destination route','pass':bool(ranks)})
   features.append({'kind':'LFO','slot':slot,'waveform':wave_names[wave],'waveform_index':wave,'rate_hz':v[f'lfo{slot}_rate'],'depth':v[f'lfo{slot}_depth'],'destination':dest_names[dest],'destination_index':dest,'maximum_bipolar_offset':v[f'lfo{slot}_depth']*dest_scale[dest],'receiving_ranks':ranks,'ordinary_circuit':'ModulationLfo::tick -> Engine LFO destination accumulator -> rank oscillator/filter/VCA/pan computation','pure':'effective depth=0, destination=0, waveform=0; stored values retained'})
  if v[f'fx{slot}_mix']>.001 and (v[f'fx{slot}_type']>0 or v[f'fx{slot}_model']>0):
   model=int(v[f'fx{slot}_model']);typ=int(v[f'fx{slot}_type'])
   feature={'kind':'FX','slot':slot,'classic_type_index':typ,'classic_type':fx_types[typ],'model_index':model,'effective_effect':fx_models[model] if model else fx_types[typ],'mix':v[f'fx{slot}_mix'],'parameter_a':v[f'fx{slot}_param_a'],'parameter_b':v[f'fx{slot}_param_b'],'oversampling_stages':v[f'fx{slot}_oversampling'],'ordinary_circuit':'Engine serial FX chain -> FxSlot::liveTypeFor -> selected classic or modern model','pure':'effective type/model/mix=0; stored values retained'}
   if model==4:feature['nominal_echo_time_ms']=1000*0.06*20**v[f'fx{slot}_param_a']
   if model==3:feature['plate_target_rt60_seconds_before_brightness_damping']=0.8*10**v[f'fx{slot}_param_a']
   features.append(feature)
 if v['arp_on']>=.5:
  mode=int(v['arp_mode'])
  features.append({'kind':'arpeggiator','mode_index':mode,'mode':arp_modes[mode],'rate_notes_per_second':v['arp_rate'],'octaves':v['arp_octaves'],'gate_fraction':.45 if mode==3 else .58,'ordinary_circuit':'held keys -> rebuilt octave sequence -> updateArpeggiator timed gate -> startVoice','pure':'effective arpOn=false, so input notes play directly; stored arp controls retained'})
 tagged='modern' in record['tags']
 if bool(features)!=tagged:tag_mismatches.append(record['name'])
 if tagged:
  checks.append({'program':record['name'],'check':'sourcePerformance=1, parity=0, macros neutral, active modern feature','pass':v['source_performance']==1 and v['parity_mode']==0 and all(v[f'macro{i}']==.5 for i in range(1,5)) and bool(features)})
  programs.append({'index':record['index'],'host_index':record['host_index'],'name':record['name'],'category':record['category'],'description':record['description'],'features':features})
expected_delay_ms={'Delay Brass':375,'Arp Bells':375,'Stereo Delay Pluck':250,'Ambient Bells':560,'Space Lead':400}
for record in programs:
 if record['name'] in expected_delay_ms:
  actual=next(f['nominal_echo_time_ms'] for f in record['features'] if f.get('model_index')==4)
  checks.append({'program':record['name'],'check':'nominal echo matches rounded description within 0.5 ms','actual_ms':actual,'described_ms':expected_delay_ms[record['name']],'pass':abs(actual-expected_delay_ms[record['name']])<.5})
checks.append({'check':'all active LFO/FX/arp programs tagged modern and every modern tag has active feature','pass':not tag_mismatches})
checks.append({'check':'complete expected modern-tagged roster','pass':len(programs)==24})
assert all(c['pass'] for c in checks)
paths=['src/core/Engine.h','src/core/Engine.cpp','src/core/ModulationLfo.h','src/core/FxSlot.h','src/core/FxModels.h','src/core/CuratedLibrary.inc','src/core/curated/Modern.inc','src/core/curated/Fx.inc','src/core/curated/Pads.inc','src/plugin/PluginEditor.cpp','src/plugin/PluginProcessor.cpp','docs/PARITY_ISOLATION.md']
report={'assignment':37,'status':'PASS','method':'Complete compiled inventory-to-source routing audit; no new audio render or native host execution. Static derived-value checks are not sound-quality or physical-equivalence measurements.','source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'library_revision':inventory['library_revision'],'inventory_sha256':hashlib.sha256(inventory_path.read_bytes()).hexdigest(),'modern_tagged_count':len(programs),'active_modern_feature_count':len(programs),'unexplained_tag_mismatches':tag_mismatches,'checks':checks,'programs':programs,'mode_contract':{'ordinary_circuit':'sourcePerformance=1 and parity=0; source models and software LFO, FX and arp overlays are active.','pure':'parity takes precedence over sourcePerformance. LFO depth/destination, FX type/model/mix and arp enable are effectively masked. Raw parameters remain stored. Factory and user-preset recall preserve the session Pure flag.','studio_visibility':'PluginEditor::showAdvanced changes overlay visibility, focus and repaint only. Its open/closed state is not an audio-policy switch.','clocking':'The authored LFO and arpeggiator rates are free-running Hz/notes-per-second; no host-tempo synchronization is advertised or used by these routes.','model_zero':'Phaser Strings and Phased Polysynth use FX model0 with classic type2. Zero modern model means the classic phaser branch, not bypass.'},'source_evidence':[{'path':path,'sha256':hashlib.sha256((ROOT/path).read_bytes()).hexdigest()} for path in paths],'limits':['An active control-flow route does not establish perceptual prominence, acoustic realism or arbitrary-performance quality.','Pure intentionally changes multiple controls, so it is not an isolated modern-effect A/B comparison.','The delay numbers are nominal target laws; tape wow/flutter and feedback decay remain active.','PARITY_ISOLATION.md has historical host-count and decimator-only latency wording. Current Engine and plugin code are authoritative for this audit; no documentation edits were made.']}
(OUT/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'modern_programs':len(programs),'checks':len(checks),'failed':sum(not c['pass'] for c in checks),'summary_sha256':hashlib.sha256((OUT/'summary.json').read_bytes()).hexdigest()}))
