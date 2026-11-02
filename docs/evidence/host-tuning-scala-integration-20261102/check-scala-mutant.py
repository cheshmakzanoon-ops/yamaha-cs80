from pathlib import Path
import subprocess,tempfile,hashlib
source=Path('src/core/Tuning.h').read_text()
needle='''        if (!std::isfinite(renderedHz) || !(renderedHz > 0.0f))
            return "tuning produced an unrenderable nominal float frequency";
'''
assert source.count(needle)==1
with tempfile.TemporaryDirectory(prefix='cs80-scala-mutant-') as folder:
 root=Path(folder);(root/'core').mkdir();(root/'core/Tuning.h').write_text(source.replace(needle,''))
 exe=root/'mutant'
 subprocess.run(['g++','-std=c++20','-O3','-DNDEBUG','-I'+str(root),'-Isrc','tests/ScalaFloatPitchBoundsTests.cpp','-o',str(exe)],check=True)
 result=subprocess.run([str(exe),str(root/'positive.f32')],text=True,capture_output=True)
 print('ACTUAL GUARD-DELETION RESULT',result.returncode);print(result.stdout);print(result.stderr)
 assert result.returncode==1 and '26 Scala float-frequency checks, 6 failures' in result.stdout
 assert result.stderr.count('FAIL: ')==6
 expected='a505b7fd85f10408eb3b7a947566dfcbaa6e49aa0086894548131dfea5f45ca2'
 assert hashlib.sha256((root/'positive.f32').read_bytes()).hexdigest()==expected
 subprocess.run(['build-integration/cs80_scala_float_pitch_bounds_tests',str(root/'restored-positive.f32')],check=True)
 assert hashlib.sha256((root/'restored-positive.f32').read_bytes()).hexdigest()==expected
 print('Positive arrays unchanged; actual guard deletion rejected semantically; original passes.')
