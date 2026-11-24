#!/usr/bin/env python3
"""Verify retained supporting CI inputs; does not qualify native releases."""
from pathlib import Path, PurePosixPath
import argparse, hashlib, json, re, subprocess, zipfile
from datetime import datetime

SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
DSP_RUN, FILTER_RUN = 45366603724, 45366603846
JOBS = {14906817736:138349815996,14908055011:138349815996,14906404653:138349815996,
        14907385925:138349815609,14906339367:138349816091,14905979921:138349815994}

def digest(raw): return hashlib.sha256(raw).hexdigest()
def require(ok, message):
    if not ok: raise ValueError(message)
def read_json(path): return json.loads(path.read_bytes())
def stamp(s): return datetime.fromisoformat(s.replace('Z','+00:00'))
def source_bytes(repo, name):
    return subprocess.check_output(['git','-C',str(repo),'show',SOURCE+':'+name])
def sums(raw):
    result={}
    for line in raw.decode().splitlines():
        match=re.fullmatch(r'([a-f0-9]{64})  (.+)',line)
        require(match is not None,'Malformed SHA256SUMS row')
        sha,name=match.groups(); require(name not in result,'Duplicate checksum member')
        result[name]=sha
    return result

def verify(repo, directory, archives):
    result={'source_commit':SOURCE,'native_release_qualified':False,'analog_evidence':False}
    runs={DSP_RUN:read_json(directory/'dsp_run.json'),FILTER_RUN:read_json(directory/'filter_run.json')}
    for number,run in runs.items():
        require(run['id']==number and run['head_sha']==SOURCE and run['run_attempt']==1,'Run/source/attempt mismatch')
    require(runs[FILTER_RUN]['conclusion']=='success','Filter workflow incomplete')
    filter_jobs=read_json(directory/'filter_jobs.json')['jobs']
    expected={f'{compiler} / batch {mode}' for compiler in ('g++','clang++') for mode in ('OFF','ON','DEFAULT')}
    require({j['name'] for j in filter_jobs}==expected and len(filter_jobs)==6,'Compiler matrix mismatch')
    observed=[]
    for job in filter_jobs:
        require(job['head_sha']==SOURCE and job['run_id']==FILTER_RUN and job['run_attempt']==1
                and job['status']=='completed' and job['conclusion']=='success','Filter job binding/outcome mismatch')
        step=next(s for s in job['steps'] if s['name']=='Exactness and scheduling gates')
        require(step['conclusion']=='success','Focused test step unsuccessful')
        log=(directory/f"job-{job['id']}.log").read_bytes()
        require(len(re.findall(rb'100% tests passed, 0 tests failed out of 4',log))==1,'Focused CTest summary missing/duplicated')
        require(SOURCE.encode() in log,'Checkout source missing from original job log')
        observed.append({'id':job['id'],'name':job['name'],'ctest_executions':4,'log_sha256':digest(log)})
    result['filter_matrix']={'jobs':observed,'registered_executions':24,'passed':24,'failed':0}
    dsp_jobs={j['id']:j for j in read_json(directory/'dsp_jobs.json')['jobs']}
    require(len(dsp_jobs)==5,'Unexpected DSP job inventory')
    api={a['id']:a for a in read_json(directory/'artifacts.json')['artifacts']}
    records=[]
    for number,job_id in JOBS.items():
        meta=api[number]; job=dsp_jobs[job_id]
        require(job['head_sha']==SOURCE and job['run_id']==DSP_RUN and job['run_attempt']==1
                and job['status']=='completed' and job['conclusion']=='success','Producing DSP job is not successful at expected source/attempt')
        raw=(archives/f'{number}.zip').read_bytes()
        require(len(raw)==meta['size_in_bytes'] and 'sha256:'+digest(raw)==meta['digest'],'Actual ZIP differs from API')
        require(meta['workflow_run']['id']==DSP_RUN and meta['workflow_run']['head_sha']==SOURCE
                and meta['expired'] is False and meta['name'].endswith(SOURCE),'Artifact context mismatch')
        require(stamp(job['started_at'])<=stamp(meta['created_at'])<=stamp(job['completed_at']),'Artifact not within producing job time')
        log=(directory/f'job-{job_id}.log').read_text()
        require(str(number) in log and meta['name'] in log and SOURCE in log,'Original upload/source binding absent')
        with zipfile.ZipFile(archives/f'{number}.zip') as archive:
            names=archive.namelist(); require(len(names)==len(set(names)),'Duplicate ZIP member')
            require(all(not n.startswith('/') and '..' not in PurePosixPath(n).parts for n in names),'Unsafe ZIP member')
            require(archive.testzip() is None,'ZIP CRC mismatch')
            data={n:archive.read(n) for n in names if not n.endswith('/')}
        rec={'id':number,'name':meta['name'],'bytes':len(raw),'sha256':digest(raw),'producer_job':job_id,
             'upload_attempt_bound':True,'members':len(data),'verified_inner_checksums':0}
        if number in (14908055011,14906404653):
            checks=sums(data['SHA256SUMS'])
            require(set(checks)==set(data)-{'SHA256SUMS'},'Engine manifest coverage mismatch')
            for name,sha in checks.items():require(digest(data[name])==sha,'Engine inner checksum mismatch')
            rec['verified_inner_checksums']=len(checks)
            source_checks=sums(data['SOURCE_SHA256SUMS'])
            for name,sha in source_checks.items():require(digest(source_bytes(repo,name))==sha,'Source hash differs from exact Git commit')
            provenance=json.loads(data['provenance.json']); capture=json.loads(data['capture.json'])
            require(provenance=={'commit':SOURCE,'run':str(DSP_RUN),'analog_evidence':False},'Engine provenance mismatch')
            require(capture['parameter_count']==204 and capture['analog_evidence'] is False,'Capture scope/count mismatch')
            require(capture['sample_count']==96000 and capture['sample_rate_hz']==48000,'Engine capture dimensions mismatch')
            rec.update(source_hashes_verified=len(source_checks),parameter_count=204,
                       executable_hash_recorded_only=True,source_manifest_is_partial=True)
        elif number==14907385925:
            manifest=json.loads(data['manifest.json']); report=json.loads(data['report.json'])
            require(report['passed'] is True and report['analog_evidence'] is False
                    and manifest['evidence_kind']=='synthetic','Synthetic scope/outcome mismatch')
            require(manifest['provenance']['source_revision']==SOURCE and report['manifest_sha256']==digest(data['manifest.json']),'Synthetic source/manifest mismatch')
            for stage in manifest['stages']:
                for role in ('candidate','reference'):
                    row=stage[role];require(digest(data[row['path']])==row['sha256'],'Synthetic trace mismatch');rec['verified_inner_checksums']+=1
            for name,sha in manifest['provenance']['source_hashes'].items():require(digest(source_bytes(repo,name))==sha,'Synthetic generator source mismatch')
            require(all(s['passed'] for s in report['stages']),'Synthetic stage failed')
            rec['stages_passed']=len(report['stages'])
        elif number==14906817736:
            report=json.loads(data['measurements.json'])
            require(report['git_head']==SOURCE and report['working_tree_status']==[] and report['probe_failures']==0
                    and report['comparative_regression_passed'] is True,'Numerical outcome/source mismatch')
            for name,sha in report['analysis_time_source_sha256'].items():require(digest(source_bytes(repo,name))==sha,'Numerical source hash mismatch')
            require(report['decimator']['response_and_declared_latency_gates_passed'] is True,'Decimator gate failed')
            rec['source_hashes_verified']=len(report['analysis_time_source_sha256'])
        else:
            report=json.loads(data['report.json']); require(report['candidate']==SOURCE and report['analog_evidence'] is False,'Compatibility scope/source mismatch')
            checks=sums(data['SHA256SUMS']); missing=[]
            for name,sha in checks.items():
                basename=PurePosixPath(name).name
                if basename in data:
                    require(digest(data[basename])==sha,'Included compatibility payload differs');rec['verified_inner_checksums']+=1
                else:missing.append(name)
            rec['omitted_raw_trace_members']=missing
            rec['raw_comparison_scope']='Original successful cmp steps and equal recorded digest pairs; omitted raw traces are not independently rehashed.'
            values=list(checks.values())
            if number==14905979921:
                require(report['default_off_byte_identical'] is True and len(missing)==4,'Default-off comparison incomplete')
                require(report['corpus_sha256']==digest(data['baseline-corpus.txt']),'Corpus hash mismatch')
                require(checks['artifacts/compatibility/baseline-default.f32']==checks['artifacts/compatibility/current-default.f32']
                        and checks['artifacts/compatibility/baseline-tails.f32']==checks['artifacts/compatibility/current-tails.f32'],'Default-off paired digests differ')
            else:
                require(report['v1_circuit_bytes_unchanged'] and report['v2_audio_and_numeric_state_bytes_unchanged']
                        and len(missing)==4 and len(set(values))==2,'Source-factory comparison incomplete')
                require(checks['artifacts/source-factory-compatibility/audio-current.bin']==checks['artifacts/source-factory-compatibility/audio-v2.bin']
                        and checks['artifacts/source-factory-compatibility/circuit-current.bin']==checks['artifacts/source-factory-compatibility/circuit-v1.bin'],'Source-factory paired digests differ')
        records.append(rec)
    result['artifacts']=records
    sanitizer=next(j for j in dsp_jobs.values() if j['name']=='sanitizers')
    require(sanitizer['head_sha']==SOURCE and sanitizer['run_id']==DSP_RUN and sanitizer['run_attempt']==1,
            'Sanitizer source/run/attempt mismatch')
    result['sanitizer']={k:sanitizer[k] for k in ('id','status','conclusion','run_attempt','head_sha')}
    sanitizer_log=directory/f"job-{sanitizer['id']}.log"
    result['sanitizer']['original_log_verified']=False
    if sanitizer['status']=='completed' and sanitizer['conclusion']=='success' and sanitizer_log.exists():
        log=sanitizer_log.read_bytes()
        counts=re.findall(rb'100% tests passed, 0 tests failed out of ([0-9]+)',log)
        step=next(s for s in sanitizer['steps'] if s['name']=='Run sanitizer and validator-fixture tests')
        require(step['conclusion']=='success' and len(counts)==1 and SOURCE.encode() in log,'Sanitizer original outcome not established')
        result['sanitizer'].update(original_log_verified=True,registered_ctest_executions=int(counts[0]),
                                  original_log_sha256=digest(log))
    result['dsp_workflow_complete']=runs[DSP_RUN]['conclusion']=='success'
    result['supporting_qualification_complete']=result['dsp_workflow_complete'] and result['sanitizer']['original_log_verified']
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--repo',type=Path,default=Path.cwd())
    parser.add_argument('--archives',type=Path,required=True);args=parser.parse_args()
    directory=Path(__file__).resolve().parent
    result=verify(args.repo,directory,args.archives)
    (directory/'verification.json').write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({'verified_artifacts':len(result['artifacts']),'filter_executions':24,
                      'sanitizer':result['sanitizer'],'supporting_qualification_complete':result['supporting_qualification_complete']}))
