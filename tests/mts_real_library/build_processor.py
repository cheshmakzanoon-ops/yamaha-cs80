#!/usr/bin/env python3
import argparse,hashlib,json,pathlib,shutil,subprocess,sys
if sys.flags.optimize: raise SystemExit('REFUSE optimized Python')
p=argparse.ArgumentParser()
p.add_argument('--evidence',required=True,type=pathlib.Path)
p.add_argument('--native-build',required=True,type=pathlib.Path)
p.add_argument('--runpath',default='')
p.add_argument('--cmake',default='cmake')
a=p.parse_args();e=a.evidence.resolve();b=a.native_build.resolve();cfg=json.loads((e/'config.json').read_text());r=pathlib.Path(cfg['repo'])
def run(cmd):
 print('+ '+repr(cmd),flush=True);subprocess.run(cmd,check=True)
run(['bash',str(e/'build.sh')])
run([a.cmake,'-S',str(r),'-B',str(b),'-DCS80_MTS_REAL_CLIENT_SOURCE='+str(e/'Client/libMTSClient.cpp'),'-DCS80_MTS_REAL_BUILD_RPATH='+a.runpath])
run([a.cmake,'--build',str(b),'--target','cs80_mts_real_processor','--parallel','2'])
shutil.copyfile(b/'cs80_mts_real_processor',e/'processor');(e/'processor').chmod(0o755)
symbols=subprocess.check_output(['nm','-C','--defined-only',str(e/'processor')],text=True)
(e/'processor-symbols.txt').write_text(symbols)
for name in ['MTS_RegisterClient','cs80::MtsEspClient::fillTable(cs80::tuning::TuningTable&) const']:
 matches=[line for line in symbols.splitlines() if line.endswith(' T '+name)]
 if len(matches)!=1: raise SystemExit('Expected exactly one implementation: '+name)
(e/'processor-dynamic.txt').write_text(subprocess.check_output(['readelf','-d',str(e/'processor')],text=True))
# The archive, production sources, optional harness and exact copied executables
# are captured after build. The runner refuses changed artifacts before loading.
archives=list(b.glob('CS80Reference_artefacts/*/lib*.a'))
if len(archives)!=1: raise SystemExit('Expected one CS80Reference static archive: '+repr(archives))
inputs=[e/name for name in ['processor','master','processor.cpp','prepare_processor.py','build_processor.py','run_processor.py','postbuild.sha256','source-hashes.txt','config.json']]
inputs += archives+[r/'CMakeLists.txt',r/'tests/mts_real_library/processor.cpp']
inputs += sorted((r/'src/plugin').rglob('*.cpp'))+sorted((r/'src/plugin').rglob('*.h'))+sorted((r/'src/core').rglob('*.h'))
# Also pin resolved dependency files without weakening loader-environment guards.
deps=subprocess.check_output(['ldd',str(e/'processor')],text=True);(e/'processor-dependencies.txt').write_text(deps)
if 'not found' in deps: raise SystemExit('Unresolved processor runtime dependency')
for line in deps.splitlines():
 parts=line.split()
 if '=>' in parts and len(parts)>2 and parts[2].startswith('/'):
  inputs.append(pathlib.Path(parts[2]).resolve())
manifest={str(path):hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs}
(e/'processor-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Verified single real adapter/client implementation; processor manifest ready',flush=True)
