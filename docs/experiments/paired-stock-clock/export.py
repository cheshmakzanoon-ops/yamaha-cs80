#!/usr/bin/env python3
"""One completed stock-only trial export. Refuse live/partial data and unverifiable decisions."""
import argparse
from collections import Counter
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import shutil
import sys
sys.dont_write_bytecode=True

HERE=Path(__file__).resolve().parent
def require(ok,why):
    if not ok:raise ValueError(why)
def sha(data):return hashlib.sha256(data).hexdigest()
def digest(path):return sha(Path(path).read_bytes())
def load(path):return json.loads(Path(path).read_text())
def checked(path,expected):
    require(len(expected)==64 and digest(path)==expected,'unverified final input: '+Path(path).name)
    return load(path)
def write(path,data):
    with Path(path).open('xb')as f:f.write(data)
def compact(data):return json.dumps(data,sort_keys=True,separators=(',',':'),allow_nan=False).encode()
def jsonfile(path,data):write(path,json.dumps(data,indent=2,allow_nan=False).encode()+b'\n')

def export(args):
    q=args.root.resolve();campaign=q/'campaign'
    # No raw span, attempt, image or timing file is read until both final gates pass.
    terminal=checked(campaign/'terminal.json',args.terminal_sha)
    require(terminal.get('event')=='trial-end' and terminal.get('status')=='complete'
        and terminal.get('completed_pairs')==20 and terminal.get('failure') is None and terminal.get('no_resume') is True,
        'refuse live, failed or partial campaign')
    official=checked(args.decision,args.decision_sha)
    require(official.get('protocol')=='stock-paired-clock-v1' and len(official.get('processes',[]))==20
        and all(p['integrity_pass']for p in official['processes']) and official.get('no_retry_no_resume_no_replacement') is True,
        'verified complete official integrity decision required, even for a numerical failure')
    freeze=checked(q/'freeze.json',args.freeze_sha)
    plan=checked(q/'offline/plan.json',args.plan_sha)
    require(official['freeze_sha256']==args.freeze_sha and official['plan_sha256']==args.plan_sha,'official binding mismatch')
    for name in ['offline/contract.py','offline/analyze.py','offline/validate.py']:
        require(digest(q/name)==freeze['files'][name],'frozen official source changed')
    sys.path.insert(0,str(q/'offline'))
    import contract
    import validate
    from analyze import cohort_gates
    require(official['scope']==contract.SCOPE and plan['slots']==contract.schedule(),'not exact stock-only scope/schedule')
    # The official frozen validator is read-only; this is not a collector invocation.
    reproduced=validate.analyze_campaign(q,campaign,q/'offline/plan.json',q/'freeze.json')
    # Successful field-check order depends on the original validator's set
    # iteration/Python hash seed. Check the full arrays as multisets, not subsets.
    # All observed rows, effects, numerical gates, failures and other fields stay
    # exact and ordered; the pinned original decision bytes are never modified.
    require(Counter(map(compact,reproduced['checks']))==Counter(map(compact,official['checks'])),
            'official complete integrity/numerical check multiset differs')
    reproduced['checks']=official['checks']
    for observed,expected in zip(reproduced['processes'],official['processes']):
        require(Counter(map(compact,observed['integrity_checks']))==Counter(map(compact,expected['integrity_checks'])),
                'official per-process integrity check multiset differs')
        observed['integrity_checks']=expected['integrity_checks']
    require(reproduced==official,'official frozen decision did not reproduce exactly')
    out=args.output.resolve()
    require(out.is_relative_to(HERE) and not out.exists(),'output must be new and within publication workspace')
    out.mkdir(parents=True)
    for name in ['contract.py','analyze.py']:shutil.copyfile(q/'offline'/name,out/name)
    shutil.copyfile(HERE/'replay.py',out/'replay.py')
    # Import portable orchestration only after official completion/verification.
    import importlib.util
    spec=importlib.util.spec_from_file_location('portable_replay',HERE/'replay.py');api=importlib.util.module_from_spec(spec);spec.loader.exec_module(api)
    receipts=[];receipt_lookup={};groups={};slot_provenance=[];flat=[]
    wall_origin=terminal['wall_start_ns']-1
    gold=load(q/'expected-endpoints.json')['scenes']
    for process in official['processes']:
        slot=process['slot'];number=slot['slot'];folder=campaign/('slot-%02d'%number)
        raw=folder/'span-order.jsonl';data=raw.read_bytes();require(sha(data)==process['attempt']['rows_sha256'],'original raw digest changed')
        rows=[json.loads(line)for line in data.splitlines()];thread_origin=min(r['clocks']['thread_begin_ns']for r in rows if r['timed'])-1
        normalized=[]
        for original in rows:
            row=json.loads(json.dumps(original))
            clock=row['clocks']
            if clock is not None:
                for key in clock:clock[key]-=thread_origin if key.startswith('thread_')else wall_origin
            receipt={k:v for k,v in row.items()if k not in api.META+['clocks']}
            key=compact(receipt)
            if key not in receipt_lookup:receipt_lookup[key]=len(receipts);receipts.append(receipt)
            item=dict(slot=number,**{k:row[k]for k in api.META})
            for k in ['extra_enabled','timed']:item[k]=int(item[k])
            for k in contract.CLOCK_KEYS:item[k.replace('_ns','_relative_ns')]=clock[k]if clock else ''
            item['receipt']=receipt_lookup[key];flat.append(item);normalized.append(row)
        groups[number]=normalized
        attempt=process['attempt'];stdout=load(campaign/('stdout-%02d.json'%number))
        slot_provenance.append(dict(slot=number,original_rows_sha256=sha(data),original_rows=len(rows),
            timed_rows=sum(r['timed']for r in rows),raw_attempt_sha256=process['attempt_sha256'],
            raw_stdout_sha256=attempt['stdout_sha256'],raw_stderr_sha256=attempt['stderr_sha256'],
            raw_image_proof_sha256=digest(folder/'image-proof.json'),raw_loader_sha256=digest(folder/'loader.json'),
            host_sha256=attempt['host_sha256'],dso_a_sha256=attempt['dso_a_sha256'],dso_b_sha256=attempt['dso_b_sha256'],
            work_receipt=stdout,integrity_pass=process['integrity_pass'],integrity_check_count=len(process['integrity_checks']),
            integrity_checks_sha256=sha(compact(process['integrity_checks'])),
            process_wall_start_relative_ns=attempt['started_wall_ns']-wall_origin,
            process_wall_end_relative_ns=attempt['ended_wall_ns']-wall_origin,process_wall_elapsed_ns=attempt['wall_elapsed_ns'],
            resource_user_ns=attempt['resource_user_ns'],resource_system_ns=attempt['resource_system_ns']))
    csv_out=io.StringIO(newline='');writer=csv.DictWriter(csv_out,fieldnames=api.FIELDS,lineterminator='\n');writer.writeheader();writer.writerows(flat)
    csv_data=csv_out.getvalue().encode();write(out/'spans.csv',csv_data)
    # Retain only original immutable gold endpoints referenced by these actual rows.
    needed={s:set()for s in contract.SCENES}
    for slot in contract.schedule():
        for row in groups[slot['slot']]:
            for phase in ['before','after']:
                if row[phase] is not None:needed[slot['scene']].update([str(row[phase]['main_next']),str(row[phase]['shadow_next'])])
    subset={scene:{'frame_hash':gold[scene]['frame_hash'],'endpoints':{k:gold[scene]['endpoints'][k]for k in sorted(keys,key=int)}}for scene,keys in needed.items()}
    work=dict(receipts=receipts,gold=subset,original_complete_gold_sha256=digest(q/'expected-endpoints.json'))
    write(out/'work-receipts.json.gz',gzip.compress(compact(work),mtime=0))
    protocol=dict(schema='paired-stock-clock-portable-protocol-v1',scope=contract.SCOPE,limits=contract.LIMITS,
        slots=contract.schedule(),caller_fp=plan['caller_fp'],clock_order=plan['clock_order'],percentile=plan['percentile'],
        overhead=plan['overhead'],numeric_arithmetic=plan['numeric_arithmetic'],failure_policy=plan['failure_policy'],
        terminal={k:terminal[k]for k in ['status','completed_pairs','failure','no_resume','wall_elapsed_ns','total_cpu_ns']},
        official_decision_verified=True,official_all_process_integrity_pass=True,normalized_records_sha256=sha(contract.canonical(groups)),
        normalization='Each slot thread origin and the common trial wall origin removed by integer subtraction, first origin set to 1 ns. Every interval, order and within-domain gap preserved. Absolute origins are not exported.',
        raw_row_count=len(flat),timed_row_count=sum(r['timed']for r in flat),receipt_count=len(receipts))
    result=api.calculate(groups,protocol,work)
    for portable,original in zip(result['processes'],official['processes']):
        for k in ['slot','integrity_pass','cpu','wall','numerical_checks','numerical_failures']:require(portable[k]==original[k],'effect or gate changed: '+k)
    for k in ['admission','status','cells','pooled_sham_cycles','failures']:require(result[k]==official[k],'official numerical decision differs: '+k)
    require(api.decode(csv_data,receipts)==groups,'CSV is not lossless after origin removal')
    jsonfile(out/'protocol.json',protocol);write(out/'numerical-result.json.gz',gzip.compress(compact(result),mtime=0))
    provenance=dict(schema='paired-stock-clock-portable-provenance-v1',repository='cheshmakzanoon-ops/yamaha-cs80',
        stock_revision='e9e8175f4278d2457279fcb8809cbba1ddb7be2b',stock_archive_sha256=contract.STOCK_ARCHIVE_SHA256,
        stock_source_file_count=sum(k.startswith('stock/src/')for k in freeze['files']),
        stock_source_inventory_sha256=sha(compact({k.removeprefix('stock/src/'):v for k,v in freeze['files'].items()if k.startswith('stock/src/')})),
        terminal_sha256=args.terminal_sha,official_decision_sha256=args.decision_sha,official_decision_original_bytes=args.decision.stat().st_size,
        original_freeze_sha256=args.freeze_sha,original_plan_sha256=args.plan_sha,roles_sha256=plan['role_sha256'],
        original_source_sha256={k:v for k,v in freeze['files'].items()if k.startswith(('src/','fixture/'))or k in ['run_once.py','build.py','offline/contract.py','offline/analyze.py','offline/validate.py','PROTOCOL.md']},
        original_expected_endpoints_sha256=digest(q/'expected-endpoints.json'),official_check_count=len(official['checks']),
        official_failed_checks=len(official['failures']),all_official_effects_and_gates_reproduced=True,
        official_integrity_replay='Exact multiset of every expanded integrity check; list order alone is ignored because frozen validator iterates Python sets. Every numerical result/failure and all remaining decision fields match exactly.',
        official_integrity_omission='The giant raw validator decision is not bundled. All numerical effects/gates are exact; full original integrity-check arrays, loader proof, binaries, absolute addresses and PIDs are represented by SHA256 commitments and pass/count receipts, not independently replayed.',
        omitted=['absolute clock origins','process IDs','argv and private host paths','ASLR/base/function/errno addresses','raw original loader/image proof','binary files','redundant expanded successful integrity-check records'],
        retained='Every ordered raw row, paired CPU/wall interval and empty-clock/zero-boundary observation; all work, endpoint, FP, affinity and errno receipts; all numerical effects and gates.',
        slots=slot_provenance)
    jsonfile(out/'provenance.json',provenance)
    jsonfile(out/'tests.json',api.self_tests(groups,protocol,work))
    # Do not alter source campaign, official validator or original decision.
    require(digest(campaign/'terminal.json')==args.terminal_sha and digest(args.decision)==args.decision_sha
        and digest(q/'freeze.json')==args.freeze_sha,'input drift during export')
    print(json.dumps(dict(exported=True,slots=20,rows=len(flat),timed_rows=protocol['timed_row_count'],failures=len(result['failures']),
        bytes=sum(p.stat().st_size for p in out.iterdir()),status=result['status'],README_and_manifest_still_required=True),indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--decision',type=Path,required=True)
    for name in ['terminal','decision','freeze','plan']:p.add_argument('--'+name+'-sha',required=True)
    p.add_argument('--output',type=Path,required=True);export(p.parse_args())
