// Source-informed PRA carrier continuity. TR retriggers the separate frequency
// envelope, not the IG00150 timing state. This is not measured phase parity.
#define main cs80_ring_continuity_audit_main
#include "../src/tools/engine_audit.cpp"
#undef main

#include <bit>
#include <cstdio>

namespace {
unsigned continuityChecks = 0;
unsigned continuityFailures = 0;

void continuityCheck(bool ok, const std::string& name)
{
    ++continuityChecks;
    if (!ok && ++continuityFailures <= 30)
        std::fprintf(stderr, "FAIL: %s\n", name.c_str());
}

bool samePhase(float a, float b)
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

void advanceCarrier(Engine& engine, int frames)
{
    std::vector<float> audio(2 * std::size_t(frames));
    engine.render(audio.data(), frames);
}

void phaseRules()
{
    for (double rate : {44100., 48000., 96000.})
    for (int policy : {0, 1, 2})
    for (float amount : {0.f, .8f})
    {
        ParamValues params;
        params[P_parityMode] = policy == 1 ? 1.f : 0.f;
        params[P_sourcePerformance] = policy == 2 ? 1.f : 0.f;
        params[P_ringSpeed] = 87.3f;
        params[P_ringDepth] = .6f;
        params[P_ringAttack] = .1f;
        params[P_ringDecay] = .2f;
        params[P_ringAmount] = amount;
        Engine engine;
        engine.prepare(rate);
        engine.setParams(params);
        engine.reset();
        const std::string label = std::to_string(int(rate)) + "/policy="
            + std::to_string(policy) + "/wet=" + std::to_string(amount);

        continuityCheck(samePhase(engine.getRingCarrierPhase(), 0.f), label + " cold reset phase");
        advanceCarrier(engine, 157);
        const float before = engine.getRingCarrierPhase();
        continuityCheck(before > 0.f && before < 1.f, label + " bypassed/idle carrier advances");
        engine.noteOn(60, .8f);
        continuityCheck(samePhase(engine.getRingCarrierPhase(), policy ? before : 0.f),
                        label + " first phrase phase rule");
        continuityCheck(samePhase(engine.getRingEnvelope(), 0.f), label + " first phrase AD restart");

        advanceCarrier(engine, 97);
        const float legato = engine.getRingCarrierPhase();
        const float envelope = engine.getRingEnvelope();
        continuityCheck(envelope > 0.f, label + " frequency envelope advances");
        engine.noteOn(64, .8f);
        continuityCheck(samePhase(engine.getRingCarrierPhase(), legato), label + " legato retains carrier");
        continuityCheck(samePhase(engine.getRingEnvelope(), envelope), label + " legato retains frequency envelope");
        engine.noteOff(60);
        engine.noteOff(64);
        advanceCarrier(engine, 91);
        const float release = engine.getRingCarrierPhase();
        engine.noteOn(67, .8f);
        continuityCheck(samePhase(engine.getRingCarrierPhase(), policy ? release : 0.f),
                        label + " retrigger phase rule");
        continuityCheck(samePhase(engine.getRingEnvelope(), 0.f),
                        label + " retrigger still restarts frequency envelope");
        advanceCarrier(engine, 83);
        continuityCheck(!samePhase(engine.getRingCarrierPhase(), policy ? release : 0.f),
                        label + " carrier not frozen by retrigger");
        continuityCheck(engine.getRingEnvelope() > 0.f, label + " retrigger frequency envelope continues");

        for (bool releaseNotes : {false, true})
        {
            const float panicPhase = engine.getRingCarrierPhase();
            engine.allNotesOff(releaseNotes);
            continuityCheck(samePhase(engine.getRingCarrierPhase(), panicPhase),
                            label + " panic/release does not reset carrier");
            engine.noteOn(72, .8f);
            continuityCheck(samePhase(engine.getRingCarrierPhase(), policy ? panicPhase : 0.f),
                            label + " post-panic phrase phase rule");
            continuityCheck(samePhase(engine.getRingEnvelope(), 0.f),
                            label + " post-panic frequency envelope restart");
            advanceCarrier(engine, 89);
        }
        engine.prepare(rate);
        continuityCheck(samePhase(engine.getRingCarrierPhase(), 0.f), label + " reprepare resets carrier");
        advanceCarrier(engine, 31);
        auto other = params;
        other[P_parityMode] = 0;
        other[P_sourcePerformance] = policy == 0 ? 1.f : 0.f;
        engine.setParams(other);
        continuityCheck(samePhase(engine.getRingCarrierPhase(), 0.f), label + " policy transition resets carrier");
        engine.reset();
        continuityCheck(samePhase(engine.getRingCarrierPhase(), 0.f), label + " explicit reset remains deterministic");
    }
}

void gongRegression()
{
    const auto& programs = programLibrary();
    const auto found = std::find_if(programs.begin(), programs.end(), [](const auto& program) {
        return program.name == "Gong" && program.library == ProgramLibrary::curated;
    });
    continuityCheck(found != programs.end(), "curated Gong exists");
    if (found == programs.end()) return;
    Setup setup;
    setup.name = found->name;
    setup.group = "curated";
    setup.policy = "source";
    setup.values = found->values;
    setPolicy(setup.values, true);
    const auto audio = render(setup, 44100, phrase(), T::length);
    const auto row = analyse(setup, audio);
    continuityCheck(row.nonfinite == 0, "Gong phrase remains finite");
    // Original 8 ms window, complete chord reference, and existing <=1 threshold.
    // No denominator, preset, phrase event or frame-coordinate adjustment.
    continuityCheck(row.clickRetrig <= 1.0, "Gong original retrigger metric <=1");
    std::printf("Gong unchanged click metric: %.17g\n", row.clickRetrig);
}
}

int main()
{
    phaseRules();
    gongRegression();
    std::printf("Ring carrier continuity: %u checks, %u failures\n", continuityChecks, continuityFailures);
    return continuityFailures ? 1 : 0;
}
