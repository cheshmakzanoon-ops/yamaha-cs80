#include "MtsEspClient.h"

// Compile-time switch: the client is built in only when its sources are present.
#if defined(__has_include)
 #if __has_include("../../deps/mts-esp/Client/libMTSClient.h")
  #define CS80_WITH_MTS_ESP 1
  #include "../../deps/mts-esp/Client/libMTSClient.h"
  #include "../../deps/mts-esp/Client/libMTSClient.cpp"
 #endif
#endif
#ifndef CS80_WITH_MTS_ESP
 #define CS80_WITH_MTS_ESP 0
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
    for (int n = 0; n < 128; ++n)
    {
        table.pitch[(std::size_t) n] = tuning::TuningTable::pitchFromHz(MTS_NoteToFrequency(c, (char) n, -1));
        table.mapped[(std::size_t) n] = !MTS_ShouldFilterNote(c, (char) n, -1);
    }
    return true;
}
void MtsEspClient::parseMidi(const std::uint8_t* data, int size) noexcept
{
    if (client != nullptr) MTS_ParseMIDIDataU(static_cast<MTSClient*>(client), data, size);
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
