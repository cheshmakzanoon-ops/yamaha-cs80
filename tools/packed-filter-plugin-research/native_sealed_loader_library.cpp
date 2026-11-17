// Inert exports only; no synth, factory object or audio.
#include <cstdlib>
#include <cstdio>
__attribute__((constructor)) static void initialize(){if(const char* path=std::getenv("CS80_SEALED_MUTATE")){if(auto* f=std::fopen(path,"w")){std::fputs("changed",f);std::fclose(f);}}}
extern "C" {
#ifndef OMIT_ENTRY
bool ModuleEntry(void*){return true;}
#endif
void* GetPluginFactory(){return nullptr;}
bool ModuleExit(){return true;}
}
