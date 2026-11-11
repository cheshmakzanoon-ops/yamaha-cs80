#include "MtsEspClient.h"

#include <cmath>
#include <limits>

// The shipping client is pinned in-tree: missing source is a build error,
// never a silent feature downgrade. Explicit 0 exercises the fallback build.
#ifndef CS80_WITH_MTS_ESP
 #define CS80_WITH_MTS_ESP 1
#endif
#if CS80_WITH_MTS_ESP
 #include "vendor/mts-esp/Client/libMTSClient.h"
 // Only the standalone contract test supplies its own C API implementation.
 #ifndef CS80_MTS_ESP_TEST_BACKEND
  #include "vendor/mts-esp/Client/libMTSClient.cpp"
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
    // The master is external data. Stage a complete update so one invalid
    // mapped frequency cannot partly replace the caller's current tuning.
    tuning::TuningTable next = table;
    for (int n = 0; n < 128; ++n)
    {
        const auto i = static_cast<std::size_t>(n);
        const double hz = MTS_NoteToFrequency(c, static_cast<char>(n), -1);
        const bool mapped = !MTS_ShouldFilterNote(c, static_cast<char>(n), -1);
        // Pitch/frequency calculations in the Engine use float. Reject values
        // outside its range, reserving 16 octaves for downstream multipliers.
        constexpr double dspHeadroom = 65536.0;
        const bool safeHz = std::isfinite(hz) && hz > 0.0
            && hz >= static_cast<double>(std::numeric_limits<float>::denorm_min())
            && hz <= static_cast<double>((std::numeric_limits<float>::max)()) / dspHeadroom;
        const float pitch = safeHz ? tuning::TuningTable::pitchFromHz(hz) : static_cast<float>(n);
        if (!safeHz || !std::isfinite(pitch))
        {
            if (mapped) return false;
            // Filtered keys can still have a sounding voice from an earlier
            // mapping. Keep their stored pitch finite when updating the table.
            next.pitch[i] = static_cast<float>(n);
            next.mapped[i] = false;
        }
        else
        {
            next.pitch[i] = pitch;
            next.mapped[i] = mapped;
        }
    }
    // Reject an observed disconnect, which can make upstream queries fall
    // back to their local table. This is not a coherent source snapshot:
    // rapid reconnects or concurrent master edits have no revision API here.
    if (!hasMaster()) return false;
    table = next;
    return true;
}

bool MtsEspClient::fillChannelTables(ChannelTables& tables, bool channelIndependent) const noexcept
{
    if (channelIndependent)
    {
        auto table = tables[0];
        if (!fillTable(table)) return false;
        tables.fill(table);
        return true;
    }
    if (!hasMaster()) return false;
    auto* c = static_cast<MTSClient*>(client);
    auto next = tables;
    // The pinned client remembers channel-filter capability. Prime it before
    // the first frequency after a prior global/MPE query; discard this result.
    (void) MTS_ShouldFilterNote(c, 0, 0);
    for (int n = 0; n < 128; ++n)
    {
        bool havePitch = false;
        double previousHz = 0.0;
        float previousPitch = 0.0f;
        for (int channel = 0; channel < 16; ++channel)
        {
            const auto note = static_cast<char>(n);
            const auto ch = static_cast<signed char>(channel);
            const double hz = MTS_NoteToFrequency(c, note, ch);
            const bool mapped = !MTS_ShouldFilterNote(c, note, ch);
            constexpr double dspHeadroom = 65536.0;
            const bool safeHz = std::isfinite(hz) && hz > 0.0
                && hz >= static_cast<double>(std::numeric_limits<float>::denorm_min())
                && hz <= static_cast<double>((std::numeric_limits<float>::max)()) / dspHeadroom;
            float pitch = static_cast<float>(n);
            if (safeHz)
            {
                // Global masters commonly return the same Hz on all channels.
                // Reuse only this pure conversion, never a mapping or query.
                if (havePitch && hz == previousHz) pitch = previousPitch;
                else pitch = tuning::TuningTable::pitchFromHz(hz);
            }
            if (!safeHz || !std::isfinite(pitch))
            {
                if (mapped) return false;
                next[(size_t) channel].pitch[(size_t) n] = static_cast<float>(n);
                next[(size_t) channel].mapped[(size_t) n] = false;
            }
            else
            {
                havePitch = true; previousHz = hz; previousPitch = pitch;
                next[(size_t) channel].pitch[(size_t) n] = pitch;
                next[(size_t) channel].mapped[(size_t) n] = mapped;
            }
        }
    }
    if (!hasMaster()) return false;
    tables = next;
    return true;
}

#else
MtsEspClient::MtsEspClient() = default;
MtsEspClient::~MtsEspClient() = default;
bool MtsEspClient::available() noexcept { return false; }
bool MtsEspClient::hasMaster() const noexcept { return false; }
bool MtsEspClient::fillTable(tuning::TuningTable&) const noexcept { return false; }
bool MtsEspClient::fillChannelTables(ChannelTables&, bool) const noexcept { return false; }
#endif
} // namespace cs80
