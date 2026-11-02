#include "MtsEspClient.h"

#include <cmath>
#include <limits>

// Default shipping branch: the pinned client is vendored with its licence.
// An explicit 0 is retained for the no-client fallback compile test.
#ifndef CS80_WITH_MTS_ESP
 #if defined(__has_include)
  #if __has_include("../../deps/mts-esp/Client/libMTSClient.h")
   #define CS80_WITH_MTS_ESP 1
  #endif
 #endif
#endif
#ifndef CS80_WITH_MTS_ESP
 #define CS80_WITH_MTS_ESP 0
#endif
#if CS80_WITH_MTS_ESP
 #include "../../deps/mts-esp/Client/libMTSClient.h"
 // The contract fixture supplies the same C API at link time. Only the test
 // target defines this; normal plugin builds include the untouched client.
 #ifndef CS80_MTS_ESP_TEST_BACKEND
  #include "../../deps/mts-esp/Client/libMTSClient.cpp"
 #endif
#endif

namespace cs80
{
#if CS80_WITH_MTS_ESP
MtsEspClient::MtsEspClient() : client(MTS_RegisterClient()) {}
MtsEspClient::~MtsEspClient()
{
    if (client != nullptr) MTS_DeregisterClient(static_cast<MTSClient*>(client));
}
bool MtsEspClient::available() noexcept { return true; }
bool MtsEspClient::hasMaster() const noexcept
{
    return client != nullptr && MTS_HasMaster(static_cast<MTSClient*>(client));
}
bool MtsEspClient::fillTable(tuning::TuningTable& table) const noexcept
{
    if (!hasMaster()) return false;
    auto* c = static_cast<MTSClient*>(client);
    // An external MTS-ESP master may supply malformed Hz values. Stage all
    // notes so one bad mapped value never leaks a partly updated tuning table
    // (or non-finite pitch) into the audio engine.
    tuning::TuningTable next = table;
    for (int n = 0; n < 128; ++n)
    {
        const auto i = (std::size_t) n;
        const double hz = MTS_NoteToFrequency(c, (char) n, -1);
        const bool mapped = !MTS_ShouldFilterNote(c, (char) n, -1);
        // Core pitch/oscillator/key-voltage calculations use float. Preserve
        // 16 octaves of headroom for downstream filter CV and pitch multipliers
        // before converting Hz, and prevent underflow or invalid logarithms.
        constexpr double dspHeadroom = 65536.0;
        const bool safeHz = std::isfinite(hz) && hz > 0.0
                            && hz >= (double) std::numeric_limits<float>::denorm_min()
                            && hz <= (double) (std::numeric_limits<float>::max)() / dspHeadroom;
        const float pitch = safeHz ? tuning::TuningTable::pitchFromHz(hz) : (float) n;
        if (!safeHz || !std::isfinite(pitch))
        {
            if (mapped) return false;
            // A filtered note is not played, but may still be retuned in a
            // currently sounding voice. Its fallback must remain finite.
            next.pitch[i] = (float) n;
            next.mapped[i] = false;
            continue;
        }
        next.pitch[i] = pitch;
        next.mapped[i] = mapped;
    }
    table = next;
    return true;
}
void MtsEspClient::parseMidi(const std::uint8_t* data, int size) noexcept
{
    if (client != nullptr && data != nullptr && size > 0)
        MTS_ParseMIDIDataU(static_cast<MTSClient*>(client), data, size);
}
#else
MtsEspClient::MtsEspClient() = default;
MtsEspClient::~MtsEspClient() = default;
bool MtsEspClient::available() noexcept { return false; }
bool MtsEspClient::hasMaster() const noexcept { return false; }
bool MtsEspClient::fillTable(tuning::TuningTable&) const noexcept { return false; }
void MtsEspClient::parseMidi(const std::uint8_t*, int) noexcept {}
#endif
} // namespace cs80
