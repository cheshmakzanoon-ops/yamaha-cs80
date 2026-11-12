#!/usr/bin/env python3
"""Fixed, descriptive endpoint-clock diagnostic; never production comparison."""
import argparse
from fractions import Fraction
import math
import os
from pathlib import Path
import sys

import render_event_aware_envelope_sine_vca as analytic
import render_reference_chain as chain

helpers=analytic.helpers
require=helpers.require
RATES=(8000,16000,32000)
SPAN=Fraction(1,50)
STEPS=8
TOLERANCE=1e-7
MAX_BYTES=128*1024*1024
MAX_FILES=128
EVENT_BYTES=b'event,time_s,action\n0,.00037,note_on\n1,.00711,note_off\n2,.01173,retrigger\n'
NATIVE_SOURCES={'renderer':'src/tools/vca_reference.cpp','reference':'src/reference/IG00151Reference.h',
    'wav_helper':'src/tools/FloatWav.h','decimal_conversion':'src/tools/DecimalConversion.h',
    'hash_helper':'src/tools/TruthInputSha256.h','declared_core_model':'src/core/Vca.h',
    'model_document':'docs/IG00151_VCA_MODEL.md','workflow_document':'docs/VCA_REFERENCE.md'}


def parse(argv):
    p=argparse.ArgumentParser(description=__doc__,allow_abbrev=False)
    p.add_argument('--vca',required=True)
    p.add_argument('--out',required=True)
    opts=[x.split('=',1)[0] for x in argv if x.startswith('--')]
    require(len(opts)==len(set(opts)),'Duplicate option')
    return p.parse_args(argv)


def analytic_args(root,rate):
    return analytic.parse(['--events',str(root/'events.csv'),'--attack','.004','--decay','.03',
        '--sustain','.4','--release','.02','--input-amplitude','.4','--input-frequency','440',
        '--input-phase','.125','--lc','.8','--ei','-.5','--initial-li','.1','--initial-coupling','0',
        '--rate',str(rate),'--samples',str(int(SPAN*rate)+1),'--out',str(root/f'analytic_{rate}')])


def exact(value):
    return Fraction(value['numerator'],value['denominator'])


def bucket(t,boundaries):
    if t==0: return 't0'
    return 'boundary_near' if any(t-Fraction(1,8000)<b<=t for b in boundaries) else 'smooth'


def metrics(rows):
    result={}
    for group in ('all','t0','boundary_near','smooth'):
        chosen=rows if group=='all' else [r for r in rows if r['bucket']==group]
        result[group]={'count':len(chosen)}
        for name in ('li','coupling','output'):
            values=[r['delta_'+name] for r in chosen]
            result[group][name]=dict(max_abs=max(map(abs,values),default=0),
                rms=math.sqrt(math.fsum(v*v for v in values)/len(values)) if values else None)
    return result


def artifacts(root):
    files=sorted(root.rglob('*'))
    require(len(files)<=MAX_FILES,'Artifact count budget exceeded')
    result={}
    size=0
    for p in files:
        require(not p.is_symlink(),'Unexpected artifact symlink')
        if p.is_file():
            data=chain.snapshot(p,MAX_BYTES)
            size+=len(data)
            require(size<=MAX_BYTES,'Bundle byte budget exceeded')
            result[str(p.relative_to(root))]=helpers.metadata(data)
    return result


def run(args):
    root=Path(args.out).absolute()
    require(not os.path.lexists(root) and root.parent.is_dir(),'New output directory with existing parent required')
    repo=Path(__file__).resolve().parents[1]
    binary=chain.snapshot(args.vca,32*1024*1024)
    sources={path:helpers.snapshot(repo/path) for path in NATIVE_SOURCES.values()}
    for path in ('CMakeLists.txt','scripts/compare_envelope_endpoint_clock.py',
                 'scripts/render_event_aware_envelope_sine_vca.py','scripts/render_event_aware_envelope_vca.py',
                 'scripts/render_amplitude_envelope_reference.py','scripts/render_reference_chain.py'):
        sources[path]=helpers.snapshot(repo/path)
    owned=chain.Owned()
    owned.mkdir(root)
    commands={}
    try:
        for name in ('tools','inputs','sources'): owned.mkdir(root/name)
        owned.write(root/'tools/vca',binary,executable=True)
        owned.write(root/'events.csv',EVENT_BYTES)
        for path,data in sources.items():
            # Flat, unambiguous snapshot names; original paths recorded in manifest.
            owned.write(root/'sources'/path.replace('/','__'),data)
        rate_results={}
        all_rows=[]
        reference_common=None
        boundaries=None
        for rate in RATES:
            aa=analytic_args(root,rate)
            result=analytic.render(aa)
            here=sorted(set([t for t,_ in helpers.parse_events(EVENT_BYTES,SPAN)]+
                            [exact(x['time']) for x in result['transitions']]))
            if boundaries is None: boundaries=here
            require(here==boundaries,'Analytic boundaries differ across clocks')
            drives=['sample,input,lc,ei,envelope\n']
            for row in result['rows']:
                drives.append(str(row['sample'])+','+','.join(format(v,'.17g') for v in
                    (row['input'],float(aa.lc),float(aa.ei),row['envelope']))+'\n')
            data=''.join(drives).encode('ascii')
            input_name=f'inputs/native_{rate}.csv'
            owned.write(root/input_name,data)
            name=f'native_{rate}'
            command=['./tools/vca','--input',input_name,'--rate',str(rate),'--samples',str(aa.samples),
                '--steps',str(STEPS),'--tolerance',str(TOLERANCE),'--initial-li','.1','--out',name]
            commands[name]=command
            code,error=chain.execute(command,root)
            owned.write(root/f'{name}_execution.json',helpers.json_bytes(dict(command=command,exit_code=code,stderr_tail=error)))
            require(code==0,f'{name} exited {code}: {error}; no retry')
            require(set(p.name for p in (root/name).iterdir())=={'trace.csv','stimulus.csv','manifest.json'},'Unexpected native artifacts')
            vm=chain.read_json(root/name/'manifest.json')
            require(vm['schema']=='cs80-ig00151-reference-v1' and vm['rate']==rate and vm['samples']==aa.samples
                    and vm['intervals']==aa.samples-1 and vm['duration_seconds']==float(SPAN),'Native clock mismatch')
            require(vm['compare_realtime'] is False and vm['max_abs_realtime_difference'] is None,'Realtime must remain disabled')
            require(vm['initial_li']==.1 and vm['initialization']=='explicit LI or first LC; zero coupling; instantaneous output at t=0',
                    'Native initialization mismatch')
            require(vm['coarse_steps_per_interval']==STEPS and vm['fine_steps_per_interval']==2*STEPS
                    and vm['absolute_refinement_tolerance']==TOLERANCE,'Native refinement mismatch')
            require(vm['units']=={'input_lc_li_output_coupling':'uncalibrated engine units','ei':'octaves','envelope_gain':'unitless'},'Units mismatch')
            require(vm['li_seconds']==analytic.constant.TAU_LI and vm['ac_seconds']==analytic.constant.TAU_AC
                    and vm['input_offset']==analytic.constant.OFFSET and vm['feedthrough']==analytic.constant.FEEDTHROUGH,'Constants mismatch')
            require(vm['stimulus_sha256']==helpers.metadata(data)['sha256'] and vm['stimulus_bytes']==len(data)
                    and chain.snapshot(root/name/'stimulus.csv')==data,'Native forcing mismatch')
            for key,path in NATIVE_SOURCES.items():
                require(vm['source_sha256'][key]==helpers.metadata(sources[path])['sha256'],'Embedded source identity mismatch: '+key)
            native=chain.trace(root/name/'trace.csv',chain.VCA_HEADER,aa.samples,rate,
                               ['input','lc','ei','envelope','li','coupling','output'])
            require(native['li'][0]==.1 and native['coupling'][0]==0,'Native time-zero state advanced')
            for i,row in enumerate(result['rows']):
                require(native['input'][i]==row['input'] and native['envelope'][i]==row['envelope']
                        and native['lc'][i]==float(aa.lc) and native['ei'][i]==float(aa.ei),'Native endpoint drive mismatch')
            common=[]
            common_analytic=[]
            for i in range(161):
                j=i*(rate//8000)
                t=Fraction(i,8000)
                row=result['rows'][j]
                require(exact(row['time'])==t,'Common analytic clock mismatch')
                r=dict(rate=rate,sample_8000=i,sample_native=j,time_s=float(t),bucket=bucket(t,boundaries))
                for key in ('li','coupling','output'):
                    r['analytic_'+key]=row[key]
                    r['native_'+key]=native[key][j]
                    r['delta_'+key]=native[key][j]-row[key]
                common.append(r)
                common_analytic.append(tuple(row[k] for k in ('li','coupling','output')))
            if reference_common is None: reference_common=common_analytic
            require(common_analytic==reference_common,'Analytic common-grid identity failed')
            all_rows.extend(common)
            rate_results[str(rate)]=dict(samples=aa.samples,coarse_steps=STEPS,fine_steps=2*STEPS,
                native_refinement=vm['max_abs_h_h2_difference'],metrics=metrics(common),
                native_manifest=f'{name}/manifest.json',analytic_manifest=f'analytic_{rate}/manifest.json')
            artifacts(root)
        report=dict(schema='cs80-envelope-endpoint-clock-v1',rate_results=rate_results,
            common_endpoint_count=161,common_rate_hz=8000,span=helpers.exact_time(SPAN),
            boundaries=[helpers.exact_time(t) for t in boundaries],
            boundary_bucket='t0 separate; boundary_near if deduplicated event/autonomous boundary in (t-1/8000,t]; otherwise smooth',
            meaning='Native fine PWL minus analytic event-aware sine values. Differences combine forcing interpolation, event placement, residual numerical and arithmetic-order rounding. Not interpolation-only error or hardware/production equivalence.',
            refinement_meaning='Native h/h2 endpoint differences use the same forcing knots; independent numerical refinement diagnostic, not a rigorous bound.',
            claims='Descriptive metrics only; no monotonic rate or maximum-error assertion. All 161 common endpoints included for every rate.')
        cols=('rate','sample_8000','sample_native','time_s','bucket','analytic_li','native_li','delta_li',
              'analytic_coupling','native_coupling','delta_coupling','analytic_output','native_output','delta_output')
        text=[','.join(cols)+'\n']
        for r in all_rows:
            text.append(','.join(format(r[k],'.17g') if isinstance(r[k],float) else str(r[k]) for k in cols)+'\n')
        owned.write(root/'common.csv',''.join(text).encode('ascii'))
        owned.write(root/'report.json',helpers.json_bytes(report))
        inventory=artifacts(root)
        manifest=dict(schema='cs80-envelope-endpoint-clock-bundle-v1',complete=True,compare_realtime=False,
            commands=commands,native_executable=helpers.metadata(binary),
            source_snapshots={path:dict(artifact='sources/'+path.replace('/','__'),**helpers.metadata(data)) for path,data in sources.items()},
            budgets=dict(native_processes=3,child_timeout_seconds=chain.CHILD_TIMEOUT,max_files=MAX_FILES,max_bytes=MAX_BYTES,
                         native_rk4_steps=sum(3*STEPS*int(SPAN*r) for r in RATES)),
            provenance='Copied binary and retained source bytes are identity evidence, not authentication or proof that arbitrary binary bytes were built from those sources. Embedded native source hashes are checked against snapshots.',
            failure_policy='No retries. Failure retains partial diagnostic artifacts and failure.json; only successful verified bundles have a complete manifest.',artifacts=inventory)
        manifest_bytes=helpers.json_bytes(manifest)
        require(sum(x['bytes'] for x in inventory.values())+len(manifest_bytes)<=MAX_BYTES, 'Final bundle byte budget exceeded')
        require(len(list(root.rglob('*')))+1<=MAX_FILES, 'Final artifact count budget exceeded')
        owned.write(root/'manifest.json',manifest_bytes)
        return report
    except BaseException as error:
        # Keep useful completed native diagnostics, never declare partial output complete.
        if not (root/'manifest.json').exists():
            owned.write(root/'failure.json',helpers.json_bytes(dict(complete=False,error=str(error)[:16384],commands=commands)))
        raise


def main(argv=None):
    try: run(parse(sys.argv[1:] if argv is None else argv))
    except (ValueError,OSError,UnicodeError,OverflowError) as error:
        print('endpoint clock diagnostic: '+str(error),file=sys.stderr)
        return 1
    return 0


if __name__=='__main__': sys.exit(main())
