// Compile and exercise the actual vendored client with the shipping adapter.
// This standalone process creates no master; no installed library is required.
#include "plugin/MtsEspClient.h"
#include "../deps/mts-esp/Client/libMTSClient.h"
#include "Ws1Tolerances.h"

#include <cmath>
#include <algorithm>
#include <array>
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

            // Extended scale/octave 2-byte tuning is a 24-byte stream of
            // seven-bit pairs. The signed word used by the upstream parser
            // must not carry earlier pairs into the next 14-bit detune's sign.
            std::array<unsigned char, 33> scale {};
            scale[0] = 0xF0; scale[1] = 0x7F; scale[2] = 0x7F;
            scale[3] = 0x08; scale[4] = 0x09; // extended 2-byte scale/octave
            scale[5] = 0x00; scale[6] = 0x7F; scale[7] = 0x7F; // channel bitmap
            // 0x48 has bit 3 set: after five unreset shift/or operations its
            // older bytes make the signed accumulator negative, even though
            // each individual 14-bit detune is positive.
            for (int i = 0; i < 24; ++i) scale[(std::size_t) i + 8] = 0x48;
            scale.back() = 0xF7;
            MTS_ParseMIDIDataU(client, scale.data(), static_cast<int>(scale.size()));
            check(MTS_GetMapSize(client) == 12,
                  "native MTS two-byte scale update preserves the twelve-note keyboard mapping");
            constexpr float expectedDetune = 1096.0f / 8191.0f;
            const auto scalePitch60 = cs80::tuning::TuningTable::pitchFromHz(MTS_NoteToFrequency(client, 60, -1));
            const auto scalePitch69 = cs80::tuning::TuningTable::pitchFromHz(MTS_NoteToFrequency(client, 69, -1));
            check(std::abs(scalePitch60 - (60.0f + expectedDetune)) <= tolerance["pitch_error_semitones"]
                      && std::abs(scalePitch69 - (69.0f + expectedDetune)) <= tolerance["pitch_error_semitones"],
                  "native two-byte scale decodes all successive 14-bit detunes exactly");
            double maxScaleHzError = 0.0;
            for (int note = 60; note < 72; ++note)
            {
                const double expectedHz = 440.0 * std::exp2((note + 1096.0 / 8191.0 - 69.0) / 12.0);
                maxScaleHzError = std::max(maxScaleHzError,
                    std::abs(MTS_NoteToFrequency(client, (char) note, -1) - expectedHz));
            }
            check(maxScaleHzError <= 1e-8,
                  "each native two-byte detune uses its own 14-bit sign, not prior payload bits");

            // An incomplete detune must not corrupt the previously accepted
            // scale. The final data byte lacks its matching 7-bit partner.
            const unsigned char shortScale[] {
                0xF0, 0x7F, 0x7F, 0x08, 0x09, 0x00, 0x7F, 0x7F, 0x7F, 0xF7
            };
            const double beforeShort = MTS_NoteToFrequency(client, 60, -1);
            MTS_ParseMIDIDataU(client, shortScale, static_cast<int>(sizeof(shortScale)));
            check(MTS_NoteToFrequency(client, 60, -1) == beforeShort,
                  "truncated 14-bit MTS detune cannot overwrite the previous tuning");
            MTS_DeregisterClient(client);
        }
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
