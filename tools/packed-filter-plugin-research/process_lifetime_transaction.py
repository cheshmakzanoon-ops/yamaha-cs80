"""One supervised process-lifetime A-D observation. No E-transfer authority/CLI."""
import copy,hashlib,json,math,os,re,secrets,stat,struct
from pathlib import Path
from identity import require,capture_files
from suite_transaction import owned,bounded_entries,environment_digest

LEGACY_SCHEMA='cs80-process-lifetime-child-v1'
SCHEMA='cs80-process-lifetime-child-v2'
PROFILE='process-lifetime'
HISTORY='cs80-instance-history-A-D-v1'
BODY_CAP=96*1024**2
PAYLOAD_CAP=64*1024**2
CASE_CAP=1024**2
FRAME_CAP=BODY_CAP+80

def digest(data):return hashlib.sha256(data).hexdigest()
def sha(value):require(isinstance(value,str) and re.fullmatch('[0-9a-f]{64}',value),'canonical SHA256 required');return value

class _Reader:
    def __init__(self,data):self.data=memoryview(data);self.at=0
    def raw(self,n):
        require(isinstance(n,int) and 0<=n<=len(self.data)-self.at,'truncated/overflow receipt');value=self.data[self.at:self.at+n];self.at+=n;return value
    def u64(self):return struct.unpack('<Q',self.raw(8))[0]
    def u32(self):return struct.unpack('<I',self.raw(4))[0]
    def blob(self,cap):
        n=self.u64();require(n<=cap,'receipt field bound');return self.raw(n)
    def text(self,cap=4096):return self.blob(cap).tobytes().decode('utf-8',errors='strict')
    def end(self):require(self.at==len(self.data),'trailing receipt/payload bytes')
    def count(self,cap):
        n=self.u64();require(n<=cap,'receipt vector bound');return n

def _payload(data,case,policy,controller_result,c):
    r=_Reader(data);require(r.text()==HISTORY and r.blob(CASE_CAP)==case,'payload schema/case mismatch')
    # Fully consume the canonical A-D envelope. Inner metadata/protocol/PCM
    # semantics are admitted by the frozen native private producer, not invented here.
    spans=[r.blob(PAYLOAD_CAP) for _ in range(8)]
    require(all(len(v)>0 for v in spans),'incomplete A-D protocol/metadata/state')
    require([digest(v) for v in spans[:3]]==c['protocol_sha256'],'actual initial/alter/render protocol mismatch')
    connected=_Reader(spans[3]);require(connected.text()=='native-observer-connected-v1' and connected.text()=='native-observer-created-identity-v1','connected Created identity schema')
    require(connected.raw(16)==bytes.fromhex(c['selected_cid_hex']),'actual selected class CID mismatch')
    separate=connected.u32();require(separate in (0,1) and bool(separate)==bool(c['controller_cid_hex']),'controller class kind mismatch')
    if separate:require(connected.raw(16)==bytes.fromhex(c['controller_cid_hex']),'actual controller CID mismatch')
    state=_Reader(spans[-1]);require(state.text()=='cs80-native-state-v2' and state.u32()==policy,'state policy mismatch')
    require(state.u32()==0,'component state failed');component=state.blob(PAYLOAD_CAP);require(len(component)>0,'empty component state')
    require(state.u32()==controller_result,'controller state result mismatch');controller=state.blob(PAYLOAD_CAP);state.end()
    require(policy==1 or len(controller)==0,'unsupported controller state must be empty')
    renders=[r.blob(PAYLOAD_CAP) for _ in range(4)];require(all(len(v)>0 for v in renders),'missing A-D render')
    for sequence,kind in enumerate((1,2,1,2,2,2),1):require(r.u64()==sequence and r.u32()==kind,'complete six-task history order required')
    for index in range(3):
        attempted=r.u32();require(attempted==(1 if index<2 or policy==1 else 0),'restore policy/attempt mismatch')
        if attempted:require(r.u32()==0 and r.blob(PAYLOAD_CAP)==(component if index<2 else controller),'restored state bytes/result differ')
    for cap in (128,128):
        count=r.count(cap);require(count>0,'missing cleanup ledger')
        for _ in range(count):require(bool(r.text(256)),'empty cleanup name');r.u64()
    require([r.u64() for _ in range(5)]==[6,6,6,6,0],'worker join totals differ');r.end()
    return {'render_sha256':[digest(v) for v in renders],'post_A_state_sha256':digest(spans[-1]),'controller_state_policy':policy,'inner_semantics_trusted_native_producer_required':True}

def _maps(text):
    require(text and text.endswith('\n') and '\0' not in text,'bounded complete maps text')
    rows=[]
    for line in text.splitlines():
        require(0<len(line)<=16384 and len(rows)<32768,'maps row bound')
        match=re.fullmatch(r'([0-9a-f]+)-([0-9a-f]+) ([-r][-w][-x][ps]) +([0-9a-f]+) +([0-9a-f]+):([0-9a-f]+) +([0-9]+)(?: +(.*))?',line)
        require(match is not None,'malformed provider maps')
        a,b,perms,offset,major,minor,inode,path=match.groups();numbers=[int(x,16) for x in (a,b,offset,major,minor)]+[int(inode)]
        require(all(n<2**64 for n in numbers),'maps integer overflow');begin,end,off,maj,minr,ino=numbers
        require(begin<end and begin%4096==end%4096==off%4096==0 and off+end-begin<2**64 and (not rows or rows[-1][1]<=begin),'maps ranges/order')
        rows.append((begin,end,off,maj,minr,ino,perms,path or ''))
    return rows

def _parse(data,c,nonce,pid,module_stat):
    frame=_Reader(data);n=frame.u64();require(0<n<=BODY_CAP,'bounded receipt body');body=frame.raw(n);claimed=sha(frame.text(64));frame.end();require(digest(body)==claimed,'receipt body SHA mismatch')
    r=_Reader(body);require(r.text()==c['schema'] and r.text()==PROFILE,'receipt schema/profile mismatch')
    require(r.text(64)==nonce and r.u64()==pid,'receipt nonce/PID mismatch')
    require(r.text(64)==c['arm'],'receipt arm mismatch')
    for key in ('expected_manifest_digest','expected_evidence_digest','module_sha256'):require(sha(r.text(64))==c[key],'receipt identity mismatch: '+key)
    require(r.text()==HISTORY,'history schema mismatch');case=r.blob(CASE_CAP);require(case==bytes.fromhex(c['case_identity_hex']),'receipt case mismatch')
    query_details={}
    if c['schema']==SCHEMA:
        query_policy,query_result,query_pointer=r.u32(),r.u32(),r.u32()
        require(query_policy==c['controller_query_policy'] and query_result==c['controller_query_result'] and query_pointer==int(c['controller_query_pointer_present']),'controller query policy/result/pointer mismatch')
        query_details={'controller_query_policy':query_policy,'controller_query_result':query_result,'controller_query_pointer_present':bool(query_pointer)}
    require(r.u32()==1,'native dlclose return not zero');disposition=r.text(64);require(disposition in ('provider_removed','provider_retained'),'provider disposition missing')
    payload_hash=sha(r.text(64));payload=r.blob(PAYLOAD_CAP);require(len(payload)>0 and digest(payload)==payload_hash,'full A-D payload SHA mismatch')
    details=_payload(payload,case,c['controller_state_policy'],c['controller_state_result'],c)
    maps=[r.text(4*1024**2) for _ in range(3)];require(all(v and v.endswith('\n') for v in maps),'incomplete provider maps')
    objects=[]
    for _ in range(2):
        count=r.count(4096);require(count>0,'empty object inventory');objects.append([r.text(4096) for _ in range(count)])
    base=r.u64();addresses=[r.u64() for _ in range(3)];offsets=[r.u64() for _ in range(3)];require(base>0 and all(p>=base for p in addresses),'provider addresses missing')
    parsed_maps=[_maps(v) for v in maps];device=(os.major(module_stat.st_dev),os.minor(module_stat.st_dev),module_stat.st_ino)
    target=lambda rows:[row for row in rows if row[3:6]==device]
    before_target,loaded_target,after_target=map(target,parsed_maps);module_path=str(Path(c['module_root'])/c['module_relative'])
    require(all(row[3:6]==device for rows in parsed_maps for row in rows if row[7]==module_path),'provider pathname/inode disagreement')
    require(not before_target and loaded_target and all(row[7]==module_path for row in loaded_target),'provider target before/loaded mapping identity')
    require(module_path not in objects[0],'preexisting provider object')
    if disposition=='provider_removed':require(not after_target and module_path not in objects[1],'removed provider remains represented')
    else:require(after_target==loaded_target and objects[1].count(module_path)==1,'retained provider mapping/object mismatch')
    for address,offset in zip(addresses,offsets):
        matches=[row for row in loaded_target if row[0]<=address<row[1] and row[6][2]=='x']
        require(len(matches)==1 and address-matches[0][0]+matches[0][2]==offset and offset<module_stat.st_size,'export mapped identity/file offset mismatch')
    threads=[]
    for _ in range(2):
        count=r.count(4096);require(count>0,'empty thread inventory');items=[(r.u64(),r.u64()) for _ in range(count)]
        require(all(t>0 and start>0 for t,start in items) and items==sorted(items) and len({t for t,_ in items})==len(items),'noncanonical thread inventory');threads.append(items)
    require(threads[0]==threads[1] and pid in {t for t,_ in threads[0]},'post-ModuleExit thread identity differs')
    calls=[(r.text(256),r.u64()) for _ in range(r.count(128))];r.end()
    for name,value in (('ModuleExit',1),('loader-close',0)):
        require(sum(n==name for n,_ in calls)==1 and (name,value) in calls,'exact successful module teardown ledger required')
    require(next(i for i,v in enumerate(calls) if v[0]=='ModuleExit')<next(i for i,v in enumerate(calls) if v[0]=='loader-close'),'module exit/close order')
    return {'body_sha256':claimed,'payload_sha256':payload_hash,'payload_bytes':len(payload),'payload':payload,'native_dlclose_returned_zero':True,'provider_disposition':disposition,'provider_removed':disposition=='provider_removed','provider_retained':disposition=='provider_retained','provider_maps_sha256':[digest(v.encode()) for v in maps],'provider_objects':objects,'provider_base':base,'export_addresses':addresses,'export_offsets':offsets,'threads':threads,'module_calls':calls,**details,**query_details}

def _read_receipt(path,cap,commands):
    owned(path);fd=os.open(path,os.O_RDONLY|os.O_NOFOLLOW|os.O_CLOEXEC|os.O_NONBLOCK)
    try:
        before=os.fstat(fd);require(stat.S_ISREG(before.st_mode) and before.st_nlink==1 and before.st_uid==os.geteuid() and stat.S_IMODE(before.st_mode)==0o600 and 0<before.st_size<=cap,'fresh owned bounded receipt metadata')
        parts=[];size=0
        while chunk:=os.read(fd,min(1024*1024,cap+1-size)):
            commands.remaining();size+=len(chunk);require(size<=cap,'receipt growth exceeds cap');parts.append(chunk)
        after=os.fstat(fd);require(size==before.st_size and (before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns,before.st_mode)==(after.st_dev,after.st_ino,after.st_size,after.st_mtime_ns,after.st_ctime_ns,after.st_mode),'receipt changed during read')
        return b''.join(parts),(before.st_dev,before.st_ino,before.st_size,before.st_mtime_ns,before.st_ctime_ns,before.st_mode)
    finally:os.close(fd)

def _request_preselection(c,manifest,commands):
    # Same bounded owned request bytes as the child, before any native execution.
    # The native parser remains responsible for the complete identity/caps tail.
    data,_=_read_receipt(manifest,8*1024**2,commands)
    require(digest(data)==c['manifest_sha256'],'prelaunch request SHA256 mismatch')
    r=_Reader(data);schema=r.text(64)
    require(schema in ('cs80-process-lifetime-request-v1','cs80-process-lifetime-request-v2'),'prelaunch request schema')
    require(r.text(32)==PROFILE and r.text(16)==c['arm'] and r.text(32)==c['selected_cid_hex'],'prelaunch profile/arm/CID mismatch')
    selectors=[r.u64() for _ in range(5)]
    case=_Reader(bytes.fromhex(c['case_identity_hex']))
    def case_text(cap):
        n=case.u32();require(n<=cap,'prelaunch canonical case text bound');return case.raw(n).tobytes().decode('utf-8',errors='strict')
    require(case_text(64)=='packed-filter-workload-draft-v1','prelaunch canonical case schema')
    sha(case_text(64));sha(case_text(64))
    require(selectors==[case.u32() for _ in range(5)],'prelaunch workload selectors mismatch')
    require(r.u64()==c['controller_state_policy'],'prelaunch controller state policy mismatch')
    query_policy=r.u64() if schema=='cs80-process-lifetime-request-v2' else 1
    require(query_policy==c['controller_query_policy'],'prelaunch controller query policy mismatch')


def _process_lifetime(contract,commands,*,allow_script=False,allow_fixture=False):
    out=None;created=False;arm=None
    try:
        c=copy.deepcopy(contract);commands.remaining();arm=c['arm'];require(arm in ('stock','candidate'),'explicit arm')
        require(c['profile']==PROFILE and (c['schema']==SCHEMA or ((allow_script or allow_fixture) and c['schema']==LEGACY_SCHEMA)) and c['history_schema']==HISTORY,'frozen process-lifetime schema/profile only; E blocked')
        if c['schema']==SCHEMA:
            require(type(c.get('controller_query_policy')) is int and c['controller_query_policy'] in (1,2),'explicit controller query policy required')
            require(type(c.get('controller_query_result')) is int and 0<=c['controller_query_result']<2**32 and type(c.get('controller_query_pointer_present')) is bool,'explicit raw controller query result/pointer required')
            if c['controller_query_pointer_present']:
                require(c['controller_query_result']==0 and c['controller_cid_hex'] is None,'combined controller query expected success pointer')
            else:
                require(c['controller_cid_hex'] is not None and (c['controller_query_result']==0xffffffff or (c['controller_query_policy']==2 and c['controller_query_result']==1)),'separate controller query policy result not admitted')
        else:
            require(c.get('controller_query_policy',1)==1,'legacy receipt cannot authorize compatibility query policy')
        require(c['controller_state_policy'] in (1,2) and type(c['controller_state_policy']) is int,'frozen controller policy')
        require(type(c['controller_state_result']) is int and 0<=c['controller_state_result']<2**32 and (c['controller_state_result']==0 if c['controller_state_policy']==1 else c['controller_state_result']!=0),'frozen controller state result')
        require(isinstance(c['case_identity_hex'],str) and re.fullmatch('[0-9a-f]+',c['case_identity_hex']) and len(c['case_identity_hex'])%2==0 and len(c['case_identity_hex'])<=2*CASE_CAP,'canonical bounded case identity')
        require(isinstance(c['protocol_sha256'],list) and len(c['protocol_sha256'])==3,'three frozen actual protocol hashes')
        for value in c['protocol_sha256']:sha(value)
        require(re.fullmatch('[0-9a-f]{32}',c['selected_cid_hex']) and c['selected_cid_hex']!='0'*32,'selected CID identity')
        require(c['controller_cid_hex'] is None or (re.fullmatch('[0-9a-f]{32}',c['controller_cid_hex']) and c['controller_cid_hex']!='0'*32),'controller CID identity')
        for key in ('manifest_sha256','expected_manifest_digest','expected_evidence_digest','module_sha256'):sha(c[key])
        require(type(c['receipt_cap']) is int and 0<c['receipt_cap']<=FRAME_CAP,'bounded receipt cap')
        require(isinstance(c['timeout'],(float,int)) and not isinstance(c['timeout'],bool) and math.isfinite(c['timeout']) and c['timeout']>0,'finite child timeout')
        require(type(c['module_entry_cap']) is int and 0<c['module_entry_cap']<=250000 and type(c['module_byte_cap']) is int and 0<c['module_byte_cap']<=2**31,'module identity bounds')
        driver,manifest,module,out,logs=map(owned,(c['driver'],c['manifest'],c['module_root'],c['output'],commands.out));cwd=owned(c['cwd'])
        require(cwd.is_dir() and module.is_dir() and not os.path.lexists(out),'existing owned cwd/module and fresh output root')
        require(all(out!=p and not out.is_relative_to(p) and not p.is_relative_to(out) for p in (module,logs,cwd,manifest,driver)),'independent process output root')
        runtime=c['runtime_hashes'];require(str(driver) in runtime,'frozen exact child driver')
        require(str(Path(__file__).resolve()) in c['helper_hashes'],'frozen parent helper required')
        require(all(c['helper_hashes'][p]==runtime[p] for p in set(c['helper_hashes'])&set(runtime)),'conflicting runtime/helper identity')
        script=c.get('driver_script');require(script is None or allow_script,'public supervisor requires native driver')
        prefix=[str(driver)]
        if script is not None:
            script=owned(script);require(str(script) in runtime,'frozen inert script');prefix.append(str(script))
        if allow_script or allow_fixture:
            require(re.fullmatch('[a-z0-9-]{1,64}',c['driver_mode']),'frozen inert fixture mode')
            driver_argument=c['driver_mode']
        else:
            require('driver_mode' not in c or c['driver_mode']==c['manifest_sha256'],'public driver argument must be exact request SHA256')
            driver_argument=c['manifest_sha256']
        env=c['environment'];env_hash=environment_digest(env)
        module_path=owned(module/c['module_relative']);require(module_path.is_relative_to(module) and str(module_path.relative_to(module))==c['module_relative'],'owned module relative path')
        require(c['module_hashes'].get(c['module_relative'])==c['module_sha256'],'actual module hash required')
        for group in c['inputs']:
            root=owned(group['root']);require(out!=root and not out.is_relative_to(root) and not root.is_relative_to(out),'output/input independence')
        module_stat=None
        def audit():
            commands.remaining();require(capture_files(manifest.parent,[manifest.name])[manifest.name]==c['manifest_sha256'],'driver manifest changed')
            for path,wanted in {**runtime,**c['helper_hashes']}.items():
                p=owned(path);require(capture_files(p.parent,[p.name])[p.name]==wanted,'frozen process runtime changed')
            for group in c['inputs']:require(capture_files(owned(group['root']),list(group['sha256']))==group['sha256'],'frozen process input changed')
            files=[];dirs=[];modes={'.':stat.S_IMODE(module.stat().st_mode)};size=0
            for p in bounded_entries(module,limit=c['module_entry_cap']):
                commands.remaining();s=p.lstat();require(stat.S_ISDIR(s.st_mode) or stat.S_ISREG(s.st_mode),'module special/symlink');name=p.relative_to(module).as_posix();modes[name]=stat.S_IMODE(s.st_mode)
                if stat.S_ISDIR(s.st_mode):dirs.append(name)
                else:require(s.st_nlink==1,'module hardlink');size+=s.st_size;require(size<=c['module_byte_cap'],'module byte cap');files.append(name)
            require(set(files)==set(c['module_hashes']) and sorted(dirs)==c['module_directories'] and modes==c['module_modes'],'complete module inventory/mode changed')
            require(capture_files(module,files)==c['module_hashes'],'actual module bytes changed')
            if module_stat is not None:
                current=module_path.stat();require((current.st_dev,current.st_ino)==(module_stat.st_dev,module_stat.st_ino),'module inode replaced')
            commands.remaining()
        audit();
        if not allow_script and not allow_fixture:_request_preselection(c,manifest,commands)
        module_stat=module_path.stat();out.mkdir(mode=0o700,parents=True);created=True;nonce=secrets.token_hex(32);receipt=out/'child.receipt'
        common={k:c[k] for k in ('driver','driver_script','driver_mode','manifest','manifest_sha256','profile','schema','history_schema','arm','case_identity_hex','protocol_sha256','selected_cid_hex','controller_cid_hex','controller_query_policy','controller_query_result','controller_query_pointer_present','controller_state_policy','controller_state_result','runtime_hashes','helper_hashes','inputs','module_root','module_relative','module_hashes','module_directories','module_modes','expected_manifest_digest','expected_evidence_digest','module_sha256','receipt_cap','module_entry_cap','module_byte_cap','timeout','cwd') if k in c}
        common['driver_request_argument']=driver_argument;common['environment_sha256']=env_hash;contract_hash=digest(json.dumps(common,sort_keys=True,separators=(',',':')).encode());argv=[*prefix,str(manifest),driver_argument,nonce,str(receipt)]
        require(list(out.iterdir())==[],'output root not empty before child')
        audit();stdout,stderr=commands.run('process-lifetime-'+arm,argv,cwd=cwd,timeout=c['timeout'],env=env,replace_env=True,require_group_empty=True)
        row=commands.ledger[-1]
        require(row.get('argv')==argv and row.get('executed_argv')==argv and row.get('cwd')==str(cwd) and row.get('exit')==0 and row.get('owned_process_group_empty') is True and row.get('replace_environment') is True and row.get('environment_sha256')==env_hash and type(row.get('pid')) is int and row['pid']>0 and not row.get('error'),'actual one-child process ledger binding')
        audit();require({p.name for p in out.iterdir()}=={'child.receipt'},'missing/extra child output');data,stamp=_read_receipt(receipt,c['receipt_cap'],commands)
        evidence=_parse(data,c,nonce,row['pid'],module_stat);payload=evidence.pop('payload')
        with (out/'A-D.payload').open('xb') as stream:
            for pos in range(0,len(payload),1024*1024):commands.remaining();stream.write(payload[pos:pos+1024*1024])
        require(capture_files(out,['A-D.payload'])['A-D.payload']==evidence['payload_sha256'],'retained full A-D payload changed')
        audit();again,current=_read_receipt(receipt,c['receipt_cap'],commands);require(current==stamp and digest(again)==digest(data),'receipt mutated after admission')
        commands.remaining();result={**evidence,'receipt_sha256':digest(data),'nonce':nonce,'contract_sha256':contract_hash,'frozen_contract':common,'process_completed':True,'process_exit':0,'actual_pid':row['pid'],'owned_process_group_empty':True,'actual_command':dict(row),'log_sha256':{Path(p).name:capture_files(Path(p).parent,[Path(p).name])[Path(p).name] for p in (stdout,stderr)},'full_payload_path':str(out/'A-D.payload'),'receipt_path':str(receipt),'production_qualified':False,'E_transfer_authority':False,'native_private_producer_binding_required':True}
        (out/'process-result.json').write_text(json.dumps(result,indent=2)+'\n');commands.remaining();return result
    except BaseException as error:
        commands.terminal_failure=True;commands.ledger.append({'label':str(arm)+'-process-lifetime-failure','error':repr(error),'terminal_failure':True});commands.persist()
        if created:
            if (out/'process-result.json').exists():(out/'process-result.json').rename(out/'process-result-incomplete.json')
            (out/'process-failure.json').write_text(json.dumps({'error':repr(error),'production_qualified':False,'E_transfer_authority':False})+'\n')
        raise


def process_lifetime_arm(contract,commands):return _process_lifetime(contract,commands)
