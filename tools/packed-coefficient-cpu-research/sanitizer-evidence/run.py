from pathlib import Path
import hashlib,json,subprocess,os
r=Path('/workspace/shared/cs80-audits/packed-coefficient-admission');v=r/'explicit-or-variant';out=v/'sanitizer';stock=Path('/workspace/shared/cs80-audits/draft1x-design/native-study/stock');control=r/'sanitizer/stock.bin'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert sha(v/'Ig00156Batch.cpp')=='a612510e598be7471aa8ced68b57e56868cd7345335dc475d44bfab3758bb6fd'
assert sha(control)=='af13f05f7445ebfacdc607c36ed125e4386c7c522d78af0a8d02236a2fd55c0e'
before=(r/'candidate/src/core/Ig00156Batch.cpp').read_text();after=(v/'Ig00156Batch.cpp').read_text()
old='        for (V value : {g1,f1,fromFb,fromIn,fromU,fromBp,kiGs,invA11,inFromX})\n            badCoefficient=_mm256_or_si256(badCoefficient,outsideCoefficientBound(value));'
new='\n'.join('        badCoefficient=_mm256_or_si256(badCoefficient,outsideCoefficientBound('+x+'));' for x in ['g1','f1','fromFb','fromIn','fromU','fromBp','kiGs','invA11','inFromX'])
assert before.count(old)==1 and before.replace(old,new)==after
inputs={str(p):sha(p) for p in (stock/'src').rglob('*') if p.is_file()}
for p in [r/'component.cpp',v/'Ig00156Batch.cpp',control]:inputs[str(p)]=sha(p)
(out/'inputs.json').write_text(json.dumps(inputs,indent=2)+'\n');ledger=[]
def run(label,argv):
 rec={'label':label,'argv':list(map(str,argv)),'timeout_seconds':120};ledger.append(rec)
 try:
  with (out/(label+'.stdout')).open('wb') as stdout,(out/(label+'.stderr')).open('wb') as stderr:
   proc=subprocess.run(rec['argv'],stdout=stdout,stderr=stderr,timeout=120,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0:halt_on_error=1','UBSAN_OPTIONS':'halt_on_error=1:print_stacktrace=1'});rec['exit']=proc.returncode
 except BaseException as e:rec['exception']=type(e).__name__;raise
 finally:(out/'commands.json').write_text(json.dumps(ledger,indent=2)+'\n')
 assert rec['exit']==0,rec
try:
 run('compiler',['g++','--version']);assert '14.2.0' in (out/'compiler.stdout').read_text()
 flags=['-std=gnu++20','-O1','-g','-DNDEBUG','-fPIC','-ffp-contract=off','-fno-omit-frame-pointer','-fsanitize=address,undefined','-fno-sanitize-recover=all']
 run('compile',['g++',*flags,'-I'+str(stock/'src'),'-I'+str(stock/'src/core'),'-DBATCH_SOURCE="'+str(v/'Ig00156Batch.cpp')+'"',r/'component.cpp','-o',out/'candidate'])
 run('component',[out/'candidate',out/'candidate.bin'])
 run('compare',['cmp',control,out/'candidate.bin'])
 assert all(sha(Path(p))==digest for p,digest in inputs.items())
 result={'outcome':'ASAN_UBSAN_EXPLICIT_OR_COMPONENT_PASS','variant_sha256':sha(v/'Ig00156Batch.cpp'),'source_review':'Only exact nine-entry integer OR loop unroll, same order; no FP/guard/solver changes.','control_reused_not_rerun':str(control),'corpus_sha256':sha(out/'candidate.bin'),'corpus_bytes':(out/'candidate.bin').stat().st_size,'checks':(out/'component.stdout').read_text(),'binary_sha256':sha(out/'candidate'),'inputs_unchanged':len(inputs),'stderr_empty':all(p.stat().st_size==0 for p in out.glob('*.stderr')),'limits':['LeakSanitizer disabled due documented sandbox ptrace incompatibility; leaks unqualified.','Instrumented GNU14 O1 component only; no timing, full engine or production certification.']}
except BaseException as e:result={'outcome':'FAIL_OR_BLOCKED','exception':type(e).__name__,'reason':str(e)}
(out/'RESULT.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
