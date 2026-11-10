from pathlib import Path
import subprocess,json,hashlib,re
r=Path('/workspace/scratch/ee447c520988');s=r/'release-followup';out=r/'mpe-released-target-evidence/superseded-oracles/tuning-corrected-command';out.mkdir(exist_ok=False);base=r/'sixteen-note-release';lib=r/'build-sixteen-release-profile-gcc-generic/libcs80_core.a';binary=out/'probe';records=[]
args=['c++','-std=c++20','-O3','-DNDEBUG','-DCS80_DISABLE_AVX2_FILTER_BATCH=1','-DCS80_TUNING_DATA_DIR="'+str(base/'tests/data/tuning')+'"','-I'+str(base/'src'),str(s/'tests/TuningMidiTests.cpp'),str(lib),'-o',str(binary)]
for stage,cmd in [('compile',args),('run',[str(binary)])]:
    log=out/(stage+'.log')
    with log.open('x') as f:p=subprocess.run(cmd,cwd=s,stdout=f,stderr=subprocess.STDOUT)
    records.append(dict(stage=stage,argv=cmd,cwd=str(s),exit_code=p.returncode,log=log.name,sha256=hashlib.sha256(log.read_bytes()).hexdigest()));(out/'commands.json').write_text(json.dumps(records,indent=2)+'\n')
    if p.returncode!=(0 if stage=='compile' else 1):raise RuntimeError((stage,p.returncode))
    if stage=='run':
        text=log.read_text();last=text.splitlines()[-1];m=re.fullmatch(r'Tuning / MIDI / voice modes: (\d+) checks, (\d+) failures',last)
        if not m or tuple(map(int,m.groups()))!=(22085,6):raise RuntimeError(last)
        print(text)
(out/'inputs.json').write_text(json.dumps(dict(head=subprocess.check_output(['git','-C',str(s),'rev-parse','HEAD'],text=True).strip(),test_sha256=hashlib.sha256((s/'tests/TuningMidiTests.cpp').read_bytes()).hexdigest(),baseline_head=subprocess.check_output(['git','-C',str(base),'rev-parse','HEAD'],text=True).strip(),library_sha256=hashlib.sha256(lib.read_bytes()).hexdigest(),data_sha256={str(p.relative_to(base)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((base/'tests/data/tuning').rglob('*')) if p.is_file()},binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest()),indent=2)+'\n')
