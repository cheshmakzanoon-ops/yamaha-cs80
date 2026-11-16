"""Read-only runtime/libm qualification evidence; never enables the memo."""
import argparse,hashlib,json,os,pathlib,re,subprocess,traceback
HERE=pathlib.Path(__file__).resolve().parent
REQUEST={'request_id':'memo-libm-inspection-4681265-02','candidate_reference':'46812652364337aec1c8d51443427d1b63df5351','scope':'inspection-only-no-memo-no-timing'}
def sha(p):return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def check_binding(text,relocs,assembly):
 rows=[x.split('\t') for x in text.splitlines() if x.startswith('BIND\t')];assert len(rows)==1
 _,slot,ptr,base,offset,path=rows[0];slotnum=int(slot,16)
 assert int(ptr,16)-int(base,16)==int(offset,16)
 matches=[]
 for line in relocs.splitlines():
  fields=line.split()
  if fields and re.fullmatch('[0-9a-fA-F]+',fields[0]) and int(fields[0],16)==slotnum and 'powf@' in line:matches.append(line)
 assert len(matches)==1 and 'R_X86_64_GLOB_DAT' in matches[0],matches
 raw=re.search(r'<raw_pow>:\n(.*?)(?=\n\s*\n|\Z)',assembly,re.S);assert raw
 assert re.search(r'\bjmp\s+[0-9a-f]+ <powf@plt>',raw.group(1)),raw.group(1)
 plt=re.search(r'<powf@plt>:\n(.*?)(?=\n\s*\n|\Z)',assembly,re.S);assert plt
 # Intel objdump's comment is the absolute GOT slot for this non-PIE probe.
 assert any(re.search(r'\bjmp\b',line) and re.search(r'#\s*0*'+format(slotnum,'x')+r'\b',line) for line in plt.group(1).splitlines()),plt.group(1)
 result=[x.split('\t') for x in text.splitlines() if x.startswith('RESULT\t')];assert result==[['RESULT','2097168','0','1']],result
 return {'got_slot':slot,'resolved_pointer':ptr,'mapped_base':base,'implementation_offset':offset,'library_path':path,'relocation':matches[0]}
def inspect(out):
 assert os.environ.get('GITHUB_RUN_ATTEMPT','1')=='1','no automatic reruns'
 assert json.loads((HERE/'request.json').read_text())==REQUEST
 (out/'source-hashes.json').write_text(json.dumps({p.name:sha(p) for p in HERE.iterdir() if p.is_file()},indent=2)+'\n')
 workflow=HERE.parents[1]/'.github/workflows/memo-libm-inspection.yml'
 (out/'workflow-hash.txt').write_text(sha(workflow)+'\n')
 for name in ['LD_PRELOAD','LD_AUDIT','LD_LIBRARY_PATH']:assert not os.environ.get(name),f'unexpected {name}'
 def cmd(args,name,optional=False):
  args=list(map(str,args));(out/(name+'-command.json')).write_text(json.dumps(args,indent=2)+'\n')
  with (out/(name+'.txt')).open('w') as f:p=subprocess.run(args,stdout=f,stderr=subprocess.STDOUT,timeout=180)
  (out/(name+'.exit')).write_text(str(p.returncode)+'\n');assert optional or p.returncode==0,name
 (out/'environment.json').write_text(json.dumps({'uname':list(os.uname()),'affinity':sorted(os.sched_getaffinity(0)),'run_id':os.environ.get('GITHUB_RUN_ID'),'workflow_sha':os.environ.get('GITHUB_SHA'),'scope':REQUEST['scope']},indent=2)+'\n')
 (out/'cpuinfo.txt').write_text(pathlib.Path('/proc/cpuinfo').read_text())
 cmd(['g++','--version'],'compiler');cmd(['lscpu'],'lscpu',optional=True)
 exe=out/'proof'
 cmd(['g++','-std=c++20','-O3','-DNDEBUG','-fno-pie','-no-pie','-Wl,-z,now',HERE/'Proof.cpp','-ldl','-o',exe],'compile')
 cmd([exe],'proof');cmd(['objdump','-d','-Mintel',exe],'proof-assembly');cmd(['readelf','-Wr',exe],'proof-relocations')
 bind=check_binding((out/'proof.txt').read_text(),(out/'proof-relocations.txt').read_text(),(out/'proof-assembly.txt').read_text())
 lib=pathlib.Path(bind['library_path']).resolve();assert lib.is_file() and lib.name.startswith('libm.so'),str(lib)
 objects=[]
 for line in (out/'proof.txt').read_text().splitlines():
  if not line.startswith('OBJECT\t'):continue
  p=pathlib.Path(line.split('\t',1)[1])
  if p.is_absolute() and p.exists():objects.append(p.resolve())
 libs={}
 for kind,predicate in [('libm',lambda n:n.startswith('libm.so')),('libc',lambda n:n.startswith('libc.so')),('loader',lambda n:n.startswith('ld-linux'))]:
  found=sorted(set(p for p in objects if predicate(p.name)));assert len(found)==1,(kind,found);p=found[0]
  cmd(['readelf','-n',p],kind+'-notes');cmd(['readelf','-Ws',p],kind+'-symbols')
  ids=re.findall(r'Build ID:\s*([0-9a-fA-F]+)',(out/(kind+'-notes.txt')).read_text());assert len(ids)==1
  libs[kind]={'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size,'build_id':ids[0]}
 assert libs['libm']['path']==str(lib)
 cmd(['objdump','-d','-Mintel',lib],'libm-complete-disassembly')
 # Preserve constants needed to prove normal-domain branch exclusion; no execution.
 cmd(['objdump','-s',lib],'libm-complete-section-bytes')
 start=int(bind['implementation_offset'],16);cmd(['objdump','-d','-Mintel','--start-address='+hex(start),'--stop-address='+hex(start+4096),lib],'resolved-powf-window')
 # This is a collection result, NEVER an automatic implementation approval.
 (out/'RESULT.json').write_text(json.dumps({'status':'evidence collected; independent normal-domain control-flow review required','memo_enabled':False,'timing_executed':False,'binding':bind,'libraries':libs,'proof_executable_sha256':sha(exe),'source_sha256':sha(HERE/'Proof.cpp'),'sample_calls':2097168},indent=2)+'\n')
 # Exclude the locally compiled executable from upload; retain its digest.
 exe.rename(out.parent/(out.name+'-proof-local-only'))
def main():
 p=argparse.ArgumentParser();p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args();out=a.out.resolve();out.mkdir(exist_ok=False)
 try:inspect(out)
 except BaseException:(out/'FAILED.txt').write_text(traceback.format_exc());(out/'run.exit').write_text('1\n');raise
 else:(out/'run.exit').write_text('0\n')
 finally:
  manifest={str(p.relative_to(out)):{'sha256':sha(p),'bytes':p.stat().st_size} for p in out.rglob('*') if p.is_file() and p.name!='proof'}
  (out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
if __name__=='__main__':main()
