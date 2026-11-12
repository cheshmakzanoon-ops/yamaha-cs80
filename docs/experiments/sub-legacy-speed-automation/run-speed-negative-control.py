import pathlib,subprocess,json,hashlib,time
root=pathlib.Path(__file__).resolve().parents[1]
out=root/'build/legacy-speed-negative-control'
out.mkdir(exist_ok=False)
host=root/'build-native/cs80_vst3_sub_legacy_speed_automation_tests'
module=root/'../yamaha-cs80-touch/build-native/CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'
contract=root/'tests/parity/tolerances-v2'
cmd=[str(host),str(module),str(contract/'host-automation.json'),str(contract/'host-automation-sub-legacy-speed-v1.json'),str(out/'captures'),'48000','realtime']
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
data=dict(command=cmd,host_sha256=sha(host),module_sha256=sha(module),purpose='Capability negative control: full gate on frozen 27-lane baseline, expected six partition failures; NOT a passing full qualification',started=time.time())
with (out/'run.log').open('wb') as log:
 try: data['exit_code']=subprocess.run(cmd,cwd=root,stdout=log,stderr=subprocess.STDOUT,timeout=180).returncode
 except subprocess.TimeoutExpired: data.update(exit_code=None,error='timeout; no retry')
data.update(finished=time.time(),host_sha256_after=sha(host),module_sha256_after=sha(module),log_sha256=sha(out/'run.log'))
(out/'manifest.json').write_text(json.dumps(data,indent=2)+'\n')
print(json.dumps(data,indent=2))
