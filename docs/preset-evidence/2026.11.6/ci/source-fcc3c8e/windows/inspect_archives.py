#!/usr/bin/env python3
"""Prepare exact Windows byte inspections after authentic final snapshots exist.

This never accepts a platform by itself or executes a downloaded product.
Existing outputs are refused. Run from the repository root.
"""
import argparse, hashlib, json, os, subprocess, sys
from pathlib import Path
HERE=Path(__file__).resolve().parent
REPO=Path.cwd()
SOURCE="fcc3c8e055223fe4f786ca162ff46076c33471c8"
RUN=45366603800
NAMES={"install":"cs80-windows-install-bundle", "vst3":"cs80-windows-vst3", "test-evidence":"cs80-windows-test-evidence", "pluginval":"cs80-pluginval-windows"}
def require(ok,message):
    if not ok: raise ValueError(message)
def save(path,obj):
    with path.open("x") as f: json.dump(obj,f,indent=2);f.write("\n")
def main(downloads):
    metadata=json.loads((HERE/"artifacts-final-api.json").read_bytes())["artifacts"]
    selected={}
    for role,name in NAMES.items():
        candidates=[a for a in metadata if a["name"]==name]
        require(len(candidates)==1,"Missing/duplicate artifact: "+name)
        api=candidates[0];raw=(downloads/(str(api["id"])+".zip")).read_bytes()
        require(api["workflow_run"]["id"]==RUN and api["workflow_run"]["head_sha"]==SOURCE and api["expired"] is False,"Artifact context")
        require(len(raw)==api["size_in_bytes"] and "sha256:"+hashlib.sha256(raw).hexdigest()==api["digest"],"Actual archive identity: "+role)
        selected[role]=api
    aliases=downloads/"windows-fcc3c8e";aliases.mkdir(exist_ok=True)
    for role,api in selected.items():
        target=aliases/(role+".zip")
        require(not target.exists(),"Refusing existing archive alias")
        os.link(downloads/(str(api["id"])+".zip"),target)
        save(HERE/(role+"-api.json"),api)
    template=json.loads((REPO/"docs/preset-evidence/2026.11.6/verification/a52/inputs-template.json").read_bytes())
    template.pop("_template_status",None)
    template["native"]=[{"target":"windows-x86_64","archive":str(aliases/"test-evidence.zip")}]
    template["validators"]=[{"target":"windows-x86_64","archive":str(aliases/"pluginval.zip")}]
    save(HERE/"native-validator-inputs.json",template)
    scripts=REPO/"docs/preset-evidence"
    common=["--repo",str(REPO),"--source",SOURCE,"--run-id",str(RUN),"--attempt","1"]
    commands=[
      [sys.executable,str(scripts/"2026.11.6/verification/a52/parse-native-evidence.py"),"--inputs",str(HERE/"native-validator-inputs.json"),"--out",str(HERE/"native-validator-observations.json")],
      [sys.executable,str(scripts/"2026.11.6/verification/a52/bind-parser-archives.py"),*common,"--target","windows-x86_64","--observations",str(HERE/"native-validator-observations.json"),"--native-zip",str(aliases/"test-evidence.zip"),"--native-api",str(HERE/"test-evidence-api.json"),"--validator-zip",str(aliases/"pluginval.zip"),"--validator-api",str(HERE/"pluginval-api.json"),"--out",str(HERE/"parser-archive-binding.json")],
      [sys.executable,str(scripts/"2026.11.5/verification/a41-verify-install-artifact.py"),*common,"--revision","2026.11.6","--run-api",str(HERE/"run-attempt-final-api.json"),"--platform","windows","--architecture","x86_64","--archive",str(aliases/"install.zip"),"--artifact-api",str(HERE/"install-api.json"),"--out",str(HERE/"install-acceptance.json")],
      [sys.executable,str(scripts/"2026.11.5/verification/a36-verify-validator-payload.py"),*common,"--revision","2026.11.6","--run-api",str(HERE/"run-attempt-final-api.json"),"--platform","windows","--install-archive",str(aliases/"install.zip"),"--install-api",str(HERE/"install-api.json"),"--validator-archive",str(aliases/"vst3.zip"),"--validator-api",str(HERE/"vst3-api.json"),"--consumer-log",str(HERE/"validator-job.log"),"--out",str(HERE/"payload-binding.json")]]
    save(HERE/"inspection-commands.json",commands)
    for i,command in enumerate(commands,1):
        result=subprocess.run(command,cwd=REPO,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
        (HERE/f"inspection-{i}.log").write_text(result.stdout)
        require(result.returncode==0,f"Inspection {i} failed; preserve its output and investigate")
    print("Byte inspections complete; original-job platform acceptance remains separate.")
if __name__=="__main__":
    p=argparse.ArgumentParser(description=__doc__);p.add_argument("--downloads",required=True,type=Path);a=p.parse_args();main(a.downloads.resolve())
