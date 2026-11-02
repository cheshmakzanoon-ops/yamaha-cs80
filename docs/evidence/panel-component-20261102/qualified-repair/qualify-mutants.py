import collections,concurrent.futures,datetime,hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
base=Path('/workspace/scratch/aa102d573ba3');source=base/'cs80-panel-component';build=base/'cs80-panel-s2/build-native-red';root=base/'cs80-host-tool-evidence/panel-component'
mode=sys.argv[1];dest=root/'qualification'/mode;dest.mkdir(parents=True,exist_ok=False)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(source),*args],text=True).strip()
original=json.loads((root/'green/receipt.json').read_text());assert original['head']=='439036a33efcaf4cf7a7327530d9152f487d08e7'
assert git('rev-parse','HEAD')==original['head'];assert git('status','--porcelain')==''
for p,v in original['inputs'].items():assert sha(source/p)==v,p
for row in original['reused_objects']:assert sha(row['path'])==row['sha256'],row['path']
for row in original['compiled']:assert sha(row['object'])==row['sha256'],row['object']
assert sha(root/'green/panel-components')==original['binary_sha256']
receipt={'source_head':original['head'],'source_tree':original['source_tree'],'mode':mode,'started_at':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'original_positive_binary_sha256':original['binary_sha256'],'scope':'isolated actual source mutant, unpeered component assertions; no native/render/DAW'}
print('LOCKED_CASE',mode,receipt['started_at'],flush=True)
if mode.startswith('positive-'):
 binary=root/'green/panel-components';expected=[]
else:
 tree=dest/'source'
 shutil.copytree(source/'src',tree/'src');shutil.copytree(source/'tests',tree/'tests',ignore=shutil.ignore_patterns('golden'));shutil.copytree(source/'deps',tree/'deps')
 for p,v in original['inputs'].items():assert sha(tree/p)==v,p
 substitutions=[]
 def replace(rel,old,new):
  p=tree/rel;s=p.read_text();assert s.count(old)==1,(rel,old,s.count(old));before=sha(p);p.write_text(s.replace(old,new));substitutions.append({'file':rel,'old':old,'new':new,'before_sha256':before,'after_sha256':sha(p)})
 target='src/plugin/UiComponents.cpp'
 if mode=='geometry':
  replace(target,'return juce::Slider::proportionOfLengthToValue(isDownwardHardwareSlider() ? 1.0 - proportion : proportion);','return juce::Slider::proportionOfLengthToValue(proportion);')
  replace(target,'return isDownwardHardwareSlider() ? 1.0 - proportion : proportion;','return proportion;')
 elif mode=='keys':
  target='src/plugin/UiComponents.h';replace(target,'if (isDownwardHardwareSlider())','if (false)')
 elif mode=='rebind':
  replace(target,'    slider.getProperties().set("hardwareIncreasesDown", hardwareSlider && !horizontalFader && hardwareLeverIncreasesDown(index));\n','')
 elif mode=='layout':
  target='src/plugin/CentreRow.cpp';p=tree/target;s=p.read_text();before=sha(p)
  block='''    lever(cs80::P_kbdBrillianceLow, "LOW", greenCap());
    lever(cs80::P_kbdBrillianceHigh, "HIGH", greenCap());
    group("KBD BRILL", "BRILL");
    lever(cs80::P_kbdLevelLow, "LOW", greyCap());
    lever(cs80::P_kbdLevelHigh, "HIGH", greyCap());
    group("KBD LEVEL", "LEVEL");
'''
  needle='    // Initial pitch bend, then the after-touch amounts;'
  assert s.count(block)==1 and s.count(needle)==1;s=s.replace(block,'');s=s.replace(needle,block+needle);p.write_text(s)
  substitutions.append({'file':target,'operation':'move exact keyboard group before TOUCH, restoring old order','block':block,'before_sha256':before,'after_sha256':sha(p)})
 elif mode=='legends':
  target='src/plugin/PluginEditor.cpp'
  replace(target,'''            const juce::String bottom = original ? "EXT" : "RAND";
            if (props["faderLegendTop"].toString() != "SINE" || props["faderLegendBottom"].toString() != bottom)
            {
                props.set("faderLegendTop", "SINE");
                props.set("faderLegendBottom", bottom);''','''            const juce::String top = original ? "EXT" : "RAND";
            if (props["faderLegendTop"].toString() != top || props["faderLegendBottom"].toString() != "SINE")
            {
                props.set("faderLegendTop", top);
                props.set("faderLegendBottom", "SINE");''')
 else:raise ValueError(mode)
 changed=[p for p,v in original['inputs'].items() if sha(tree/p)!=v];assert changed==[target],changed
 receipt['substitutions']=substitutions;receipt['changed_input_files']=changed
 selected=original['compiled'] if mode=='keys' else [r for r in original['compiled'] if r['original_source']==target]
 assert len(selected)==(26 if mode=='keys' else 1)
 def compile_one(row):
  args=[a.replace(str(source),str(tree)) for a in row['argv']];obj=dest/Path(row['object']).name;args[args.index('-o')+1]=str(obj)
  print('compile',row['original_source'],flush=True)
  with (dest/(obj.name+'.log')).open('w') as f:subprocess.run(args,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=240)
  return {'original_source':row['original_source'],'original_object':row['object'],'object':str(obj),'sha256':sha(obj),'argv':args}
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:compiled=list(pool.map(compile_one,selected))
 receipt['compiled']=compiled;mapping={r['original_object']:r['object'] for r in compiled}
 link=[mapping.get(a,a) for a in original['link_argv']];binary=dest/'panel-components';link[link.index('-o')+1]=str(binary)
 link=[('-Wl,--dependency-file='+str(dest/'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in link]
 with (dest/'link.log').open('w') as f:subprocess.run(link,cwd=build,stdout=f,stderr=subprocess.STDOUT,check=True,timeout=120)
 receipt['link_argv']=link
 red=(root/'red-preservation/test.log').read_text().splitlines()
 filters={'geometry':lambda s:'FAIL: owner lever endpoints:' in s or 'FAIL: owner lever downward pointer drag:' in s,
 'keys':lambda s:'FAIL: owner lever physical arrow direction:' in s,
 'rebind':lambda s:s.startswith('FAIL: ') and 'rebound from MIX' in s,
 'layout':lambda s:s.startswith('FAIL: S2 front-strip order:'),
 'legends':lambda s:s.startswith('FAIL: SUB legend ')}
 expected=[s for s in red if filters[mode](s)];assert len(expected)=={'geometry':54,'keys':54,'rebind':8,'layout':4,'legends':8}[mode]
env=os.environ.copy();env.pop('DISPLAY',None);env['XDG_CACHE_HOME']=str(dest/'cache');env['XDG_CONFIG_HOME']=str(dest/'config')
with (dest/'test.log').open('w') as f:result=subprocess.run([str(binary)],cwd=source,env=env,stdout=f,stderr=subprocess.STDOUT,timeout=120)
log=(dest/'test.log').read_text();print(log,flush=True)
actual=[s for s in log.splitlines() if s.startswith('FAIL: ')]
matched=result.returncode==(1 if expected else 0) and collections.Counter(actual)==collections.Counter(expected) and ('Panel component-only total: 729 checks, '+str(len(expected))+' failures; desktop components0') in log
receipt.update({'expected_failure_multiset':expected,'actual_failure_multiset':actual,'matched':matched,'exit_code':result.returncode,'binary_sha256':sha(binary),'finished_at':datetime.datetime.now(datetime.timezone.utc).isoformat()})
for p,v in original['inputs'].items():assert sha(source/p)==v,p
for row in original['compiled']:assert sha(row['object'])==row['sha256'],row['object']
assert sha(root/'green/panel-components')==original['binary_sha256'];assert git('rev-parse','HEAD')==original['head']
(dest/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
assert matched,'Intended exact ordinary assertion inventory not matched; raw evidence retained'
