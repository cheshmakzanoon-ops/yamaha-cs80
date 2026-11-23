import argparse,hashlib,json,os,platform,statistics,subprocess,time
from pathlib import Path
parser=argparse.ArgumentParser(description="Reproduce paired full-engine CPU trials with separately built baseline/current probes.")
parser.add_argument('--repo',type=Path,required=True)
parser.add_argument('--baseline-source',type=Path,required=True)
parser.add_argument('--work',type=Path,required=True,help="Contains baseline-probe/current-probe and baseline-core.a/current-core.a")
args=parser.parse_args()
root=args.repo.resolve();work=args.work.resolve();source=args.baseline_source.resolve()
scenarios=['body_off_8','piano_low_8','piano_mid_8','tine_8','bowed_8','brass_8','piano_low_16_proxy']
repetitions=7
cpus=sorted(os.sched_getaffinity(0));cpu=cpus[len(cpus)//2]
os.sched_setaffinity(0,{cpu})
records=[]
def run(version,arg):
    p=subprocess.run([str(work/f'{version}-probe'),arg],capture_output=True,text=True,check=True)
    value=json.loads(p.stdout)
    return value
for trial in range(repetitions):
    names=scenarios[trial%len(scenarios):]+scenarios[:trial%len(scenarios)]
    for name in names:
        order=('baseline','current') if trial%2==0 else ('current','baseline')
        for version in order:
            value=run(version,name);value.update(version=version,trial=trial)
            records.append(value)
    (work/'raw-trials.json').write_text(json.dumps(records,indent=2)+'\n')
    print(f'Finished trial {trial+1}/{repetitions}',flush=True)
allocations={version:run(version,'--allocations') for version in ('baseline','current')}
summary=[]
for name in scenarios:
    before=[r for r in records if r['scenario']==name and r['version']=='baseline']
    after=[r for r in records if r['scenario']==name and r['version']=='current']
    assert len(before)==len(after)==repetitions
    assert len({r['prefix198_hash'] for r in before+after})==1
    assert len({r['audio_hash'] for r in before})==1 and len({r['audio_hash'] for r in after})==1
    assert {r['core_parameters'] for r in before}=={198} and {r['core_parameters'] for r in after}=={200}
    ratios=[n['cpu_seconds']/b['cpu_seconds'] for b,n in zip(before,after)]
    summary.append(dict(scenario=name,baseline_median_cpu_seconds=statistics.median(r['cpu_seconds'] for r in before),current_median_cpu_seconds=statistics.median(r['cpu_seconds'] for r in after),median_paired_ratio=statistics.median(ratios),paired_ratio_range=[min(ratios),max(ratios)],paired_ratios=ratios,first198_parameter_hash=before[0]['prefix198_hash'],baseline_audio_hash=before[0]['audio_hash'],current_audio_hash=after[0]['audio_hash'],deterministic_each_version=True,engines=before[0]['engines'],voices=before[0]['voices']))
assert summary[0]['baseline_audio_hash']==summary[0]['current_audio_hash']
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
files=['src/core/Engine.cpp','src/core/Engine.h','src/core/InstrumentBody.h','src/core/Params.h']
report=dict(schema=1,worker=20,evidence_kind='development_full_engine_cpu_and_allocation_probe',scope='Fixed identical sound controls; no factory recipes. CPU-time benchmark of static core, not real DAW deadline qualification or a native wrapper.',method=dict(clock='CLOCK_THREAD_CPUTIME_ID',host_rate=48000,oversampling_mode='Standard',effective_oversampling=2,measured_audio_seconds_per_trial=1,block_frames=256,warmup_frames=8192,trials=repetitions,interleave='AB then BA on alternating trials; scenario order rotates',affinity_cpu=cpu,logical_cpus_available=len(cpus),architecture=platform.machine(),system=platform.system(),python=platform.python_version(),compiler=subprocess.check_output(['g++','--version'],text=True).splitlines()[0],allocation_scope=allocations['current']['scope'],proxy_note='16 voices are serial rendering by two separate 8-voice core Engines. This is a cost proxy, not the real processor scheduling/mixing implementation.'),baseline=dict(commit='fa57967c7317828cf769cc7ca97f5857e566520f',archive_sha256=sha(work/'baseline-core.a'),probe_sha256=sha(work/'baseline-probe'),source_sha256={p:sha(source/p) for p in files},engine_bytes=records[0]['engine_bytes'],body_bytes=records[0]['body_bytes']),current=dict(label='Uncommitted final A06/A07/A10 DSP snapshot; factory/level changes do not enter manual patches',archive_sha256=sha(work/'current-core.a'),probe_sha256=sha(work/'current-probe'),source_sha256={p:sha(root/p) for p in files+['src/core/InstrumentBodyFilter.h']},engine_bytes=next(r['engine_bytes'] for r in records if r['version']=='current'),body_bytes=next(r['body_bytes'] for r in records if r['version']=='current')),harness_sha256=sha(root/'docs/preset-evidence/2026.11.4/reviews/20-runtime-cost.cpp'),results=summary,allocation_probe=allocations,regressions_over_25_percent=[s['scenario'] for s in summary if s['median_paired_ratio']>1.25],limitations=['CPU clocks exclude descheduling but do not eliminate frequency/cache contention; seven interleaved pairs report observed spread.','Absolute CPU seconds describe this development machine and fixture, not a promised realtime deadline on other hardware.','This covers two body-bearing ranks and representative models/registers; it is not an exhaustive every-preset performance census.','Allocation counters wrap static-core C entry points and override C++ allocation families; wrapper/UI and hidden allocations inside dynamically linked libraries are outside this probe.'],trials=records)
out=root/'docs/preset-evidence/2026.11.4/reviews/20-runtime-cost.json'
out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'results':summary,'allocations':allocations,'regressions_over_25_percent':report['regressions_over_25_percent']},indent=2),flush=True)
