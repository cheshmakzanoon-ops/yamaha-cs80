from pathlib import Path
import subprocess, hashlib, json, re, datetime, shlex, shutil
root=Path('/workspace/scratch/ee447c520988'); src=root/'release-followup'; ev=root/'mpe-released-target-evidence'; run=ev/'full-suite-final'; out=root/'mpe-released-target-independent-review'; build=root/'build-mpe-released-target-core'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
freeze='04597f5ee87da1f6390a8682298a1a0e656f834c'
r=json.loads((run/'source-bracket-and-builds.json').read_text()); before=json.loads(Path(r['before_snapshot']['path']).read_text()); commands=json.loads((run/'commands.json').read_text()); profiles=json.loads((ev/'profiles-final/commands.json').read_text())
assert before['head']==r['post_run_head']==r['final_source_test_freeze']==freeze
assert sha(r['before_snapshot']['path'])==r['before_snapshot']['sha256']
assert r['before_snapshot']['before_profile_first_command_utc']==profiles[0]['started_utc']<commands[0]['start']==r['full_suite_first_command_utc']
assert datetime.datetime.fromisoformat(commands[1]['start'])<datetime.datetime.fromisoformat(r['post_run_recorded_utc'])
tracked=subprocess.check_output(['git','-C',str(src),'ls-files','src','tests','CMakeLists.txt'],text=True).splitlines()
current={p:sha(src/p) for p in tracked}
frozen={p:hashlib.sha256(subprocess.check_output(['git','-C',str(src),'show',f'{freeze}:{p}'])).hexdigest() for p in tracked}
assert len(tracked)==411 and current==frozen==before['hashes']==r['after_source_sha256']
diff=subprocess.run(r['tracked_source_diff']['argv'],capture_output=True,text=True)
assert (diff.returncode,diff.stdout,diff.stderr)==(0,'','')
assert r['tracked_source_diff']['exit_code']==0 and r['tracked_source_diff']['stdout']==r['tracked_source_diff']['stderr']==''
assert (run/'driver.status').read_text().strip()=='0'
assert (run/'source-record.status').read_text().strip()=='0'
assert len(commands)==2 and [c['stage'] for c in commands]==['build','ctest']
for c in commands:
 assert c['exit_code']==0 and sha(run/c['log'])==c['sha256']
for field,file in [('run_command_record_sha256','commands.json'),('ctest_console_sha256','ctest.log'),('ctest_last_test_sha256','ctest-LastTest.log')]:assert sha(run/file)==r[field]
assert len(r['build_artifacts'])==131
for p,item in r['build_artifacts'].items():assert sha(p)==item['sha256'] and Path(p).stat().st_size==item['bytes']
console=(run/'ctest.log').read_text(); last=(run/'ctest-LastTest.log').read_text()
passed=re.findall(r'^\s*(\d+)/145 Test\s+#(\d+): (\S+)\s+\.+\s+Passed\s+([0-9.]+) sec$',console,re.M)
assert len(passed)==145 and {int(x[0]) for x in passed}=={*range(1,146)} and {int(x[1]) for x in passed}=={*range(1,146)}
assert console.count('100% tests passed, 0 tests failed out of 145')==1
assert not re.search(r'\*\*\*|Not Run|Skipped|Timeout|Failed',console)
entries=re.findall(r'^(\d+)/145 Testing: (\S+)\n(.*?)(?=^\d+/145 Testing:|\Z)',last,re.M|re.S)
assert len(entries)==145 and {int(x[0]) for x in entries}=={*range(1,146)}
byname={name:body for _,name,body in entries}; assert len(byname)==145
assert {x[2] for x in passed}==set(byname)
for n,name,body in entries:
 assert body.count('Test Passed.\n')==1 and 'Test Failed.' not in body and '<end of output>\n' in body
 assert re.search(rf'^{n}/145 Test: {re.escape(name)}$',body,re.M)
reg=re.findall(r'^add_test\(\[=\[(.*?)\]=\] (.*)\)$',(build/'CTestTestfile.cmake').read_text(),re.M)
assert len(reg)==145 and {n for n,args in reg}==set(byname)
registration={n:shlex.split(args) for n,args in reg}
for n,body in byname.items():
 command=shlex.split(re.search(r'^Command: (.*)$',body,re.M)[1])
 assert command[1:]==registration[n][1:] and Path(command[0]).resolve()==Path(shutil.which(registration[n][0]) or registration[n][0]).resolve(), (n,command,registration[n])
expected={'cs80_mpe_released_target_tests':7488,'cs80_mpe_member_onset_tests':2501,'cs80_controller_reset_tests':3513,'cs80_sixteen_voice_release_tests':1418,'cs80_tests':11508,'cs80_tuning_midi_tests':22085,'cs80_poly_pressure_routing_tests':4504}
exact={}
for name,count in expected.items():
 ctest_name='cs80_core_tests' if name=='cs80_tests' else name; body=byname[ctest_name]; lines=body.split('<end of output>')[0].splitlines()
 if name=='cs80_tests': assert f'ALL {count} CHECKS PASSED' in body, body
 else: assert f'{count} checks, 0 failures' in body, body
 exact[name]={'ctest_name':ctest_name,'checks':count,'failures':0,'binary_sha256':sha(build/name)}
cache=(build/'CMakeCache.txt').read_text()
flags={key:re.search(rf'^{key}:[^=]+=(.*)$',cache,re.M)[1] for key in ['CMAKE_BUILD_TYPE','CMAKE_CXX_COMPILER','CMAKE_CXX_FLAGS_RELEASE','CS80_BUILD_PLUGIN','CS80_ENABLE_AVX2_FILTER_BATCH']}
assert flags['CS80_BUILD_PLUGIN']=='OFF' and flags['CS80_ENABLE_AVX2_FILTER_BATCH']=='OFF' and flags['CMAKE_CXX_FLAGS_RELEASE']=='-O3 -DNDEBUG'
# These directly invoked repository scripts are outside the historical 411-file snapshot.
script_args=sorted({str(Path(a).relative_to(src)) for args in registration.values() for a in args if a.startswith(str(src/'scripts')+'/') and Path(a).is_file()})
scripts={p:sha(src/p) for p in script_args}
for p,h in scripts.items():assert h==hashlib.sha256(subprocess.check_output(['git','-C',str(src),'show',f'{freeze}:{p}'])).hexdigest()
result={'reviewed_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'test_freeze':freeze,'source_record_sha256':sha(run/'source-bracket-and-builds.json'),'driver_sha256':sha(ev/'run_full_suite_final.py'),'recorder_sha256':sha(ev/'record_full_suite_final.py'),'driver_status':0,'record_status':0,'verified_source_snapshot_count':411,'snapshots_equal_current_and_commit':True,'profile_start_utc':profiles[0]['started_utc'],'full_build_start_utc':commands[0]['start'],'ctest_start_utc':commands[1]['start'],'post_run_recorded_utc':r['post_run_recorded_utc'],'build_artifact_count':131,'all_recorded_artifact_hashes_and_sizes_match':True,'console_pass_count':len(passed),'last_test_pass_count':len(entries),'registration_count':len(reg),'registration_and_recorded_command_match':True,'expected_focused_results':exact,'build_flags':flags,'console_sha256':sha(run/'ctest.log'),'last_test_sha256':sha(run/'ctest-LastTest.log'),'commands_sha256':sha(run/'commands.json'),'current_ctest_registration_sha256':sha(build/'CTestTestfile.cmake'),'directly_invoked_repo_scripts_outside_snapshot_currently_match_commit':scripts,'independent_tests_executed_for_this_review':0,'independent_builds_performed_for_this_review':0}
(out/'full-suite-final-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
