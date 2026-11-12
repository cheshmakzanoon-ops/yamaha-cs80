#!/usr/bin/env python3
"""Run only the three authorized finite-input hosts; never retry outcomes."""
import concurrent.futures, hashlib, json, os, pathlib, subprocess, time
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'build/legacy-speed-scoped-results'
BASE = ROOT / '../yamaha-cs80-touch/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
CAND = ROOT / 'build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
CONTRACT = ROOT / 'tests/parity/tolerances-v2'
GATES = {
 'speed': ('cs80_vst3_sub_legacy_speed_automation_tests', 'host-automation-sub-legacy-speed-v1.json', 6),
 'touch': ('cs80_vst3_sub_touch_automation_tests', 'host-automation-sub-touch-v1.json', 18),
 'combined-touch': ('cs80_vst3_sub_touch_combined_automation_tests', 'host-automation-sub-touch-v1.json', 12),
}
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def save(p, data): p.write_text(json.dumps(data, indent=2)+'\n')
def run_config(gate, rate, mode):
    name, contract, cases = GATES[gate]
    host = ROOT / 'build-native' / name
    records=[]
    for kind, module in [('candidate', CAND), ('baseline', BASE)]:
        dest = OUT / f'{gate}-{kind}-{rate}-{mode}'
        dest.mkdir()
        cmd = [str(host),str(module),str(CONTRACT/'host-automation.json'),str(CONTRACT/contract),str(dest),str(rate),mode]
        if kind == 'baseline': cmd.append('--compat-only')
        log = dest.with_suffix('.log')
        record = dict(gate=gate,kind=kind,rate=rate,mode=mode,command=cmd,expected_cases=cases,expected_captures=cases*3,
                      host_sha256=sha(host),module_sha256=sha(module),started=time.time())
        with log.open('wb') as stream:
            try:
                result=subprocess.run(cmd,cwd=ROOT,stdout=stream,stderr=subprocess.STDOUT,timeout=180)
                record['exit_code']=result.returncode
            except subprocess.TimeoutExpired:
                record.update(exit_code=None,error='timeout after 180 seconds; not retried')
            except Exception as ex:
                record.update(exit_code=None,error=repr(ex))
        record.update(finished=time.time(),log=str(log),log_sha256=sha(log),capture_count=len(list(dest.glob('*.pcm'))),
                      host_sha256_after=sha(host),module_sha256_after=sha(module))
        save(dest/'run.json',record)
        records.append(record)
        print(gate,kind,rate,mode,record['exit_code'],flush=True)
    return records

def main():
    if os.environ.get('CS80_SCOPED_BUILD_REVIEW_APPROVED') != '1':
        raise SystemExit('Coordinator build/source-review confirmation required before setting CS80_SCOPED_BUILD_REVIEW_APPROVED=1')
    if OUT.exists(): raise SystemExit('Refusing existing result directory: outcomes must never be replaced/retried')
    paths=[CAND, BASE, CONTRACT/'host-automation.json']
    for name,contract,_ in GATES.values(): paths.extend([ROOT/'build-native'/name,CONTRACT/contract])
    provenance={str(p):sha(p) for p in paths}
    assert sha(BASE)=='a1915dd34d7c63519ef6bedc1e276b2f7429512c7845527e5d55596e05fe252d'
    OUT.mkdir()
    save(OUT/'inputs-before.json',provenance)
    records=[]
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        futures=[pool.submit(run_config,g,r,m) for g in GATES for r in [44100,48000,96000] for m in ['realtime','offline']]
        for future in concurrent.futures.as_completed(futures):
            records.extend(future.result())
            save(OUT/'manifest.json',records)
    after={str(p):sha(p) for p in paths}
    save(OUT/'inputs-after.json',after)
    pairs=[]
    for gate,(_,_,cases) in GATES.items():
        for rate in [44100,48000,96000]:
            for mode in ['realtime','offline']:
                a=OUT/f'{gate}-candidate-{rate}-{mode}'; b=OUT/f'{gate}-baseline-{rate}-{mode}'
                names_a={p.name for p in a.glob('*.pcm')}; names_b={p.name for p in b.glob('*.pcm')}
                for name in sorted(names_a|names_b):
                    pa,pb=a/name,b/name
                    ha=sha(pa) if pa.exists() else None; hb=sha(pb) if pb.exists() else None
                    pairs.append(dict(gate=gate,rate=rate,mode=mode,name=name,candidate_sha256=ha,baseline_sha256=hb,
                                      equal=ha is not None and ha==hb))
    valid=all(r['exit_code']==0 and r['capture_count']==r['expected_captures'] for r in records)
    result=dict(expected_pairs=648,actual_pairs=len(pairs),all_runs_pass=valid,inputs_unchanged=provenance==after,
                all_pairs_equal=len(pairs)==648 and all(p['equal'] for p in pairs),pairs=pairs)
    save(OUT/'capture-comparison.json',result)
    raise SystemExit(0 if valid and provenance==after and result['all_pairs_equal'] else 1)
if __name__ == '__main__': main()
