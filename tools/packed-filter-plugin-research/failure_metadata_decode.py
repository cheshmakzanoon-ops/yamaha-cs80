"""Bounded read-only rejected-run diagnostics; never success or qualification."""
import hashlib,math,re,struct
MAX_METADATA=16*1024**2
MAX_BODY=MAX_METADATA+4096
MAX_PARAMETERS=16384

def require(ok,message):
    if not ok:raise ValueError(message)
def sha(data):return hashlib.sha256(data).hexdigest()
def digest(value):
    require(isinstance(value,str) and re.fullmatch('[0-9a-f]{64}',value),'canonical SHA256 required');return value
class Reader:
    def __init__(self,data):self.data=memoryview(data);self.pos=0
    def raw(self,n):
        require(type(n) is int and 0<=n<=len(self.data)-self.pos,'truncated bounded diagnostic');out=self.data[self.pos:self.pos+n];self.pos+=n;return out.tobytes()
    def u32(self):return struct.unpack('<I',self.raw(4))[0]
    def u64(self):return struct.unpack('<Q',self.raw(8))[0]
    def blob(self,cap):
        n=self.u64();require(n<=cap,'diagnostic field cap');return self.raw(n)
    def text(self,cap=4096):return self.blob(cap).decode('utf-8',errors='strict')
    def f64(self):
        raw=self.raw(8);v=struct.unpack('<d',raw)[0];require(math.isfinite(v),'nonfinite diagnostic parameter');return v,raw.hex()
    def utf16(self):
        n=self.u64();require(n<=127,'String128 length cap');raw=self.raw(n*2);units=list(struct.unpack('<'+'H'*n,raw));require(0 not in units,'embedded terminated-string NUL');return {'text':raw.decode('utf-16-le',errors='backslashreplace'),'utf16le_hex':raw.hex(),'code_units':units}
    def end(self):require(self.pos==len(self.data),'trailing diagnostic bytes')
def signed(v):return v if v<2**31 else v-2**32

def connected_metadata(data):
    require(isinstance(data,bytes) and 0<len(data)<=MAX_METADATA,'complete bounded metadata bytes required')
    r=Reader(data);require(r.text(64)=='native-observer-connected-v1' and r.text(64)=='native-observer-created-identity-v1','Connected metadata schema')
    component=r.raw(16).hex();require(component!='0'*32,'nonzero component CID');kind=r.u32();require(kind in (0,1),'controller kind');controller=r.raw(16).hex() if kind else None
    require(controller!='0'*32,'nonzero separate CID');buses=[]
    for media,direction in ((0,0),(0,1),(1,0),(1,1)):
        require((r.u32(),r.u32())==(media,direction),'bus inventory section order');count=r.u32();require(count<=1024,'bus count cap');rows=[]
        for index in range(count):
            actual_media,actual_direction,channels=r.u32(),r.u32(),r.u32();require((actual_media,actual_direction)==(media,direction),'bus metadata identity mismatch');name=r.utf16();bus_type,flags=r.u32(),r.u32();arrangement=r.u64() if media==0 else None
            rows.append({'index':index,'media':media,'direction':direction,'channel_count_raw':channels,'channel_count':signed(channels),'name':name,'bus_type_raw':bus_type,'flags_raw':flags,'speaker_arrangement':arrangement})
        buses.append({'media':media,'direction':direction,'count':count,'buses':rows})
    count=r.u32();require(0<count<=MAX_PARAMETERS and count<=(len(data)-r.pos)//56,'bounded parameter count');parameters=[];ids=set()
    for index in range(count):
        id=r.u32();require(id!=0xffffffff and id not in ids,'unique non-sentinel parameter ID');ids.add(id);title,short_title,units=r.utf16(),r.utf16(),r.utf16();steps=r.u32();default,default_bits=r.f64();unit,flags=r.u32(),r.u32();current,current_bits=r.f64();require(0<=default<=1 and 0<=current<=1,'normalized parameter range')
        parameters.append({'index':index,'id':id,'id_hex':f'{id:08x}','title':title,'short_title':short_title,'units':units,'step_count_raw':steps,'step_count':signed(steps),'default_normalized':default,'default_f64le_hex':default_bits,'unit_id_raw':unit,'unit_id':signed(unit),'flags_raw':flags,'current_normalized':current,'current_f64le_hex':current_bits})
    r.end();return {'schema':'native-observer-connected-v1','component_cid_hex':component,'controller_cid_hex':controller,'buses':buses,'parameter_count':count,'parameters':parameters,'metadata_sha256':sha(data),'metadata_bytes':len(data),'admitted':False}

def decode_failure(data,*,expected,observed_exit,observed_pid):
    """Caller must independently authenticate argv/nonce/PID/exit/input provenance."""
    require(observed_exit==24 and type(observed_exit) is int,'observed rejected exit24 required');require(type(observed_pid) is int and observed_pid>0,'actual child PID required');require(isinstance(data,bytes) and len(data)<=MAX_BODY+80,'failure receipt cap')
    f=Reader(data);n=f.u64();require(0<n<=MAX_BODY,'failure body cap');body=f.raw(n);claimed=digest(f.text(64));f.end();require(sha(body)==claimed,'failure frame digest mismatch')
    r=Reader(body);require(r.text(64)=='cs80-process-lifetime-failure-metadata-v1' and r.text(32)=='rejected' and r.u32()==24,'nonadmitting failure schema/status');nonce=r.text(64);require(nonce==expected['nonce'] and re.fullmatch('[0-9a-f]{64}',nonce),'failure nonce mismatch');pid=r.u64();require(pid==observed_pid,'failure PID mismatch');arm=r.text(16);require(arm==expected['arm'] and arm in ('stock','candidate'),'failure arm mismatch')
    identities={}
    for name in ('manifest_digest','evidence_digest','module_sha256'):
        identities[name]=digest(r.text(64));require(identities[name]==expected[name],'failure preload identity mismatch: '+name)
    require(r.text(64)=='preload-identities-not-postfailure-verification','failure identity scope required')
    outer,module,operation,primary=[r.text(1024) for _ in range(4)];metadata_hash=digest(r.text(64));metadata=r.blob(MAX_METADATA);r.end();require(sha(metadata)==metadata_hash,'failure metadata digest mismatch');decoded=connected_metadata(metadata)
    return {'schema':'cs80-process-lifetime-failure-metadata-v1','status':'rejected','admitted':False,'production_qualified':False,'observed_exit':24,'actual_pid':pid,'nonce':nonce,'arm':arm,**identities,'identity_scope':'preload-identities-not-postfailure-verification','outer_step':outer,'module_step':module,'history_operation':operation,'primary_error':primary,'body_sha256':claimed,'receipt_sha256':sha(data),'connected':decoded,'postfailure_identity_verified':False,'parent_execution_authentication_required':True}

PINNED_WRAPPER_SHA256='16031325b31be2e32b23689f07338bd6c631941475b9bbf87d176d1f2565c202'
PINNED_MIDI_HEADER_SHA256='c51527d390b09d62e71be172908b81990f77c25920047f729cfc9b5c69f73e4b'
def classify_roster(decoded,generated_controls,memory_controls=None):
    """Read-only source-pattern comparison, never permission to write wrapper IDs."""
    require(isinstance(generated_controls,list) and generated_controls,'explicit generated workload roster required');generated={}
    for item in generated_controls:
        name,id=item['name'],item['id'];require(isinstance(name,str) and name and name.isascii() and type(id) is int and 0<=id<0x80000000,'generated ASCII control identity');hash=0
        for char in name:hash=(hash*31+ord(char))&0xffffffff
        require(id==(hash&0x7fffffff) and id not in generated,'generated control hash/collision');generated[id]=name
    memory={}
    for item in memory_controls or []:
        name,id=item['name'],item['id'];require(isinstance(name,str) and re.fullmatch(r'memory_[1-4]_[a-zA-Z0-9_]+',name) and type(id) is int,'source-derived memory identity');hash=0
        for char in name:hash=(hash*31+ord(char))&0xffffffff
        require(id==(hash&0x7fffffff) and id not in generated and id not in memory,'memory hash/collision');memory[id]=name
    rows=[];found=set();found_memory=set();counts={}
    for parameter in decoded['parameters']:
        id=parameter['id'];title=parameter['title']['text'];flags=parameter['flags_raw'];role='unclassified';detail={}
        if id in generated:role='generated_workload_control';detail['control_name']=generated[id];found.add(id)
        elif id in memory:role='source_memory_control';detail['control_name']=memory[id];found_memory.add(id)
        elif id==0x62797073 and title=='Bypass' and flags==65537 and parameter['step_count']==1 and parameter['default_normalized']==0:role='pinned_juce_wrapper_bypass_pattern'
        elif id==0x70727374 and title=='Program' and flags==32769 and parameter['unit_id']==0 and parameter['step_count']==522 and parameter['default_normalized']==0:role='pinned_juce_wrapper_program_pattern'
        elif 0x6d636d00<=id<0x6d636d00+16*130:
            channel,controller=divmod(id-0x6d636d00,130)
            if title==f'MIDI CC {channel}|{controller}' and flags==0 and parameter['step_count']==0 and parameter['unit_id']==0 and parameter['units']['text']=='' and parameter['short_title']['text']=='' and parameter['default_normalized']==0:
                role='pinned_juce_midi_emulation_pattern';detail.update(channel_zero_based=channel,controller_number=controller)
        counts[role]=counts.get(role,0)+1;rows.append({'index':parameter['index'],'id':id,'role':role,**detail})
    return {'admitted':False,'read_only':True,'write_permission':False,'source_pattern_only':True,'pinned_wrapper_sha256':PINNED_WRAPPER_SHA256,'pinned_midi_header_sha256':PINNED_MIDI_HEADER_SHA256,'generated_count':len(generated),'source_memory_count':len(memory),'missing_source_memory':[{'id':id,'name':name} for id,name in memory.items() if id not in found_memory],'observed_count':len(rows),'missing_generated':[{'id':id,'name':name} for id,name in generated.items() if id not in found],'role_counts':counts,'rows':rows}
