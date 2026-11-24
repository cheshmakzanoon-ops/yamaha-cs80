#!/usr/bin/env python3
"""Bind actual Windows evidence to its original successful jobs. Executes no product.

Run only after all four actual Windows archives and original API/log inputs exist.
The platform result is separate from the overall cross-platform release gate.
"""
import argparse, hashlib, io, json, re, stat, subprocess, zipfile
from pathlib import Path, PurePosixPath
from datetime import datetime, timezone, timedelta

SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
PRIOR = 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
RUN = 45366603800
ATTEMPT = 1
PRODUCER = 138349817051
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'
ROOT = Path(__file__).resolve().parent
REPO = ROOT.parents[5]
REPOSITORY = 'cheshmakzanoon-ops/yamaha-cs80'

def require(ok, reason):
    if not ok: raise ValueError(reason)

def unique(pairs):
    out = {}
    for key, value in pairs:
        require(key not in out, 'Duplicate JSON key: '+key); out[key] = value
    return out

def read(path):
    return json.loads(Path(path).read_bytes(), object_pairs_hook=unique)

def sha(raw): return hashlib.sha256(raw).hexdigest()
def stamp(s): return datetime.fromisoformat(s.replace('Z','+00:00'))
def decode(raw): return raw.decode('utf-16' if raw[:2] in (b'\xff\xfe',b'\xfe\xff') else 'utf-8-sig').replace('\r\n','\n')
def record(path):
    p=Path(path); raw=p.read_bytes()
    return {'path':str(p),'bytes':len(raw),'sha256':sha(raw)}
def save(name,obj):
    with (ROOT/name).open('x') as f: json.dump(obj,f,indent=2); f.write('\n')
def rows(raw):
    text=re.sub(r'\x1b\[[0-9;]*m','',decode(raw))
    return [(m[1],m[2]) if (m:=re.match(r'^(\d{4}-\d\d-\d\dT[^ ]+Z) (.*)$',line)) else (None,line) for line in text.splitlines()]
def step(job,name):
    items=[s for s in job['steps'] if s['name']==name]
    require(len(items)==1 and items[0]['status']=='completed' and items[0]['conclusion']=='success','Original step not successful: '+name)
    return items[0]
def in_step(timestamp,s):
    return stamp(s['started_at']) <= stamp(timestamp) < stamp(s['completed_at'])+timedelta(seconds=1)

def inventory(raw,label):
    out=[]; names={}; folded=set()
    require(0<len(raw)<=512*1024**2,label+': ZIP bound')
    with zipfile.ZipFile(io.BytesIO(raw)) as z:
        members=z.infolist()
        require(0<len(members)<=4096 and sum(m.file_size for m in members)<=1024**3,label+': expanded bound')
        for m in members:
            n=m.filename.rstrip('/') if m.is_dir() else m.filename
            require(n and not n.startswith('/') and '\\' not in n and PurePosixPath(n).as_posix()==n,'Noncanonical ZIP path')
            require(all(p not in ('','.','..') and ':' not in p for p in n.split('/')),'Unsafe ZIP path')
            require(all(ord(c)>=32 and ord(c)!=127 for c in n),'Control character')
            require(n not in names and n.casefold() not in folded,'Duplicate/case folded ZIP path')
            require(not m.flag_bits&1 and m.compress_type in (0,8),'Encrypted/unsupported ZIP')
            require(0<=m.file_size<=256*1024**2,'Member size bound')
            kind=stat.S_IFMT(m.external_attr>>16)
            require(kind in (0,stat.S_IFDIR if m.is_dir() else stat.S_IFREG),'Unsupported ZIP type')
            data=z.read(m);require(len(data)==m.file_size and (not m.is_dir() or not data),'Member length')
            names[n]='directory' if m.is_dir() else 'file';folded.add(n.casefold())
            out.append({'path':m.filename,'type':names[n],'bytes':len(data),'sha256':sha(data),'crc32':f'{m.CRC:08x}','mode':oct(m.external_attr>>16)})
        for n in names:
            require(all(names.get(str(p),'directory')=='directory' for p in PurePosixPath(n).parents if str(p)!='.'),'File parent collision')
    return {'archive':label,'bytes':len(raw),'sha256':sha(raw),'members':out,'all_member_bytes_crc_and_hashes_checked':True}

def main(downloads):
    run=read(ROOT/'run-attempt-final-api.json')
    jobs=read(ROOT/'jobs-final-api.json')['jobs']
    artifacts=read(ROOT/'artifacts-final-api.json')['artifacts']
    require(run['id']==RUN and run['run_attempt']==ATTEMPT and run['head_sha']==SOURCE,'Run identity')
    require(run['path']=='.github/workflows/build.yml' and run['event']=='push','Run workflow/event')
    require(run['repository']['full_name']==run['head_repository']['full_name']==REPOSITORY,'Repository identity')
    producer=next(x for x in jobs if x['id']==PRODUCER)
    consumer_run=read(ROOT/'validator-run-attempt-api.json')
    consumer_jobs=read(ROOT/'validator-attempt-jobs-api.json')['jobs']
    require(consumer_run['id']==RUN and consumer_run['head_sha']==SOURCE and type(consumer_run['run_attempt']) is int and consumer_run['run_attempt']>0,'Consumer exact-attempt run context')
    consumer_attempt=consumer_run['run_attempt']
    require(consumer_attempt==ATTEMPT,'Different validator attempt requires a new reviewed qualification protocol')
    consumers=[x for x in consumer_jobs if x['name']=='pluginval / Windows VST3 (strictness 10)']
    require(len(consumers)==1,'Missing unique Windows consumer in exact-attempt collection')
    consumer=consumers[0]
    required_producer=['Configure','Verify configured JUCE source','Build','Test','Upload Windows VST3','Package Windows VST3 and Standalone','Upload Windows install bundle','Upload test and JUCE source evidence']
    required_consumer=['Download Windows VST3','Fetch pluginval 1.0.4','Validate VST3 headless','Validate state regression seed 0x5037a0c','Upload pluginval reports']
    for job,names,job_attempt in ((producer,required_producer,ATTEMPT),(consumer,required_consumer,consumer_attempt)):
        require(job['run_id']==RUN and job['run_attempt']==job_attempt and job['head_sha']==SOURCE,'Job provenance')
        require(job['status']=='completed' and job['conclusion']=='success','Original job incomplete/failed')
        for n in names: step(job,n)
    producer_rows=rows((ROOT/'producer-job.log').read_bytes())
    consumer_rows=rows((ROOT/'validator-job.log').read_bytes())
    for kind,data in (('producer',producer_rows),('consumer',consumer_rows)):
        require(any(m==SOURCE for _,m in data),kind+': original checkout source absent')
        require(not any(re.search(r'(^##\[error\]|Process completed with exit code [1-9]|0xC0000005)',m) for _,m in data),kind+': original failure marker')
    sources={}
    for p in ('.github/workflows/build.yml','scripts/download-pluginval-windows.py','scripts/verify-ci-juce-source.cmake','scripts/package-ci-artifacts.py','scripts/verify-ci-bundle.py','CMakeLists.txt'):
        raw=subprocess.check_output(['git','-C',str(REPO),'show',SOURCE+':'+p])
        require(raw==(REPO/p).read_bytes(),'Working source changed: '+p)
        sources[p]={'bytes':len(raw),'sha256':sha(raw)}
    workflow=subprocess.check_output(['git','-C',str(REPO),'show',SOURCE+':.github/workflows/build.yml']).decode()
    native_section=workflow.split('\n  windows:\n')[1].split('\n  pluginval-windows:\n')[0]
    validator_section=workflow.split('\n  pluginval-windows:\n')[1]
    require('continue-on-error' not in validator_section,'Validator failure suppression')
    require(validator_section.count('if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }')==4,'Validator exit propagation')
    require(validator_section.count('--strictness-level 10')==2 and validator_section.count('--skip-gui-tests')==2 and validator_section.count('--validate-in-process')==2,'Validator lane contract')
    require('--random-seed 0x5037a0c' in validator_section and validator_section.count("-Pattern '^SUCCESS$'")==2,'Fixed seed/success gates')
    test_block=native_section.split('      - name: Test\n')[1].split('      - name: ')[0]
    require('continue-on-error' not in test_block and 'ctest --test-dir build -C Release --output-on-failure --no-tests=error' in test_block,'Original CTest command')
    specifications=[
        ('install','cs80-windows-install-bundle',producer,producer_rows,'Upload Windows install bundle'),
        ('vst3','cs80-windows-vst3',producer,producer_rows,'Upload Windows VST3'),
        ('test-evidence','cs80-windows-test-evidence',producer,producer_rows,'Upload test and JUCE source evidence'),
        ('pluginval','cs80-pluginval-windows',consumer,consumer_rows,'Upload pluginval reports')]
    bindings=[]; inventories=[]
    for stem,name,job,log,upload_name in specifications:
        api=read(ROOT/(stem+'-api.json'))
        matches=[a for a in artifacts if a['name']==name]
        require(matches==[api],'Artifact differs from final authentic collection: '+name)
        raw=(downloads/(stem+'.zip')).read_bytes()
        require(api['size_in_bytes']==len(raw) and api['digest']=='sha256:'+sha(raw),'Artifact bytes mismatch: '+name)
        require(api['expired'] is False and stamp(api['expires_at'])>datetime.now(timezone.utc),'Artifact expired')
        require(api['workflow_run']['id']==RUN and api['workflow_run']['head_sha']==SOURCE,'Artifact source/run')
        require(api['workflow_run']['repository_id']==run['repository']['id']==api['workflow_run']['head_repository_id'],'Artifact repository')
        s=step(job,upload_name)
        exact=f"Artifact {name} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {api['id']}"
        indices=[i for i,(_,m) in enumerate(log) if m==exact]
        require(len(indices)==1,'Original unique upload ID/name/size missing: '+name)
        i=indices[0]; start=max(k for k in range(i) if log[k][1]=='##[group]Run actions/upload-artifact@v5')
        section=[m for _,m in log[start:i+1]]
        require('  name: '+name in section and 'SHA256 digest of uploaded artifact zip is '+sha(raw) in section,'Original upload digest/name mismatch')
        require(in_step(log[i][0],s) and in_step(api['created_at'],s) and in_step(api['updated_at'],s),'Upload timestamp not in original step')
        bindings.append({'name':name,'artifact_id':api['id'],'api':api,'archive':record(downloads/(stem+'.zip')),'producer_job_id':job['id'],'attempt':job['run_attempt'],'upload_step':s,'logged_upload_utc':log[i][0],'original_upload_id_size_digest_and_time_bound':True})
        inventories.append(inventory(raw,stem+'.zip'))
    with zipfile.ZipFile(downloads/'install.zip') as z:
        inner=[n for n in z.namelist() if n.endswith('.zip')]
        require(len(inner)==1,'Inner installation archive')
        inventories.append(inventory(z.read(inner[0]),inner[0]))
    with zipfile.ZipFile(downloads/'test-evidence.zip') as z:
        require(set(z.namelist())=={'Testing/Temporary/LastTest.log','ctest-results.xml','ci-juce-provenance.txt'},'Native evidence member roster')
        lasttest=decode(z.read('Testing/Temporary/LastTest.log'))
    observations=read(ROOT/'native-validator-observations.json')
    require(observations['release_qualified'] is False and observations['all_requested_observations_green'],'Raw parser outcomes/scope')
    require(len(observations['native'])==len(observations['validators'])==1,'Unexpected parser targets')
    native=observations['native'][0]; validator=observations['validators'][0]
    require(native['target']==validator['target']=='windows-x86_64','Parser target')
    for observed,api,label in ((native,read(ROOT/'test-evidence-api.json'),'native'),(validator,read(ROOT/'pluginval-api.json'),'validator')):
        require(isinstance(observed.get('archive'),dict),label+': parser archive binding absent')
        require(observed['archive']['bytes']==api['size_in_bytes'] and observed['archive']['sha256']==api['digest'].split(':',1)[1],label+': parser archive differs from actual ZIP/API')
    parser_binding=read(ROOT/'parser-archive-binding.json')
    require(parser_binding['status']=='parser_archive_and_member_input_binding_verified' and parser_binding['parser_archive_binding_verified'] is True,'Independent parser/member binding absent')
    require(parser_binding['source']==SOURCE and parser_binding['run_id']==RUN and parser_binding['attempt_context']==ATTEMPT and parser_binding['target']=='windows-x86_64','Parser-binding context')
    require(parser_binding['checker_sha256']==sha((REPO/'docs/preset-evidence/2026.11.6/verification/a52/bind-parser-archives.py').read_bytes()) and parser_binding['parser_sha256']=='f192bf2ff70d4501f73875eb5bb6416b9adc3b77912e165da4305302724cd846','Parser-binding checker/source identity')
    require(all(parser_binding[k] is False for k in ('native_results_accepted','original_attempt_binding_verified','platform_qualified','release_qualified')),'Independent input checker scope changed')
    for key,path in {'observations':ROOT/'native-validator-observations.json','native_zip':downloads/'test-evidence.zip','native_api':ROOT/'test-evidence-api.json','validator_zip':downloads/'pluginval.zip','validator_api':ROOT/'pluginval-api.json'}.items():
        actual=record(path);bound=parser_binding['inputs'][key]
        require(bound['bytes']==actual['bytes'] and bound['sha256']==actual['sha256'],'Independent input binding is stale: '+key)

    counts=native['counts']
    require(counts['registered']==251 and counts['failed']==counts['disabled']==counts['unknown']==0,'Native count/fail/disabled/unknown')
    new=read(ROOT/'source-contract.json')['required_new_tests']
    cases={c['name']:c for c in native['testcases']}
    require(all(n in cases and cases[n]['result']=='passed' and cases[n]['lasttest_status']=='Passed' for n in new),'Missing/failing new tests')
    family=read(ROOT/'source-contract.json')['required_family_tests']
    require(all(n in cases and cases[n]['result']=='passed' for n in family),'Missing/failing family gate')
    historical_path=REPO/'docs/preset-evidence/2026.11.5/ci/source-c76ad28/windows/native-validator-observations.json'
    require(sha(historical_path.read_bytes())=='bc9de21429fe6a147c972a034a837a774d5e712b205ef868100fbddce0510792','Historical Windows roster bytes changed')
    historical=read(historical_path)
    old=historical['native'][0] if 'native' in historical else historical
    delta=subprocess.check_output(['git','-C',str(REPO),'diff','--unified=0',PRIOR,SOURCE,'--','CMakeLists.txt'],text=True)
    require(not any(line.startswith('-') and not line.startswith('---') for line in delta.splitlines()),'Registration changed beyond additions')
    added=re.findall(r'^\+\s*add_test\(NAME\s+(\w+)',delta,re.M)
    require(len(added)==len(set(added))==7 and set(added)==set(new),'Seven new registrations do not match Git source')
    require(set(cases)=={c['name'] for c in old['testcases']}|set(new),'Native roster differs beyond seven additions')
    require({x['name'] for x in native['formal_skips']}=={'cs80_filter_batch_cap_tests','cs80_stereo_pra_exact_tests'},'Unexpected formal skip roster')
    require(all(x['message']=='SKIP_RETURN_CODE=77' for x in native['formal_skips']),'Formal skip contract')
    require(counts['passed']==249 and counts['formal_skipped']==2,'Actual native execution count')
    expected_nested={s['ctest_test']:s['total'] for s in old['nested_python_skips']['suites']}
    require(native['nested_python_skips']['total']==11 and {s['ctest_test']:s['total'] for s in native['nested_python_skips']['suites']}==expected_nested,'Nested Windows skips changed; independent review required')
    host_tests=['cs80_ui_smoke','cs80_parameter_state_index_tests','cs80_typed_value_unit_tests']
    require(all(cases[n]['result']=='passed' for n in host_tests),'Native host/UI gates')
    require('UI SMOKE PHASE: instrument body host identity and recall' in lasttest and 'UI SMOKE TIMING: phase=instrument body host identity and recall' in lasttest,'Expanded body-host phase not executed')
    provenance=dict(line.split('=',1) for line in native['juce_provenance']['text'].strip().splitlines())
    require(provenance['expected_sha']==provenance['actual_sha']==JUCE and provenance['expected_version']==provenance['actual_version']=='8.0.11','JUCE pin')
    require(provenance['workflow_commit']==SOURCE and provenance['workflow_run']==str(RUN) and provenance['source_clean']=='true' and provenance['verification']=='PASS','JUCE source/run/clean verification')
    require(provenance['configured_source']==provenance['expected_source'] and provenance['source_override']=='','JUCE configured source')
    require(any(m=='100% tests passed, 0 tests failed out of 251' for _,m in producer_rows),'Original CTest terminal summary')
    require(validator['required_lanes']==validator['validation_invocations']==2 and validator['validation_retries']==0,'Validator invocation count')
    starts=[i for i,(_,m) in enumerate(consumer_rows) if m.startswith('Started validating: ')]
    require(len(starts)==2,'Original validation starts')
    lanes=[]
    with zipfile.ZipFile(downloads/'pluginval.zip') as z:
        expected={'pluginval-download.json','pluginval-version.txt','pluginval-windows.log','pluginval-windows-seed-regression.log','pluginval-reports/cs80-windows','pluginval-reports/cs80-windows-seed-regression'}
        require(set(z.namelist())==expected,'Validator member roster')
        download=json.loads(decode(z.read('pluginval-download.json')),object_pairs_hook=unique)
        require(download['verified'] is True and download['version']=='1.0.4' and download['release_id']==188928656 and download['asset_id']==210999198,'Official validator identity')
        require(download['expected_size']==download['actual_size']==2408590 and download['expected_sha256']==download['actual_sha256']=='c08e61ce3b96db41636f8ec7e76f4c7e2c13ebdac7fa1b5a1f52b4f32ec715ab','Validator binary pin')
        attempts=download['attempts']
        require(1<=len(attempts)<=3 and [a['number'] for a in attempts]==list(range(1,len(attempts)+1)),'Validator download attempts')
        routes=['https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999198','https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip']
        require([a['source_url'] for a in attempts]==[routes[i] for i in (0,1,0)[:len(attempts)]],'Validator download route order')
        require(attempts[-1]['transfer_exit']==0 and attempts[-1]['reason']=='downloaded' and attempts[-1]['bytes']==2408590,'Validator final download')
        for index,a in enumerate(attempts[:-1]):
            if a['transfer_exit']==12:
                require(index==0 and a['reason']=='http' and a['http_status']==403 and len(attempts)==2,'Alternate-route download policy')
            else:
                require(a['transfer_exit']==10 and a['reason'] in ('http','transport','truncated','wall_timeout'),'Non-transient download retry')
                if a['reason']=='http': require(a['http_status'] in (408,429) or 500<=a['http_status']<=599,'Ineligible HTTP retry')
        version=decode(z.read('pluginval-version.txt')).strip()
        require(version=='pluginval - 1.0.4' and any(m==version for _,m in consumer_rows),'Executed validator version')
        for i,lane in enumerate(validator['lanes']):
            console=decode(z.read(lane['console']['path'])).splitlines()
            report=decode(z.read(lane['report']['path'])).splitlines()
            require(console[1:]==report,'Console/report copies differ')
            start=starts[i];end=start+len(console)
            require([m for _,m in consumer_rows[start:end]]==console,'Console differs from original runtime log')
            require(consumer_rows[end][1].startswith('##[group]Run '),'Unexpected post-SUCCESS output')
            s=step(consumer,lane['required_workflow_step'])
            require(in_step(consumer_rows[start][0],s) and in_step(consumer_rows[end-1][0],s),'Validator runtime outside successful step')
            for summary in (lane['console']['summary'],lane['report']['summary']):
                require(summary['validation_start_count']==1 and summary['strictness']==[10] and summary['program_counts']==[523] and summary['success_lines']==1 and summary['clean_text_success'],'Validator markers/counts')
                require(not summary['editor_test_started'] and not summary['warnings'] and not summary['failure_markers'],'Unexpected warnings/failures/GUI')
            lanes.append({'lane':lane['lane'],'seed':lane['console']['summary']['seeds'][0],'original_step':s,'first_output_utc':consumer_rows[start][0],'terminal_success_utc':consumer_rows[end-1][0],'console_matches_original_job':True,'console_report_equivalent':True,'original_normal_exit':0,'numeric_exit_directly_printed':False,'exit_basis':'Original successful PowerShell step plus immediate source-pinned LASTEXITCODE check; no continue-on-error.','invocations':1,'retries':0,'validation_start_markers':1,'strictness':10,'programs':523,'gui':False,'explicit_skips':lane['console']['summary']['explicit_skips']})
    require(lanes[1]['seed']=='0x5037a0c','Fixed-seed replay')
    install=read(ROOT/'install-acceptance.json')
    require(install['status']=='artifact_bytes_verified' and install['release_qualified'] is False and install['native_validation_passed'] is None and install['artifact_attempt_binding_verified'] is False,'Install helper outcome/scope changed')
    require(install['source']==SOURCE and install['run_id']==RUN and install['run_attempt']==ATTEMPT and install['target']=='windows-x86_64' and install['preset_library_revision']=='2026.11.6','Install helper context')
    actual_install=read(ROOT/'install-api.json')
    require(install['artifact']['id']==actual_install['id'] and install['outer_zip']['sha256']==actual_install['digest'].split(':')[1] and install['outer_zip']['bytes']==actual_install['size_in_bytes'],'Install report byte binding')
    payload=read(ROOT/'payload-binding.json')
    require(payload['status']=='distributed_vst3_matches_validator_input' and payload['source']==SOURCE and payload['run_id']==RUN,'Payload equality missing')
    require(payload['release_qualified'] is False and payload['native_validation_passed'] is None and payload['run_context']['artifact_attempt_binding_verified'] is False,'Payload helper scope changed')
    require(payload['attempt']==ATTEMPT and payload['revision']=='2026.11.6' and payload['platform']=='windows','Payload helper context')
    actual_vst3=read(ROOT/'vst3-api.json')
    require(payload['installation_artifact']['artifact_id']==actual_install['id'] and payload['installation_artifact']['sha256']==actual_install['digest'].split(':')[1] and payload['validator_input_artifact']['artifact_id']==actual_vst3['id'] and payload['validator_input_artifact']['sha256']==actual_vst3['digest'].split(':')[1],'Payload report byte binding')
    download_step=step(consumer,'Download Windows VST3')
    downloaded=[(t,m) for t,m in consumer_rows if t and in_step(t,download_step)]
    receipt=f"- cs80-windows-vst3 (ID: {actual_vst3['id']}, Size: {actual_vst3['size_in_bytes']}, Expected Digest: {actual_vst3['digest']})"
    require(sum(m==receipt for _,m in downloaded)==1,'Original consumer artifact receipt differs')
    digest_line='SHA256 digest of downloaded artifact is '+actual_vst3['digest'].split(':',1)[1]
    require(sum(m==digest_line for _,m in downloaded)==1,'Original consumer downloaded digest differs')
    require(any(m=='Artifact download completed successfully.' for _,m in downloaded),'Original consumer download incomplete')
    selected_paths=[m[len('Started validating: '):] for _,m in consumer_rows if m.startswith('Started validating: ')]
    require(len(selected_paths)==2 and all(p.endswith('\\vst3\\CS-80 Reference.vst3') for p in selected_paths),'Original consumer selected unexpected plugin paths')
    container=payload.get('validator_container_contract');container_log_bound=False
    if container is not None:
        original=(ROOT/'validator-job.log').read_bytes();bound=container['consumer_log']
        require(bound['bytes']==len(original) and bound['sha256']==sha(original),'A36 consumer-log bytes differ from original platform log')
        require(bound['started_validation_paths']==selected_paths,'A36 original selected paths differ')
        container_log_bound=True
    else:
        require(not payload['validator_container_sidecars'],'Sidecars lack A36 container contract')
    consumer_binding={'unconditional_receipt_digest_and_completion_verified':True,'download_step':download_step,'artifact_id':actual_vst3['id'],'archive_bytes':actual_vst3['size_in_bytes'],'archive_sha256':actual_vst3['digest'].split(':',1)[1],'matching_original_rows':[{'timestamp':t,'message':m} for t,m in downloaded if m in (receipt,digest_line,'Artifact download completed successfully.')],'selected_validation_paths':selected_paths,'selected_paths_checked_independently_of_sidecars':True,'a36_container_contract_present':container is not None,'a36_consumer_log_size_and_sha_bound':container_log_bound,'original_log':record(ROOT/'validator-job.log')}

    save('zip-member-inventories.json',inventories)
    result={'schema':1,'worker':'a46','status':'windows_platform_qualified','platform_qualified':True,'release_qualified':False,'source':SOURCE,'run_id':RUN,'attempt':ATTEMPT,'target':'windows-x86_64','verified_at_utc':datetime.now(timezone.utc).isoformat(),'producer_job_id':producer['id'],'producer_run_attempt':producer['run_attempt'],'validator_job_id':consumer['id'],'validator_run_attempt':consumer['run_attempt'],'attempt_scope':'Top-level attempt is the native producer context. Each original artifact upload is independently bound to its actual producing job attempt; validator context is separately retained.','original_required_producer_steps':[step(producer,n) for n in required_producer],'original_required_consumer_steps':[step(consumer,n) for n in required_consumer],'source_contract':sources,'artifact_bindings':bindings,'native':{'counts':counts,'new_tests_all_passed':new,'old_roster_preserved':True,'formal_skips':native['formal_skips'],'nested_python_skips':native['nested_python_skips'],'seconds':native['ctest_seconds_junit'],'juce_provenance':native['juce_provenance'],'original_ctest_exit_zero_bound':True,'numeric_exit_directly_printed':False,'exit_basis':'Original Test step completed/success; direct source-pinned CTest command without suppression.','raw_parser':record(ROOT/'native-validator-observations.json')},'validators':{'lanes':lanes,'invocations':2,'validation_retries':0,'download':download,'version':version},'install_acceptance':record(ROOT/'install-acceptance.json'),'payload_binding':record(ROOT/'payload-binding.json'),'raw_input_records':[record(ROOT/n) for n in ('run-attempt-final-api.json','jobs-final-api.json','artifacts-final-api.json','validator-run-attempt-api.json','validator-attempt-jobs-api.json','producer-job.log','validator-job.log')],'limits':['This qualifies only the original Windows platform scope; all other platforms and release gates remain separate.','Windows lanes are headless and explicitly do not run the independent VST3 SDK validator.','No human listening or acoustic-equivalence determination.','No publisher signing/DAW compatibility assertion.','Original Windows child numeric exits are bound through successful steps and source propagation, not separately printed raw termination records.']}
    result['a58_closures']={'F1':{'status':'closed_with_actual_zip_and_member_inputs','parser_archive_binding':record(ROOT/'parser-archive-binding.json'),'actual_archives_bound':len(parser_binding['bindings']),'raw_parser_scope_unchanged':True},'F2':{'status':'closed_with_unconditional_consumer_receipt','consumer_binding':consumer_binding}}
    result['qualification_protocol_source'] = record(Path(__file__))
    result['native']['host_tests_passed']=host_tests
    result['native']['expanded_body_host_phase_executed']=True
    result['native']['nested_skip_suite_counts_unchanged']=expected_nested
    save('platform-qualification.json',result)
    print(json.dumps({'status':result['status'],'native':counts,'nested_python_skips':native['nested_python_skips']['total'],'lanes':len(lanes),'artifacts':[{'id':a['artifact_id'],'name':a['name']} for a in bindings]}))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--downloads',type=Path,required=True)
    args=p.parse_args();main(args.downloads.resolve())
