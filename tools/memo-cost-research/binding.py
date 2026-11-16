"""Common driver-only process-boundary binding receipt, outside callback clocks."""
import re
HELPER=r'''
#include <dlfcn.h>
#include <cstdint>
#include <stdexcept>
#include <cstdio>
#include <cfenv>
#include <cerrno>
static void memoResearchBinding(const char* phase) {
    fenv_t saved; const int savedErrno=errno;
    if(fegetenv(&saved)!=0) throw std::runtime_error("binding fenv capture failed");
    void** slot; asm volatile("lea powf@GOTPCREL(%%rip),%0":"=r"(slot));
    void* target=*slot; Dl_info library{},image{};
    const bool ok=target && target==dlsym(RTLD_DEFAULT,"powf") && dladdr(target,&library)
        && dladdr(reinterpret_cast<void*>(&memoResearchBinding),&image);
    if(ok) std::fprintf(stderr,"MEMO_BIND\t%s\t%p\t%p\t%p\t0x%llx\t%s\t%p\n",phase,
        static_cast<void*>(slot),target,library.dli_fbase,
        (unsigned long long)((uintptr_t)target-(uintptr_t)library.dli_fbase),library.dli_fname,image.dli_fbase);
    const int restored=fesetenv(&saved);errno=savedErrno;
    if(!ok||restored!=0) throw std::runtime_error("binding inspection failed");
}
'''
def instrument(source):
 main='int main(int argc, char** argv) {\n    try {'
 finish='        complete.close();\n        return 0;'
 assert source.count(main)==1 and source.count(finish)==1
 source=source.replace(main,HELPER+'\n'+main+'\n        memoResearchBinding("begin");',1)
 return source.replace(finish,'        complete.close();\n        memoResearchBinding("end");\n        return 0;',1)
def verify(log,relocations,approved):
 lines=[x.split('\t') for x in log.splitlines() if x.startswith('MEMO_BIND\t')]
 assert len(lines)==2 and [x[1] for x in lines]==['begin','end'],lines
 assert lines[0][2:]==lines[1][2:],'binding changed within benchmark'
 _,_,got,target,base,offset,path,image=lines[0]
 assert int(target,16)-int(base,16)==int(offset,16)==int(approved['implementation_offset'],16)
 slot=int(got,16)-int(image,16)
 matches=[]
 for line in relocations.splitlines():
  fields=line.split()
  if fields and re.fullmatch('[0-9a-fA-F]+',fields[0]) and int(fields[0],16)==slot and 'powf@'+approved['symbol_version'] in line:matches.append(line)
 assert len(matches)==1 and ('GLOB_DAT' in matches[0] or 'JUMP_SLOT' in matches[0]),matches
 return {'relative_got':hex(slot),'implementation_offset':offset,'library_path':path,'relocation':matches[0]}

def codegen(assembly,relocations,arm):
 """Require real Engine calls, plus helper miss calls, to the one powf thunk."""
 sections={m.group(2):m.group(3) for m in re.finditer(r'^([0-9a-f]+) <([^>]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',assembly,re.M|re.S)}
 thunks=[(name,body) for name,body in sections.items() if name=='powf@plt']
 assert len(thunks)==1,'missing/ambiguous powf thunk'
 body=thunks[0][1]
 targets=re.findall(r'jmp\s+QWORD PTR \[rip[^\]]*\]\s*#\s*([0-9a-f]+)',body)
 assert len(targets)==1,'unknown powf thunk lowering'
 slot=int(targets[0],16)
 lines=[line for line in relocations.splitlines() if line.split() and re.fullmatch('[0-9a-fA-F]+',line.split()[0]) and int(line.split()[0],16)==slot and 'powf@' in line]
 assert len(lines)==1 and 'GLOB_DAT' in lines[0],'powf must be eagerly bound to receipt GOT'
 engine={n:b for n,b in sections.items() if 'Engine' in n and 'processInternalSampleImpl' in n}
 raw=[n for n,b in engine.items() if re.search(r'\b(call|jmp)\s+[^\n]*<powf@plt>',b)]
 assert raw,'no actual Engine raw powf callsite'
 helpers=[]
 if arm=='C':
  helpers=[n for n,b in sections.items() if 'PitchPowMemo2hz' in n and re.search(r'\b(call|jmp)\s+[^\n]*<powf@plt>',b)]
  assert len(helpers)==1,'missing actual memo/raw helper powf miss call'
  assert any('<'+helpers[0]+'>' in b for b in engine.values()),'Engine does not call memo/raw helper'
 return {'relative_got':hex(slot),'relocation':lines[0],'engine_raw_symbols':raw,'helper_symbols':helpers,'powf_plt':body}
