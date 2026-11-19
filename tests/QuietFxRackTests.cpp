// The reference's third (or rotated spare) slot has no classic type or modern
// model, so it is inaudible for any mix. A fixed nonzero base mix keeps the
// ordinary three-slot dispatch active, independently of the code under test.
#include "core/Engine.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {
using namespace cs80;

bool same(float a, float b) { return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b); }
bool same(double a, double b) { return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b); }
constexpr std::array<int,3> type {P_fx1Type, P_fx2Type, P_fx3Type};
constexpr std::array<int,3> mix {P_fx1Mix, P_fx2Mix, P_fx3Mix};
constexpr std::array<int,3> model {P_fx1Model, P_fx2Model, P_fx3Model};

bool run(bool source, int slot)
{
    const int spare = (slot + 1) % 3;
    auto a = std::make_unique<Engine>(), b = std::make_unique<Engine>();
    ParamValues base;
    base[P_parityMode] = 0.0f;
    base[P_sourcePerformance] = source ? 1.0f : 0.0f;
    base[P_analogNoise] = 0.0f;
    base[P_volume] = 0.19f;
    base[P_fxGainCompensation] = 0.0f;
    for (int j = 0; j < 3; ++j)
        base[type[j]] = base[model[j]] = base[mix[j]] = 0.0f;
    // Keep the reverb type selected throughout the first wet/dry/wet cycle.
    // In Legacy, mix changes can take effect at the very next sample.
    base[type[slot]] = 4.0f;
    const auto apply = [&] {
        auto ref = base;
        ref[mix[spare]] = 0.5f;
        a->setParams(base);
        b->setParams(ref);
    };
    constexpr int rate = 24000;
    a->prepare(rate); b->prepare(rate);
    apply(); a->reset(); b->reset();
    for (const int note : {47, 59, 64}) { a->noteOn(note, 0.77f); b->noteOn(note, 0.77f); }
    std::array<float, 2 * 64> x{}, y{};
    bool audible = false;
    for (int block = 0; block < 370; ++block)
    {
        switch (block)
        {
            // Toggle loudness matching while the entire tested bank is dry.
            case 2: case 221: base[P_fxGainCompensation] = 1.0f; apply(); break;
            case 4: case 224: base[P_fxGainCompensation] = 0.0f; apply(); break;
            // Reverb first rings, is bypassed (must clear old tail), then
            // re-enabled. The final cycle also switches the selected model.
            case 8: case 167: case 241: base[mix[slot]] = 0.72f; apply(); break;
            case 145: case 195: case 312: base[mix[slot]] = 0.0f; apply(); break;
            case 230: base[model[slot]] = 4.0f; apply(); break;
            case 274: base[P_fxGainCompensation] = 1.0f; apply(); break;
            default: break;
        }
        a->render(x.data(), 64); b->render(y.data(), 64);
        for (std::size_t j = 0; j < x.size(); ++j)
        {
            if (!std::isfinite(x[j]) || !same(x[j], y[j])) {
                std::fprintf(stderr, "PCM differed: source=%d slot=%d block=%d word=%zu\n", source, slot, block, j);
                return false;
            }
            audible |= std::abs(x[j]) > 0.0001f;
        }
        if (a->getLatencySamples() != b->getLatencySamples()
            || a->activeVoices() != b->activeVoices()) return false;
        const auto al = a->getListeningSample(), bl = b->getListeningSample();
        if (!same(al.left, bl.left) || !same(al.right, bl.right)) return false;
        for (int tap = 0; tap < 3; ++tap)
        {
            const auto as = a->getOutputTapState(tap), bs = b->getOutputTapState(tap);
            if (!same(as.state.capacitorVolts, bs.state.capacitorVolts)
                || !same(as.state.previousSourceVolts, bs.state.previousSourceVolts)
                || !same(as.state.readoutGain, bs.state.readoutGain)
                || !same(as.nodes.audibleJackVolts, bs.nodes.audibleJackVolts)) return false;
        }
    }
    return audible;
}
}

int main()
{
    for (bool source : {false, true})
        for (int slot = 0; slot < 3; ++slot)
            if (!run(source, slot)) return 1;
    std::puts("Quiet FX rack: six complete dry/active/continuation histories matched canonical dispatch.");
    return 0;
}
