// Same-toolchain V1/current circuit-converter regression, NOT hardware evidence.
// Keep the original seven classified columns independent of later intentional
// audio-policy changes (BBD chorus and release-tail continuity). Every value is
// written explicitly: never compare struct padding or model-version metadata.
#include "core/SourceFactoryTones.h"

#include <cmath>
#include <cstdio>

namespace sf = cs80::source_factory;

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    FILE* file = std::fopen(argv[1], "wb");
    if (file == nullptr) return 3;

    bool ok = true;
    std::size_t count = 0;
    const auto value = [&](double x) {
        ++count;
        ok = ok && std::isfinite(x) && std::fwrite(&x, sizeof(x), 1, file) == 1;
    };
    const auto source = [&](sf::SourceEquivalent x) { value(x.volts); value(x.ohms); };
    constexpr int selections[7][2] {{0,1},{0,3},{0,9},{1,3},{1,7},{1,9},{1,10}};

    // The same 16,632-value corpus is independently frozen (with a portable
    // 1e-9 quantization) in SourceFactoryToneTests. Here no rounding is allowed:
    // both revisions are compiled on one runner and compared byte for byte.
    for (const auto& selection : selections)
      for (auto hpGrade : {sf::Grade::A, sf::Grade::B, sf::Grade::C})
        for (auto lpGrade : {sf::Grade::A, sf::Grade::B, sf::Grade::C})
        {
            sf::ToneControls tone;
            if (!sf::compileTone(selection[0], selection[1], tone, hpGrade, lpGrade))
            { std::fclose(file); return 4; }

            for (auto x : tone.sources) source(x);
            for (auto x : tone.busVolts) value(x);
            value(tone.wsc.sawEnabled);
            value(tone.wsc.pulseEnabled);
            value(tone.wsc.pulseThresholdVolts);
            for (auto x : {tone.dutyProxy, tone.pwmRateHz, tone.pwmDepth, tone.noiseGain,
                           tone.filterPathGain, tone.sinePathGain}) value(x);
            const auto f = tone.filterEnvelope;
            for (auto x : {f.initialVolts, f.attackVolts, f.sustainVolts,
                           f.attackSeconds, f.decaySeconds, f.releaseSeconds}) value(x);
            const auto a = tone.amplitudeEnvelope;
            for (auto x : {a.initialRatio, a.attackRatio, a.sustainRatio,
                           a.attackSeconds, a.decaySeconds, a.releaseSeconds}) value(x);
            value(tone.initialBrillianceVolts);
            value(tone.afterBrillianceVolts);
            const auto b = tone.boundary;
            source(b.qHp); source(b.qLp); source(b.level);
            value(b.initialLevelVolts); value(b.afterLevelVolts);
            for (auto m : tone.cutoff)
                for (auto x : {m.pinPerLoadedBus, m.pinPerTouch, m.pinPerEnvelopeVolt,
                               m.busBaseVolts, m.busPerTouchSum, m.busPerEnvelopeVoltSum,
                               m.effectiveCvPerPinVolt, m.referencePinVolts,
                               m.effectiveCvOffset}) value(x);

            sf::BothRankDrives drives;
            for (int rank = 0; rank < 2; ++rank)
                for (int voice = 0; voice < 8; ++voice)
                    drives[rank][voice] = {-2.7 + .7 * voice, .1 * (voice + 1), .11 * voice};
            sf::CutoffReadout cutoff;
            if (!sf::evaluateCutoff(tone, drives[0], cutoff))
            { std::fclose(file); return 5; }
            for (int voice = 0; voice < 8; ++voice)
                for (int side = 0; side < 2; ++side)
                { value(cutoff.pinVolts[voice][side]); value(cutoff.effectiveCv[voice][side]); }
            for (auto x : cutoff.loadedBusVolts) value(x);

            sf::SharedReadout shared;
            if (!sf::evaluateShared({tone.boundary, tone.boundary}, .37, drives, shared))
            { std::fclose(file); return 6; }
            for (int rank = 0; rank < 2; ++rank)
            {
                for (int voice = 0; voice < 8; ++voice)
                {
                    for (int side = 0; side < 2; ++side)
                    { value(shared.qVolts[rank][voice][side]); value(shared.q[rank][voice][side]); }
                    value(shared.levelPinVolts[rank][voice]);
                    value(shared.levelGain[rank][voice]);
                }
                for (auto x : shared.qBusVolts[rank]) value(x);
                value(shared.levelBusVolts[rank]);
            }
            value(shared.masterResVolts);
        }

    const bool closed = std::fclose(file) == 0;
    return ok && closed && count == 16632 ? 0 : 7;
}
