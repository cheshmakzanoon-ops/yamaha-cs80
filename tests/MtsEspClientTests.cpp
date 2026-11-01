// This target intentionally builds MtsEspClient.cpp even with CS80_BUILD_PLUGIN=OFF.
// It catches a missing/incompatible vendored ODDSound client at compile/link time
// instead of allowing the old __has_include fallback to disable MTS-ESP silently.
// No MTS-ESP master or JUCE installation is required for the test.

#include "plugin/MtsEspClient.h"
#include "vendor/mts-esp/Client/libMTSClient.h"

#include <array>
#include <cmath>
#include <cstdio>

namespace
{
int checks = 0;
int failures = 0;

void check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what); }
}
} // namespace

int main()
{
    check(cs80::MtsEspClient::available(), "the vendored client is compiled into the shim");

    // Registration and release are the exact lifecycle the plugin uses.
    {
        cs80::MtsEspClient shim;
        cs80::tuning::TuningTable table;
        const bool connected = shim.hasMaster();
        const bool received = shim.fillTable(table);
        check(received == connected, "the shim publishes master tunings only while connected");
        if (!connected)
            check(!received, "no master leaves the engine's standalone MTS parser in charge");
    }

    // These symbols are supplied by the actual ODDSound source, not by a mock.
    MTSClient* client = MTS_RegisterClient();
    check(client != nullptr, "register a real ODDSound MTS client");
    if (client != nullptr)
    {
        const double before = MTS_NoteToFrequency(client, 69, -1);
        check(std::isfinite(before) && before > 0.0, "MTS frequency query is finite");

        // A standard real-time single-note SysEx: tune A4 up one quarter-tone.
        constexpr std::array<unsigned char, 12> single {
            0xF0, 0x7F, 0x7F, 0x08, 0x02, 0x00, 0x01,
            69, 69, 0x20, 0x00, 0xF7
        };
        MTS_ParseMIDIDataU(client, single.data(), (int) single.size());
        check(MTS_HasReceivedMTSSysEx(client), "the vendored library parses MTS SysEx");
        if (!MTS_HasMaster(client))
        {
            const double expected = 440.0 * std::exp2(0.25 / 12.0);
            check(std::abs(MTS_NoteToFrequency(client, 69, -1) - expected) < 0.001,
                  "without a master the vendored client applies its MTS fallback");
        }
        MTS_DeregisterClient(client);
    }

    std::printf("MTS-ESP vendored-client checks: %d, failures: %d\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
