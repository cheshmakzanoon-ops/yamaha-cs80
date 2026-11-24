#!/usr/bin/env python3
"""Collect source ownership bindings and reuse original native test evidence."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import xml.etree.ElementTree as ET
import zipfile

ROOT=Path(__file__).resolve().parents[5]
OUT=Path(__file__).resolve().parent
BASE='18711f93a39aa49f3d6f9a41784be456224facbe'
SOURCE='c5549f7d095a5c5f49a5ad22ec0ccc819245d201'
SUCCESSOR='ee0dabcacb4cdff93f8803844f60348e0ceda6ed'

def sha(data): return hashlib.sha256(data).hexdigest()
def git(*args): return subprocess.check_output(['git',*args],cwd=ROOT)

files=['src/core/Engine.h','src/core/Engine.cpp','src/core/InstrumentBody.h',
       'src/core/ReconstructedOscillator.h','src/core/Dsp.h','src/core/BbdChorus.h',
       'src/core/FxSlot.h','src/plugin/PluginProcessor.h','src/plugin/PluginProcessor.cpp',
       'tests/OversamplingTests.cpp','tests/SourcePerformanceAuditTests.cpp',
       'tests/InstrumentBodyEngineTests.cpp','tests/LowChoirSpectrumTests.cpp']
bindings={}
for path in files:
    versions={name:None if name=='baseline' and path=='tests/LowChoirSpectrumTests.cpp' else git('show',commit+':'+path)
              for name,commit in [('baseline',BASE),('source',SOURCE),('successor',SUCCESSOR)]}
    bindings[path]={'sha256':{name:sha(data) if data is not None else None for name,data in versions.items()},
                    'baseline_source_equal':versions['baseline']==versions['source'],
                    'source_successor_equal':versions['source']==versions['successor']}
    assert bindings[path]['source_successor_equal']
assert all(bindings[p]['baseline_source_equal'] for p in files[:2]+files[3:11])

dependencies={}
for agent in ['a07','a11','a21','a45']:
    path=ROOT/'docs/preset-evidence/2026.11.7/reviews'/f'{agent}.md'
    dependencies[str(path.relative_to(ROOT))]=sha(path.read_bytes())
for rel in ['docs/preset-evidence/2026.11.7/development/focused-ctest-receipt.json',
            'docs/preset-evidence/2026.11.7/verification/a45/sizeof-results.json',
            'docs/preset-evidence/2026.11.7/verification/a45/static-stack-analysis.json',
            'docs/preset-evidence/2026.11.7/verification/a21/binding.json']:
    dependencies[rel]=sha((ROOT/rel).read_bytes())

zip_path=Path('/tmp/cs80-ci-1cee5a/c5549f7d/macos-x86_64-native.zip')
api_path=ROOT/'docs/preset-evidence/2026.11.7/ci/source-c5549f7d/macos-x86_64/native-api.json'
tests=['cs80_source_performance_audit_tests','cs80_oversampling_tests',
       'cs80_instrument_body_engine_tests','cs80_low_choir_spectrum_tests']
with zipfile.ZipFile(zip_path) as z:
    xml=z.read('ctest-results.xml')
    log=z.read('Testing/Temporary/LastTest.log')
    root=ET.fromstring(xml)
    cases={t.get('name'):t for t in root.iter('testcase')}
    text=log.decode()
    reused=[]
    sections=[]
    for name in tests:
        t=cases[name]
        assert t.find('failure') is None and t.find('skipped') is None
        start=re.search(r'(?m)^\d+/\d+ Testing: '+re.escape(name)+r'\n',text).start()
        end=text.find('\n\n',text.find('Test time =',start))
        section=text[start:end if end>=0 else len(text)]
        assert 'Test Passed.' in section
        matches=re.findall(r'(\d+) checks, (\d+) failures',section)
        assert len(matches)==1 and matches[0][1]=='0'
        sections.append(section)
        reused.append({'name':name,'junit':t.attrib,'checks':int(matches[0][0]),'failures':0,
                       'lasttest_section_sha256':sha(section.encode())})
    (OUT/'reused-native-test-sections.txt').write_text('\n\n'.join(sections)+'\n')
    reuse={'source':SOURCE,'run_id':45385716988,'attempt':1,'job_id':138415960996,
           'original_job_conclusion':'failure','original_suite_not_qualified':True,
           'scope':'Reuse these four passing original test cases only. Does not clear the failed full suite or qualify successor.',
           'artifact_path':str(zip_path),'artifact_sha256':sha(zip_path.read_bytes()),
           'api_path':str(api_path.relative_to(ROOT)),'api_sha256':sha(api_path.read_bytes()),
           'junit_sha256':sha(xml),'lasttest_sha256':sha(log),'tests':reused}
(OUT/'reused-tests.json').write_text(json.dumps(reuse,indent=2)+'\n')

result={'audio_source':SOURCE,'baseline_source':BASE,'successor_source':SUCCESSOR,
        'reviewed_files':bindings,'reused_evidence_sha256':dependencies,
        'successor_protected_differences':git('diff','--name-only',SOURCE,SUCCESSOR,'--','src','tests').decode().splitlines(),
        'ownership':{
          'entry':'createPluginFilter returns new Cs80AudioProcessor',
          'primary_engine':'value member of heap-owned processor; no callback Engine local',
          'extension_engine':'unique_ptr allocated and prepared unconditionally in processor constructor',
          'release_clock':'member declared before both engines; survives both destructions',
          'voice_mode':'8 voices per Engine; processor activates second Engine for 16-voice mode',
          'body_storage':'8 voices x 2 ranks x current/previous body = 32 fixed body objects per Engine',
          'byte_growth':{'body':400,'body_noise_coefficients':16,'body_formant_histories':384,'engine':12800,'both_engine_extents':25600},
          'initial_prepare':'effect vectors reserve maximum-factor capacity, then reinitialize actual factor',
          'callback_rate_transition':'setNonRealtime/setControlFrame can configureInternalRate using preallocated buffers; oscillator basis acquire takes existing mutex',
          'scratch':'prepareToPlay sizes primary, extension, and mono-input vectors; renderSegment chunks beyond capacity',
          'onset':'body noteOn captures finite harmonic budget in existing fixed bank; prior body copy/handoff uses inline storage',
          'allocation_scope_limit':'Offline C++ new/new[] hooks are not a universal malloc/host/plugin callback allocation proof',
          'deadline_scope_limit':'No per-callback deadline, concurrent-instance contention, or full plugin host guarantee'
        },'new_release_blocker_found':False,
        'native_successor_qualification':'Root-owned and not established by this review'}
(OUT/'ownership-review.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'source_bindings':len(bindings),'reused_cases':reused,'new_release_blocker_found':False},indent=2))
