#!/usr/bin/env python3
"""Bind actual Intel Mac evidence to its original successful job. Executes no product.

Run only after all three actual Mac archives and original API/log inputs exist.
The platform result is separate from the overall cross-platform release gate.
"""
# Adapted with review from A50's c76 original-job Mac evidence binder; Intel target constants.
import argparse, hashlib, io, json, re, stat, subprocess, zipfile
from pathlib import Path, PurePosixPath
from datetime import datetime, timezone, timedelta

SOURCE = 'c76ad288e02fbeca48f39652bf4fcdd99837f793'
RUN = 45351731801
ATTEMPT = 1
PRODUCER = 138299557267
JUCE = 'c352e2489000ffeb9e397051377d75eef2efe641'
TARGET = 'macos-x86_64'
RUNNER = 'macos-15-intel'
ARCH = 'x86_64'
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
    run=read(ROOT/'run-attempt-api.json')
    jobs=read(ROOT/'jobs-api.json')['jobs']
    artifacts=read(ROOT/'artifacts-api.json')['artifacts']
    require(run['id']==RUN and run['run_attempt']==ATTEMPT and run['head_sha']==SOURCE,'Run identity')
    require(run['path']=='.github/workflows/build.yml' and run['event']=='push','Run workflow/event')
    require(run['repository']['full_name']==run['head_repository']['full_name']==REPOSITORY,'Repository identity')
    selected=[x for x in jobs if x['id']==PRODUCER]
    require(len(selected)==1,'Missing unique producer')
    producer=selected[0]
    require(producer['run_id']==RUN and producer['run_attempt']==ATTEMPT and producer['head_sha']==SOURCE,'Job provenance')
    require(producer['name']=='macOS / '+RUNNER+' / VST3 + AU + Standalone','Job runner name')
    require(producer['status']=='completed' and producer['conclusion']=='success','Original job incomplete/failed')
    required=['Record native platform','Configure','Verify configured JUCE source','Build','Test','Fetch pluginval 1.0.4','Register built AU','Validate VST3 with pluginval','Validate AU with pluginval','Validate AU with pluginval GUI','Validate AU GUI fixed-seed regression','Package native macOS VST3, AU and Standalone','Upload native macOS install bundle','Upload pluginval reports','Upload test and JUCE source evidence']
    required_steps=[step(producer,n) for n in required]
    require([s['number'] for s in required_steps]==sorted(s['number'] for s in required_steps),'Original step order')
    log=rows((ROOT/'producer-job.log').read_bytes())
    require(any(m==SOURCE for _,m in log),'Original checkout source absent')
    require(not any(re.search(r'(^##\[error\]|Process completed with exit code [1-9])',m) for _,m in log),'Original job error marker')
    arch_rows=[(t,m) for t,m in log if m==ARCH and t and in_step(t,step(producer,'Record native platform'))]
    require(len(arch_rows)==1,'Native uname architecture missing')
    sources={}
    for p in ('.github/workflows/build.yml','scripts/download-pluginval-macos.py','scripts/verify-ci-juce-source.cmake','scripts/package-ci-artifacts.py','scripts/verify-ci-bundle.py','CMakeLists.txt','tests/test_parity_capture.py'):
        raw=subprocess.check_output(['git','-C',str(REPO),'show',SOURCE+':'+p])
        sources[p]={'bytes':len(raw),'sha256':sha(raw)}
    workflow=subprocess.check_output(['git','-C',str(REPO),'show',SOURCE+':.github/workflows/build.yml']).decode()
    mac=workflow.split('\n  macos:\n')[1].split('\n  windows:\n')[0]
    require('continue-on-error' not in mac,'Mac failure suppression')
    test_block=mac.split('      - name: Test\n')[1].split('      - name: ')[0]
    require('ctest --test-dir build --output-on-failure --no-tests=error' in test_block and '--output-junit' in test_block,'Source CTest full-suite command')
    test_step=step(producer,'Test')
    test_commands=[m for t,m in log if t and in_step(t,test_step) and m.startswith('ctest --test-dir build ')]
    require(len(test_commands)==1 and test_commands[0].startswith('ctest --test-dir build --output-on-failure --no-tests=error --output-junit '),'Original CTest command differs')
    require(mac.count('--strictness-level 10')==4 and mac.count('--validate-in-process')==4 and mac.count('--skip-gui-tests')==2,'Four Mac lane definitions')
    require(mac.count("grep -qx 'SUCCESS'")==4 and '--random-seed 0x2b4eef5' in mac,'Success/seed contract')
    for name in ('Fetch pluginval 1.0.4','Register built AU','Validate VST3 with pluginval','Validate AU with pluginval','Validate AU with pluginval GUI','Validate AU GUI fixed-seed regression','Package native macOS VST3, AU and Standalone'):
        body=mac.split('      - name: '+name+'\n')[1].split('      - name: ')[0]
        require('set -euo pipefail' in body,'Missing fail-fast pipe propagation: '+name)
    require(mac.count('diff -qr "$AU_SOURCE" "$AU_DEST"')==2,'Whole AU directory comparisons')
    require(mac.index('codesign --force --sign - "$AU_SOURCE"')<mac.index('cp -R "$AU_SOURCE" "$AU_DEST"')<mac.index('diff -qr "$AU_SOURCE" "$AU_DEST"'),'AU signing/copy/diff order')
    specs=[('install','cs80-'+RUNNER+'-install-bundle','Upload native macOS install bundle'),('pluginval','cs80-pluginval-'+RUNNER,'Upload pluginval reports'),('test-evidence','cs80-'+RUNNER+'-test-evidence','Upload test and JUCE source evidence')]
    bindings=[];inventories=[]
    for stem,name,upload_name in specs:
        api=read(ROOT/(stem+'-api.json'))
        require([a for a in artifacts if a['name']==name]==[api],'Artifact collection identity: '+name)
        raw=(downloads/(stem+'.zip')).read_bytes()
        require(api['size_in_bytes']==len(raw) and api['digest']=='sha256:'+sha(raw),'Actual archive API hash/size: '+name)
        require(api['expired'] is False and stamp(api['expires_at'])>datetime.now(timezone.utc),'Expired archive')
        require(api['workflow_run']['id']==RUN and api['workflow_run']['head_sha']==SOURCE,'Artifact source/run')
        require(api['workflow_run']['repository_id']==run['repository']['id']==api['workflow_run']['head_repository_id'],'Artifact repository')
        upload=step(producer,upload_name)
        exact=f"Artifact {name} has been successfully uploaded! Final size is {len(raw)} bytes. Artifact ID is {api['id']}"
        indices=[i for i,(_,m) in enumerate(log) if m==exact]
        require(len(indices)==1,'Unique original artifact ID/name/size: '+name)
        i=indices[0];begin=max(k for k in range(i) if log[k][1]=='##[group]Run actions/upload-artifact@v5')
        body=[m for _,m in log[begin:i+1]]
        require('  name: '+name in body and 'SHA256 digest of uploaded artifact zip is '+sha(raw) in body,'Original upload digest/name: '+name)
        require(in_step(log[i][0],upload) and in_step(api['created_at'],upload) and in_step(api['updated_at'],upload),'Original upload time binding')
        bindings.append({'name':name,'artifact_id':api['id'],'api':api,'archive':record(downloads/(stem+'.zip')),'producer_job_id':PRODUCER,'attempt':ATTEMPT,'upload_step':upload,'logged_upload_utc':log[i][0],'original_upload_id_size_digest_and_time_bound':True})
        inventories.append(inventory(raw,stem+'.zip'))
    obs=read(ROOT/'native-validator-observations.json')
    parser_path=REPO/'docs/preset-evidence/2026.11.4/ci/source-078c098/parse-native-evidence.py'
    parser_raw=parser_path.read_bytes()
    require(sha(parser_raw)=='f44d978bcb6d0754165c8d3d55320ac63919f22b15bcb37a54506ec00db3cc58' and obs['parser']['sha256']==sha(parser_raw) and obs['parser']['bytes']==len(parser_raw),'Raw observation parser identity differs')
    input_raw=(ROOT/'parser-inputs.json').read_bytes()
    require(obs['input_manifest']['bytes']==len(input_raw) and obs['input_manifest']['sha256']==sha(input_raw),'Raw parser input manifest identity differs')
    context=read(ROOT/'parser-inputs.json')['provenance']
    require(context['source']==SOURCE and context['run_id']==RUN and context['attempt']==ATTEMPT,'Raw parser input context differs')
    require(obs['release_qualified'] is False and obs['all_requested_observations_green'],'Raw parser outcome/scope')
    require(len(obs['native'])==len(obs['validators'])==1,'Parser target count')
    native,validator=obs['native'][0],obs['validators'][0]
    require(native['target']==validator['target']==TARGET,'Parser target')
    # A58 F1: actual ZIP/API checks above must join to the archives parsed below.
    for observed, api, label in ((native, read(ROOT/'test-evidence-api.json'), 'native'), (validator, read(ROOT/'pluginval-api.json'), 'validator')):
        require(isinstance(observed.get('archive'), dict), label+': archive binding absent')
        require(observed['archive']['bytes']==api['size_in_bytes'], label+': parsed archive size differs')
        require(observed['archive']['sha256']==api['digest'].split(':',1)[1], label+': parsed archive digest differs')
    parser_binding=read(ROOT/'parser-archive-binding.json')
    require(parser_binding['status']=='parser_archive_and_member_input_binding_verified' and parser_binding['parser_archive_binding_verified'] is True,'A58 archive/member binding absent')
    require(parser_binding['source']==SOURCE and parser_binding['run_id']==RUN and parser_binding['attempt_context']==ATTEMPT and parser_binding['target']==TARGET,'A58 binding context differs')
    require(parser_binding['checker_sha256']=='cada3812da3285e91116b0273b8bda0f4782bcc05f968e450dfb4ed2f540b682','A58 checker changed')
    require(parser_binding['parser_sha256']=='f44d978bcb6d0754165c8d3d55320ac63919f22b15bcb37a54506ec00db3cc58','Historical native parser changed')
    for flag in ('native_results_accepted','original_attempt_binding_verified','platform_qualified','release_qualified'):
        require(parser_binding[flag] is False,'A58 limited input-binding scope changed: '+flag)
    for label, path in (('observations', ROOT/'native-validator-observations.json'), ('native_zip', downloads/'test-evidence.zip'), ('native_api', ROOT/'test-evidence-api.json'), ('validator_zip', downloads/'pluginval.zip'), ('validator_api', ROOT/'pluginval-api.json')):
        raw=path.read_bytes(); bound=parser_binding['inputs'][label]
        require(bound['bytes']==len(raw) and bound['sha256']==sha(raw),'A58 bound input differs: '+label)
    counts=native['counts'];cases={x['name']:x for x in native['testcases']}
    new=read(ROOT/'source-contract.json')['required_new_tests']
    old=read(REPO/('docs/preset-evidence/2026.11.4/ci/source-078c098/'+TARGET+'-native-observations.json'))['native'][0]
    require(set(cases)=={x['name'] for x in old['testcases']}|set(new),'Native roster changes beyond 15 additions')
    require(all(cases[x]['result']=='passed' and cases[x]['lasttest_status']=='Passed' for x in new),'New native tests not passed')
    require(counts['registered']==len(cases)==266 and counts['failed']==counts['disabled']==counts['unknown']==0,'Native registrations/outcomes')
    require({x['name'] for x in native['formal_skips']}=={'cs80_filter_batch_cap_tests','cs80_stereo_pra_exact_tests'},'Formal skip roster changed')
    require(counts['formal_skipped']==2 and counts['passed']==264,'Native passed/formal-skip count')
    provenance=dict(x.split('=',1) for x in native['juce_provenance']['text'].strip().splitlines())
    require(provenance['expected_sha']==provenance['actual_sha']==JUCE and provenance['expected_version']==provenance['actual_version']=='8.0.11','JUCE pin')
    require(provenance['workflow_commit']==SOURCE and provenance['workflow_run']==str(RUN) and provenance['source_clean']=='true' and provenance['verification']=='PASS','JUCE source/run/clean verification')
    require(provenance['configured_source']==provenance['expected_source'] and provenance['source_override']=='','Configured JUCE source')
    require(any(m in ('100% tests passed, 0 tests failed out of 266', '100% tests passed out of 266') for _,m in log),'Original full CTest terminal summary')
    require(validator['required_lanes']==validator['validation_invocations']==4 and validator['validation_retries']==0,'Validator invocations/retries')
    starts=[i for i,(_,m) in enumerate(log) if m.startswith('Started validating: ')]
    require(len(starts)==4,'Original validation-start count')
    lanes=[]
    with zipfile.ZipFile(downloads/'pluginval.zip') as z:
        expected={'pluginval-download.json','au-payload-before-validation.sha256','au-payload-after-validation.sha256','pluginval-vst3.log','pluginval-au.log','pluginval-au-gui.log','pluginval-au-seed-regression.log','pluginval-reports/cs80-vst3','pluginval-reports/cs80-au','pluginval-reports/cs80-au-gui','pluginval-reports/cs80-au-seed-regression'}
        require(set(z.namelist())==expected,'Exact validator artifact member roster')
        download=json.loads(decode(z.read('pluginval-download.json')),object_pairs_hook=unique)
        require(download['verified'] is True and download['version']=='1.0.4' and download['release_id']==188928656 and download['asset_id']==210999197,'Official validator identity')
        require(download['expected_size']==download['actual_size']==9202795 and download['expected_sha256']==download['actual_sha256']=='3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f','Validator binary pin')
        attempts=download['attempts'];require(1<=len(attempts)<=3 and [a['number'] for a in attempts]==list(range(1,len(attempts)+1)),'Download attempt numbering')
        routes=['https://api.github.com/repos/Tracktion/pluginval/releases/assets/210999197','https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip']
        require([a['source_url'] for a in attempts]==[routes[i] for i in (0,1,0)[:len(attempts)]],'Source-pinned download route order')
        require(attempts[-1]['transfer_exit']==0 and attempts[-1]['reason']=='downloaded' and attempts[-1]['bytes']==9202795,'Final validator download')
        for i,a in enumerate(attempts[:-1]):
            if a['transfer_exit']==12:
                require(i==0 and a['reason']=='api_forbidden' and a['http_status']==403,'Bounded API-forbidden route fallback')
            else:
                require(a['transfer_exit']==10 and a['reason'] in ('http','transport','truncated','wall_timeout'),'Ineligible download retry')
                if a['reason']=='http':require(a['http_status'] in (408,429) or 500<=a['http_status']<=599,'Ineligible HTTP retry')
        version='pluginval - 1.0.4'
        download_line='Verified official pluginval 1.0.4 macOS asset 210999197: 9202795 bytes, SHA256 3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f'
        require(any(m==download_line and t and in_step(t,step(producer,'Fetch pluginval 1.0.4')) for t,m in log),'Original verified validator download receipt absent')
        require(any(m==version and t and in_step(t,step(producer,'Fetch pluginval 1.0.4')) for t,m in log),'Executed pluginval version')
        for i,lane in enumerate(validator['lanes']):
            console=decode(z.read(lane['console']['path'])).splitlines();report=decode(z.read(lane['report']['path'])).splitlines()
            require(console[1:]==report,'Console/report copies differ')
            start=starts[i];end=start+len(console);require([m for _,m in log[start:end]]==console,'Artifact console differs from original runtime')
            s=step(producer,lane['required_workflow_step'])
            require(in_step(log[start][0],s) and in_step(log[end-1][0],s),'Runtime output outside its successful original step')
            for summary in (lane['console']['summary'],lane['report']['summary']):
                require(summary['validation_start_count']==1 and summary['strictness']==[10] and summary['program_counts']==[523] and summary['success_lines']==1 and summary['clean_text_success'],'Validator execution markers')
                require(summary['editor_test_started']==lane['gui_required'] and not summary['failure_markers'],'Validator GUI or failure mismatch')
            lanes.append({'lane':lane['lane'],'seed':lane['console']['summary']['seeds'][0],'original_step':s,'first_output_utc':log[start][0],'terminal_success_utc':log[end-1][0],'console_matches_original_job':True,'console_report_equivalent':True,'original_normal_exit':0,'numeric_exit_directly_printed':False,'exit_basis':'Original successful bash step with source-pinned set -euo pipefail around validator/tee and unsuppressed terminal SUCCESS check.','invocations':1,'retries':0,'validation_start_markers':1,'strictness':10,'programs':523,'gui':lane['gui_required'],'warnings':lane['console']['summary']['warnings'],'explicit_skips':lane['console']['summary']['explicit_skips']})
        require(lanes[3]['seed']=='0x2b4eef5','Required AU GUI fixed seed')
        au_records={}
        for filename,step_name in [('au-payload-before-validation.sha256','Register built AU'),('au-payload-after-validation.sha256','Package native macOS VST3, AU and Standalone')]:
            lines=decode(z.read(filename)).splitlines();s=step(producer,step_name)
            require(len(lines)==2,'AU executable hash pair')
            for line in lines:require(sum(m==line and t and in_step(t,s) for t,m in log)==1,'AU hash absent from original corresponding step')
            executed=[m for t,m in log if t and in_step(t,s)]
            require('diff -qr "$AU_SOURCE" "$AU_DEST"' in executed,'Original unsuppressed AU full-directory comparison absent')
            require('set -euo pipefail' in executed,'AU comparison error propagation absent')
            au_records[filename]={'original_step':s,'sha256':sha(z.read(filename)),'matching_original_hash_lines':lines,'whole_bundle_diff_original_step_success':True}
    install=read(ROOT/'install-acceptance.json');payload=read(ROOT/'payload-binding.json')
    require(install['status']=='artifact_bytes_verified' and install['release_qualified'] is False and install['native_validation_passed'] is None and install['artifact_attempt_binding_verified'] is False,'Install helper scope changed')
    require(install['source']==SOURCE and install['run_id']==RUN and install['run_attempt']==ATTEMPT and install['target']==TARGET and install['preset_library_revision']=='2026.11.5','Install helper context')
    actual_install=read(ROOT/'install-api.json')
    require(install['artifact']['id']==actual_install['id'] and install['outer_zip']['sha256']==actual_install['digest'].split(':')[1] and install['outer_zip']['bytes']==actual_install['size_in_bytes'],'Install helper actual-byte binding')
    require(payload['status']=='archive_consistency_and_au_executable_binding_verified' and payload['release_qualified'] is False and payload['validated_bundle_identity_established'] is False and payload['run_attempt_bound'] is False,'Mac binding helper scope changed')
    require(payload['expected']=={'source':SOURCE,'run':RUN,'attempt':ATTEMPT,'architecture':ARCH,'revision':'2026.11.5','repository':REPOSITORY},'Mac binding helper context')
    require(payload['downloads']['install']['sha256']==actual_install['digest'].split(':')[1] and payload['downloads']['pluginval']['sha256']==read(ROOT/'pluginval-api.json')['digest'].split(':')[1],'Mac binding report actual bytes')
    # Original successful step ordering plus the reviewed source has no rebuild or product mutation
    # between VST3 validation and packaging. AU signing happens before all validations; successful
    # full-directory comparisons before/after bind resources as well as the hashed executable.
    order=[s['name'] for s in producer['steps'] if step(producer,'Build')['number']<s['number']<=step(producer,'Package native macOS VST3, AU and Standalone')['number']]
    require(order==['Test','Fetch pluginval 1.0.4','Register built AU','Validate VST3 with pluginval','Validate AU with pluginval','Validate AU with pluginval GUI','Validate AU GUI fixed-seed regression','Package native macOS VST3, AU and Standalone'],'Unexpected intermediate native step')
    save('zip-member-inventories.json',inventories)
    result={'schema':1,'worker':'a51','status':'macos_x86_64_platform_qualified','platform_qualified':True,'release_qualified':False,'source':SOURCE,'tree':read(ROOT/'source-contract.json')['tree'],'run_id':RUN,'attempt':ATTEMPT,'target':TARGET,'verified_at_utc':datetime.now(timezone.utc).isoformat(),'producer_job_id':PRODUCER,'original_required_steps':required_steps,'source_contract':sources,'artifact_bindings':bindings,'native':{'counts':counts,'new_tests_all_passed':new,'corrected_parity_capture_test':cases['cs80_parity_capture_tests'],'old_roster_preserved':True,'formal_skips':native['formal_skips'],'nested_python_skips':native['nested_python_skips'],'seconds':native['ctest_seconds_junit'],'juce_provenance':native['juce_provenance'],'uname_m':ARCH,'original_ctest_exit_zero_bound':True,'numeric_exit_directly_printed':False,'exit_basis':'Original Test step completed/success with direct unsuppressed CTest command.','raw_parser':record(ROOT/'native-validator-observations.json')},'validators':{'lanes':lanes,'invocations':4,'validation_retries':0,'download':download,'version':version},'payload_binding_external':{'whole_au_before_after':au_records,'vst3_original_path':payload['validator_input_paths']['vst3']['path'],'source_packager_input_matches':True,'original_build_validation_package_order_verified':True,'no_intervening_rebuild_or_product_mutation':True,'complete_au_and_vst3_identity_established_by_original_execution_and_archive_checks':True},'install_acceptance':record(ROOT/'install-acceptance.json'),'payload_binding':record(ROOT/'payload-binding.json'),'parser_archive_binding':record(ROOT/'parser-archive-binding.json'),'a58_f1_closed_with_actual_archive_and_member_records':True,'raw_input_records':[record(ROOT/n) for n in ('run-attempt-api.json','jobs-api.json','artifacts-api.json','producer-job.log')],'limits':['Intel Mac platform scope only; remaining native platforms and final release gates remain separate.','No publisher signing, notarization, every-DAW compatibility or interactive installation certification.','No human listening or acoustic-equivalence determination.','Original child numeric exits are bound through successful steps and unsuppressed source propagation, not separately printed raw termination records.','AU executable hashes alone do not bind resources; original successful full-directory comparisons are separately required and retained.']}
    save('acceptance-summary.json',result)
    print(json.dumps({'status':result['status'],'native':counts,'nested_python_skips':native['nested_python_skips']['total'],'lanes':len(lanes),'artifacts':[{'id':a['artifact_id'],'name':a['name']} for a in bindings]}))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--downloads',type=Path,required=True)
    args=p.parse_args();main(args.downloads.resolve())
