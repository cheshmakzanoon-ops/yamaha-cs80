"""Strict three-case validator transaction; import inert, no CLI or launch approval."""
import copy,hashlib,json,math,os,shutil,stat
from pathlib import Path
from identity import require,capture_files
from validator_contract import linux_validations,admit_validation
from suite_transaction import owned,bounded_entries,environment_digest


def _digest(value):return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def _inventory(root,*,entry_cap,byte_cap,commands):
    require(root.is_dir(),'owned inventory root required');files={};dirs=[];size=0
    for p in bounded_entries(root,limit=entry_cap):
        commands.remaining();s=p.lstat();require(stat.S_ISREG(s.st_mode) or stat.S_ISDIR(s.st_mode),'inventory symlink/special node')
        name=str(p.relative_to(root))
        if stat.S_ISDIR(s.st_mode):dirs.append(name);continue
        require(s.st_nlink==1,'inventory hardlink');size+=s.st_size;require(size<=byte_cap,'inventory byte cap');h=hashlib.sha256();read=0
        with p.open('rb') as stream:
            while chunk:=stream.read(1024*1024):
                commands.remaining();read+=len(chunk);require(read<=s.st_size,'inventory file grew');h.update(chunk)
        after=p.lstat();require(read==s.st_size and (s.st_dev,s.st_ino,s.st_size,s.st_mtime_ns,s.st_ctime_ns)==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns),'inventory file changed during hashing');files[name]=h.hexdigest()
    commands.remaining()
    return {'sha256':files,'directories':sorted(dirs),'bytes':size}


def run_validations(*, arm, repo, validator, bundle, output, commands,
                    expected_validator_sha256, module_root, module_hashes, case_timeouts,
                    contract):
    """contract supplies trusted complete source/runtime/tool and resource scopes.

    Candidate must match stock_common_sha256. Module file hashes intentionally
    differ across arms; their inventory and canonical location do not. No version
    or complete runtime-closure claim is inferred from a caller's release record.
    """
    created=False;output_path=None
    try:
        c=copy.deepcopy(contract);require(arm in ('stock','candidate'),'explicit arm required')
        require(isinstance(case_timeouts,dict) and set(case_timeouts)=={'headless','gui','gui-seed-regression'} and all(isinstance(v,(int,float)) and not isinstance(v,bool) and math.isfinite(v) and 0<v<=3600 for v in case_timeouts.values()),'reviewed finite per-case deadlines required')
        repo,validator,bundle,output_path,module_root=map(owned,(repo,validator,bundle,output,module_root))
        require(bundle==module_root and bundle.is_dir(),'validated bundle and sealed root differ')
        require(not os.path.lexists(output_path),'fresh validator output required')
        logs=owned(commands.out)
        require(all(output_path!=p and not output_path.is_relative_to(p) and not p.is_relative_to(output_path) for p in (repo,bundle,logs)),'independent validator output required')
        require(all(logs!=p and not logs.is_relative_to(p) and not p.is_relative_to(logs) for p in (repo,bundle)),'independent validator execution logs required')
        env=c['environment'];env_hash=environment_digest(env)
        require('PATH' in env and all(p and Path(p).is_absolute() for p in env['PATH'].split(':')),'frozen absolute PATH required')
        runtime=c['runtime_hashes'];tools=c['tool_resolutions'];source=c['source_hashes']
        require({'python3','xvfb-run','timeout'}<=set(tools),'complete explicit wrapper tool resolutions required')
        require('scripts/run-pluginval-with-core.py' in source,'core wrapper source must be frozen')
        require(c['validator_release']=={'version':'1.0.4','sha256':expected_validator_sha256},'frozen validator release identity required')
        require(runtime.get(str(validator))==expected_validator_sha256,'validator absent from frozen runtime')
        for key in ('entry_cap','byte_cap','log_byte_cap'):
            require(isinstance(c[key],int) and not isinstance(c[key],bool) and c[key]>0,'positive validator output cap')
        require(c['entry_cap']<=250000 and c['byte_cap']<=2*1024**3 and c['log_byte_cap']<=min(c['byte_cap'],16*1024**2),'bounded validator output envelope')
        require(isinstance(c['module_directories'],list) and len(set(c['module_directories']))==len(c['module_directories']),'exact bundle directory roster')
        cases=linux_validations(repo=repo,validator=validator,bundle=bundle,output=output_path)
        actual={}
        positions={'headless':[(0,'python3')],'gui':[(0,'xvfb-run'),(4,'python3')],'gui-seed-regression':[(0,'timeout'),(4,'xvfb-run'),(8,'python3')]}
        for name,argv in cases.items():
            actual[name]=list(argv)
            for index,tool in positions[name]:
                require(argv[index]==tool,'wrapper positional contract drift');actual[name][index]=str(owned(tools[tool]))
        labels={arm+'-'+name for name in cases}
        require(not commands.terminal_failure and not any(r.get('label') in labels for r in commands.ledger),'terminal or repeated validation forbidden')
        common={'repo':str(repo),'validator':str(validator),'bundle':str(bundle),'validator_release':c['validator_release'],'runtime_hashes':runtime,'tool_resolutions':tools,'source_hashes':source,'environment_sha256':env_hash,'case_timeouts':case_timeouts,'module_files':sorted(module_hashes),'module_directories':sorted(c['module_directories']),'entry_cap':c['entry_cap'],'byte_cap':c['byte_cap'],'log_byte_cap':c['log_byte_cap']}
        common_sha=_digest(common)
        if arm=='candidate':require(c['stock_common_sha256']==common_sha,'validator cross-arm common contract differs')
        captured_logs={};captured_output={'sha256':{},'directories':[]}
        def audit():
            commands.remaining();require(environment_digest(env)==env_hash,'frozen validator environment changed')
            if captured_logs:require(capture_files(logs,list(captured_logs))==captured_logs,'retained validator log changed')
            require(capture_files(repo,list(source))==source,'validator wrapper/source drift')
            for path,wanted in runtime.items():
                p=owned(path);require(capture_files(p.parent,[p.name])[p.name]==wanted,'validator runtime drift')
            for name,path in tools.items():
                require(isinstance(name,str) and name and '/' not in name and str(owned(path)) in runtime,'frozen absolute tool bytes required')
                found=shutil.which(name,path=env['PATH']);require(found is not None and Path(found).resolve(strict=True)==owned(path),'frozen PATH tool resolution drift')
            identity=_inventory(bundle,entry_cap=c['entry_cap'],byte_cap=c['byte_cap'],commands=commands)
            require(identity['sha256']==module_hashes and identity['directories']==sorted(c['module_directories']),'complete module/bundle identity differs')
            if created:
                current=_inventory(output_path,entry_cap=c['entry_cap'],byte_cap=c['byte_cap'],commands=commands)
                require(all(current['sha256'].get(p)==h for p,h in captured_output['sha256'].items()) and set(captured_output['directories'])<=set(current['directories']),'prior generated validator evidence changed')
            commands.remaining()
        audit();output_path.mkdir(parents=True);created=True;results={};retained_logs={}
        for name,argv in actual.items():
            audit();stdout,stderr=commands.run(arm+'-'+name,argv,cwd=repo,timeout=case_timeouts[name],env=env,replace_env=True,require_group_empty=True)
            record=commands.ledger[-1]
            require(record.get('cwd')==str(repo) and record.get('executed_argv')==argv and record.get('environment_sha256')==env_hash and record.get('replace_environment') is True and record.get('owned_process_group_empty') is True,'validator actual command/environment binding')
            audit();paths={'stdout':owned(stdout),'stderr':owned(stderr)}
            require(all(p.parent==logs for p in paths.values()),'validator logs outside execution root')
            require(all(p.is_file() and p.stat().st_nlink==1 for p in paths.values()),'missing or unsafe validator log')
            require(all(p.stat().st_size<=c['log_byte_cap'] for p in paths.values()),'validator individual log byte cap')
            retained_logs.update({str(p):p.stat().st_size for p in paths.values()})
            inventory=_inventory(output_path,entry_cap=c['entry_cap'],byte_cap=c['byte_cap'],commands=commands)
            require(sum(retained_logs.values())+inventory['bytes']<=c['byte_cap'] and len(retained_logs)+len(inventory['sha256'])+len(inventory['directories'])<=c['entry_cap'],'aggregate validator evidence bound')
            hashes={k:capture_files(p.parent,[p.name])[p.name] for k,p in paths.items()}
            captured_logs.update({paths[k].name:v for k,v in hashes.items()})
            receipt=admit_validation(name=name,expected_argv=argv,record=record,stdout=paths['stdout'].read_text(),stderr=paths['stderr'].read_text(),arm=arm)
            captured_output=copy.deepcopy(inventory)
            results[name]={'generated_output_inventory':copy.deepcopy(inventory),**receipt,'ledger_record':dict(record),'log_sha256':hashes,'original_argv':cases[name],'resolved_argv':argv}
        audit();retained=_inventory(output_path,entry_cap=c['entry_cap'],byte_cap=c['byte_cap'],commands=commands);commands.remaining()
        return {'arm':arm,'validators':results,'output_inventory':retained,'common_contract':common,'common_sha256':common_sha,'production_qualified':False,'trusted_release_and_runtime_closure_required':True}
    except BaseException as error:
        commands.terminal_failure=True;commands.ledger.append({'label':str(arm)+'-validator-admission-failure','error':repr(error),'terminal_failure':True});commands.persist()
        # Preserve originals; do not perform an unbounded recursive failure copy.
        raise
