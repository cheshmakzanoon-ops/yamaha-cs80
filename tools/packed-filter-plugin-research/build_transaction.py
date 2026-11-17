"""Configured normal all-target build and clean-link transaction; no CLI.

The independently frozen configure/toolchain contract is a prerequisite, not
something this function manufactures from a possibly altered build tree.
"""
import json,re,shutil,shlex,hashlib,os,stat
from pathlib import Path
from identity import require,capture_files,changed,FILTER
from compile_inventory import compile_inventory,filter_outputs
from link_binding import verify_binding,MEMBER
from suite_transaction import owned,environment_digest,bounded_entries


def _build_arm(contract,commands,*,expected_count):
    result={'production_qualified':False,'build_admitted':False};out=None;created=False;arm=None
    try:
        import copy
        c=copy.deepcopy(contract);arm=c['arm'];require(arm in ('stock','candidate'),'explicit build arm')
        commands.remaining();root=owned(c['repo']);build=owned(c['build']);out=owned(c['output'])
        require(not out.exists() and all(out!=p and not out.is_relative_to(p) and not p.is_relative_to(out) for p in (root,build)),'fresh disjoint build evidence root')
        env=c['environment'];env_hash=environment_digest(env)
        cmake=owned(c['cmake']);ar=owned(c['ar']);ninja=owned(c['ninja'])
        require(str(cmake) in c['runtime_hashes'] and str(ar) in c['runtime_hashes'] and str(ninja) in c['runtime_hashes'],'absolute frozen build/archive tools required')
        database=json.loads(owned(build/'compile_commands.json').read_bytes())
        require(database==c['compile_database'],'exact independent compile database required')
        rows=compile_inventory(database,source_root=root,build_root=build)
        require(len(rows)==expected_count,'complete normal compile roster required')
        filters=filter_outputs(database,source_root=root,build_root=build)
        require(len(filters['all_filter_objects'])==2,'exact normal and cap-test filter outputs required')
        targets={'CS80Reference_VST3','CS80Reference_Standalone'}
        require(set(c['finals'])==targets,'exact final link target roster')
        if expected_count==587:
            require(c['archive']=='libcs80_core.a','normal core archive required')
            require(c['finals']['CS80Reference_VST3']['binary']=='CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so' and c['finals']['CS80Reference_Standalone']['binary']=='CS80Reference_artefacts/Release/Standalone/CS-80 Reference','normal production final paths required')
        outputs={c['archive'],*(r[k] for r in c['finals'].values() for k in ('binary','map'))}
        require(len(outputs)==5 and all(isinstance(p,str) and p and not Path(p).is_absolute() and '..' not in Path(p).parts for p in outputs),'distinct owned relative final outputs required')
        require(all(r['map']==target+'.research.map' for target,r in c['finals'].items()),'fixed map output names required')
        for path in set(rows)|outputs:owned(build/path)
        required_absent=set(filters['all_filter_objects'])|outputs
        require(set(c['required_actions'])=={'core_compile','cap_compile','archive',*targets},'complete required build action roster')
        require(set(c['required_actions'].values())<=set(c['ninja_commands']),'required action absent from frozen Ninja roster')
        require(len(set(c['required_actions'].values()))==5,'distinct compile/archive/link actions required')
        for label,path in [('core_compile',filters['plugin_filter_object']),('cap_compile',next(p for p in filters['all_filter_objects'] if p!=filters['plugin_filter_object']))]:
            actual=shlex.split(c['required_actions'][label])
            require(actual.count('-MD')==actual.count('-MT')==actual.count('-MF')==1,'exact Ninja dependency switches required')
            require(actual[actual.index('-MT')+1]==path and actual[actual.index('-MF')+1]==path+'.d','Ninja dependency output differs')
            actual.remove('-MD')
            for flag in ('-MT','-MF'):
                index=actual.index(flag);del actual[index:index+2]
            require(actual==rows[path]['arguments'],'actual filter compile action differs from frozen compile row')
        for target,paths in c['finals'].items():
            argv=shlex.split(c['required_actions'][target]);maps=[arg for arg in argv if '-Map' in arg or '--print-map' in arg]
            require(maps==['-Wl,-Map,'+str(build/paths['map'])+',--cref'],'one exact canonical map-only link option required')
            require(argv.count('-o')==1 and argv[argv.index('-o')+1]==paths['binary'],'final link output differs')
        archive_tokens=shlex.split(c['required_actions']['archive'])
        ar_positions=[i for i,arg in enumerate(archive_tokens) if Path(arg).is_absolute() and Path(arg).resolve()==ar]
        require(len(ar_positions)==1,'one exact frozen archiver action required')
        ai=ar_positions[0];end=next((i for i in range(ai+1,len(archive_tokens)) if archive_tokens[i] in ('&&',';')),len(archive_tokens))
        archive_argv=archive_tokens[ai:end]
        require(len(archive_argv)>=4 and archive_argv[1] in ('qc','rcs') and archive_argv[2]==c['archive'],'exact archive output/action required')
        core_objects=sorted(p for p in rows if p.startswith('CMakeFiles/cs80_core.dir/'))
        require(sorted(archive_argv[3:])==core_objects,'complete core archive object roster required')
        frozen_keys=('repo','build','cmake','ar','ninja','runtime_hashes','compile_database','ninja_commands','required_actions','input_manifests','archive','finals','build_tree_entry_cap')
        frozen_contract={key:c[key] for key in frozen_keys};frozen_contract['environment_sha256']=env_hash
        contract_sha256=hashlib.sha256(json.dumps(frozen_contract,sort_keys=True,separators=(',',':')).encode()).hexdigest()
        if arm=='candidate':
            require(c['stock_contract_sha256']==contract_sha256,'cross-arm build contract changed')
            require(changed(c['stock_source_hashes'],c['source_hashes'])=={FILTER},'candidate changes only exact filter source')
            require(c['source_hashes'][FILTER]==c['candidate_source_sha256'],'frozen candidate source differs')
        if arm=='candidate':
            stock=owned(c['stock_seal'])
            require(all(stock!=p and not stock.is_relative_to(p) and not p.is_relative_to(stock) for p in (root,build,out)),'separate stock seal')
        out.mkdir(parents=True);created=True
        def audit():
            commands.remaining()
            cap=c['build_tree_entry_cap'];require(isinstance(cap,int) and not isinstance(cap,bool) and 0<cap<=250000,'bounded complete build-tree entry cap required')
            for entry in bounded_entries(build,limit=cap):
                metadata=entry.lstat();mode=metadata.st_mode
                require(stat.S_ISREG(mode) or stat.S_ISDIR(mode),'build tree symlink/special output forbidden')
                require(not stat.S_ISREG(mode) or metadata.st_nlink==1,'build tree hardlinked file forbidden')
            require(capture_files(root,list(c['source_hashes']))==c['source_hashes'],'build source changed')
            for group in c['input_manifests']:
                require(capture_files(owned(group['root']),list(group['sha256']))==group['sha256'],'configured/generated/dependency input changed')
            require(json.loads((build/'compile_commands.json').read_bytes())==database,'compile database changed')
            for name,wanted in c['runtime_hashes'].items():
                p=owned(name);require(capture_files(p.parent,[p.name])[p.name]==wanted,'build runtime changed')
            commands.remaining()
        def run(label,argv):
            audit();stdout,stderr=commands.run(label,argv,cwd=build,env=env,replace_env=True,require_group_empty=True)
            record=commands.ledger[-1]
            require(record['argv']==argv and record['executed_argv']==argv and record['cwd']==str(build) and record['exit']==0 and record.get('owned_process_group_empty') is True and record['environment_sha256']==env_hash,'actual build command/environment binding')
            audit();return Path(stdout),Path(stderr),dict(record)
        audit()
        listing,_,_=run(arm+'-frozen-ninja-commands',[str(ninja),'-C',str(build),'-t','commands'])
        require(listing.read_text().splitlines()==c['ninja_commands'],'actual Ninja command roster differs')
        if arm=='stock':
            require(all(not os.path.lexists(build/p) for p in set(rows)|outputs),'stock compiler/link outputs must be absent')
        else:
            before={**c['stock_objects'],**c['stock_link_outputs']}
            require(set(c['stock_objects'])==set(rows) and set(c['stock_link_outputs'])==outputs,'complete stock output seal roster')
            require(capture_files(build,list(before))==capture_files(stock,list(before))==before,'equivalent stock copies required before removal')
            for p in sorted(required_absent):
                commands.remaining();require(capture_files(build,[p])[p]==capture_files(stock,[p])[p]==before[p],'stock output changed before removal');(build/p).unlink()
        require(all(not os.path.lexists(build/p) for p in required_absent),'fresh compile/archive/link/map absence required')
        stdout,stderr,record=run(arm+'-normal-all-build',[str(cmake),'--build',str(build),'--parallel','2','--verbose'])
        lines=[re.sub(r'^\[[0-9]+/[0-9]+\] ','',line) for line in stdout.read_text().splitlines()]
        for label,action in c['required_actions'].items():require(lines.count(action)==1,'missing/duplicate actual build action '+label)
        objects=capture_files(build,list(rows));links=capture_files(build,sorted(outputs))
        if arm=='candidate':require(changed(c['stock_objects'],objects)==set(filters['all_filter_objects']),'only exact two filter objects may change')
        seal=out/'sealed';seal.mkdir()
        for p in sorted(set(rows)|outputs):
            commands.remaining();destination=seal/p;destination.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(build/p,destination)
        require(capture_files(seal,list(rows))==objects and capture_files(seal,sorted(outputs))==links,'postbuild equivalent complete seal failed')
        names,_,_=run(arm+'-archive-list',[str(ar),'t',str(build/c['archive'])]);member,_,_=run(arm+'-archive-member',[str(ar),'p',str(build/c['archive']),MEMBER])
        bindings={}
        for target,paths in c['finals'].items():
            bindings[target]=verify_binding(object_bytes=(seal/filters['plugin_filter_object']).read_bytes(),member_bytes=member.read_bytes(),member_names=names.read_text().splitlines(),archive_path=str(build/c['archive']),map_text=(seal/paths['map']).read_text(),cwd=build,final_bytes=(seal/paths['binary']).read_bytes())
        audit();require(capture_files(build,list(rows))==objects and capture_files(build,sorted(outputs))==links,'outputs changed during binding')
        commands.remaining();result.update(build_admitted=True,source_hashes=c['source_hashes'],contract_sha256=contract_sha256,frozen_contract=frozen_contract,objects=objects,link_outputs=links,filter_outputs=filters,bindings=bindings,command=record,log_sha256={p.name:capture_files(p.parent,[p.name])[p.name] for p in (stdout,stderr)},sealed=str(seal))
        (out/'build-result.json').write_text(json.dumps(result,indent=2)+'\n');return result
    except BaseException as error:
        commands.terminal_failure=True;commands.ledger.append({'label':str(arm)+'-build-transaction-failure','error':repr(error),'terminal_failure':True});commands.persist()
        if created:(out/'build-failure.json').write_text(json.dumps({'error':repr(error),'production_qualified':False})+'\n')
        raise


def build_arm(contract,commands):
    return _build_arm(contract,commands,expected_count=587)
