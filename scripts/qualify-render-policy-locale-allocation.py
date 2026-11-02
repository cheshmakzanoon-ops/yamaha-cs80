#!/usr/bin/env python3
"""POSIX allocation-failure fault injection, never a shipping runtime hook."""
import argparse, hashlib, json, subprocess
from pathlib import Path
import importlib.util

p=argparse.ArgumentParser(description=__doc__)
for name in ("source","receipt","dependency-build","out","lock"):
    p.add_argument("--"+name,type=Path,required=True)
a=p.parse_args()
spec=importlib.util.spec_from_file_location("policy_qualifier",Path(__file__).with_name("qualify-render-policy-locale.py"))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
sha=module.sha
with module.locked(a.lock):
    r=json.loads(a.receipt.read_text())
    assert not subprocess.check_output(["git","diff","--","src","tests"],cwd=a.source)
    assert not subprocess.check_output(["git","diff","--name-only",r["head"],"HEAD","--","src","tests"],cwd=a.source)
    assert subprocess.check_output(["git","rev-parse","HEAD:src"],cwd=a.source,text=True).strip()==r["src_tree"]
    for e in r["compile_commands"]:assert sha(e["fresh_object"])==e["sha256"]
    for e in r["reused_module_and_font_objects"]:assert sha(e["file"])==e["sha256"]
    positive=Path(r["link_argv"][r["link_argv"].index("-o")+1])
    assert sha(positive)==r["binary_sha256"] and r["test_exit_code"]==0
    a.out.mkdir(parents=True,exist_ok=True)
    def original_pass(name):
        result=subprocess.run([str(positive),"--render-policy-locale-test"],capture_output=True,text=True,timeout=120)
        text=result.stdout+result.stderr;(a.out/name).write_text(text)
        assert result.returncode==0 and "Render policy locale processor: 2290 checks, 0 failures" in text
    original_pass("initial-positive.log")
    file="src/plugin/PluginProcessor.cpp"
    original=(a.source/file).read_text()
    old='locale_t handle = newlocale(LC_NUMERIC_MASK, "C", nullptr);'
    assert original.count(old)==1
    cpp=a.out/"PluginProcessor.cpp";cpp.write_text(original.replace(old,"locale_t handle = nullptr;"))
    entries={e["file"]:e for e in r["compile_commands"]}
    cppEntry=entries[file];testEntry=entries["src/tools/ui_snapshot.cpp"]
    cppObj=a.out/"processor.o";testObj=a.out/"allocation-test.o"
    commands=[]
    for entry,source,obj in ((cppEntry,cpp,cppObj),
                            (testEntry,a.source/"tests/RenderPolicyLocaleAllocationProbe.cpp",testObj)):
        command=entry["argv"].copy()
        command[command.index("-c")+1]=str(source);command[command.index("-o")+1]=str(obj)
        command.insert(1,"-I"+str(a.source/"src/plugin"))
        with (a.out/(obj.stem+"-compile.log")).open("w") as log:
            subprocess.run(command,cwd=a.dependency_build,stdout=log,stderr=subprocess.STDOUT,check=True)
        commands.append(command)
    link=r["link_argv"].copy()
    assert link.count(cppEntry["fresh_object"])==1 and link.count(testEntry["fresh_object"])==1
    link[link.index(cppEntry["fresh_object"])]=str(cppObj)
    link[link.index(testEntry["fresh_object"])]=str(testObj)
    binary=a.out/"allocation-failure-probe";link[link.index("-o")+1]=str(binary)
    with (a.out/"link.log").open("w") as log:
        subprocess.run(link,cwd=a.dependency_build,stdout=log,stderr=subprocess.STDOUT,check=True)
    result=subprocess.run([str(binary)],capture_output=True,text=True,timeout=120)
    text=result.stdout+result.stderr;(a.out/"test.log").write_text(text)
    summary={"source_head":r["head"],"source_tree":r["src_tree"],"receipt_sha256":sha(a.receipt),
      "scope":"compiled POSIX null-locale fault injection; not an observed allocation failure",
      "driver_sha256":sha(a.source/"tests/RenderPolicyLocaleAllocationProbe.cpp"),
      "mutant_source_sha256":sha(cpp),"positive_binary_sha256":sha(positive),"fault_binary_sha256":sha(binary),
      "compile_commands":commands,"link_argv":link,"exit_code":result.returncode,
      "expected_summary":"Policy locale allocation failure simulation: 98 checks, 0 failures"}
    (a.out/"receipt.json").write_text(json.dumps(summary,indent=2)+"\n")
    assert result.returncode==0 and summary["expected_summary"] in text
    assert sha(positive)==r["binary_sha256"]
    original_pass("restored-positive.log")
    print(text)
