// Inert GNU_UNIQUE retention control, not a production flag/source change.
#include "native_observer_module_history_library.cpp"
inline int cs80_inert_gnu_unique_anchor=17;
__attribute__((constructor)) static void useUnique(){++cs80_inert_gnu_unique_anchor;}
extern "C" int* cs80_inert_unique_address(){return &cs80_inert_gnu_unique_anchor;}
