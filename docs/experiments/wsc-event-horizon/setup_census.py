#!/usr/bin/env python3
"""Separate, untimed setup-predicate counts; consumes the clean matrix results."""
from pathlib import Path
import hashlib,json,shutil,subprocess,sys
if len(sys.argv)!=2:raise SystemExit('usage: setup_census.py EXISTING-CLEAN-ENGINE-WORK')
w=Path(sys.argv[1]).resolve(); base=w/'witness';d=w/'setup-census';d.mkdir(exist_ok=False)
shutil.copytree(base/'src',d/'src')
def replace(s,a,b):assert s.count(a)==1,a;return s.replace(a,b)
h=d/'src/core/ReconstructedOscillator.h';s=h.read_text()
s=replace(s,'    bool certifyHorizon() noexcept {','''    inline static std::uint64_t attempts=0,numeric=0,resetRejected=0,foldRejected=0,knotTests=0,knotRejected=0,turnTests=0,turnRejected=0;
    bool certifyHorizon() noexcept { ++attempts;''')
s=replace(s,'        constexpr unsigned K=4;','        ++numeric; constexpr unsigned K=4;')
s=replace(s,'if(state.charge<=K*(f*invRate+1.e-10))return false;','if(state.charge<=K*(f*invRate+1.e-10)){++resetRejected;return false;}')
s=replace(s,'std::abs(endNodes.u-state.heldThresholdVolts)<=du)return false;','std::abs(endNodes.u-state.heldThresholdVolts)<=du){++foldRejected;return false;}')
s=replace(s,'for(int k=-4;k<=4;++k)if(k!=0 && std::abs(endNodes.w-(.25*half)*double(k))<=dw)return false;','for(int k=-4;k<=4;++k)if(k!=0){++knotTests;if(std::abs(endNodes.w-(.25*half)*double(k))<=dw){++knotRejected;return false;}}')
s=replace(s,'        if(gain!=0.) {','        if(gain!=0.) { ++turnTests;')
s=replace(s,'std::abs(endNodes.w)+dw)+gain*du+1.e-8)return false;','std::abs(endNodes.w)+dw)+gain*du+1.e-8){++turnRejected;return false;}')
s=replace(s,'    friend struct WscCensusAccess;','    friend struct WscCensusAccess;') # observer only
h.write_text(s)
x=(w/'engine.cpp').read_text()
fields=['attempts','numeric','resetRejected','foldRejected','knotTests','knotRejected','turnTests','turnRejected']
a=' static void dump(FILE*f,const Engine&e)'
x=replace(x,a,' static void clearSetup(){'+''.join('ReconstructedOscillator::'+v+'=0;' for v in fields)+'}\n static void setupReport(){std::printf("{'+ ','.join('\\\"'+v+'\\\":%llu' for v in fields)+'}\\n",'+','.join('(unsigned long long)ReconstructedOscillator::'+v for v in fields)+');}\n'+a)
x=replace(x,'    unsigned nonzero=0,quietBlocks=0,playingBlocks=0;','    cs80::WscCensusAccess::clearSetup();\n    unsigned nonzero=0,quietBlocks=0,playingBlocks=0;')
x=replace(x,'    if(!nonzero&&scene!=5)std::abort();','    cs80::WscCensusAccess::setupReport();\n    if(!nonzero&&scene!=5)std::abort();')
(d/'harness.cpp').write_text(x)
flags=['-std=gnu++20','-O3','-DNDEBUG','-fPIC','-Wall','-Wextra','-Wpedantic','-DCS80_WSC_HORIZON_OBSERVE','-DCS80_WSC_HORIZON_VERIFY','-I'+str(d/'src')]
for member,src in [('Engine',d/'src/core/Engine.cpp'),('harness',d/'harness.cpp')]:
 with (d/(member+'-build.log')).open('x') as out:
  subprocess.run(['g++',*flags,'-c',str(src),'-o',d/(member+'.o')],check=True,stdout=out,stderr=out)
subprocess.run(['g++',d/'harness.o',d/'Engine.o',base/'FactoryPresets.o',base/'Ig00156Batch.o','-o',d/'probe'],check=True)
rows=[r for r in json.loads((w/'engine-results.json').read_text())['rows'] if r['arm']=='witness']
res=[]
for i,r in enumerate(rows):
 f=d/f'{i}.trace';run=subprocess.run([d/'probe',str(r['scene']),str(f),str(r['rate']),str(int(r['filterKernel']))],check=True,capture_output=True,text=True)
 a,b=map(json.loads,run.stdout.splitlines())
 for k,v in a.items():assert r[k]==v,(k,v,r[k])
 assert f.stat().st_size==r['traceBytes'] and hashlib.sha256(f.read_bytes()).hexdigest()==r['sha256']
 assert b['attempts']>=b['numeric'];assert b['numeric']==b['resetRejected']+b['foldRejected']+b['knotRejected']+b['turnRejected']+r['certificates']
 assert b['knotTests']<=8*b['numeric'] and b['turnTests']<=b['numeric'] and r['fast']<=4*r['certificates']
 q={'matrix_row':i,**a,**b,'trace_sha256':r['sha256']};res.append(q);print(json.dumps(q),flush=True);f.unlink()
(d/'setup-counts.json').write_text(json.dumps(res,indent=2)+'\n')
