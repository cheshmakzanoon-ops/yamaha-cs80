"""Actual baseline->map-only configuration transaction; no CLI or build launch."""
import copy,hashlib,json,os,re,shlex,stat
from pathlib import Path
from identity import require,capture_files
from configure_preflight import configure_argv,provisional_tests
from compile_inventory import compile_inventory
from suite_transaction import owned,bounded_entries,environment_digest,definition_syntax
from split_ctest import audit_runtime

IMPLEMENTATION_INPUTS=tuple(Path(__file__).resolve().parent/name for name in ('configure_transaction.py','configure_preflight.py','compile_inventory.py','suite_transaction.py','split_ctest.py','definition_commands.py','identity.py','roster.py'))+(Path(__file__).resolve().parent.parent/'production-pic-pgo-research/execution.py',)


def _digest(data):return hashlib.sha256(data).hexdigest()
def _json_hash(value):
    digest=hashlib.sha256();size=0
    for fragment in json.JSONEncoder(sort_keys=True,separators=(',',':'),ensure_ascii=True).iterencode(value):
        data=fragment.encode();size+=len(data);require(size<=64*1024**2,'configuration identity metadata cap');digest.update(data)
    return digest.hexdigest()
def _tree(root,entry_cap,byte_cap,check_time=lambda:None):
    files={};directories=[];total=0
    for p in bounded_entries(root,limit=entry_cap):
        check_time();s=p.lstat();require(not s.st_mode&0o7000,'configure special mode forbidden');require(stat.S_ISDIR(s.st_mode) or stat.S_ISREG(s.st_mode),'configure tree symlink/special node')
        name=p.relative_to(root).as_posix()
        if stat.S_ISDIR(s.st_mode):directories.append(name);continue
        require(s.st_nlink==1,'configure tree hardlink');total+=s.st_size;require(total<=byte_cap,'configure tree byte cap')
        h=hashlib.sha256();read=0
        with p.open('rb') as stream:
            while chunk:=stream.read(1024*1024):
                check_time();read+=len(chunk);require(read<=s.st_size,'configure input grew during hashing');h.update(chunk)
        after=p.lstat();require(read==s.st_size and (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),'configure input changed during hashing')
        files[name]=h.hexdigest()
    return {'sha256':dict(sorted(files.items())),'directories':sorted(directories)}
def _retain(source,destination,wanted,commands):
    before=source.lstat();require(stat.S_ISREG(before.st_mode) and before.st_nlink==1,'regular retained input required')
    h=hashlib.sha256();count=0;destination.parent.mkdir(parents=True,exist_ok=True)
    with source.open('rb') as src,destination.open('xb') as dst:
        while chunk:=src.read(1024*1024):
            commands.remaining();count+=len(chunk);require(count<=before.st_size,'retention input grew');h.update(chunk);dst.write(chunk)
    after=source.lstat()
    require(count==before.st_size and (before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns,before.st_mode)==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns,after.st_mode) and h.hexdigest()==wanted,'retention source changed')
    destination.chmod(stat.S_IMODE(before.st_mode));commands.remaining()
    with destination.open('rb') as stream:
        copied=hashlib.sha256()
        while chunk:=stream.read(1024*1024):commands.remaining();copied.update(chunk)
    require(copied.hexdigest()==wanted,'retained bytes differ')

def _modes(root,tree):
    return {'.':stat.S_IMODE(root.stat().st_mode),**{name:stat.S_IMODE((root/name).lstat().st_mode) for name in [*tree['sha256'],*tree['directories']]}}

def _read_metadata(path):
    with path.open('rb') as stream:data=stream.read(64*1024**2+1)
    require(len(data)<=64*1024**2,'configuration metadata byte cap');return data

def _cache(path):
    result={}
    for line in _read_metadata(path).decode().splitlines():
        if not line or line.startswith(('#','//')):continue
        require('=' in line and ':' in line.split('=',1)[0],'unsupported CMake cache entry')
        key,value=line.split('=',1);name,kind=key.split(':',1);require(name not in result,'duplicate cache key');result[name]={'type':kind,'value':value}
    return result

def _map_parity(baseline,overlay,build,finals):
    require(len(baseline)==len(overlay) and len(set(baseline))==len(baseline),'complete ordered unique Ninja command roster')
    seen=set()
    for before,after in zip(baseline,overlay):
        a,b=shlex.split(before),shlex.split(after)
        require(not any('@' in token for token in a+b),'opaque response file requires reviewed expansion')
        maps=lambda argv:[x for x in argv if '-Map' in x or '--print-map' in x or '--cref' in x or x in ('-M','-Wl,-M')]
        require(not maps(a),'baseline already contains map options')
        target=None
        if b.count('-o')==1:
            output=b[b.index('-o')+1]
            matches=[t for t,value in finals.items() if value['binary']==output]
            if matches:target=matches[0]
        if target is None:require(before==after,'non-final Ninja command changed')
        else:
            require(target not in seen,'duplicate final linker command');seen.add(target)
            flag='-Wl,-Map,'+str(build/(target+'.research.map'))+',--cref'
            require(maps(b)==[flag] and b.count(flag)==1,'one exact final map option required')
            b.remove(flag);require(a==b,'final linker differs beyond map-only option')
    require(seen==set(finals),'missing final map link actions')


def _ninja_graph_parity(before,after,build,overlay):
    # Literal subset only: no generic normalization of generated build semantics.
    require(not re.search(r'[\s$#:]',''.join(map(str,(build,overlay)))),'Ninja graph audit requires literal unescaped canonical paths')
    lines=after.decode().splitlines(keepends=True);seen=set();references=0
    for index,line in enumerate(lines):
        if line.startswith('  LINK_FLAGS ='):
            for target in ('CS80Reference_VST3','CS80Reference_Standalone'):
                flag=' -Wl,-Map,'+str(build/(target+'.research.map'))+',--cref'
                if flag in line:
                    require(target not in seen and line.count(flag)==1,'duplicate generated map flag');seen.add(target);line=line.replace(flag,'',1)
        if line.startswith('build ') and (': RERUN_CMAKE ' in line or line.rstrip().endswith(': phony')):
            token=' '+str(overlay)
            if token in line:
                require(line.count(token)==1,'duplicate overlay graph input');references+=1;line=line.replace(token,'',1)
        lines[index]=line
    require(seen=={'CS80Reference_VST3','CS80Reference_Standalone'} and references==2,'exact map/overlay graph additions required')
    require(''.join(lines).encode()==before,'Ninja graph differs beyond reviewed map/overlay dependencies')

def _configure_arm(contract,commands,*,expected_compile_count,expected_test_count):
    out=None;created=False;arm=None
    try:
        c=copy.deepcopy(contract);arm=c['arm'];require(arm in ('stock','candidate'),'explicit configure arm')
        if expected_compile_count==587:require(arm=='stock','one pre-stock configuration transaction only')
        commands.remaining();repo,build,out=map(owned,(c['repo'],c['build'],c['output']))
        require(repo.is_dir() and not out.exists(),'owned source and fresh configure evidence required')
        require(all(a!=b and not a.is_relative_to(b) and not b.is_relative_to(a) for a,b in ((repo,build),(repo,out),(build,out))),'disjoint configuration roots')
        for name in ('tree_entry_cap','tree_byte_cap'):
            require(isinstance(c[name],int) and not isinstance(c[name],bool) and c[name]>0,'positive bounded configuration cap')
        require(c['tree_entry_cap']<=250000 and c['tree_byte_cap']<=16*1024**3,'configuration cap envelope')
        logs=owned(commands.out);require(all(logs!=p and not logs.is_relative_to(p) and not p.is_relative_to(logs) for p in (repo,build,out)),'independent configure execution logs required')
        for key,maximum in (('retained_entry_cap',1000000),('retained_byte_cap',16*1024**3)):
            require(isinstance(c[key],int) and not isinstance(c[key],bool) and 0<c[key]<=maximum,'positive bounded cumulative retention cap')
        def budget(extra_entries=0,extra_bytes=0):
            commands.remaining();entries=extra_entries;size=extra_bytes
            for root in (out,logs):
                if not root.exists():continue
                for p in bounded_entries(root,limit=c['retained_entry_cap']):
                    commands.remaining();metadata=p.lstat();require(stat.S_ISDIR(metadata.st_mode) or stat.S_ISREG(metadata.st_mode),'retention root special node')
                    entries+=1;size+=metadata.st_size if stat.S_ISREG(metadata.st_mode) else 0
                    require(entries<=c['retained_entry_cap'] and size<=c['retained_byte_cap'],'cumulative retained artifact/log cap')
            require(entries<=c['retained_entry_cap'] and size<=c['retained_byte_cap'],'cumulative retained artifact/log cap');return {'entries':entries,'bytes':size}
        env=c['environment'];env_hash=environment_digest(env)
        cmake,ninja,ctest,overlay=map(owned,(c['cmake'],c['ninja'],c['ctest'],c['overlay']))
        helpers=c['helper_hashes'];require({str(p) for p in IMPLEMENTATION_INPUTS}<=set(helpers),'configuration implementation scope omitted')
        runtime=c['runtime_hashes'];require(all(str(p) in runtime for p in (cmake,ninja,ctest,overlay)),'frozen configure tools and overlay required')
        require(set(c['finals'])=={'CS80Reference_VST3','CS80Reference_Standalone'},'exact final target roster')
        require(all(value['map']==target+'.research.map' for target,value in c['finals'].items()),'fixed map names')
        if expected_compile_count==587:
            require(expected_test_count==569,'full normal tests required')
            require(c['finals']['CS80Reference_VST3']['binary']=='CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so' and c['finals']['CS80Reference_Standalone']['binary']=='CS80Reference_artefacts/Release/Standalone/CS-80 Reference','normal final output paths')
            require({'CMAKE_C_COMPILER','CMAKE_CXX_COMPILER','CMAKE_AR','CMAKE_RANLIB','CMAKE_MAKE_PROGRAM'}<=set(c['cache_runtime']),'complete compiler/archive/cache runtime fields required')
        require(c['cache_runtime']['CMAKE_MAKE_PROGRAM']==str(ninja),'exact Ninja cache runtime')
        require(set(c['cache_runtime_files'])==set(c['cache_runtime']),'complete cache alias resolution contract')
        require({'CMAKE_AR','CMAKE_RANLIB'}<=set(c['cache_runtime']),'canonical archive tools required downstream')
        require(all(helpers[k]==runtime[k] for k in set(helpers)&set(runtime)),'conflicting helper/runtime identities')
        require(all(value in runtime for value in c['cache_runtime_files'].values()),'cache tools absent from frozen runtime')
        seed=c['seed'];require(set(seed)=={'sha256','directories'},'exact seed inventory required')
        if not build.exists():require(not seed['sha256'] and not seed['directories'],'missing dependency seed');build.mkdir(parents=True)
        require(build.is_dir() and _tree(build,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining)==seed,'unconfigured build differs from trusted seed')
        require(not any(name in seed['sha256'] for name in ('CMakeCache.txt','build.ninja','compile_commands.json')),'preconfigured build not admitted')
        observed_mode=c['generated_evidence_mode'];require(isinstance(observed_mode,bool),'explicit generated evidence mode')
        expected=c['expected'];require(set(expected)=={'bootstrap','baseline','overlay'},'three planned configuration phases required')
        require(expected['baseline']['compile_sha256']==expected['overlay']['compile_sha256'] and expected['baseline']['definitions']==expected['overlay']['definitions'],'expected compile/definition parity required')
        if observed_mode:
            require(set(expected['baseline']['generated'])==set(expected['overlay']['generated']),'common independently declared generated scope')
            for phase in expected.values():require(phase['compile_sha256'] is None and phase['ninja_commands'] is None and all(v is None for group in ('definitions','generated') for v in phase[group].values()),'observed generated mode must not imply preauthorized hashes')
        required={'CMakeCache.txt','build.ninja','CMakeFiles/rules.ninja','compile_commands.json'}|set(expected['baseline']['definitions'])
        for phase in expected.values():
            require(required<=set(phase['generated']),'mandatory generated input scope omitted')
            require(any(re.fullmatch(r'CMakeFiles/[^/]+/CMakeCXXCompiler.cmake',p) for p in phase['generated']) and any(re.fullmatch(r'CMakeFiles/[^/]+/CMakeSystem.cmake',p) for p in phase['generated']),'compiler/system generated identity omitted')
        out.mkdir(parents=True);created=True
        def audit():
            commands.remaining()
            require(capture_files(repo,list(c['source_hashes']))==c['source_hashes'],'configure source changed')
            for group in c['dependencies']:
                root=owned(group['root']);require(capture_files(root,list(group['sha256']))==group['sha256'],'trusted configure dependency changed')
            for key,spelling in c['cache_runtime'].items():
                require(Path(spelling).is_absolute() and Path(spelling).resolve(strict=True)==owned(c['cache_runtime_files'][key]),'frozen cache runtime alias changed')
            for path,digest in {**runtime,**helpers}.items():
                p=owned(path);require(capture_files(p.parent,[p.name])[p.name]==digest,'configure runtime/overlay changed')
            _tree(build,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining);budget();commands.remaining()
        log_hashes={}
        def run(label,argv,before=None):
            audit()
            if before:before()
            stdout,stderr=commands.run(arm+'-'+label,argv,cwd=repo,env=env,replace_env=True,require_group_empty=True)
            row=commands.ledger[-1]
            require(row.get('label')==arm+'-'+label and row.get('argv')==argv and row.get('executed_argv')==argv and row.get('cwd')==str(repo) and row.get('exit')==0 and not row.get('error') and row.get('owned_process_group_empty') is True and row.get('replace_environment') is True and row.get('environment_sha256')==env_hash,'configure actual command/environment binding')
            audit()
            for path in (Path(stdout),Path(stderr)):log_hashes[path.name]=capture_files(path.parent,[path.name])[path.name]
            return Path(stdout),Path(stderr),dict(row)
        phases={};base=configure_argv(repo,build);base[0]=str(cmake)
        for phase in ('bootstrap','baseline','overlay'):
            argv=base+(['-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES='+str(overlay)] if phase=='overlay' else [])
            _,_,record=run('configure-'+phase,argv)
            tree=_tree(build,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining);want=expected[phase]
            actual_defs={p:h for p,h in tree['sha256'].items() if Path(p).name=='CTestTestfile.cmake'}
            require((set(actual_defs)==set(want['definitions'])) if observed_mode else actual_defs==want['definitions'],'complete raw CTest definition identity differs')
            db_bytes=_read_metadata(build/'compile_commands.json');require(observed_mode or _digest(db_bytes)==want['compile_sha256'],'independent compile database identity differs')
            db=json.loads(db_bytes);rows=compile_inventory(db,source_root=repo,build_root=build);require(len(rows)==expected_compile_count,'normal compile roster count differs')
            require(all(not os.path.lexists(build/p) for p in rows),'project compiler outputs already exist during configuration')
            generated=capture_files(build,list(want['generated']));require(observed_mode or generated==want['generated'],'configured generated input differs from trusted manifest')
            if expected_compile_count==587:require(any(re.fullmatch(r'CMakeFiles/[^/]+/CMakeCCompiler.cmake',p) for p in generated),'C compiler generated identity omitted')
            cache=_cache(build/'CMakeCache.txt')
            fixed={'CMAKE_HOME_DIRECTORY':str(repo),'CMAKE_CACHEFILE_DIR':str(build),'CMAKE_BUILD_TYPE':'Release','CMAKE_GENERATOR':'Ninja','CS80_BUILD_PLUGIN':'ON','CS80_BUILD_TESTS':'ON',**c['cache_runtime']}
            for key,value in fixed.items():require(cache.get(key,{}).get('value')==value,'configured cache/toolchain differs: '+key)
            overlay_cache=cache.get('CMAKE_PROJECT_TOP_LEVEL_INCLUDES',{}).get('value','')
            require(overlay_cache==(str(overlay) if phase=='overlay' else ''),'unexpected configured overlay')
            def test_audit():
                require(capture_files(build,list(generated))==generated,'generated input changed before show-only')
                for path in actual_defs:_read_metadata(build/path)
                definition_syntax(build,actual_defs)
                audit_runtime(build=build,ctest=ctest,expected_ctest_sha256=runtime[str(ctest)],definition_hashes=actual_defs,environment=env)
            tests_path,_,_=run('configured-tests-'+phase,[str(ctest),'--test-dir',str(build),'--show-only=json-v1'],before=test_audit)
            tests=json.loads(_read_metadata(tests_path));summary=provisional_tests(tests)
            require(summary['count']==expected_test_count and sorted(t['name'] for t in tests['tests'])==c['expected_test_names'],'full independent configured test roster differs')
            ninja_path,_,_=run('configured-ninja-'+phase,[str(ninja),'-C',str(build),'-t','commands'])
            ninja_commands=_read_metadata(ninja_path).decode().splitlines();require(observed_mode or ninja_commands==want['ninja_commands'],'independent Ninja command roster differs')
            # Recheck all admitted generated bytes after both inventory processes.
            require(capture_files(build,list(generated))==generated,'generated input changed during inspection')
            tree=_tree(build,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining)
            modes=_modes(build,tree);budget(extra_entries=1+len(tree['sha256'])+len(tree['directories']),extra_bytes=sum((build/p).stat().st_size for p in tree['sha256']));dest=out/phase;dest.mkdir()
            for path in tree['directories']:(dest/path).mkdir(parents=True,exist_ok=True)
            for path in tree['sha256']:
                _retain(build/path,dest/path,tree['sha256'][path],commands)
            for path in sorted(tree['directories'],key=lambda p:p.count('/'),reverse=True):(dest/path).chmod(modes[path])
            dest.chmod(modes['.'])
            require(_modes(build,tree)==_modes(dest,tree)==modes,'retained configuration modes differ')
            require(_tree(build,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining)==_tree(dest,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining)==tree,'build/equivalent retained configuration changed')
            phases[phase]={'build_tree_manifest':tree,'mode_manifest':modes,'compile_sha256':_digest(db_bytes),'definitions':actual_defs,'generated':generated,'compile_database':db,'ninja_commands':ninja_commands,'cache':cache,'tests':tests,'command':record}
        a,b=phases['baseline'],phases['overlay'];require(a['compile_sha256']==b['compile_sha256'] and a['definitions']==b['definitions'],'raw configure parity failed')
        ac,bc=copy.deepcopy(a['cache']),copy.deepcopy(b['cache']);ac.pop('CMAKE_PROJECT_TOP_LEVEL_INCLUDES',None);bc.pop('CMAKE_PROJECT_TOP_LEVEL_INCLUDES',None);require(ac==bc,'cache differs beyond exact overlay')
        require(a['tests']==b['tests'],'configured CTest semantics changed')
        _map_parity(a['ninja_commands'],b['ninja_commands'],build,c['finals'])
        _ninja_graph_parity(_read_metadata(out/'baseline/build.ninja'),_read_metadata(out/'overlay/build.ninja'),build,overlay)
        ta,tb=a['build_tree_manifest'],b['build_tree_manifest']
        require(ta['sha256'].keys()==tb['sha256'].keys() and ta['directories']==tb['directories'] and a['mode_manifest']==b['mode_manifest'],'configuration tree inventory/modes changed')
        deltas={p for p in ta['sha256'] if ta['sha256'][p]!=tb['sha256'][p]}
        require(deltas<={'CMakeCache.txt','build.ninja','Testing/Temporary/LastTest.log'},'unreviewed generated artifact change')
        require(not any(os.path.lexists(build/value[k]) for value in c['finals'].values() for k in ('binary','map')),'project final/map outputs exist before build')
        inputs=[*c['dependencies'],{'root':str(build),'sha256':b['generated']}]
        common={'repo':str(repo),'build':str(build),'cmake':str(cmake),'ninja':str(ninja),'ctest':str(ctest),'ar':c['cache_runtime_files']['CMAKE_AR'],'overlay':str(overlay),'runtime_hashes':runtime,'helper_hashes':helpers,'cache_runtime':c['cache_runtime'],'cache_runtime_files':c['cache_runtime_files'],'environment_sha256':env_hash,'compile_database':b['compile_database'],'ninja_commands':b['ninja_commands'],'input_manifests':inputs,'finals':c['finals'],'expected_configuration':expected,'observed_phase_artifact_sha256':{p:_json_hash({'tree':v['build_tree_manifest'],'modes':v['mode_manifest'],'argv':v['command']['argv']}) for p,v in phases.items()},'generated_evidence_mode':observed_mode,'expected_test_names':c['expected_test_names'],'build_tree_entry_cap':c['tree_entry_cap'],'tree_entry_cap':c['tree_entry_cap'],'tree_byte_cap':c['tree_byte_cap'],'retained_entry_cap':c['retained_entry_cap'],'retained_byte_cap':c['retained_byte_cap']}
        audit()
        for phase in phases:
            require(_tree(out/phase,c['tree_entry_cap'],c['tree_byte_cap'],commands.remaining)==phases[phase]['build_tree_manifest'],'retained baseline/overlay evidence changed')
            require(_modes(out/phase,phases[phase]['build_tree_manifest'])==phases[phase]['mode_manifest'],'retained evidence mode changed')
        require(capture_files(logs,list(log_hashes))==log_hashes,'configure execution logs changed')
        retirement=[]
        for path,digest in b['build_tree_manifest']['sha256'].items():
            if path.startswith('Testing/') and (Path(path).name.startswith('LastTest') or Path(path).name in ('TAG','Test.xml')):
                retirement.append({'path':str(build/path),'retained_path':str(out/'overlay'/path),'sha256':digest,'mode':b['mode_manifest'][path]})
        commands.remaining();result={'production_qualified':False,'configuration_admitted':True,'generated_evidence_provenance':('authenticated-configure-observation-not-preauthorized-hashes' if observed_mode else 'independently-expected-generated-hashes'),'arm':arm,'source_hashes':c['source_hashes'],'common_contract':common,'common_contract_sha256':_json_hash(common),'definition_hashes':b['definitions'],'configured_tests':b['tests'],'phases':phases,'log_sha256':log_hashes,'retained_ctest_artifacts_for_verified_retirement':retirement,'expected_input_scope_authority_required':True,'runtime_dependency_closure_not_inferred':True}
        result['retained_usage_before_receipt']=budget();usage=budget(extra_entries=1);written=0
        with (out/'configure-result.json').open('xb') as stream:
            for fragment in json.JSONEncoder(indent=2).iterencode(result):
                commands.remaining();data=fragment.encode();written+=len(data);require(written<=64*1024**2 and usage['bytes']+written<=c['retained_byte_cap'],'configuration receipt cumulative byte cap');stream.write(data)
        budget();commands.remaining();return result
    except BaseException as error:
        commands.terminal_failure=True;commands.ledger.append({'label':str(arm)+'-configure-transaction-failure','error':repr(error),'terminal_failure':True});commands.persist()
        if created and (out/'configure-result.json').exists():(out/'configure-result.json').rename(out/'configure-result-incomplete.json')
        if created:(out/'configure-failure.json').write_text(json.dumps({'error':repr(error),'production_qualified':False})+'\n')
        raise


def configure_arm(contract,commands):
    return _configure_arm(contract,commands,expected_compile_count=587,expected_test_count=569)
