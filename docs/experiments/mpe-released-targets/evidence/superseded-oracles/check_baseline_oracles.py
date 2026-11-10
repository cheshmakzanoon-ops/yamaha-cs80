from pathlib import Path
import subprocess,json,hashlib,re
r=Path('/workspace/scratch/ee447c520988');s=r/'release-followup';out=r/'mpe-released-target-evidence/superseded-oracles';baseline=r/'sixteen-note-release';lib=r/'build-sixteen-release-profile-gcc-generic/libcs80_core.a';records=[]
for name,checks,failures in [('PolyPressureRoutingTests',4504,0),('TuningMidiTests',22085,6)]:
    binary=out/(name+'-baseline-final-oracle');source=s/'tests'/f'{name}.cpp';compile=['c++','-std=c++20','-O3','-DNDEBUG','-DCS80_DISABLE_AVX2_FILTER_BATCH=1','-I'+str(baseline/'src'),str(source),str(lib),'-o',str(binary)]
    for stage,args in [('compile',compile),('run',[str(binary)])]:
        log=out/(name+'-baseline-final-'+stage+'.log')
        with log.open('x') as f:p=subprocess.run(args,cwd=s,stdout=f,stderr=subprocess.STDOUT)
        record=dict(name=name,stage=stage,argv=args,cwd=str(s),exit_code=p.returncode,log=log.name,log_sha256=hashlib.sha256(log.read_bytes()).hexdigest(),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),baseline_head=subprocess.check_output(['git','-C',str(baseline),'rev-parse','HEAD'],text=True).strip(),library_sha256=hashlib.sha256(lib.read_bytes()).hexdigest());records.append(record);(out/'baseline-final-commands.json').write_text(json.dumps(records,indent=2)+'\n')
        expected=0 if stage=='compile' or failures==0 else 1
        if p.returncode!=expected:raise RuntimeError((name,stage,p.returncode))
        if stage=='run':
            text=log.read_text();last=text.splitlines()[-1];pattern=(r'(\d+) checks, (\d+) failures' if name=='PolyPressureRoutingTests' else r'Tuning / MIDI / voice modes: (\d+) checks, (\d+) failures');m=re.fullmatch(pattern,last)
            if not m or tuple(map(int,m.groups()))!=(checks,failures):raise RuntimeError((name,last))
            print(name,last,flush=True)
