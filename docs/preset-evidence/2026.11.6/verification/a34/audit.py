#!/usr/bin/env python3
"""Read-only exact-source parameter/layout comparison; no host runtime claim."""
import hashlib,json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[5]
OUT=Path(__file__).resolve().parent
VERSIONS={'published_4':'078c0981ffa1b5472342e0fb5ab8702bc3bd2889','preceding_5':'c76ad288e02fbeca48f39652bf4fcdd99837f793','candidate_6':'fcc3c8e055223fe4f786ca162ff46076c33471c8'}
def git(sha,path):return subprocess.check_output(['git','-C',str(ROOT),'show',sha+':'+path])
def digest(b):return hashlib.sha256(b).hexdigest()
def layout(text):
 start=text.index('juce::AudioProcessorValueTreeState::ParameterLayout Cs80AudioProcessor::createParameterLayout()')
 end=text.index('\nbool Cs80AudioProcessor::isBusesLayoutSupported',start)
 return text[start:end]
records={}; rows={}
with tempfile.TemporaryDirectory(prefix='cs80-a34-') as temp:
 for name,sha in VERSIONS.items():
  p=Path(temp)/name;p.mkdir();raw=git(sha,'src/core/Params.h');(p/'Params.h').write_bytes(raw)
  cmd=['c++','-std=c++20','-O2','-I'+str(p),str(OUT/'metadata-probe.cpp'),'-o',str(p/'probe')]
  subprocess.run(cmd,check=True)
  output=subprocess.check_output([str(p/'probe')]);rows[name]=output.splitlines(keepends=True)
  records[name]={'source':sha,'parameters':len(rows[name]),'header_sha256':digest(raw),'metadata_sha256':digest(output)}
 assert len(rows['published_4'])==200 and len(rows['preceding_5'])==202 and len(rows['candidate_6'])==204
 assert rows['candidate_6'][:200]==rows['published_4']
 assert rows['candidate_6'][:202]==rows['preceding_5']
old=layout(git(VERSIONS['preceding_5'],'src/plugin/PluginProcessor.cpp').decode())
new=layout(git(VERSIONS['candidate_6'],'src/plugin/PluginProcessor.cpp').decode())
start=new.index('    // Preserve all 308 previous host slots')
end=new.index('    return layout;',start)
addition=new[start:end]
assert new[:start]+new[end:]==old
assert 'bodyMaterialParam(rank)), 19' in addition and 'body::defaultMaterial' in addition
assert git(VERSIONS['preceding_5'],'src/core/InstrumentBodyVariants.h')==git(VERSIONS['candidate_6'],'src/core/InstrumentBodyVariants.h')
def model_names(raw):
 text=raw.decode();start=text.index('inline constexpr const char* name(int model) noexcept')
 return text[start:text.index('\nclass InstrumentBody',start)]
assert model_names(git(VERSIONS['preceding_5'],'src/core/InstrumentBody.h'))==model_names(git(VERSIONS['candidate_6'],'src/core/InstrumentBody.h'))
bound_paths=['src/core/Params.h','src/plugin/PluginProcessor.cpp','src/plugin/PluginProcessorLibrary.cpp','src/plugin/PluginProcessor.h','src/plugin/ParameterStateChildIndex.h','src/core/Engine.cpp','src/core/InstrumentBody.h','tests/InstrumentBodyHostTests.h','tests/ParameterStateChildIndexTests.cpp']
bound={}
for p in bound_paths:
 raw=(ROOT/p).read_bytes();assert raw==git(VERSIONS['candidate_6'],p),p
 bound[p]=digest(raw)
result={'versions':records,'normalization_probes_per_parameter':513,'published_200_prefix_identical':True,'preceding_202_prefix_identical':True,'preceding_host_layout_after_removing_only_new_append_is_byte_identical':True,'prior_model_names_and_variant_metadata_identical':True,'host_material_append_au_hint':19,'old_host_layout_sha256':digest(old.encode()),'new_append_sha256':digest(addition.encode()),'bound_current_inputs':bound,'scope':'Core metadata and exact layout source; no actual APVTS, processor, GUI or native AU execution.'}
(OUT/'metadata-summary.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k not in ('bound_current_inputs','versions')},indent=2))
print('PASS: 200 published and 202 preceding parameter rows, 513 probes per row; exact preexisting host layout; 10 committed-source inputs')
