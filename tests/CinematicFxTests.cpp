// Unit tests for the 7 cinematic FX models (src/core/CinematicFxModels.h),
// 4-slot routing, tempo sync, global FX master wet/bypass, and the 500-preset
// Film Scoring library.
#include "core/CinematicFxModels.h"
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/FxSlot.h"

#include <cmath>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

using namespace cs80;
using namespace cs80::fx;

namespace
{
int failures = 0;
void check(bool ok, const std::string& msg)
{
    if (!ok)
    {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", msg.c_str());
    }
}

void testAllCinematicModels()
{
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    for (double sr : rates)
    {
        for (int m = 1; m < kExtModelCount; ++m)
        {
            FxSlot slot;
            slot.prepare((float) sr);

            SlotControls c;
            c.extModel = m;
            c.mix = 0.65f;
            c.a = 0.72f;
            c.b = 0.75f;
            c.c = 0.65f;
            c.d = 0.60f;
            c.sync = false;
            c.bpm = 120.0f;

            bool finite = true;
            double maxAbs = 0.0;
            double diffEnergy = 0.0;
            double outEnergy = 0.0;
            const int n = (int) (sr * 0.45);
            for (int i = 0; i < n; ++i)
            {
                const float dryL = i < (int) (sr * 0.15)
                    ? 0.35f * std::sin(6.2831853f * 330.0f * (float) i / (float) sr)
                    : 0.0f;
                const float dryR = i < (int) (sr * 0.15)
                    ? 0.35f * std::sin(6.2831853f * 331.5f * (float) i / (float) sr)
                    : 0.0f;
                const auto out = slot.process(c, dryL, dryR, 0.0f, (float) sr);
                if (!std::isfinite(out.l) || !std::isfinite(out.r))
                    finite = false;
                maxAbs = std::max(maxAbs, (double) std::max(std::fabs(out.l), std::fabs(out.r)));
                const double dl = (double) out.l - (double) dryL;
                const double dr = (double) out.r - (double) dryR;
                diffEnergy += dl * dl + dr * dr;
                outEnergy += (double) out.l * out.l + (double) out.r * out.r;
            }
            check(finite, "finite output for " + std::string(extModelName(m)) + " @ " + std::to_string((int) sr));
            check(maxAbs < 2.2, "bounded headroom for " + std::string(extModelName(m)) + " (" + std::to_string(maxAbs) + ")");
            check(outEnergy > 1.0, "audible output for " + std::string(extModelName(m)));
            check(diffEnergy > 0.05, "wet effect alters signal for " + std::string(extModelName(m)));

            // Switching off must clear stale tail
            c.mix = 0.0f;
            const auto silent = slot.process(c, 0.0f, 0.0f, 0.0f, (float) sr);
            check(silent.l == 0.0f && silent.r == 0.0f, "bypass clears slot for " + std::string(extModelName(m)));
        }
    }
}

void testTempoSyncPingPong()
{
    auto renderAtBpm = [](float bpm) {
        FxSlot slot;
        slot.prepare(48000.0f);
        SlotControls c;
        c.extModel = (int) ExtModel::pingPong;
        c.mix = 1.0f;
        c.a = 0.56f; // 1/4 note
        c.b = 0.45f;
        c.c = 0.80f;
        c.d = 1.0f;
        c.sync = true;
        c.bpm = bpm;
        std::vector<float> out;
        out.reserve(32000);
        for (int i = 0; i < 32000; ++i)
        {
            const float x = (i < 64) ? 0.5f : 0.0f;
            const auto s = slot.process(c, x, x, 0.0f, 48000.0f);
            out.push_back(std::fabs(s.l) + std::fabs(s.r));
        }
        return out;
    };
    const auto slow = renderAtBpm(100.0f);
    const auto fast = renderAtBpm(180.0f);
    double diff = 0.0;
    for (size_t i = 0; i < slow.size(); ++i)
        diff += std::fabs(slow[i] - fast[i]);
    check(diff > 10.0, "tempo-synced Ping-Pong Delay responds to host BPM");
}

void testFourSlotsAndMasterControls()
{
    auto renderWith = [](const ParamValues& p) {
        auto engine = std::make_unique<Engine>();
        engine->prepare(48000.0);
        engine->setParams(p);
        engine->noteOn(60, 0.85f, 1);
        std::vector<float> buf(4800 * 2);
        engine->render(buf.data(), 4800);
        return buf;
    };

    ParamValues dry;
    dry[P_sourcePerformance] = 1.0f;
    dry[P_fx1Mix] = 0.0f;
    dry[P_fx4Mix] = 0.0f;

    ParamValues wet4 = dry;
    wet4[P_fx4ExtModel] = (float) ExtModel::hall;
    wet4[P_fx4Mix] = 0.55f;
    wet4[P_fx4ParamA] = 0.7f;
    wet4[P_fx4ParamB] = 0.7f;

    ParamValues bypassed = wet4;
    bypassed[P_fxMasterBypass] = 1.0f;

    ParamValues zeroWet = wet4;
    zeroWet[P_fxMasterWet] = 0.0f;

    const auto aDry = renderWith(dry);
    const auto aWet4 = renderWith(wet4);
    const auto aBypass = renderWith(bypassed);
    const auto aZeroWet = renderWith(zeroWet);

    double diffWet = 0.0, diffBypass = 0.0, diffZero = 0.0;
    for (size_t i = 0; i < aDry.size(); ++i)
    {
        diffWet += std::fabs(aDry[i] - aWet4[i]);
        diffBypass += std::fabs(aDry[i] - aBypass[i]);
        diffZero += std::fabs(aDry[i] - aZeroWet[i]);
    }
    check(diffWet > 1.0, "FX Slot 4 processes audio when active");
    check(diffBypass == 0.0, "Master FX Bypass produces bit-exact dry output");
    check(diffZero == 0.0, "Master FX Wet = 0 produces dry output");
}

void testFilmScoringLibrary()
{
    const auto& scoring = filmScoringLibrary();
    check(scoring.size() == 500, "Film Scoring library has exactly 500 presets");
    check(programLibrary().size() == (size_t) legacyProgramCount + (size_t) curatedProgramCount() + 500u,
          "programLibrary() contains legacy + curated + 500 Film Scoring");
    check(programIndexForMidi(2, 0, 0) == legacyProgramCount + curatedProgramCount(),
          "MIDI Bank MSB 2 addresses Film Scoring library");
    check(programIndexForMidi(2, 3, 115) == legacyProgramCount + curatedProgramCount() + 499,
          "MIDI Bank MSB 2 last Film Scoring preset");
    check(programIndexForMidi(2, 3, 116) == -1, "MIDI Bank MSB 2 past end is unassigned");

    std::set<std::string> names;
    std::set<std::string> roles;
    for (const auto& preset : scoring)
    {
        check(preset.category == "Film Scoring", "Film Scoring category: " + preset.name);
        check(names.insert(preset.name).second, "unique Film Scoring name: " + preset.name);
        for (const auto& t : preset.tags)
            roles.insert(t);
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& d = paramDef(i);
            check(std::isfinite(preset.values[i]) && preset.values[i] >= d.min && preset.values[i] <= d.max,
                  "param in bounds: " + preset.name + " " + paramId(i));
        }
    }
    for (const char* role : { "brass-braams", "pads-atmospheres", "strings-choirs",
                              "pulses-ostinatos", "drones-subscapes", "leads-themes",
                              "keys-plucks-bells", "impacts-risers", "scifi-noir" })
        check(roles.count(role) == 1, std::string("Film Scoring role tag populated: ") + role);
}
} // namespace

int main()
{
    testAllCinematicModels();
    testTempoSyncPingPong();
    testFourSlotsAndMasterControls();
    testFilmScoringLibrary();
    if (failures != 0)
    {
        std::fprintf(stderr, "%d cinematic FX test failures\n", failures);
        return 1;
    }
    std::printf("cinematic FX and Film Scoring tests passed.\n");
    return 0;
}
