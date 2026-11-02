DRIVER="#include \"plugin/PluginProcessor.h\"\n#include <clocale>\n#include <cstdio>\n#include <memory>\n#include <string>\nint main()\n{\n    juce::ScopedJuceInitialiser_GUI messageManagerOnly;\n    const std::string original=std::setlocale(LC_NUMERIC,nullptr);\n    int checks=0,failures=0;\n    auto check=[&](bool ok,const char* text){++checks;if(!ok){++failures;std::printf(\"FAIL: %s\\n\",text);}};\n    for(const char* name:{\"C\",\"de_DE.utf8\"})\n    {\n        const auto* selected=std::setlocale(LC_NUMERIC,name);\n        check(selected!=nullptr,\"required locale exists\");\n        if(selected==nullptr)return 2;\n        const std::string expectedLocale=selected;\n        juce::TemporaryFile root(\"-cs80-null-numeric-locale\");\n        auto host=std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile(\"presets\"));\n        host->selectHardwareSource(0,0);host->selectHardwareSource(1,1);\n        auto* p=host->getAPVTS().getParameter(\"factory_tone_revision\");\n        if(p==nullptr)return 3;\n        p->setValueNotifyingHost(1);\n        check(host->getAPVTS().getRawParameterValue(\"factory_tone_revision\")->load()==1,\n              \"fallback probe begins with a real live V2 opt-in\");\n        juce::MemoryBlock saved;host->getStateInformation(saved);\n        auto xml=juce::parseXML(saved.toString());auto tree=juce::ValueTree::fromXml(*xml);\n        check(tree.getChildWithProperty(\"id\",\"factory_tone_revision\").getProperty(\"value\").toString()==\"1.0\",\n              \"fallback probe restores actual V2 serialization\");\n        host->setStateInformation(saved.getData(),(int)saved.getSize());\n        check(host->getAPVTS().getRawParameterValue(\"factory_tone_revision\")->load()==0,\n              \"simulated locale-allocation failure falls back to V1, not live V2\");\n        host->prepareToPlay(48000,32);\n        for(int rank=0;rank<2;++rank)\n        {\n            cs80::source_factory::ToneControls expected;\n            check(cs80::source_factory::compileTone(rank,rank,expected)\n                      && host->getEngine().getSourceBusReadout(rank)==expected.busVolts,\n                  \"allocation-failure fallback reaches actual V1 source buses\");\n        }\n        check(expectedLocale==std::setlocale(LC_NUMERIC,nullptr),\n              \"allocation-failure fallback leaves caller locale unchanged\");\n    }\n    std::setlocale(LC_NUMERIC,original.c_str());\n    std::printf(\"Locale allocation failure simulation: %d checks, %d failures\\n\",checks,failures);\n    return failures?1:0;\n}\n"
import fcntl,hashlib,json,subprocess,time
from contextlib import contextmanager
from pathlib import Path
base=Path('/workspace/scratch/aa102d573ba3')
source=base/'cs80-tone-host-locale'
build=base/'cs80-panel-s2/build-native-red'
out=base/'cs80-host-tool-evidence/factory-tone-host/locale-followups'
receiptPath=base/'cs80-host-tool-evidence/factory-tone-host/locale-green/build-receipt.json'
@contextmanager
def locked():
    with (base/'cs80-heavy-check.lock').open('a') as f:
        fcntl.flock(f,fcntl.LOCK_EX)
        try:yield
        finally:fcntl.flock(f,fcntl.LOCK_UN)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
with locked():
    receipt=json.loads(receiptPath.read_text())
    assert subprocess.check_output(['git','rev-parse','HEAD:src'],cwd=source,text=True).strip()==receipt['src_tree']
    assert not subprocess.check_output(['git','diff','--','src'],cwd=source)
    for e in receipt['compile_commands']:assert sha(e['fresh_object'])==e['sha256']
    for e in receipt['reused_module_and_font_objects']:assert sha(e['file'])==e['sha256']
    out.mkdir(parents=True,exist_ok=True)
    positive=Path(receipt['link_argv'][receipt['link_argv'].index('-o')+1])
    assert sha(positive)==receipt['binary_sha256']
    entries={r['file']:r for r in receipt['compile_commands']}
    original=(source/'src/plugin/PluginProcessor.cpp').read_text()
    contract={'source_head':receipt['head'],'src_tree':receipt['src_tree'],
      'plain_strtod':{'expected_exit':1,'checks':296,'required_failure':'actual session choice round-trips independently of numeric locale'},
      'null_locale':{'scope':'compiled fault injection, not observed OS allocation failure','expected_exit':0,'checks':14}}
    (out/'contract.json').write_text(json.dumps(contract,indent=2)+'\n')
cases=[
 ('plain-strtod','const double parsed = numericLocale.parse(begin, &end);','const double parsed = std::strtod(begin, &end);'),
 ('null-locale','locale_t handle = newlocale(LC_NUMERIC_MASK, "C", nullptr);','locale_t handle = nullptr;')
]
results=[]
for name,old,new in cases:
    with locked():
        print(time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),name,flush=True)
        assert original.count(old)==1
        case=out/name;case.mkdir(exist_ok=True)
        cpp=case/'PluginProcessor.cpp';cpp.write_text(original.replace(old,new))
        record=entries['src/plugin/PluginProcessor.cpp']
        obj=case/'processor.o'
        command=record['argv'].copy()
        command[command.index('-c')+1]=str(cpp);command[command.index('-o')+1]=str(obj)
        command.insert(1,'-I'+str(source/'src/plugin'))
        with (case/'compile.log').open('w') as log:subprocess.run(command,cwd=build,stdout=log,stderr=subprocess.STDOUT,check=True)
        link=receipt['link_argv'].copy();link[link.index(record['fresh_object'])]=str(obj)
        flag='--factory-tone-locale-test'
        if name=='null-locale':
            driver=out/'allocation-failure-probe.cpp'
            driver.write_text(DRIVER)
            entry=entries['src/tools/ui_snapshot.cpp'];cmd=entry['argv'].copy();testObj=case/'probe.o'
            cmd[cmd.index('-c')+1]=str(driver);cmd[cmd.index('-o')+1]=str(testObj)
            subprocess.run(cmd,cwd=build,check=True)
            link[link.index(entry['fresh_object'])]=str(testObj)
            flag=None
        binary=case/'probe';link[link.index('-o')+1]=str(binary)
        subprocess.run(link,cwd=build,check=True)
        result=subprocess.run([str(binary)]+([flag] if flag else []),stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
        (case/'test.log').write_text(result.stdout)
        if name=='plain-strtod':
            assert result.returncode==1 and 'Factory tone locale processor: 296 checks,' in result.stdout
            assert 'FAIL: actual session choice round-trips independently of numeric locale' in result.stdout
        else:
            assert result.returncode==0 and 'Locale allocation failure simulation: 14 checks, 0 failures' in result.stdout
        results.append({'name':name,'exit':result.returncode,'source_sha256':sha(cpp),'binary_sha256':sha(binary),'compile_argv':command,'link_argv':link})
        print(result.stdout,flush=True)
with locked():
    assert sha(positive)==receipt['binary_sha256']
    restored=subprocess.run([str(positive),'--factory-tone-locale-test'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=120)
    assert restored.returncode==0 and '296 checks, 0 failures' in restored.stdout
    (out/'restored-positive.log').write_text(restored.stdout)
    (out/'summary.json').write_text(json.dumps({'source_head':receipt['head'],'src_tree':receipt['src_tree'],'results':results,'restored_positive_sha256':sha(positive),'restored_exit':restored.returncode},indent=2)+'\n')
    print('Both locale controls behaved as registered; unchanged original296-check positive restored.',flush=True)
