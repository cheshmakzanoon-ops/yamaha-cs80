#include "MtsEspClient.h"

// The exact ODDSound client is vendored under src/vendor/mts-esp/ (0BSD).
// Build it in this translation unit so CS80 core-only builds do not need a
// separate library, and so a missing vendor file fails the build explicitly.
// The client searches for an installed libMTS master at runtime; that master is
// not bundled with this plugin.
#include "../vendor/mts-esp/Client/libMTSClient.h"
#include "../vendor/mts-esp/Client/libMTSClient.cpp"

namespace cs80
{
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
} // namespace cs80
