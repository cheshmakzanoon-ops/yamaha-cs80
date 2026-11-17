"""Validate exported timing arrays/statistics without loading a plugin."""
import argparse,gzip,hashlib,io,json,math,pathlib,statistics
from identity import require
from timing_study_decode import paired_summary,source_protocols
ROOT=pathlib.Path(__file__).parent
CAP=32*1024*1024
H=lambda b:hashlib.sha256(b).hexdigest()
def read(path,cap):
    require(not path.is_symlink() and path.is_file() and path.stat().st_size<=cap,'bounded regular evidence')
    with path.open('rb') as f:b=f.read(cap+1)
    require(len(b)<=cap,'evidence grew');return b

def load(root):
    root=pathlib.Path(root);manifest=json.loads(read(root/'manifest.json',65536));require(manifest['schema']=='cs80-paired-plugin-timing-public-manifest-v1','manifest schema')
    packed=read(root/'campaign.json.gz',8*1024*1024);require(len(packed)==manifest['compressed_bytes'] and H(packed)==manifest['compressed_sha256'],'compressed integrity')
    with gzip.GzipFile(fileobj=io.BytesIO(packed)) as f:raw=f.read(CAP+1)
    require(len(raw)<=CAP and len(raw)==manifest['uncompressed_bytes'] and H(raw)==manifest['uncompressed_sha256'],'expanded integrity')
    require(all(p not in raw for p in (b'/workspace/',b'/home/agent',b'/tmp/')),'private path in public export')
    require(H(read(ROOT/'timing_study_decode.py',1024*1024))==manifest['statistics_helper_sha256'],'frozen statistics helper changed')
    data=json.loads(raw);return data,manifest

def same(a,b):
    if isinstance(a,float) and isinstance(b,(int,float)):return math.isclose(a,b,rel_tol=1e-12,abs_tol=1e-15)
    if type(a)!=type(b):return False
    if isinstance(a,dict):return a.keys()==b.keys() and all(same(a[k],b[k]) for k in a)
    if isinstance(a,list):return len(a)==len(b) and all(same(x,y) for x,y in zip(a,b))
    return a==b

def validate(data):
    require(data['schema']=='cs80-paired-plugin-timing-public-export-v1' and data['export_kind']=='sanitized-portable-statistical-export','public timing schema')
    scope=data['scope'];require(all(scope[k] is False for k in ('production_qualified','gain_claim_supported','candidate_promoted','raw_receipts_exported','raw_pcm_samples_exported','independent_native_execution_attestation','runtime_closure_complete')),'nonqualification export scope')
    records=data['records'];get=lambda name:records[name]['data'];frozen=get('campaign-v2/frozen-plan.json');plan=get('campaign-v2/study-plan.json')
    require(records['campaign-v2/study-plan.json']['original_sha256']==frozen['plan_sha256'] and records['timing-host-v2/overlay-manifest.json']['original_sha256']==frozen['overlay_manifest_sha256'],'frozen plan/overlay identity')
    require(get('timing-host-v2/build-evidence.json')['observer']==frozen['driver_sha256'] and get('timing-host-v2/build-evidence.json')['expected-probe']==frozen['probe_sha256'],'frozen driver/probe identity')
    require(plan['process_wall_cap_seconds']==25 and plan['subtract_clock_overhead'] is False and plan['aa_blocks_per_policy']==2 and plan['comparison_blocks_per_policy']==6 and plan['measured_frames']==524288,'fixed study bounds')
    for arm in ('stock','candidate'):
        for policy in range(3):
            contract=get(f'campaign-v2/{arm}-{policy}.contract.json');protocols=source_protocols(bytes.fromhex(contract['case_identity_hex']))
            require(H(protocols[0])==contract['protocol_sha256'][2] and H(protocols[4])==contract['warmup_protocol_sha256'] and H(protocols[5])==contract['measured_protocol_sha256'],'independent source protocol identity')
    schedule=[]
    for family,count in (('aa',2),('comparison',6)):
        for block in range(count):
            for policy in range(3):
                for slot,label in enumerate('ABBA'):schedule.append({'family':family,'block':block,'policy':policy,'slot':slot,'label':label,'arm':'stock' if family=='aa' or label=='A' else 'candidate'})
    require(frozen['schedule']==schedule and len(data['runs'])==96,'exact frozen 96-slot schedule')
    nonces=set();pcm={};blockrows={};total_cpu_samples=total_clock_samples=receipt_bytes=0;wall=[];cpu=[]
    for index,(run,slot) in enumerate(zip(data['runs'],schedule)):
        require(run['index']==index,'run ordering');e=get(run['execution_record']);d=get(run['decoded_record']);require(all(d[k]==v for k,v in slot.items()),'decoded schedule mismatch')
        require(e['returncode']==0 and e['watchdog_reason'] is None and e['group_empty'] is True and 0<e['wall_seconds']<=25,'process exit/watchdog/group bound')
        require(e['environment']==frozen['environment'] and e['nonce']==e['argv'][3] and e['nonce'] not in nonces,'exact environment/fresh nonce');nonces.add(e['nonce']);wall.append(e['wall_seconds'])
        require(e['argv'][2]==frozen['requests'][f"{slot['arm']}-{slot['policy']}"] and run['raw_receipt']['sha256']==e['files']['receipt']['sha256'],'request/receipt identity')
        require('provider_objects' not in d and records[run['decoded_record']]['omitted_fields']==['provider_objects'],'provider path omission declared')
        require(d['production_qualified'] is False and d['clock_subtracted'] is False and d['exact_cross_run_pcm_match'] is True,'diagnostic timing semantics')
        require(len(d['timing_segments'])==6,'all six segments retained');contract=get(f"campaign-v2/{slot['arm']}-{slot['policy']}.contract.json")
        require(contract['module_sha256']==frozen['module_sha256'][slot['arm']],'module identity')
        totals=[];clocks=[];hashes=[]
        for position,(segment,frames) in enumerate(zip(d['timing_segments'],[4096]*4+[65536,524288])):
            samples=segment['callback_cpu_ns'];empty=segment['clock_pair_ns'];count=frames//64
            require(segment['frames']==frames and segment['callback_count']==count and len(samples)==count and all(type(v) is int and 0<v<=25_000_000_000 for v in samples),'complete CPU callback array')
            require(len(empty)==128 and all(type(v) is int and 0<=v<=25_000_000_000 for v in empty),'complete empty-clock array')
            total=sum(samples);require(total==segment['callback_cpu_total_ns'],'callback integer total')
            require(segment['pcm_bytes']==frames*8 and math.isfinite(segment['pcm_rms']) and segment['pcm_rms']>0 and math.isfinite(segment['pcm_peak']) and segment['pcm_peak']>0 and 0<segment['pcm_nonzero_count']<=frames*2,'recorded PCM quality')
            if position==5:require(segment['nonzero_callback_fraction']==1 and segment['minimum_callback_rms']>0,'every measured callback nonzero')
            expected_protocol=contract['protocol_sha256'][2] if position<4 else contract['warmup_protocol_sha256'] if position==4 else contract['measured_protocol_sha256'];require(segment['protocol_sha256']==expected_protocol and segment['render_sha256']==d['render_sha256'][position],'PCM/render protocol identity')
            hashes.append(segment['pcm_sha256']);totals.append(total);clocks.append(sum(empty));total_cpu_samples+=count;total_clock_samples+=128
        require(sum(totals)==run['process_cpu_callback_total_ns'] and sum(clocks)==run['process_empty_clock_total_ns'] and sum(totals)+sum(clocks)<=25_000_000_000,'per-process CPU totals/bound')
        cpu.append(totals[-1]);receipt_bytes+=run['raw_receipt']['bytes'];measured=d['timing_segments'][5]
        require(d['measured_cpu_ns_per_frame']==totals[-1]/524288 and d['measured_clock_overhead_fraction']==statistics.mean(measured['clock_pair_ns'])*8192/totals[-1],'derived measured metrics')
        if slot['policy'] in pcm:require(pcm[slot['policy']]==hashes,'all six PCM hashes across same-policy runs')
        else:pcm[slot['policy']]=hashes
        blockrows.setdefault((slot['family'],slot['block'],slot['policy']),[]).append((index,d['measured_cpu_ns_per_frame'],d['measured_clock_overhead_fraction']))
    require(len(data['blocks'])==24 and len(blockrows)==24 and {(b['family'],b['block'],b['policy']) for b in data['blocks']}==set(blockrows),'complete unique independent block count')
    for b in data['blocks']:
        rows=blockrows[(b['family'],b['block'],b['policy'])];values=[r[1] for r in rows];overheads=[r[2] for r in rows];log=(math.log(values[0])+math.log(values[3])-math.log(values[1])-math.log(values[2]))/2
        require(b['run_indices']==[r[0] for r in rows] and b['cpu_per_frame']==values and b['overhead_fraction']==overheads and same(b['log_speedup'],log) and same(b['gain_fraction'],1-math.exp(-log)),'whole-block statistics')
    summary={}
    for policy in range(3):
        blocks=[b for b in data['blocks'] if b['policy']==policy and b['family']=='comparison'];aa=[b for b in data['blocks'] if b['policy']==policy and b['family']=='aa'];summary[str(policy)]=paired_summary(blocks,aa)
        require(same(summary[str(policy)],get('campaign-v2/summary.json')[str(policy)]) and summary[str(policy)]['gain_claim_supported'] is False,'recomputed negative/inconclusive bootstrap result')
    source=[get(f'sealed-{arm}/source-manifest.json') for arm in ('stock','candidate')];objects=[get(f'sealed-{arm}/objects.json') for arm in ('stock','candidate')]
    require(source[0].keys()==source[1].keys() and [k for k in source[0] if source[0][k]!=source[1][k]]==['src/core/Ig00156Batch.cpp'],'single changed source manifest')
    require(objects[0].keys()==objects[1].keys() and [k for k in objects[0] if objects[0][k]!=objects[1][k]]==['CMakeFiles/cs80_core.dir/src/core/Ig00156Batch.cpp.o'],'single changed object manifest')
    sealed=[get(f'sealed-{arm}/files-sha256.json') for arm in ('stock','candidate')]
    for name in get('pair-verification.json')['identical_build_contract_files']:require(sealed[0][name]==sealed[1][name],'paired build-contract hashes')
    prior=get('campaign-v1/000-aa-0-0-0-stock/execution.json');failure=get('campaign-v1/000-aa-0-0-0-stock/rejected-metadata.json');require(prior['returncode']==24 and failure['status']=='rejected' and failure['history_operation']=='steady-measured' and failure['primary_error']=='owned host pump failed','retained rejected v1 failure')
    require(get('attempt-history.json')['attempts'][0]['actual_runloop_failure_enum']=='not captured','v1 inferred cause not overstated')
    return {'run_count':96,'independent_blocks':24,'callback_cpu_samples':total_cpu_samples,'empty_clock_samples':total_clock_samples,'original_receipt_bytes_local':receipt_bytes,'wall_seconds_range':[min(wall),max(wall)],'measured_cpu_seconds_range':[min(cpu)/1e9,max(cpu)/1e9],'same_policy_recorded_PCM_hashes_match':True,'summary':summary,'credible_gain_demonstrated':False,'candidate_promoted':False,'production_qualified':False,'native_execution_independently_reproduced':False,'complete_runtime_closure':False}

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--evidence',type=pathlib.Path,default=ROOT/'evidence'/'paired-plugin-timing');args=p.parse_args();data,manifest=load(args.evidence);print(json.dumps(validate(data),indent=2))
