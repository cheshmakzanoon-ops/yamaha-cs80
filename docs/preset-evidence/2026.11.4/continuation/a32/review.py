from pathlib import Path
import collections,hashlib,json,struct,subprocess
repo=Path('/workspace/scratch/dde4ad85ed5b/yamaha-cs80')
out=Path('/workspace/scratch/dde4ad85ed5b/a32')
paths={'baseline':repo/'docs/preset-evidence/2026.11.4/baseline/inventory.json','final':out.parent/'evidence/final/inventory.json','prior_c4_export':repo/'docs/preset-evidence/2026.11.4/development/final-bank/inventory.json','reported_coverage':out.parent/'evidence/final/parameter-coverage.json'}
a,b,c,r=[json.loads(paths[k].read_bytes()) for k in ['baseline','final','prior_c4_export','reported_coverage']]
f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
identity=lambda p:(p['index'],p['host_index'],p['name'],p['category'],p['tags'])
assert len(a['programs'])==len(b['programs'])==len(c['programs'])==523
assert b['parameter_ids']==a['parameter_ids']+['body1_size','body2_size']
assert len(set(b['parameter_ids']))==200
assert c['parameter_ids']==b['parameter_ids']
volume=b['parameter_ids'].index('volume')
families=collections.Counter();changed_families=collections.Counter();changes=[];inits=[];seen={};duplicates=[];descriptions=[];prior_field_changes=[];active_models=collections.Counter();body_count=0
for old,new,prior in zip(a['programs'],b['programs'],c['programs']):
 assert identity(old)==identity(new)==identity(prior)
 assert len(old['values'])==198 and len(new['values'])==len(prior['values'])==200
 ov=tuple(map(f32,old['values']));nv=tuple(map(f32,new['values']));pv=tuple(map(f32,prior['values']))
 changed=[a['parameter_ids'][i] for i in range(198) if i!=volume and ov[i]!=nv[i]]
 families[new['category']]+=1
 if new['category']=='Init':
  assert not changed and ov==nv[:198] and nv[198:]==(1.,1.)
  inits.append({'name':new['name'],'host_index':new['host_index'],'all_previous_values_exact':True,'appended_sizes':list(nv[198:])})
 else:
  assert changed
  changes.append({'name':new['name'],'host_index':new['host_index'],'existing_non_volume_changes':len(changed)})
  changed_families[new['category']]+=1
 fingerprint=nv[:volume]+nv[volume+1:]
 if fingerprint in seen:duplicates.append([seen[fingerprint],new['name']])
 seen[fingerprint]=new['name']
 fields=[k for k in new if new[k]!=prior[k]]
 if fields:
  prior_field_changes.append({'name':new['name'],'host_index':new['host_index'],'changed_fields':fields})
  if 'description' in fields:descriptions.append({'name':new['name'],'host_index':new['host_index'],'before':prior['description'],'after':new['description']})
 assert nv==pv, new['name']
 enabled=[]
 for rank in range(2):
  model,mix=nv[190+4*rank:192+4*rank]
  if model!=0 and mix>0:enabled.append(int(model));active_models[int(model)]+=1
 body_count+=bool(enabled)
assert not duplicates and len(changes)==521 and len(inits)==2
computed_families=[{'category':k,'programs':families[k],'changed_musical_programs':changed_families[k]} for k in sorted(families)]
assert computed_families==r['summary']['families']
assert body_count==r['summary']['body_enabled_programs']==259
assert len(changes)==r['validation']['musical_programs_changed']==521
assert r['before']['inventory_sha256']==hashlib.sha256(paths['baseline'].read_bytes()).hexdigest()
assert r['after']['inventory_sha256']==hashlib.sha256(paths['final'].read_bytes()).hexdigest()
summary={'review':'a32 independent inventory coverage','source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=repo,text=True).strip(),'source_tree':subprocess.check_output(['git','rev-parse','HEAD^{tree}'],cwd=repo,text=True).strip(),'scope':'Independent JSON and IEEE-754 binary32 comparison, no rerender, no acoustic-match claim. The prior_c4_export is byte-identical to the previously checked c4 export; c4 itself changed DSP only.','input_sha256':{k:hashlib.sha256(p.read_bytes()).hexdigest() for k,p in paths.items()},'ordered_identities_and_tags_preserved':523,'old_parameter_prefix_exact':198,'current_core_parameters':200,'appended_ids':b['parameter_ids'][198:],'musical_existing_non_volume_controls_changed':521,'initialization_programs':inits,'duplicate_non_volume_vectors':duplicates,'configured_body_enabled_programs':body_count,'circuit_only_programs_including_init':523-body_count,'families':computed_families,'previous_c4_all_523_parameter_vectors_unchanged':True,'previous_c4_field_changes':prior_field_changes,'previous_c4_description_changes':descriptions,'reported_coverage_counts_match_independent_calculation':True,'changed_existing_non_volume_count_range':[min(p['existing_non_volume_changes'] for p in changes),max(p['existing_non_volume_changes'] for p in changes)]}
(out/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print(json.dumps(summary,indent=2))
