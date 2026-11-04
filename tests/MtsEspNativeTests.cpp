// Compile and exercise the actual vendored client with the shipping adapter.
// This standalone process creates no master; no installed library is required.
#include "plugin/MtsEspClient.h"
#include "../deps/mts-esp/Client/libMTSClient.h"
#include "Ws1Tolerances.h"

#include <cmath>
#include <iostream>

int main(int argc, char** argv)
{
    int failures = 0;
    const auto check = [&](bool pass, const char* message) {
        if (!pass) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
    };
    try
    {
        const ws1::Tolerances tolerance(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/mts-client.json", "mts_client");
        check(cs80::MtsEspClient::available(), "shipping branch contains the vendored client");
        for (int instance = 0; instance < 32; ++instance)
        {
            cs80::MtsEspClient shim;
            check(!shim.hasMaster(), "native standalone process has no master");
            cs80::tuning::TuningTable table;
            table.pitch[69] = 72.0f;
            table.mapped[60] = false;
            const auto before = table;
            check(!shim.fillTable(table), "no master returns no table");
            check(table.pitch == before.pitch && table.mapped == before.mapped,
                  "no master leaves the caller's local tuning intact");
            shim.parseMidi(nullptr, 0);
        }
        MTSClient* client = MTS_RegisterClient();
        check(client != nullptr, "native client registers");
        if (client != nullptr)
        {
            const unsigned char message[] { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0, 1, 69, 70, 0, 0, 0xF7 };
            MTS_ParseMIDIDataU(client, message, static_cast<int>(sizeof(message)));
            check(MTS_HasReceivedMTSSysEx(client), "native client recognizes single-note MTS SysEx");
            const auto pitch = cs80::tuning::TuningTable::pitchFromHz(MTS_NoteToFrequency(client, 69, -1));
            check(std::abs(pitch - 70.0f) <= tolerance["pitch_error_semitones"],
                  "native client applies one-semitone retuning without a master");
            MTS_DeregisterClient(client);
        }
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
