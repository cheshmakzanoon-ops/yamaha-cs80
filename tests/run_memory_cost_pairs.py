#!/usr/bin/env python3
"""Optional OFFLINE paired cost probe runner; not a CI timing/performance gate.

Both binaries must be built from the identical complete-output MemorySparseProbe
source/options, changing only the declared candidate header(s). The output
manifest supplied by the research report must identify those inputs.
"""
from pathlib import Path
import csv, json, os, statistics, subprocess, sys
assert len(sys.argv)==4, 'baseline-probe candidate-probe output-directory'
binaries={key:Path(value).resolve() for key,value in zip(['baseline','candidate'],sys.argv[1:3])}
out=Path(sys.argv[3]).resolve();out.mkdir(parents=True,exist_ok=True)
allowed=os.sched_getaffinity(0)
assert allowed, 'no allowed CPU'
cpu=min(allowed)
metadata={'allowed_parent_before':sorted(allowed),'selected_child_cpu':cpu,
          'affinity_scope':'child only, set and verified before exec; parent unchanged',
          'phases':['unpinned','affinity'],'trials':7,'modes':[2,3],
          'evaluations_per_fixture_row':128,
          'order':'baseline then candidate for even trials, reversed for odd trials',
          'binaries':{k:str(v) for k,v in binaries.items()}}
(out/'affinity.json').write_text(json.dumps(metadata,indent=2)+'\n')
fields=['phase','version','pair_position','scenario','pot_ohms','shape','trial','iterations',
        'mean_ns','failures','max_newton_steps','max_node_bound_V','mean_process_cpu_ns']
all_rows=[]
with (out/'timings.csv').open('w',newline='') as file:
    writer=csv.DictWriter(file,fieldnames=fields);writer.writeheader();file.flush()
    for phase in ['unpinned','affinity']:
        for trial in range(7):
            order=['baseline','candidate'] if trial%2==0 else ['candidate','baseline']
            for mode in [2,3]:
                for position,version in enumerate(order):
                    def affinity():
                        os.sched_setaffinity(0,{cpu})
                        if os.sched_getaffinity(0)!={cpu}:os._exit(125)
                    raw=out/f'{phase}-{trial}-{mode}-{version}.csv'
                    run=subprocess.run([str(binaries[version]),str(raw),str(mode),str(trial)],
                        text=True,capture_output=True,preexec_fn=affinity if phase=='affinity' else None)
                    (out/f'{phase}-{trial}-{mode}-{version}.log').write_text(run.stdout+run.stderr)
                    rows=list(csv.DictReader(raw.open())) if raw.exists() else []
                    for row in rows:
                        row={'phase':phase,'version':version,'pair_position':position,**row}
                        all_rows.append(row);writer.writerow(row)
                    file.flush()
                    assert run.returncode==0,(phase,trial,mode,version,run.returncode,run.stderr)
                    assert len(rows)==3 and all(int(row['iterations'])==128 and int(row['failures'])==0 for row in rows)
                    assert [(int(row['pot_ohms']),int(row['shape'])) for row in rows]==[(50000,5),(100000,4),(100000,5)]
                    assert all(int(row['trial'])==trial for row in rows)
metadata['allowed_parent_after']=sorted(os.sched_getaffinity(0))
assert metadata['allowed_parent_after']==metadata['allowed_parent_before']
(out/'affinity.json').write_text(json.dumps(metadata,indent=2)+'\n')
summary=[]
keys=sorted({(r['phase'],r['scenario'],r['pot_ohms'],r['shape']) for r in all_rows})
for phase,scenario,pot,shape in keys:
    subset=[r for r in all_rows if (r['phase'],r['scenario'],r['pot_ohms'],r['shape'])==(phase,scenario,pot,shape)]
    for clock in ['mean_ns','mean_process_cpu_ns']:
        baseline={int(r['trial']):float(r[clock]) for r in subset if r['version']=='baseline'}
        candidate={int(r['trial']):float(r[clock]) for r in subset if r['version']=='candidate'}
        assert set(baseline)==set(candidate)==set(range(7))
        changes=[100*(1-candidate[t]/baseline[t]) for t in range(7)]
        summary.append({'phase':phase,'scenario':scenario,'pot_ohms':pot,'shape':shape,'clock':clock,
                        'baseline_median_ns':statistics.median(baseline.values()),
                        'candidate_median_ns':statistics.median(candidate.values()),
                        'median_paired_reduction_percent':statistics.median(changes),
                        'minimum_paired_reduction_percent':min(changes),
                        'maximum_paired_reduction_percent':max(changes),
                        'improved_pairs':sum(x>0 for x in changes),'pairs':7})
with (out/'paired-summary.csv').open('w',newline='') as file:
    writer=csv.DictWriter(file,fieldnames=list(summary[0]));writer.writeheader();writer.writerows(summary)
print('Paired complete-output rows:',len(all_rows),'summary rows:',len(summary),'failed evaluations:',sum(int(r['failures']) for r in all_rows))
print('Parent affinity unchanged; pinned child affinity set and verified before exec:',cpu)
