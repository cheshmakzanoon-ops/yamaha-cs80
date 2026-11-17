// Test-only inert shared library. Not a VST3 plugin: factory is always null.
#include <cstdio>
#include <cstdlib>
#ifndef ENTRY_FAIL
#define ENTRY_FAIL 0
#endif
#ifndef EXIT_FAIL
#define EXIT_FAIL 0
#endif
namespace Steinberg {class IPluginFactory;}
static void record(const char* event) {
    if(const char* path=std::getenv("CS80_LOADER_FIXTURE_LOG"))if(auto* file=std::fopen(path,"a")){std::fprintf(file,"%s\n",event);std::fclose(file);}
}
__attribute__((constructor)) static void loaded(){record("open");}
__attribute__((destructor)) static void unloaded(){record("unload");}
extern "C" {
#if defined(DATA_ENTRY)
int ModuleEntry=1;
#elif !defined(OMIT_ENTRY)
bool ModuleEntry(void*) {record("entry");return !bool(ENTRY_FAIL);}
#endif
#ifndef OMIT_FACTORY
Steinberg::IPluginFactory* GetPluginFactory(){return nullptr;}
#endif
#ifndef OMIT_EXIT
bool ModuleExit(){record("exit");return !bool(EXIT_FAIL);}
#endif
}
