#include "core/Engine.h"
extern "C" __attribute__((noinline)) void stock_set(cs80::Engine* e,const cs80::modern::ModernConfig* c) { e->setModernConfig(*c); }
extern "C" __attribute__((noinline)) void stock_mirror(cs80::Engine* e,const cs80::Engine* s) { e->setModernConfig(s->getModernConfig()); e->setTransport(s->getModernTransport()); }
extern "C" unsigned long stock_size() { return sizeof(cs80::Engine); }
