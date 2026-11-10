from pathlib import Path
import subprocess,json,hashlib,datetime,shutil
r=Path('/workspace/scratch/ee447c520988');s=r/'release-followup';ev=r/'mpe-released-target-evidence';out=ev/'full-suite-final';b=r/'build-mpe-released-target-core'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
before_path=ev/'profiles-final/inputs.json';before=json.loads(before_path.read_text());full=json.loads((out/'commands.json').read_text());profiles=json.loads((ev/'profiles-final/commands.json').read_text())
if (out/'driver.status').read_text().strip()!='0' or any(x['exit_code']!=0 for x in full):raise RuntimeError('full run incomplete')
if before['head']!='04597f5ee87da1f6390a8682298a1a0e656f834c':raise RuntimeError('unexpected before source')
if not profiles[0]['started_utc'] < full[0]['start']:raise RuntimeError('snapshot is not known to precede final build')
after={p:sha(s/p) for p in subprocess.check_output(['git','-C',str(s),'ls-files','src','tests','CMakeLists.txt'],text=True).splitlines()}
if after!=before['hashes']:raise RuntimeError('bracket source inventory differs')
args=['git','-C',str(s),'diff','--exit-code',before['head'],'--','src','tests','CMakeLists.txt'];p=subprocess.run(args,capture_output=True,text=True)
if p.returncode:raise RuntimeError('tracked source differs')
shutil.copyfile(b/'Testing/Temporary/LastTest.log',out/'ctest-LastTest.log')
files=[b/'libcs80_core.a',b/'CMakeCache.txt',b/'compile_commands.json',b/'build.ninja',b/'CMakeFiles/rules.ninja']
for f in b.iterdir():
 if f.is_file():
  with f.open('rb') as stream:magic=stream.read(4)
  if magic==b'\x7fELF':files.append(f)
artifacts={str(f):dict(sha256=sha(f),bytes=f.stat().st_size) for f in sorted(files)}
record=dict(final_source_test_freeze=before['head'],post_run_head=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),post_run_recorded_utc=datetime.datetime.now(datetime.timezone.utc).isoformat(),before_snapshot=dict(path=str(before_path),sha256=sha(before_path),before_profile_first_command_utc=profiles[0]['started_utc'],explanation='qualify_profiles_final.py writes the source inventory before starting its first command; that recorded command precedes final full-suite build. This is a before/after snapshot, not continuous filesystem enforcement.'),full_suite_first_command_utc=full[0]['start'],after_source_sha256=after,source_inventory_matches_before=True,tracked_source_diff=dict(argv=args,exit_code=p.returncode,stdout=p.stdout,stderr=p.stderr),run_command_record_sha256=sha(out/'commands.json'),ctest_console_sha256=sha(out/'ctest.log'),ctest_last_test_sha256=sha(out/'ctest-LastTest.log'),build_artifacts=artifacts,qualification_scope='145 default core CTests, not full JUCE/native suite or full sanitizer suite; source snapshots bracket this run, while the full-suite driver itself only records commands/statuses.')
(out/'source-bracket-and-builds.json').write_text(json.dumps(record,indent=2)+'\n');print('Verified before/after source inventory:',len(after),'files; artifacts',len(artifacts))
