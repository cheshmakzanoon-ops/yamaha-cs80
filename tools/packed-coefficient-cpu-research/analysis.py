"""Prospective reducer/schedule. Pure analysis, never launches measurements."""
import array,csv,hashlib,json,math,struct,sys
from pcm_store import read_pcm
from pathlib import Path

def require(ok,why):
    if not ok: raise ValueError(why)

from protocol_math import schedule, decision

def expected_events(row):
    require(row in range(7),'row outside roster')
    result=[]
    for k in range(11250):
        phase=k%750;value=(phase if phase<=375 else 750-phase)/375 if row==4 else 0.0
        bits=struct.unpack('<I',struct.pack('<f',value))[0]
        result.append({'callback':str(k),'frame':str(k*256),'noteons':str(8 if row==5 and k%1125==0 else 0),'noteoffs':str(8 if row==5 and k%1125==375 else 0),'pressure_event':str(int(row==4)),'pressure_bits':str(bits)})
    return result

def validate_events(path,row):
    with Path(path).open() as f: actual=list(csv.DictReader(f))
    require(actual==expected_events(row),'independent event table mismatch')

def validate_receipt(path,item,*,root=None,phase="timed"):
    path=Path(path);r=json.loads((path/'result.json').read_text())
    require(phase in ('timed','dry'),'invalid receipt phase')
    require(r['phase']==phase and r['arm']==item['arm'] and r['row']==item['row'],'receipt identity')
    require(r['callbacks']==11250 and r['frames']==2880000 and r['warmup_callbacks']==375 and r['warmup_frames']==96000,'duration/callback mismatch')
    require(r['factor']==2,'factor mismatch')
    require(r['selector']==[0,4,7,10,4,4,4][item['row']] and r['rate']==48000 and r['block']==256,'selector/rate/block mismatch')
    require(r['latency']==(63 if item['row']==6 else 67),'latency mismatch')
    require(type(r['batch_kernel']) is bool,'backend telemetry missing')
    require(type(r['mxcsr_initial']) is int and type(r['mxcsr_final']) is int and (r['mxcsr_initial'] & ~0x3f)==0x9fc0 and (r['mxcsr_final'] & ~0x3f)==0x9fc0,'FP control state mismatch')
    require(item['arm'] in ('stock','candidate') and item['row'] in range(7),'arm/row outside roster')
    require(r['render_policy']==(0 if item['row']==6 else 1),'render policy mismatch')
    validate_parameters(path/'parameters.csv',item['row'])
    validate_events(path/'events.csv',item['row'])
    require(r['finite'] is True and type(r['cpu_ns']) is int and (r['cpu_ns']>0 if phase=='timed' else r['cpu_ns']==0),'invalid CPU/output')
    require((r['measured_noteons'],r['measured_noteoffs'],r['pressure_events'])==((80,80,0) if item['row']==5 else (0,0,11250) if item['row']==4 else (0,0,0)),'event counts')
    with (path/'callbacks.csv').open() as f: callbacks=list(csv.DictReader(f))
    require(len(callbacks)==11250 and [int(x['callback']) for x in callbacks]==list(range(11250)),'callback inventory')
    require(sum(int(x['cpu_ns']) for x in callbacks)==r['cpu_ns'] and all(int(x['cpu_ns'])>=0 for x in callbacks),'CPU total accounting')
    data=read_pcm(path,root);require(len(data)==2880000*2*4,'exact PCM bytes')
    values=array.array('f');values.frombytes(data)
    if sys.byteorder!='little':values.byteswap()
    require(all(math.isfinite(x) for x in values),'independent nonfinite PCM rejection')
    return r,hashlib.sha256(data).hexdigest()

# Orchestrator must call validation and terminal gate AFTER each completedphase,
# never collect candidate costs before the AA gate.


def validate_completed_prefix(paths,*,root=None):
    """Cross-repeat exactPCM, stock/off identity, inputs, and ratio receipts. No execution."""
    plan=schedule();require(len(paths) in (42,84),'wholephase prefix required')
    pcm={};inputs={};times={};receipts=[];backend=None
    for path,item in zip(paths,plan):
        r,digest=validate_receipt(path,item,root=root);key=(item['arm'],item['row'])
        require(backend is None or backend==r['batch_kernel'],'backend changed across runs');backend=r['batch_kernel']
        require(key not in pcm or pcm[key]==digest,'withinarm nondeterministic PCM')
        pcm[key]=digest
        if item['arm']=='candidate':require(pcm.get(('stock',item['row']))==digest,'stock/candidate PCM differs')
        path=Path(path)
        keys=tuple(hashlib.sha256((path/n).read_bytes()).hexdigest() for n in ('events.csv','parameters.csv'))
        require(item['row'] not in inputs or inputs[item['row']]==keys,'crossarm event/parameter inputs differ')
        inputs[item['row']]=keys;times[(item['phase'],item['round'],item['row'],item['role'])]=r['cpu_ns'];receipts.append({'schedule':item,'result':r,'pcm_sha256':digest})
    matrices=[]
    for phase in ('aa','candidate')[:len(paths)//42]:
        matrices.append([[times[(phase,round,row,'B')]/times[(phase,round,row,'A')] for row in range(7)] for round in range(3)])
    return {'decision':decision(*matrices),'receipts':receipts}


def verify_source_contract(driver,stock_benchmark,baseline_params,candidate_params):
    def blob(data):return hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
    stock=Path(stock_benchmark).read_bytes();params=Path(baseline_params).read_bytes()
    require(blob(stock)=='6ca2b8268515f05ee01e1418c3a35012e366de1d','stock benchmark source identity')
    require(blob(params)=='5eb632c4b72abbecec0cc31c65ec0de04eada255' and Path(candidate_params).read_bytes()==params,'unchanged pinned parameter definitions')
    text=stock.decode();function=text[text.index('ControlFrame makeFrame('):text.index('\ndouble quantile(')]
    require(Path(driver).read_text().count(function)==1,'copied frame builder differs from pinned stock')
    return {'stock_benchmark_blob':blob(stock),'parameter_definitions_blob':blob(params),'frame_builder_sha256':hashlib.sha256(function.encode()).hexdigest()}


def validate_parameters(path,row):
    require(row in range(7),'row outside roster')
    oracle=Path(__file__).with_name('parameter-oracle')/f'row-{row}.csv'
    hashes=['2a631c6abaafa192fca8f76f40e8c9d4e154d6327c862528ed9c19fdcedd4040', 'd457faa3d434bd6007caa5afb7e5a4e30bf95ac868dd957f6a58337b516bbebe', '98bc4fa4c2eb5cec361439dee27f8d8682ffe3c04eaa19d179e76eeb9db411a8', 'a48773dfc014e6631fe73b3ffa82c401e3e86d6aa23c6c70edf113a191e8086a', 'd457faa3d434bd6007caa5afb7e5a4e30bf95ac868dd957f6a58337b516bbebe', 'd457faa3d434bd6007caa5afb7e5a4e30bf95ac868dd957f6a58337b516bbebe', '13b7a4edf8c14188955502ceac98cd2ea96deef038f2d00fb5a04473aa7cba7a']
    require(hashlib.sha256(oracle.read_bytes()).hexdigest()==hashes[row],'frozen parameter oracle modified')
    with oracle.open() as f:expected=list(csv.DictReader(f))
    with Path(path).open() as f:actual=list(csv.DictReader(f))
    require(actual==expected,'independent frozen parameter table mismatch')
