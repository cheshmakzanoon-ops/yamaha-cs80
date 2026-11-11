// Native MIDI bank/program admission follows the effective MPE channel role.
// The selected bank and recalled instrument are intentionally global, not per-zone.
#include "plugin/PluginProcessor.h"
#include "core/FactoryPresets.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{
using namespace cs80;
int checks = 0, failures = 0;
std::string context;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << " [" << context << "]\n"; }
}
void mcm(juce::MidiBuffer& midi, int manager, int members, int offset = 0)
{
    for (const auto pair : std::array<std::array<int, 2>, 3>{{{101, 0}, {100, 6}, {6, members}}})
        midi.addEvent(juce::MidiMessage::controllerEvent(manager, pair[0], pair[1]), offset);
}
struct Host
{
    std::unique_ptr<Cs80AudioProcessor> processor;
    juce::AudioBuffer<float> audio {2, 0};
    juce::MidiBuffer midi;
    Host(const juce::File& fixture, bool circuit, bool sixteen)
    {
        processor = std::make_unique<Cs80AudioProcessor>(fixture);
        processor->setCurrentProgram(circuit ? legacyProgramCount : 0);
        processor->setParamValue(P_parityMode, 0);
        processor->setParamValue(P_mpeOn, 1);
        processor->setParamValue(P_voiceMode, sixteen ? 1 : 0);
        processor->setParamValue(P_kasScanTiming, 0);
        processor->prepareToPlay(48000, 256);
        tick();
        check(processor->getEngine().isMpeEnabled(), "fixture has effective MPE before recall");
        check(processor->getParamValue(P_voiceMode) == (sixteen ? 1 : 0),
              "fixture starts with the requested voice count");
        check(processor->getEngine().getRenderPolicy()
                  == (circuit ? RenderPolicy::sourcePerformance : RenderPolicy::legacy),
              "fixture starts with the requested render policy");
    }
    void tick(int frames = 0)
    {
        audio.setSize(2, frames); audio.clear();
        processor->processBlock(audio, midi); midi.clear();
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < frames; ++i)
            finite &= std::isfinite(audio.getSample(c, i));
        check(finite, "host output is finite");
    }
    void layout(int lower, int upper)
    {
        mcm(midi, 1, 0); mcm(midi, 16, 0);
        if (lower) mcm(midi, 1, lower);
        if (upper) mcm(midi, 16, upper);
        tick();
    }
    void bank(int channel, int msb, int lsb, int offset = 0)
    {
        midi.addEvent(juce::MidiMessage::controllerEvent(channel, 0, msb), offset);
        midi.addEvent(juce::MidiMessage::controllerEvent(channel, 32, lsb), offset);
    }
    void program(int channel, int number, int offset = 0)
    {
        midi.addEvent(juce::MidiMessage::programChange(channel, number), offset);
    }
    void expect(int index)
    {
        check(processor->getCurrentProgram() == index, "recall resolves the expected global program index");
        const auto& values = index < legacyProgramCount ? legacyLibrary()[(size_t) index].values
            : curatedLibrary()[(size_t) (index - legacyProgramCount)].values;
        // Check actual sound controls as well as the displayed index. Recall can
        // change engine, MPE enable and voice count; subsequent tests use new hosts.
        for (int p : std::array<int, 4>{P_detune, P_sourcePerformance, chParam(0, CP_lpfCutoff), chParam(1, CP_fA)})
            check(std::abs(processor->getParamValue(p) - values[p])
                      <= std::max(1.0e-4f, (paramDef(p).max - paramDef(p).min) * 1.0e-5f),
                  "recalled program publishes its sound parameters");
    }
};
struct Scenario { const char* name; int channel, lower, upper; bool enabled, allowed; };
constexpr std::array<Scenario, 12> scenarios {{
    {"lower-manager", 1, 2, 2, true, true},
    {"upper-manager", 16, 2, 2, true, true},
    {"lower-member", 2, 2, 2, true, false},
    {"upper-member", 15, 2, 2, true, false},
    {"channel1-upper-member", 1, 0, 15, true, false},
    {"channel16-lower-member", 16, 15, 0, true, false},
    {"conventional-gap", 8, 2, 2, true, false},
    {"channel1-fallback", 1, 0, 2, true, true},
    {"inactive-channel16", 16, 2, 0, true, false},
    {"zero-zone-channel1", 1, 0, 0, true, true},
    {"zero-zone-other", 8, 0, 0, true, false},
    {"MPE-off-member", 15, 2, 2, false, true}
}};
void testRouting(const juce::File& fixture, bool circuit, bool sixteen, int frames)
{
    const auto suffix = context;
    for (const auto& s : scenarios)
    {
        context = s.name + suffix;
        // Isolate Program Change admission from bank admission.
        Host host(fixture, circuit, sixteen); host.layout(s.lower, s.upper);
        host.processor->setParamValue(P_mpeOn, s.enabled ? 1 : 0); host.tick();
        const int before = host.processor->getCurrentProgram();
        host.processor->setParamValue(P_detune, 17); host.tick();
        host.program(s.channel, 7, 11); host.tick(frames);
        if (s.allowed) host.expect(legacyProgramCount + 7);
        else
        {
            check(host.processor->getCurrentProgram() == before, "rejected Program Change keeps the current index");
            check(std::abs(host.processor->getParamValue(P_detune) - 17) < 1.0e-4f,
                  "rejected Program Change leaves edited sound intact");
        }

        // Each ignored MSB and LSB would select a different valid program if
        // accidentally latched. Recall through the allowed manager afterwards.
        for (int controller : {0, 32})
        {
            Host bankHost(fixture, circuit, sixteen); bankHost.layout(s.lower, s.upper);
            bankHost.processor->setParamValue(P_mpeOn, s.enabled ? 1 : 0); bankHost.tick();
            const int manager = s.lower ? 1 : (s.upper ? 16 : 1);
            bankHost.bank(manager, 0, 0); bankHost.tick();
            bankHost.midi.addEvent(juce::MidiMessage::controllerEvent(s.channel, controller, 1), 11);
            bankHost.tick(frames);
            check(bankHost.processor->getCurrentProgram() == (circuit ? legacyProgramCount : 0),
                  "bank selection alone does not recall a program");
            bankHost.program(manager, 7, 11); bankHost.tick(frames);
            bankHost.expect(!s.allowed ? legacyProgramCount + 7
                : controller == 0 ? 7 : legacyProgramCount + 135);
        }
    }
    context = suffix;
}
void testSharedBankAndOrdering(const juce::File& fixture, bool circuit, bool sixteen, int frames)
{
    const auto suffix = context;
    for (int bankManager : {1, 16})
    {
        context = "shared-bank/manager=" + std::to_string(bankManager) + suffix;
        Host host(fixture, circuit, sixteen); host.layout(2, 2);
        host.bank(bankManager, 1, 1, 11);
        host.program(17 - bankManager, 5, 11); host.tick(frames);
        host.expect(133);
    }
    for (bool upper : {false, true}) for (bool entering : {false, true})
    {
        context = std::string("same-offset/") + (upper ? "upper" : "lower")
            + (entering ? "/enter" : "/leave") + suffix;
        // Channel 16 changes between upper manager and unassigned; channel 1
        // changes between upper member and the conventional channel-1 fallback.
        const int channel = upper ? 16 : 1;
        const int beforeMembers = upper ? (entering ? 0 : 2) : (entering ? 15 : 0);
        const int afterMembers = upper ? (entering ? 2 : 0) : (entering ? 0 : 15);
        Host host(fixture, circuit, sixteen); host.layout(0, beforeMembers);
        const int before = host.processor->getCurrentProgram();
        mcm(host.midi, 16, afterMembers, 11);
        host.program(channel, 7, 11); host.tick(frames);
        if (entering) host.expect(legacyProgramCount + 7);
        else check(host.processor->getCurrentProgram() == before,
                   "same-offset program follows the role established by the preceding MCM");

        // Bank selection also uses event-time roles, and only the manager's
        // later Program Change exposes whether the bank event was accepted.
        Host bankHost(fixture, circuit, sixteen); bankHost.layout(0, beforeMembers);
        mcm(bankHost.midi, 16, afterMembers, 11);
        bankHost.bank(channel, 1, 1, 11);
        bankHost.program(afterMembers ? 16 : 1, 5, 11); bankHost.tick(frames);
        bankHost.expect(entering ? 133 : legacyProgramCount + 5);

        Host reverse(fixture, circuit, sixteen); reverse.layout(0, beforeMembers);
        reverse.bank(channel, 1, 1, 11);
        mcm(reverse.midi, 16, afterMembers, 11);
        reverse.program(afterMembers ? 16 : 1, 5, 11); reverse.tick(frames);
        reverse.expect(entering ? legacyProgramCount + 5 : 133);
    }
    context = "PURE" + suffix;
    Host pure(fixture, circuit, sixteen); pure.layout(2, 2);
    pure.processor->setParamValue(P_parityMode, 1); pure.tick();
    const int before = pure.processor->getCurrentProgram();
    for (int channel : {1, 2, 8, 15, 16})
    {
        pure.bank(channel, 1, 1, 11); pure.program(channel, 7, 11);
    }
    pure.tick(frames);
    check(pure.processor->getCurrentProgram() == before && pure.processor->getParamValue(P_parityMode) == 1,
          "PURE rejects bank and program messages on all channel roles");
    pure.processor->setParamValue(P_parityMode, 0);
    pure.processor->setParamValue(P_mpeOn, 0); pure.tick();
    pure.program(8, 7, 11); pure.tick(frames); pure.expect(legacyProgramCount + 7);
    context = suffix;
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-program-routing");
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) for (int frames : {0, 32})
    {
        context = std::string(circuit ? "/circuit" : "/legacy")
            + (sixteen ? "/16" : "/8") + "/frames=" + std::to_string(frames);
        testRouting(fixture.getFile(), circuit, sixteen, frames);
        testSharedBankAndOrdering(fixture.getFile(), circuit, sixteen, frames);
    }
    std::cout << "MPE program routing: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
