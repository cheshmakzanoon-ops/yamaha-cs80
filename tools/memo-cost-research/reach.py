"""Separate count-only archive/driver. No counter enters timed binaries."""
import json,pathlib,shlex,subprocess,shutil
from metrics import read,check,groups

def qualify(r):
 assert r.binding_approved
 src=r.sources['C'];b=r.builds['C'];p=r.work/'reach';p.mkdir()
 original=(src/'src/core/Engine.cpp').read_text()
 replacements=[('namespace cs80\n{','extern "C" { unsigned long long memoCounts[3] = {}; }\nnamespace cs80\n{'),('return tuningRef * value;','{ ++memoCounts[0]; return tuningRef * value; }'),('const float computed = std::pow(2.0f, exponent);','++memoCounts[1]; const float computed = std::pow(2.0f, exponent);'),('return midiToHz(note, tuningRef);','++memoCounts[2]; return midiToHz(note, tuningRef);')]
 s=original
 for a,c in replacements:assert s.count(a)==1;s=s.replace(a,c,1)
 (p/'Engine.cpp').write_text(s)
 entries=json.loads((b/'compile_commands.json').read_text());matches=[e for e in entries if e['file']==str(src/'src/core/Engine.cpp') and 'CMakeFiles/cs80_core.dir/' in e['command']];assert len(matches)==1
 cmd=shlex.split(matches[0]['command']);cmd[cmd.index('-o')+1]=str(p/'Engine.cpp.o');cmd[cmd.index('-c')+1]=str(p/'Engine.cpp');cmd+=['-I'+str(src/'src/core')]
 if '-MF' in cmd:cmd[cmd.index('-MF')+1]=str(p/'Engine.d')
 r.command(cmd,'reach-core',cwd=b);shutil.copyfile(b/'libcs80_core.a',p/'lib.a');r.command(['ar','r',p/'lib.a',p/'Engine.cpp.o'],'reach-archive')
 assert subprocess.check_output(['ar','t',str(p/'lib.a')],text=True).splitlines().count('Engine.cpp.o')==1
 s=(src/'src/tools/source_factory_benchmark.cpp').read_text();a='namespace {\n';assert s.count(a)==1;s=s.replace(a,'extern "C" unsigned long long memoCounts[3];\n'+a,1)
 a='auto measure = [&](Phase measurementPhase) {';assert s.count(a)==1;s=s.replace(a,a+'\n        for(auto& count:memoCounts)count=0;',1)
 a='    out.flush();';assert s.count(a)==1;s=s.replace(a,'    std::cerr << "MEMO_COUNTS," << modeName(c) << "," << scenario << "," << c.selector << "," << phase << "," << memoCounts[0] << "," << memoCounts[1] << "," << memoCounts[2] << "\\n";\n'+a,1)
 (p/'Benchmark.cpp').write_text(s)
 r.command(['g++','-std=c++20','-O3','-DNDEBUG','-DCS80_EXPERIMENTAL_PITCH_MEMO_SSE_LIBM=1','-I'+str(src/'src'),'-I'+str(src/'src/tools'),p/'Benchmark.cpp',p/'lib.a','-o',p/'probe'],'reach-driver-build')
 r.command(['readelf','-Wr',p/'probe'],'reach-driver-relocations')
 summary={}
 for low in [False,True]:
  suite='lowload' if low else 'active'
  for arm,exe in [('A',r.executables['A']),('countedC',p/'probe')]:
   name='qualification-count-'+suite+'-'+arm
   r.command([exe,'--out',r.out/name,'--source-commit', '5836041138bc1708c3fd2d4240cf63a3e1def4b3' if arm=='A' else '46812652364337aec1c8d51443427d1b63df5351','--suite','baseline' if low else 'performance','--rate','48000','--block','128','--callbacks','512' if low else '256','--quick','--callback-cpu'],name,pin=True)
  from binding import verify
  for arm,reloc in [('A','A-dynamic-relocations.log'),('countedC','reach-driver-relocations.log')]:
   receipt=verify((r.out/('qualification-count-'+suite+'-'+arm+'.log')).read_text(),(r.out/reloc).read_text(),r.approved_binding)
   from run import sha
   assert sha(pathlib.Path(receipt['library_path']).resolve())==r.approved_binding['libraries']['libm']['sha256']
  a=read(r.out/('qualification-count-'+suite+'-A'),low);c=read(r.out/('qualification-count-'+suite+'-countedC'),low);check(a,c)
  lines=(r.out/('qualification-count-'+suite+'-countedC.log')).read_text().splitlines();counts={}
  for line in lines:
   if not line.startswith('MEMO_COUNTS,'):continue
   f=line.split(',');assert len(f)==8;key=tuple(f[1:5]);assert key not in counts;counts[key]=list(map(int,f[5:]))
  assert set(counts)==set(c)
  for keys in groups(c,low).values():
   for key in keys:assert counts[key][0]>0 and counts[key][2]==0,(key,counts[key])
  if not low:
   for key in c:
    if key[0]=='source_performance' and key[1].startswith('mpe_'):assert counts[key][2]>0 and counts[key][0]==0,(key,counts[key])
  summary[suite]={str(k):v for k,v in counts.items()}
 r.save('actual-memo-reach.json',{'scope':'separate count-only qualification; timings incidental and excluded from cost analysis','columns':['hits','pow_misses','helper_fallbacks'],'counts':summary})
