// Deterministic protocol fixture. Only this test target replaces the upstream
// C API; it compiles the same adapter and the real upstream declarations.
#include "core/Engine.h"
#include "plugin/MtsEspClient.h"
#include "../deps/mts-esp/Client/libMTSClient.h"
#include "Ws1Tolerances.h"

#include <cmath>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

struct MTSClient {};
namespace {
bool master = false, failRegistration = false;
int invalidMasterNote = -1;
double invalidMasterHz = 0.0;
int registered = 0, deregistered = 0, frequencyQueries = 0, mappingQueries = 0, midiCalls = 0;
bool channelCorrect = true;
const unsigned char* receivedMidi = nullptr;
int receivedSize = 0;
}
extern "C" {
MTSClient* MTS_RegisterClient()
{
    if (failRegistration) return nullptr;
    ++registered;
    return new MTSClient;
}
void MTS_DeregisterClient(MTSClient* client)
{
    ++deregistered;
    delete client;
}
bool MTS_HasMaster(MTSClient* client) { return client != nullptr && master; }
double MTS_NoteToFrequency(MTSClient*, char note, signed char channel)
{
    ++frequencyQueries;
    channelCorrect = channelCorrect && channel == -1;
    // External masters are not constrained to supply safe numerical data.
    if (static_cast<unsigned char>(note) == invalidMasterNote) return invalidMasterHz;
    return 440.0 * std::exp2((static_cast<unsigned char>(note) + 0.25 - 69.0) / 12.0);
}
bool MTS_ShouldFilterNote(MTSClient*, char note, signed char channel)
{
    ++mappingQueries;
    channelCorrect = channelCorrect && channel == -1;
    return note == 60;
}
void MTS_ParseMIDIDataU(MTSClient*, const unsigned char* bytes, int size)
{
    ++midiCalls;
    receivedMidi = bytes;
    receivedSize = size;
}
}
namespace {
int failures = 0;
void check(bool pass, const char* message)
{
    if (!pass) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool tableBytesEqual(const cs80::tuning::TuningTable& a, const cs80::tuning::TuningTable& b)
{
    return std::memcmp(a.pitch.data(), b.pitch.data(), sizeof(float) * 128) == 0
        && std::memcmp(a.mapped.data(), b.mapped.data(), sizeof(bool) * 128) == 0;
}
std::vector<float> render(const cs80::tuning::TuningTable& table, bool supplied, bool pure, int tuningMode)
{
    auto parameters = cs80::defaultValues();
    parameters[cs80::P_sourcePerformance] = 1.0f;
    parameters[cs80::P_parityMode] = pure ? 1.0f : 0.0f;
    parameters[cs80::P_tuningMode] = static_cast<float>(tuningMode);
    parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0.0f;
    cs80::Engine engine;
    engine.setParams(parameters);
    engine.prepare(48000.0);
    if (supplied) engine.setMtsTuning(table);
    engine.noteOn(64, 0.8f);
    std::vector<float> output(4096 * 2);
    engine.render(output.data(), 4096);
    return output;
}
}
int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tolerance(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/mts-client.json", "mts_client");
        cs80::tuning::TuningTable received;
        {
            cs80::MtsEspClient shim;
            check(shim.available(), "adapter compiles the upstream API branch");
            check(registered == 1 && deregistered == 0, "adapter registers exactly once");
            const auto before = received;
            check(!shim.hasMaster() && !shim.fillTable(received), "absent master supplies no table");
            check(received.pitch == before.pitch && received.mapped == before.mapped,
                  "absent master preserves caller table");
            master = true;
            check(shim.hasMaster() && shim.fillTable(received), "connected master supplies its table");
            bool pitches = true, mapping = true;
            for (int note = 0; note < 128; ++note)
            {
                pitches = pitches && std::abs(received.pitch[static_cast<std::size_t>(note)] - (note + 0.25f))
                                      <= tolerance["pitch_error_semitones"];
                mapping = mapping && received.mapped[static_cast<std::size_t>(note)] == (note != 60);
            }
            check(pitches, "all 128 frequencies reach the fractional-MIDI table");
            check(mapping, "unmapped notes reach the table");
            check(frequencyQueries == 128 && mappingQueries == 128 && channelCorrect,
                  "one frequency and mapping query per note with channel -1");
            // First, an invalid Hz on an intentionally filtered note must never
            // enter the pitch array as NaN/-Inf; healthy peers stay exact.
            invalidMasterNote = 60;
            invalidMasterHz = 0.0;
            cs80::tuning::TuningTable filtered;
            check(shim.fillTable(filtered), "invalid Hz on a filtered note is safely ignored");
            bool safeFiltered = !filtered.mapped[60] && filtered.pitch[60] == 60.0f;
            for (int note = 0; note < 128; ++note)
                if (note != 60)
                    safeFiltered = safeFiltered && filtered.pitch[static_cast<std::size_t>(note)]
                            == received.pitch[static_cast<std::size_t>(note)]
                        && filtered.mapped[static_cast<std::size_t>(note)]
                            == received.mapped[static_cast<std::size_t>(note)];
            check(safeFiltered, "filtered bad Hz has a finite fallback and leaves healthy notes exact");

            // A mapped bad frequency fails the *whole* update. The sentinel
            // table differs from the master's healthy pitches so a partial
            // write on notes preceding the fault is observable.
            const std::pair<int, double> badMappedHz[] {
                {61, 0.0}, {62, -1.0},
                {63, std::numeric_limits<double>::quiet_NaN()},
                {64, std::numeric_limits<double>::infinity()},
                {65, 1e300}, // overflows float frequency immediately
                {66, 1e-300}, // underflows float frequency to zero
                {67, 1e36} // representable float Hz; filter CV multiplication can overflow
            };
            for (const auto& [note, hz] : badMappedHz)
            {
                invalidMasterNote = note;
                invalidMasterHz = hz;
                cs80::tuning::TuningTable sentinel;
                const auto before = sentinel;
                check(!shim.fillTable(sentinel), "invalid mapped master Hz rejects update");
                check(tableBytesEqual(sentinel, before),
                      "failed master update leaves caller table byte-unchanged");
            }
            invalidMasterNote = -1;
            const std::uint8_t midi[] { 0xF0, 0x7F, 0x7F, 0x08, 0x02, 0, 0, 0xF7 };
            shim.parseMidi(midi, static_cast<int>(sizeof(midi)));
            check(midiCalls == 1 && receivedMidi == midi && receivedSize == sizeof(midi),
                  "adapter forwards valid MIDI bytes without copying");
            shim.parseMidi(nullptr, 4);
            shim.parseMidi(midi, 0);
            check(midiCalls == 1, "invalid MIDI buffers do not reach the client");
            master = false;
            const auto tuned = received;
            check(!shim.fillTable(received) && received.pitch == tuned.pitch && received.mapped == tuned.mapped,
                  "master disconnect is reported without overwriting caller state");
        }
        check(registered == deregistered, "adapter deregisters exactly once");
        failRegistration = true;
        {
            cs80::MtsEspClient shim;
            check(!shim.hasMaster() && !shim.fillTable(received), "failed registration remains safe");
            shim.parseMidi(reinterpret_cast<const std::uint8_t*>("x"), 1);
            check(midiCalls == 1, "null client cannot parse MIDI");
        }
        check(registered == deregistered, "null registration is not deregistered");
        check(render(received, true, false, 0) == render(received, false, false, 0),
              "default tuning mode ignores the MTS table byte-for-byte");
        check(render(received, true, true, 2) == render(received, false, true, 2),
              "PURE CS-80 ignores the MTS table byte-for-byte");
        check(render(received, true, false, 2) != render(received, false, false, 2),
              "modern tuning mode actually consumes the fixture table");
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
