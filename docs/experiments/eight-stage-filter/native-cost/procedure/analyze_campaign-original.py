#!/usr/bin/env python3
"""Complete-campaign offline replay only. Refuses timed prefixes; no clocks/DSP."""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
from study_statistics import PLAN, analyze, need, isint, schedule_audit

ACTION='native-eight-average-cost-v1-96'
RECEIPT_KEYS={'schema','slot','argv','environment','initial_home_empty','host_sha256','module_sha256','module_stat','authorization_sha256','plan_sha256','freeze_sha256','pid','exit','error','wall_begin_ns','wall_end_ns','resource_user_ns','resource_system_ns','stdout','stderr','artifacts','integrity'}

def strict(text):
    def pairs(values):
        out={}
        for key,value in values:
            need(key not in out,'duplicate JSON key');out[key]=value
        return out
    return json.loads(text,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError('nonfinite '+x)))

def read(path):return strict(Path(path).read_text())
def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False)
def same(a,b):return canonical(a)==canonical(b)

def load_module(path,name):
    spec=importlib.util.spec_from_file_location(name,path);module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);return module

def check_freeze(root):
    root=Path(root).resolve();freeze=read(root/'freeze.json');plan=read(root/'plan.json')
    need(freeze['untimed_gate']['passed'] is True and freeze['untimed_gate']['host_measurement_clock_reads']==0,'untimed gate absent')
    need(same(plan['statistical_plan'],PLAN),'statistical plan differs from implemented blind contract')
    need(same(read(root/'offline/statistical-plan.json'),PLAN),'statistical-plan file differs')
    need(plan['statistical_plan_sha256']==sha(root/'offline/statistical-plan.json'),'statistical plan pin')
    need(plan['protocol_sha256']==sha(root/'PROTOCOL.md'),'protocol pin')
    need(plan['schedule_sha256']==sha(root/'schedule.json') and same(plan['schedule'],read(root/'schedule.json')),'schedule pin')
    generated=load_module(root/'schedule.py','frozen_eight_schedule').make_schedule()
    need(same(generated,plan['schedule']),'randomized schedule does not reproduce seed/generator')
    schedule_audit(plan['schedule'])
    need(freeze['plan_sha256']==sha(root/'plan.json'),'freeze plan binding')
    need(same(freeze['roles'],plan['roles']),'physical role bindings differ')
    need(plan['no_exclusions'] is True and plan['no_retries'] is True and plan['outcome_access_only_after_complete96'] is True,'one-shot policy changed')
    for rel,pin in freeze['files'].items():
        path=root/rel
        need(not Path(rel).is_absolute() and '..' not in Path(rel).parts and path.resolve().is_relative_to(root),'unsafe frozen relative path')
        need(path.is_file() and sha(path)==pin,'frozen file drift: '+rel)
    for key,role in freeze['roles'].items():
        p=Path(role['path']);st=p.stat()
        need(p.is_file() and sha(p)==role['sha256'],'physical artifact drift: '+key)
        need((st.st_dev,st.st_ino,st.st_size)==(role['device'],role['inode'],role['bytes']),'physical role stat drift: '+key)
    need(freeze['roles']['stock']['sha256']!=freeze['roles']['candidate']['sha256'],'C is not a distinct actual candidate')
    for path,pin in freeze['loader_files'].items():need(sha(path)==pin,'loader dependency drift')
    for path,target in freeze['loader_links'].items():need(str(Path(path).resolve())==target,'loader symlink drift')
    return plan,freeze

def check_terminal(terminal):
    need(terminal['schema']==1 and terminal['status']=='complete' and terminal['completed_slots']==96 and terminal['error'] is None,'incomplete/failed/interrupted trial; no prefix analysis')
    need(terminal['no_resume'] is True and terminal['analysis_performed'] is False,'collection analyzed or resumed')
    for k in ('wall_begin_ns','wall_end_ns','cpu_elapsed_ns'):need(isint(terminal[k],True),'invalid terminal resource')
    need(0<terminal['wall_end_ns']-terminal['wall_begin_ns']<=360_000_000_000 and terminal['cpu_elapsed_ns']<=240_000_000_000,'whole trial budget')

def check_ledger(ledger,terminal,receipts,pins,ph,fh,ah):
    check_terminal(terminal)
    need(len(receipts)==96 and len(ledger)==194,'missing/extra process or journal events')
    expected_start={'event':'start','action':ACTION,'authorization_sha256':ah,'plan_sha256':ph,'freeze_sha256':fh,'wall_ns':terminal['wall_begin_ns']}
    need(same(ledger[0],expected_start),'journal start identity')
    need(same(ledger[-1],{'event':'terminal',**terminal}),'journal terminal differs')
    last=terminal['wall_begin_ns'];pids=set();child_cpu=0
    for i,r in enumerate(receipts):
        need(type(r) is dict and set(r)==RECEIPT_KEYS,'receipt schema '+str(i))
        need(r['slot']['slot']==i,'receipt temporal slot order')
        need(r['integrity']=='PASS' and type(r['exit']) is int and r['exit']==0 and r['error'] is None and r['stderr']=='','unsuccessful process')
        need(isint(r['pid'],True) and r['pid'] not in pids,'not96 fresh processes');pids.add(r['pid'])
        need(all(isint(r[k]) for k in ('wall_begin_ns','wall_end_ns','resource_user_ns','resource_system_ns')),'noninteger process resource')
        need(last<=r['wall_begin_ns']<r['wall_end_ns']<=terminal['wall_end_ns'],'overlap/reversed process chronology')
        need(r['wall_end_ns']-r['wall_begin_ns']<=6_000_000_000,'child wall budget')
        cpu=r['resource_user_ns']+r['resource_system_ns'];need(cpu<=4_000_000_000,'child CPU budget');child_cpu+=cpu
        need(same(ledger[1+2*i],{'event':'slot-start','slot':i,'wall_ns':r['wall_begin_ns']}),'journal slot start')
        need(same(ledger[2+2*i],{'event':'slot-end','slot':i,'receipt_sha256':pins[i],'integrity':'PASS'}),'journal slot finish')
        last=r['wall_end_ns']
    need(child_cpu<=terminal['cpu_elapsed_ns'],'whole CPU omitted child work')


def load_campaign(root):
    root=Path(root).resolve();campaign=root/'campaign'
    # Complete terminal is examined before reading any process numerical outcome.
    terminal=read(campaign/'terminal.json');check_terminal(terminal)
    plan,freeze=check_freeze(root);ph,fh=sha(root/'plan.json'),sha(root/'freeze.json')
    auth=read(campaign/'authorization.json');ah=sha(campaign/'authorization.json')
    need(same(auth,{'action':ACTION,'plan_sha256':ph,'freeze_sha256':fh,'exclusive_approved':True}),'authorization does not bind final plan/freeze')
    need(same(read(root/'trial-consumed.json'),{'action':ACTION,'authorization_sha256':ah,'plan_sha256':ph,'freeze_sha256':fh,'one_shot':True}),'one-shot latch')
    need(same(read(campaign/'plan.json'),plan) and same(read(campaign/'freeze.json'),freeze),'campaign frozen-copy mismatch')
    need({p.name for p in campaign.glob('slot-*.json')}=={f'slot-{i:02d}.json' for i in range(96)},'extra/missing attempt records')
    need({p.name for p in campaign.glob('data-*')}=={f'data-{i:02d}' for i in range(96)},'extra/missing data directories')
    need({p.name for p in campaign.glob('home-*')}=={f'home-{i:02d}' for i in range(96)},'extra/missing HOME directories')
    receipts=[read(campaign/f'slot-{i:02d}.json') for i in range(96)];pins=[sha(campaign/f'slot-{i:02d}.json') for i in range(96)]
    ledger=[strict(line) for line in (campaign/'ledger.jsonl').read_text().splitlines()]
    check_ledger(ledger,terminal,receipts,pins,ph,fh,ah)
    host=load_module(root/'host_contract.py','frozen_eight_host_contract');rows=[];wall=[]
    for i,(r,s) in enumerate(zip(receipts,plan['schedule']['slots'])):
        need(same(r['slot'],s),'schedule/role/count swap')
        role=freeze['roles'][s['module_role']]
        expected={'schema':1,'host_sha256':freeze['roles']['host']['sha256'],'module_sha256':role['sha256'],
            'module_stat':{k:role[k] for k in ('device','inode','bytes')},'authorization_sha256':ah,'plan_sha256':ph,'freeze_sha256':fh,'initial_home_empty':True}
        for key,value in expected.items():need(same(r[key],value),'receipt binding '+key)
        env={**freeze['loader_environment'],'HOME':str(campaign/f'home-{i:02d}'),'CS80_NATIVE_AVERAGE_AUTH':ah}
        need(same(r['environment'],env),'unfrozen loader environment')
        out=campaign/f'data-{i:02d}'
        need(same(r['argv'],host.command(freeze,s,out,ah)),'actual invocation differs')
        o=strict(r['stdout'])
        artifacts=host.validate(o,s,freeze,out_dir=out,auth=ah,pid=r['pid'],before=r['wall_begin_ns'],after=r['wall_end_ns'])
        need(same(artifacts,r['artifacts']),'raw artifact hashes/bytes differ from receipt')
        need(o['cpu_ns_sum']<=r['resource_user_ns']+r['resource_system_ns'],'measured CPU exceeds process CPU')
        rows.append({**s,'cpu_ns':o['cpu_ns_sum'],'pid':r['pid'],'module_sha256':r['module_sha256'],'integrity_pass':True})
        wall.append({'slot':i,'wall_ns_sum':o['wall_ns_sum'],'enclosing_wall_ns':o['enclosing_wall_ns'],
            'outer_process_wall_ns':r['wall_end_ns']-r['wall_begin_ns'],
            'resource_cpu_ns':r['resource_user_ns']+r['resource_system_ns'],
            'cpu_ns_per_baseline_callback':o['cpu_ns_sum']/512,
            'cpu_ns_per_baseline_frame':o['cpu_ns_sum']/(512*64),
            'rtf_cpu_baseline512':o['cpu_ns_sum']/(512*64/48000*1e9)})
    return rows,plan['schedule'],{'plan_sha256':ph,'freeze_sha256':fh,'authorization_sha256':ah,'terminal':terminal,
        'process_receipt_sha256':pins,'wall_and_absolute_totals':wall,'integrity_pass':True}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1]);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    try:
        rows,schedule,integrity=load_campaign(a.root);result=analyze(rows,schedule);result['integrity']=integrity
    except (OSError,ValueError,KeyError,TypeError,ArithmeticError) as error:
        result={'status':'NO ADMISSION: invalid or incomplete complete-campaign evidence','admission':False,'integrity_pass':False,'error':str(error),'scope':PLAN['scope']}
    with a.output.open('x') as f:json.dump(result,f,indent=2,sort_keys=True,allow_nan=False);f.write('\n')
    print(json.dumps({'status':result['status'],'admission':result['admission'],'output':str(a.output)}))
    return 0 if result['admission'] else 1

if __name__=='__main__':raise SystemExit(main())
