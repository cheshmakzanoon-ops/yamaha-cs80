#!/usr/bin/env python3
"""Inspect preserved original failed Mac evidence without converting it to acceptance."""
from pathlib import Path
import hashlib,importlib.util,json,sys,zipfile
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent;REPO=ROOT.parents[6]
DOWNLOADS=REPO.parent/'ci-c76-macos-arm64-downloads/attempt-1-failed'
def module(name,path):
 spec=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
b=module('a50_binder',ROOT.parent/'qualify-macos-arm64.py');require=b.require
parser_path=REPO/'docs/preset-evidence/2026.11.4/ci/source-078c098/parse-native-evidence.py'
require(b.sha(parser_path.read_bytes())=='f44d978bcb6d0754165c8d3d55320ac63919f22b15bcb37a54506ec00db3cc58','Historical parser changed')
p=module('a50_native_parser',parser_path)
job=next(x for x in b.read(ROOT/'jobs-api.json')['jobs'] if x['id']==138299557178)
log=b.rows((ROOT/'original-producer-job.log').read_bytes())
require(job['conclusion']=='failure' and job['run_attempt']==1 and job['head_sha']==b.SOURCE,'Original failure provenance')
obs=b.read(ROOT/'native-only-observations.json');require(obs['release_qualified'] is False and obs['validators']==[],'Native-only scope')
n=obs['native'][0];api=b.read(ROOT/'test-evidence-api.json');raw=(DOWNLOADS/'test-evidence.zip').read_bytes()
require(n['archive']['bytes']==len(raw)==api['size_in_bytes'] and n['archive']['sha256']==b.sha(raw)==api['digest'].split(':')[1],'Native observation actual archive binding')
with zipfile.ZipFile(DOWNLOADS/'test-evidence.zip') as z:
 for rec in [*n['inputs'].values(),n['juce_provenance']]:
  member=z.read(rec['path']);require(rec['bytes']==len(member) and rec['sha256']==b.sha(member),'Native parsed member binding')
provenance=dict(x.split('=',1) for x in n['juce_provenance']['text'].strip().splitlines())
require(provenance['actual_sha']==provenance['expected_sha']==b.JUCE and provenance['actual_version']==provenance['expected_version']=='8.0.11','JUCE pin')
require(provenance['source_clean']=='true' and provenance['workflow_commit']==b.SOURCE and provenance['workflow_run']==str(b.RUN) and provenance['verification']=='PASS','JUCE execution provenance')
cases={x['name']:x for x in n['testcases']};new=b.read(ROOT.parent/'source-contract.json')['required_new_tests']
require(all(cases[x]['result']=='passed' and cases[x]['lasttest_status']=='Passed' for x in new),'New native regression failure')
require(n['counts']=={'registered':266,'passed':264,'failed':0,'formal_skipped':2,'disabled':0,'unknown':0},'Native observed totals')
require(cases['cs80_parity_capture_tests']['result']=='passed','Corrected capture test did not pass')
test_step=b.step(job,'Test')
require(any(m=='100% tests passed, 0 tests failed out of 266' and t and b.in_step(t,test_step) for t,m in log),'Original CTest summary binding')
starts=[i for i,(_,m) in enumerate(log) if m.startswith('Started validating: ')]
require(len(starts)==3,'Original invocation count')
lanes=[]
with zipfile.ZipFile(DOWNLOADS/'pluginval.zip') as z:
 for index,(lane,console,report,step_name) in enumerate([
  ('vst3','pluginval-vst3.log','pluginval-reports/cs80-vst3','Validate VST3 with pluginval'),
  ('au','pluginval-au.log','pluginval-reports/cs80-au','Validate AU with pluginval'),
  ('au-gui','pluginval-au-gui.log','pluginval-reports/cs80-au-gui','Validate AU with pluginval GUI')]):
  cb=z.read(console);rb=z.read(report);cs=p.console_summary(cb);rs=p.console_summary(rb)
  lines=b.decode(cb).splitlines();i=starts[index]
  require([m for _,m in log[i:i+len(lines)]]==lines,'Console is not original runtime output')
  s=next(x for x in job['steps'] if x['name']==step_name)
  require(b.in_step(log[i][0],s) and b.in_step(log[i+len(lines)-1][0],s),'Console outside original step')
  require(cs['validation_start_count']==1 and cs['strictness']==[10] and cs['program_counts']==[523],'Original validator settings')
  if index<2:
   require(s['conclusion']=='success' and cs['clean_text_success'] and rs['clean_text_success'] and lines[1:]==b.decode(rb).splitlines(),'Completed headless lane binding')
   result='completed_success';exit_value=0
  else:
   require(s['conclusion']=='failure' and cs['success_lines']==0 and not cs['clean_text_success'] and len(rb)==0,'Incomplete GUI evidence')
   require(cs['seeds']==['0x3c99748'],'Original failing seed')
   require('Starting tests in: pluginval / Editor Automation...' in lines and 'Completed tests in pluginval / Editor Automation' not in lines,'Original failure phase')
   require(any('AppleParavirtCommandBuffer.mm, line 266.' in line for line in lines),'Driver assertion missing')
   require('pluginval received Abort trap: 6, exiting immediately' in lines,'Original abort marker missing')
   require(any(m=='##[error]Process completed with exit code 9.' and t and b.in_step(t,s) for t,m in log),'Original failed step exit missing')
   result='failed_in_editor_automation';exit_value=9
  lanes.append({'lane':lane,'result':result,'original_step':s,'seed':cs['seeds'][0],'strictness':10,'programs':523,'original_runtime_console_matches_artifact':True,'first_output_utc':log[i][0],'last_output_utc':log[i+len(lines)-1][0],'console':{'path':console,'bytes':len(cb),'sha256':b.sha(cb),'summary':cs},'report':{'path':report,'bytes':len(rb),'sha256':b.sha(rb),'summary':rs},'step_exit':exit_value,'exit_basis':'Original successful unsuppressed pipeline' if index<2 else 'Original GitHub step explicitly reports exit9 after pluginval Abort trap6 message; raw signal/exit ownership is not independently proved.'})
 names=set(z.namelist())
 require('pluginval-au-seed-regression.log' not in names and 'pluginval-reports/cs80-au-seed-regression' not in names and 'au-payload-after-validation.sha256' not in names,'Unexpected post-failure evidence')
 download=json.loads(b.decode(z.read('pluginval-download.json')))
 require(download['verified'] is True and download['version']=='1.0.4' and download['asset_id']==210999197 and download['actual_size']==9202795 and download['actual_sha256']=='3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f','Pinned validator receipt')
for name in ('Validate AU GUI fixed-seed regression','Package native macOS VST3, AU and Standalone','Upload native macOS install bundle'):
 require(next(s for s in job['steps'] if s['name']==name)['conclusion']=='skipped','Expected unexecuted post-failure gate')
require(not any(a['name']=='cs80-macos-15-install-bundle' for a in b.read(ROOT/'artifacts-api.json')['artifacts']),'Unexpected original install artifact')
result={'schema':1,'worker':'a50','source':b.SOURCE,'run_id':b.RUN,'attempt':1,'producer_job_id':job['id'],'original_job_conclusion':'failure','platform_qualified':False,'release_qualified':False,'install_artifact_produced':False,'native':{'counts':n['counts'],'formal_skips':n['formal_skips'],'nested_python_skips':n['nested_python_skips'],'new_tests_all_passed':new,'corrected_parity_capture_test':cases['cs80_parity_capture_tests'],'original_test_step':test_step,'actual_archive_and_members_bound':True,'juce_provenance':n['juce_provenance'],'native_only_observations':b.record(ROOT/'native-only-observations.json')},'validators':{'version':'1.0.4','required_lanes':4,'actual_invocations_started':3,'completed_successful_invocations':2,'failed_invocations':1,'unexecuted_required_lanes':['au-gui-seed'],'runtime_validation_retries':0,'lanes':lanes,'download':download},'failure':{'phase':'Editor Automation','assertion':'(_commandCurrentStorage.currentOffset)','function':'-[AppleParavirtCommandBuffer endCurrentChunk]','file':'AppleParavirtCommandBuffer.mm','line':266,'assertion_utc':'2026-11-24T08:37:18.1273420Z','pluginval_message':'pluginval received Abort trap: 6, exiting immediately','reported_step_exit':9,'gui_report_bytes':0,'cause_ownership_established':False,'scope':'Observed graphics-driver-named assertion during an AU editor automation phase. No crash stack/core or controlled reproduction establishes whether the runner graphics path, JUCE, plugin or an interaction caused it.'},'preservation':b.record(ROOT/'preservation-summary.json'),'rerun_assessment':{'one_clean_same_source_job_rerun_reasonable':True,'reason':'The observed assertion names a graphics command-buffer path while native tests and both headless lanes completed; one clean runner execution can test reproducibility without changing code, thresholds, required GUI coverage or packages. This is a bounded diagnostic, not proof of infrastructure ownership or a passing release.','would_replay_failed_random_seed':False,'failed_seed_retained':'0x3c99748','required_existing_fixed_seed_retained':'0x2b4eef5','any_successful_new_attempt_must_be_qualified_independently':True},'rerun_request':{'reported_by':'coordinator root message','http_status':403,'message':'The workflow run containing this job is already running.','attempt_started':False,'attempt_changed':False,'classification':'GitHub API run-state restriction, not an approval denial','next_action':'Wait for the original workflow to become terminal, then request only the Apple Silicon job rerun; preserve Windows/Intel/Linux original attempt provenance.'},'limits':['Native passes and two headless successes do not qualify this failed Mac job.','The AU GUI fixed-seed lane and packaging were never executed.','No installation bytes exist to bind to this attempt.','A future successful rerun cannot erase or relabel this original failure.','A clean rerun uses a fresh random seed and is not a deterministic reproduction of seed0x3c99748.']}
with (ROOT/'failure-summary.json').open('x') as f:json.dump(result,f,indent=2);f.write('\n')
print(json.dumps({'original_job_conclusion':'failure','native':n['counts'],'nested_python_skips':n['nested_python_skips']['total'],'started_validator_invocations':3,'successful':2,'failed':1,'fixed_seed_not_run':True,'install_produced':False,'platform_qualified':False,'exact_archives_preserved':True}))
