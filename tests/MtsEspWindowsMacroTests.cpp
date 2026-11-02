// Compile the shipping adapter as a single translation unit with the
// function-like max macro exported by Windows SDK headers. A bare
// numeric_limits<float>::max() is not portable even when Linux builds pass.
#include "plugin/MtsEspClient.h"
#include "../deps/mts-esp/Client/libMTSClient.h"

#include <cmath>
#include <cstdint>
#include <limits>

// Standard headers are preloaded before simulating Windows' min/max macros.
// The real MTS-ESP C API is replaced only in this deterministic test binary.
#define max(a, b) (((a) > (b)) ? (a) : (b))
#include "../src/plugin/MtsEspClient.cpp"
#undef max

struct MTSClient {};
namespace { MTSClient client; }
extern "C" {
MTSClient* MTS_RegisterClient() { return &client; }
void MTS_DeregisterClient(MTSClient*) {}
bool MTS_HasMaster(MTSClient*) { return true; }
double MTS_NoteToFrequency(MTSClient*, char note, signed char)
{
    return 440.0 * std::exp2((static_cast<unsigned char>(note) - 69.0) / 12.0);
}
bool MTS_ShouldFilterNote(MTSClient*, char, signed char) { return false; }
void MTS_ParseMIDIDataU(MTSClient*, const unsigned char*, int) {}
}

int main()
{
    cs80::MtsEspClient adapter;
    cs80::tuning::TuningTable table;
    return adapter.hasMaster() && adapter.fillTable(table) && table.pitch[69] == 69.0f ? 0 : 1;
}
