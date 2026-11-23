import csv, hashlib, json, pathlib, statistics, subprocess, time

base = pathlib.Path(__file__).resolve().parent
columns = ['case','preset','moving','thread_cpu_seconds','peak','rms','audio_fnv1a64','active_voices','engine_bytes']
rows = []
started = time.time()
with (base/'trials.csv').open('w', newline='') as out:
    writer = csv.DictWriter(out, fieldnames=['trial','version','order']+columns)
    writer.writeheader()
    for trial in range(7):
        for case in range(4):
            for moving in (0,1):
                versions = ['baseline','candidate'] if trial%2 == 0 else ['candidate','baseline']
                for order, version in enumerate(versions):
                    result = subprocess.run(['taskset','-c','7',str(base/(version+'-probe')),str(case),str(moving)],capture_output=True,text=True,check=True)
                    data = dict(zip(columns, next(csv.reader(result.stdout.splitlines()))))
                    row = dict(trial=trial,version=version,order=order,**data)
                    rows.append(row);writer.writerow(row);out.flush()
        print(f'Completed trial {trial+1}/7; elapsed {time.time()-started:.1f}s',flush=True)

cases=[]
for case in range(4):
    for moving in (0,1):
        selected=[r for r in rows if int(r['case'])==case and int(r['moving'])==moving]
        ratios=[]
        for trial in range(7):
            pair={r['version']:r for r in selected if int(r['trial'])==trial}
            ratios.append(float(pair['candidate']['thread_cpu_seconds'])/float(pair['baseline']['thread_cpu_seconds']))
        cases.append(dict(case=case,preset=selected[0]['preset'],moving=bool(moving),body_off_control=case==3,
                          paired_ratios=ratios,median_ratio=statistics.median(ratios),minimum_ratio=min(ratios),maximum_ratio=max(ratios),
                          baseline_cpu_median=statistics.median(float(r['thread_cpu_seconds']) for r in selected if r['version']=='baseline'),
                          candidate_cpu_median=statistics.median(float(r['thread_cpu_seconds']) for r in selected if r['version']=='candidate'),
                          baseline_audio_hashes=sorted(set(r['audio_fnv1a64'] for r in selected if r['version']=='baseline')),
                          candidate_audio_hashes=sorted(set(r['audio_fnv1a64'] for r in selected if r['version']=='candidate'))))
summary=dict(scope='Full Engine, 8 active voices, native existing presets, realtime Standard2x at48kHz,64frame blocks; no host/plugin wrappers.',
             baseline='Tine-corrected header19f9fb23 before bowed correction, a09 static archive',
             candidate='Bowed-corrected headerd33f34f5',
             measurements='7 interleaved process pairs per case; alternating order; pinned allowedCPU7; CLOCK_THREAD_CPUTIME_ID around0.5s render after12032warmup frames.',
             pitch='Moving=5Hz external pitchbend +/-40cents updated per64frames. Fixed=no added pitchbend; original preset drift and modulation remain.',
             limits='Shared-machine paired estimates, not realtime deadlines or DAW CPU percentages; includes input pitchbend calls equally on both versions. Initialization excluded.',
             cases=cases,total_runs=len(rows),wall_seconds=time.time()-started)
(base/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
paths=['probe.cpp','run.py','trials.csv','summary.json','baseline-probe','candidate-probe','baseline_core.a','candidate_core.a',
       'baseline-include/core/Engine.h','candidate-include/core/Engine.h','baseline-include/core/InstrumentBody.h','candidate-include/core/InstrumentBody.h']
provenance={p:hashlib.sha256((base/p).read_bytes()).hexdigest() for p in paths}
(base/'provenance.json').write_text(json.dumps(provenance,indent=2)+'\n')
for case in cases:print(case['preset'],case['moving'],case['body_off_control'],'ratio',case['median_ratio'],'range',case['minimum_ratio'],case['maximum_ratio'])
