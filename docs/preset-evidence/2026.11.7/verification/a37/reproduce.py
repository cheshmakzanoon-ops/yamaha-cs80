#!/usr/bin/env python3
"""Bounded paired whole-Engine probe; frozen source and supplied core hashes."""
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import random
import statistics
import subprocess
import tarfile
import time

ROOT = Path(__file__).resolve().parents[5]
EVIDENCE = Path(__file__).resolve().parent
WORK = Path('/tmp/cs80-a37-1cee5a')
BINDINGS = {
    'baseline': ('18711f93a39aa49f3d6f9a41784be456224facbe', Path('/tmp/cs80-reference-1cee5a/libcs80_core.a'), 'b07691dee826d26f71176b5910aac0566dd717f5ff9c71b18af9ba56dcc26583'),
    'current': ('c5549f7d095a5c5f49a5ad22ec0ccc819245d201', Path('/tmp/cs80-build-1cee5a/libcs80_core.a'), 'a2954c6ba928b7b3fd51e7d143878fca8da0b0d7bff554d0343262219f175fe1'),
}

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def run(cmd):
    return subprocess.check_output(cmd, cwd=ROOT, text=True).strip()

def main():
    WORK.mkdir(parents=True, exist_ok=True)
    receipt = {'scope': 'Offline full Engine + unchanged ControlFrame per 256-frame block; not plugin/host deadline qualification',
               'started_utc': time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()), 'source': {},
               'host': {'platform': platform.platform(), 'uname': list(platform.uname()),
                        'compiler': run(['c++','--version']), 'cpuinfo_first_processor': Path('/proc/cpuinfo').read_text().split('\n\n')[0],
                        'allowed_cpu_ids_before_pin': sorted(os.sched_getaffinity(0)),
                        'process_stack_limit': run(['bash','-c','ulimit -s'])},
               'commands': [], 'metadata': {}, 'probe_sha256': digest(EVIDENCE/'engine_probe.cpp'),
               'paired_blocks_per_case': 5, 'order_seed': 370711, 'snapshots_include_all_src_files': True}
    procs = {}
    for version,(commit,core,expected_hash) in BINDINGS.items():
        assert digest(core)==expected_hash, (core,'core identity changed')
        snapshot = WORK/version
        snapshot.mkdir(exist_ok=True)
        archive_cmd = ['git','archive',commit,'src']
        archive = subprocess.check_output(archive_cmd,cwd=ROOT)
        with tarfile.open(fileobj=io.BytesIO(archive)) as tar:
            tar.extractall(snapshot,filter='data')
        files = {str(p.relative_to(snapshot)):digest(p) for p in sorted((snapshot/'src').rglob('*')) if p.is_file()}
        headers = {p:h for p,h in files.items() if p.endswith(('.h','.hpp','.inc'))}
        command = ['c++','-std=c++20','-O3','-DNDEBUG','-DCS80_ENABLE_BORROWED_HELD=1','-I',str(snapshot/'src'),
                   str(EVIDENCE/'engine_probe.cpp'),str(core),'-pthread','-o',str(WORK/('probe-'+version))]
        receipt['commands'].append(command)
        subprocess.run(command,cwd=ROOT,check=True)
        receipt['source'][version] = {'commit':commit,'core':str(core),'core_sha256':expected_hash,
            'archive_command':archive_cmd,'archive_sha256':hashlib.sha256(archive).hexdigest(),
            'all_src_file_count':len(files),'header_and_inc_count':len(headers),'all_src_sha256':files,
            'binary_sha256':digest(WORK/('probe-'+version))}
        procs[version] = subprocess.Popen([str(WORK/('probe-'+version))],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
        receipt['metadata'][version] = json.loads(procs[version].stdout.readline())
    assert receipt['metadata']['baseline']['sizeof_engine']==491976
    assert receipt['metadata']['current']['sizeof_engine']==504776
    rows=[]
    rng=random.Random(receipt['order_seed'])
    def measure(case, version, block, slot):
        proc=procs[version]; proc.stdin.write(str(case)+'\n'); proc.stdin.flush()
        line=proc.stdout.readline()
        if not line: raise RuntimeError(f'{version} probe ended during case {case}; status {proc.poll()}')
        row=json.loads(line); row.update(version=version,paired_block=block,slot=slot)
        rows.append(row)
    try:
        with (EVIDENCE/'raw.jsonl').open('w') as raw:
            for case in range(4):
                for version in BINDINGS: measure(case,version,-1,0); raw.write(json.dumps(rows[-1],sort_keys=True)+'\n');raw.flush()
                for block in range(5):
                    order=['baseline','current','current','baseline']
                    if rng.getrandbits(1): order.reverse(); order=['current' if x=='baseline' else 'baseline' for x in order]
                    for slot,version in enumerate(order):
                        measure(case,version,block,slot)
                        raw.write(json.dumps(rows[-1],sort_keys=True)+'\n'); raw.flush()
                print('completed case',case,flush=True)
    finally:
        for version,proc in procs.items():
            if proc.poll() is None:
                proc.stdin.write('-1\n'); proc.stdin.flush()
            status=proc.wait(timeout=30)
            receipt.setdefault('returncodes',{})[version]=status
    receipt['ended_utc']=time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime())
    receipt['reproduce_sha256']=digest(__file__)
    (EVIDENCE/'binding.json').write_text(json.dumps(receipt,indent=2)+'\n')
    results=[]
    for case in dict.fromkeys(row['case'] for row in rows):
        selected=[r for r in rows if r['case']==case and r['paired_block']>=0]
        ratio=[]
        for block in range(5):
            means={v:statistics.mean(r['cpu_ns'] for r in selected if r['paired_block']==block and r['version']==v) for v in BINDINGS}
            ratio.append(means['current']/means['baseline'])
        result={'case':case,'paired_cpu_ratios':ratio,'paired_cpu_ratio_median':statistics.median(ratio),
                'paired_cpu_ratio_range':[min(ratio),max(ratio)],'versions':{}}
        for version in BINDINGS:
            group=[r for r in selected if r['version']==version]
            assert len(group)==10
            assert len({r['pcm_word_fnv1a64'] for r in group})==1
            assert len({r['params_word_fnv1a64'] for r in selected})==1
            assert all(r['nonfinite']==0 for r in group)
            result['versions'][version]={'cpu_ns_per_host_frame_median':statistics.median(r['cpu_ns']/65536 for r in group),
                'wall_ns_per_host_frame_median':statistics.median(r['wall_ns']/65536 for r in group),
                'cpu_elapsed_ms_range':[min(r['cpu_ns'] for r in group)/1e6,max(r['cpu_ns'] for r in group)/1e6],
                'wall_cpu_ratio_range':[min(r['wall_ns']/r['cpu_ns'] for r in group),max(r['wall_ns']/r['cpu_ns'] for r in group)],
                'pcm_word_fnv1a64':group[0]['pcm_word_fnv1a64'],'params_word_fnv1a64':group[0]['params_word_fnv1a64'],
                'voices':group[0]['voices'],'peak':group[0]['peak'],'rms':group[0]['rms'],
                'allocations_distinct':{phase:sorted({json.dumps(r['allocation'][phase],sort_keys=True) for r in group}) for phase in group[0]['allocation']}}
        result['baseline_current_pcm_hash_equal']=result['versions']['baseline']['pcm_word_fnv1a64']==result['versions']['current']['pcm_word_fnv1a64']
        results.append(result)
    (EVIDENCE/'summary.json').write_text(json.dumps({'cases':results,'timed_trials':80,'first_touch_trials':8,'total_trials':len(rows),
        'counted_cpp_allocations_only':True,'native_plugin_validation':False},indent=2)+'\n')
    print(json.dumps([{'case':x['case'],'paired_cpu_ratio_median':x['paired_cpu_ratio_median'],'range':x['paired_cpu_ratio_range']} for x in results],indent=2))

if __name__=='__main__':
    main()
