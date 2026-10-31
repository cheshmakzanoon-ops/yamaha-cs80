// Exact event-boundary continuity, registered in completion-dsp.json before
// evaluation. This is a capacitor-state invariant, not an audible-click metric.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "Ws1Tolerances.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <vector>
using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
double difference(Ig00156Stage::State a, Ig00156Stage::State b)
{
    return std::max({ std::abs(a.input - b.input), std::abs(a.feedback - b.feedback),
                      std::abs(a.bp - b.bp), std::abs(a.lp - b.lp) });
}
std::vector<float> render(Engine& engine, int frames)
{
    std::vector<float> out(static_cast<size_t>(frames) * 2);
    engine.render(out.data(), frames);
    return out;
}
ControlFrame setup(bool funky)
{
    ControlFrame frame;
    if (funky)
    {
        int selector = -1;
        for (int i = 0; i < 11; ++i)
            if (std::string(hardwareToneName(i, 0)) == "Funky 1") selector = i;
        if (selector < 0) throw std::runtime_error("Funky 1 tab missing");
        frame.hardwareSources = { selector, selector };
        frame.effectiveValues[P_subOriginalMode] = 1.0f;
        for (int r = 0; r < 2; ++r)
        {
            ParamValues legacy;
            applyHardwareTone(legacy, r, selector);
            for (int local = 0; local < CP_count; ++local)
                if (local != CP_feet && local != CP_pan && local != CP_hpfOn && local != CP_lpfOn)
                    frame.effectiveValues[chParam(r, local)] = legacy[chParam(r, local)];
            frame.effectiveValues[chParam(r, CP_feet)] = 1.0f;
            frame.effectiveValues[chParam(r, CP_pan)] = 0.0f;
        }
    }
    else
    {
        bool found = false;
        for (const auto& p : factoryPresets()) if (p.name == "Bass Stack")
        { frame.effectiveValues = p.values; found = true; }
        if (!found) throw std::runtime_error("Bass Stack program missing");
    }
    frame.effectiveValues[P_parityMode] = 1.0f;
    return frame;
}
}
int main(int argc, char** argv)
{
    try
    {
        const ws1::Tolerances tol(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/completion-dsp.json", "click_continuity");
        const double limit = tol["note_off_capacitor_jump_max_abs"];
        double worst = 0.0;
        for (double rate : {44100.0, 48000.0, 96000.0})
            for (bool funky : {false, true})
                for (float oversampling : {0.0f, 1.0f, 2.0f})
                    for (double heldSeconds : {0.02, 0.2, 1.2})
                    {
                        Engine event, control;
                        auto frame = setup(funky);
                        frame.effectiveValues[P_oversampling] = oversampling;
                        for (auto* e : { &event, &control })
                        {
                            e->prepare(rate);
                            check(e->setControlFrame(frame), "accepted exact audit setup");
                            e->reset();
                            render(*e, static_cast<int>(0.2 * rate));
                            e->noteOn(60, 0.8f, 1);
                        }
                        check(render(event, static_cast<int>(heldSeconds * rate)) == render(control, static_cast<int>(heldSeconds * rate)),
                              "omitted-noteOff control has byte-identical audio before event");
                        std::array<Engine::RankAudioState, 2> audio;
                        std::array<Engine::RankEnvelopeState, 2> env;
                        std::array<Ig00151Vca::State, 2> vca;
                        for (int rank = 0; rank < 2; ++rank)
                        {
                            audio[rank] = event.getRankAudioState(0, rank);
                            env[rank] = event.getRankEnvelopeState(0, rank);
                            vca[rank] = event.getRankVcaState(0, rank);
                        }
                        check(event.getVoiceInfo(0).held, "fixture owns a held voice before noteOff");
                        event.noteOff(60, 1);
                        check(!event.getVoiceInfo(0).held, "noteOff actually changes gate selection");
                        check(control.getVoiceInfo(0).held, "omitted-event control gate stays held");
                        for (int rank = 0; rank < 2; ++rank)
                        {
                            const auto a = event.getRankAudioState(0, rank);
                            const auto b = event.getRankEnvelopeState(0, rank);
                            const auto c = event.getRankVcaState(0, rank);
                            const double delta = std::max({ difference(a.hpCircuit, audio[rank].hpCircuit),
                                difference(a.lpCircuit, audio[rank].lpCircuit),
                                double(std::abs(b.amplitude.value - env[rank].amplitude.value)),
                                double(std::abs(b.filter.value - env[rank].filter.value)),
                                double(std::abs(c.level - vca[rank].level)), std::abs(c.coupling - vca[rank].coupling) });
                            worst = std::max(worst, delta);
                            check(delta <= limit, "noteOff keeps both EG, VCA and filter capacitor states exactly continuous");
                            check(a.phase == audio[rank].phase, "noteOff keeps free-running oscillator phase");
                        }
                    }
        std::printf("note-off maximum instantaneous capacitor delta %.17g (limit %.17g)\n", worst, limit);
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    std::printf("Note-off continuity: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
