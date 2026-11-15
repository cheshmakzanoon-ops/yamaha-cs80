#!/usr/bin/env python3
"""Schema and deterministic fixture checks, never a timing threshold."""
import argparse,csv,json,math,pathlib,subprocess,tempfile
p=argparse.ArgumentParser();p.add_argument('--benchmark',required=True);p.add_argument('--baseline');a=p.parse_args()
with tempfile.TemporaryDirectory() as tmp:
    runs={}
    for name,exe,flags in [('disabled',a.benchmark,[]),('enabled',a.benchmark,['--callback-cpu'])]+([('baseline',a.baseline,[])] if a.baseline else []):
        out=pathlib.Path(tmp)/name
        subprocess.run([exe,'--out',str(out),'--source-commit','b5b252979764c865bbd331827db4415498979e7b','--suite','baseline','--rate','48000','--block','128','--callbacks','16','--quick',*flags],check=True,stdout=subprocess.DEVNULL)
        meta=json.loads((out/'metadata.json').read_text());rows=list(csv.DictReader((out/'results.csv').open()));runs[name]=rows
        if name=='baseline':continue
        assert meta['schema_version']==3 and meta['callback_cpu_requested']==(name=='enabled')
        for row in rows:
            if name=='disabled':
                assert row['callback_cpu_status']=='disabled' and row['callback_cpu_call_count']=='0'
                assert row['callback_cpu_seconds']==row['callback_cpu_rtf']=='unavailable'
            elif row['callback_cpu_status']=='available':
                seconds=float(row['callback_cpu_seconds']);rtf=float(row['callback_cpu_rtf']);audio=float(row['audio_seconds'])
                assert math.isfinite(seconds) and seconds>=0 and int(row['callback_cpu_call_count'])==int(row['callbacks'])
                assert math.isclose(rtf,seconds/audio,rel_tol=1e-7,abs_tol=1e-10)
            else:
                assert row['callback_cpu_status']=='unavailable'
                assert row['callback_cpu_seconds']==row['callback_cpu_rtf']=='unavailable'
    # The instrumentation must not modify audio, event execution or fixture shape.
    timing={'min_us','median_us','p95_us','p99_us','max_us','mean_rtf','median_rtf','p99_rtf','max_rtf','budget_exceedances','event_median_us','event_max_us','process_cpu_seconds_including_checks','thread_cpu_seconds_including_checks'}
    def stable(rows):return [{k:v for k,v in row.items() if k not in timing and not k.startswith('callback_cpu_')} for row in rows]
    assert stable(runs['disabled'])==stable(runs['enabled'])
    if 'baseline' in runs:assert stable(runs['baseline'])==stable(runs['disabled'])
    print('callback CPU schema, counts and unchanged deterministic fields passed:',len(runs['enabled']),'rows',len(runs),'runs')
