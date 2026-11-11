// Compile the actual adapter with Windows SDK's function-like max macro.
// This is a portability regression check, not a native Windows qualification.
#include "plugin/MtsEspClient.h"
#include "vendor/mts-esp/Client/libMTSClient.h"
#include <cmath>
#include <limits>

#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "../src/plugin/MtsEspClient.cpp"
#undef max

struct MTSClient {};
namespace { MTSClient registeredClient; }
extern "C" {
MTSClient* MTS_RegisterClient() { return &registeredClient; }
void MTS_DeregisterClient(MTSClient*) {}
const char* MTS_GetScaleName(MTSClient*) { return ""; }
bool MTS_HasMaster(MTSClient*) { return true; }
double MTS_NoteToFrequency(MTSClient*, char note, signed char)
{
    return 440.0 * std::exp2((static_cast<unsigned char>(note) - 69.0) / 12.0);
}
bool MTS_ShouldFilterNote(MTSClient*, char, signed char) { return false; }
}
int main()
{
    cs80::MtsEspClient adapter;
    cs80::tuning::TuningTable table;
    return adapter.hasMaster() && adapter.fillTable(table) && table.pitch[69] == 69.0f ? 0 : 1;
}
