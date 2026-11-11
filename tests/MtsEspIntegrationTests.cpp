// The actual adapter and upstream declarations, with only the external C API
// supplied by a deterministic fixture. No test backend ships in the plugin.
#include "core/Engine.h"
#include "plugin/MtsEspClient.h"
#include "vendor/mts-esp/Client/libMTSClient.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <vector>

struct MTSClient {};
namespace {
bool master = false, registrationFails = false;
int registered = 0, deregistered = 0, frequencyQueries = 0, mappingQueries = 0;
int invalidNote = -1, disconnectAtNote = -1;
double invalidHz = 0.0, transpose = 0.25;
bool channelsCorrect = true;
int checks = 0, failures = 0;
void check(bool pass, const char* what)
{
    ++checks;
    if (!pass) { ++failures; std::cerr << "FAIL: " << what << '\n'; }
}
bool unchanged(const cs80::tuning::TuningTable& a, const cs80::tuning::TuningTable& b)
{
    return std::memcmp(a.pitch.data(), b.pitch.data(), sizeof(float) * 128) == 0
        && std::memcmp(a.mapped.data(), b.mapped.data(), sizeof(bool) * 128) == 0;
}
cs80::tuning::TuningTable sentinel()
{
    cs80::tuning::TuningTable table;
    for (std::size_t n = 0; n < 128; ++n)
    {
        table.pitch[n] += 3.5f;
        table.mapped[n] = n % 3 != 0;
    }
    return table;
}
std::vector<float> render(const cs80::tuning::TuningTable& table, bool supplied,
                          bool pure, int tuningMode, bool circuit)
{
    auto values = cs80::defaultValues();
    values[cs80::P_sourcePerformance] = circuit ? 1.0f : 0.0f;
    values[cs80::P_parityMode] = pure ? 1.0f : 0.0f;
    values[cs80::P_tuningMode] = static_cast<float>(tuningMode);
    values[cs80::P_drift] = values[cs80::P_voiceDispersion] = values[cs80::P_analogNoise] = 0.0f;
    cs80::Engine engine;
    engine.setParams(values);
    engine.prepare(48000.0);
    if (supplied) engine.setMtsTuning(table);
    engine.noteOn(64, 0.8f);
    std::vector<float> audio(4096 * 2);
    engine.render(audio.data(), 4096);
    return audio;
}
}
extern "C" {
MTSClient* MTS_RegisterClient()
{
    if (registrationFails) return nullptr;
    ++registered;
    return new MTSClient;
}
void MTS_DeregisterClient(MTSClient* client) { ++deregistered; delete client; }
const char* MTS_GetScaleName(MTSClient*) { return ""; }
bool MTS_HasMaster(MTSClient* client) { return client != nullptr && master; }
double MTS_NoteToFrequency(MTSClient*, char note, signed char channel)
{
    ++frequencyQueries;
    channelsCorrect = channelsCorrect && channel == -1;
    const int n = static_cast<unsigned char>(note);
    if (n == disconnectAtNote) master = false;
    if (n == invalidNote) return invalidHz;
    return 440.0 * std::exp2((n + transpose - 69.0) / 12.0);
}
bool MTS_ShouldFilterNote(MTSClient*, char note, signed char channel)
{
    ++mappingQueries;
    channelsCorrect = channelsCorrect && channel == -1;
    return note == 60;
}
// Present so the unchanged baseline adapter can also be linked for RED checks.
void MTS_ParseMIDIDataU(MTSClient*, const unsigned char*, int) {}
}

int main()
{
    auto table = sentinel();
    {
        cs80::MtsEspClient client;
        check(client.available(), "shipping client is compiled in");
        check(registered == 1 && deregistered == 0, "register exactly once");
        const auto before = table;
        check(!client.hasMaster() && !client.fillTable(table), "no master produces no update");
        check(unchanged(table, before), "no master preserves non-default table exactly");
        master = true;
        check(client.hasMaster() && client.fillTable(table), "connected master supplies tuning");
        bool pitches = true, mapping = true;
        for (std::size_t n = 0; n < 128; ++n)
        {
            pitches = pitches && std::abs(table.pitch[n] - (static_cast<float>(n) + 0.25f)) < 0.0001f;
            mapping = mapping && table.mapped[n] == (n != 60);
        }
        check(pitches, "convert all 128 master frequencies to fractional MIDI pitch");
        check(mapping, "retain all 128 note-filter flags");
        check(frequencyQueries == 128 && mappingQueries == 128 && channelsCorrect,
              "query every note once using channel-independent API");

        const double badHz[] { 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
            std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
            1e-300, 1e300, 1e36 };
        for (int n : { 0, 64, 127 })
            for (double hz : badHz)
            {
                invalidNote = n;
                invalidHz = hz;
                auto candidate = sentinel();
                const auto previous = candidate;
                check(!client.fillTable(candidate), "invalid mapped frequency rejects entire update");
                check(unchanged(candidate, previous), "rejected update preserves both arrays byte-for-byte");
            }
        invalidNote = 60;
        for (double hz : badHz)
        {
            invalidHz = hz;
            auto candidate = sentinel();
            check(client.fillTable(candidate), "invalid filtered frequency does not discard healthy notes");
            bool valid = candidate.pitch[60] == 60.0f && !candidate.mapped[60];
            for (std::size_t n = 0; n < 128; ++n)
                if (n != 60)
                    valid = valid && candidate.pitch[n] == table.pitch[n]
                        && candidate.mapped[n] == table.mapped[n];
            check(valid, "filtered fallback is finite and healthy notes stay exact");
        }
        invalidNote = -1;
        for (int n : { 0, 64, 127 })
        {
            master = true;
            disconnectAtNote = n;
            auto candidate = sentinel();
            const auto previous = candidate;
            check(!client.fillTable(candidate), "disconnect still present at final check rejects update");
            check(unchanged(candidate, previous), "disconnect does not partly replace previous tuning");
        }
        disconnectAtNote = -1;
        master = true;
        transpose = -0.375;
        check(client.fillTable(table), "reconnected master can update tuning");
        check(std::abs(table.pitch[69] - 68.625f) < 0.0001f, "reconnected tuning is fresh");
        // Positive controls at the accepted float-domain boundaries. Rejecting
        // every extreme would conceal a broken guard; both renderers must also
        // remain finite when a successfully accepted table reaches a voice.
        invalidNote = 64;
        for (double hz : { static_cast<double>(std::numeric_limits<float>::denorm_min()),
                           static_cast<double>((std::numeric_limits<float>::min)()),
                           static_cast<double>((std::numeric_limits<float>::max)()) / 65536.0 })
        {
            invalidHz = hz;
            cs80::tuning::TuningTable boundary;
            check(client.fillTable(boundary), "accepted frequency boundary supplies a complete table");
            for (bool circuit : { false, true })
            {
                const auto audio = render(boundary, true, false, 2, circuit);
                bool finite = true;
                for (float sample : audio) finite = finite && std::isfinite(sample);
                check(finite, "accepted frequency boundary renders finite audio");
            }
        }
        invalidNote = -1;
    }
    check(registered == deregistered, "release registered client exactly once");
    registrationFails = true;
    {
        cs80::MtsEspClient client;
        const auto previous = table;
        check(!client.hasMaster() && !client.fillTable(table), "failed registration is safe");
        check(unchanged(table, previous), "failed registration preserves table");
    }
    check(registered == deregistered, "do not deregister a null client");

    for (bool circuit : { false, true })
    {
        check(render(table, true, false, 0, circuit) == render(table, false, false, 0, circuit),
              "12-TET ignores master table byte-for-byte");
        check(render(table, true, false, 2, circuit) != render(table, false, false, 2, circuit),
              "MTS mode actually consumes master table");
    }
    check(render(table, true, true, 2, true) == render(table, false, true, 2, true),
          "PURE CS-80 ignores master table byte-for-byte");
    std::cout << "MTS-ESP integration: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
