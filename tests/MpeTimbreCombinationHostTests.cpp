// Native host regression for the product CC74 Max combination policy supported
// by MPE section 2.2.8, not a claim of full MPE conformance. Manager and note-local
// inputs remain independent across onset, release/reuse, and engine activation.
#include "plugin/PluginProcessor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace {
using namespace cs80;
int checks = 0, failures = 0;
std::string context;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << " [" << context << "]\n"; }
}
bool near(float a, float b) { return std::abs(a - b) < 1.0e-6f; }
void cc(juce::MidiBuffer& midi, int channel, int number, int value)
{ midi.addEvent(juce::MidiMessage::controllerEvent(channel, number, value), 0); }
void mcm(juce::MidiBuffer& midi, int manager, int members)
{ cc(midi, manager, 101, 0); cc(midi, manager, 100, 6); cc(midi, manager, 6, members); }
void on(juce::MidiBuffer& midi, int channel, int note)
{ midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 80), 0); }
void timbre(juce::MidiBuffer& midi, int channel, int value)
{ cc(midi, channel, 74, value); }
struct FoundVoice {
    const Engine* engine = nullptr;
    int index = -1;
    Engine::VoiceInfo info;
};
FoundVoice find(Cs80AudioProcessor& host, int note, int channel)
{
    for (const auto* engine : std::array<const Engine*, 2>{&host.getEngine(), &host.getExtensionEngine()})
        for (int i = 0; i < 8; ++i) {
            const auto info = engine->getVoiceInfo(i);
            if (info.active && info.note == note && info.channel == channel) return {engine, i, info};
        }
    return {};
}
struct Host {
    std::unique_ptr<Cs80AudioProcessor> processor;
    juce::AudioBuffer<float> audio {2, 0};
    juce::MidiBuffer midi;
    Host(const juce::File& fixture, bool circuit, bool sixteen)
    {
        processor = std::make_unique<Cs80AudioProcessor>(fixture);
        processor->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit
                                         : Cs80AudioProcessor::EngineMode::legacy);
        for (int p : std::array<int, 6>{P_parityMode, P_kasScanTiming, P_arpOn, P_voiceDispersion, P_drift,
                      P_analogNoise})
            processor->setParamValue(p, 0);
        processor->setParamValue(P_mpeOn, 1);
        processor->setParamValue(P_voiceMode, sixteen ? 1 : 0);
        processor->setParamValue(P_keyRelease, 5);
        processor->prepareToPlay(48000, 256);
        tick();
        mcm(midi, 1, 0); mcm(midi, 16, 0);
        mcm(midi, 1, 2); mcm(midi, 16, 2); tick();
    }
    void tick(int frames = 0)
    {
        audio.setSize(2, frames); processor->processBlock(audio, midi); midi.clear();
        bool finite = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < frames; ++i)
            finite &= std::isfinite(audio.getSample(c, i));
        check(finite, "host output is finite");
    }
    void fillPrimary(bool sixteen)
    { if (sixteen) { for (int n = 40; n < 48; ++n) on(midi, 8, n); tick(); } }
    void expect(int note, int channel, float value, const char* label)
    {
        const auto v = find(*processor, note, channel);
        check(v.info.active && near(v.info.timbre, value / 127.0f), label);
    }
};
void testCombination(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    for (int frames : {0, 32}) for (bool upper : {false, true})
    for (bool managerFirst : {false, true}) {
        context = std::string("combination/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + (managerFirst ? "/manager-first" : "/member-first")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.fillPrimary(sixteen);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        const int other = upper ? 2 : 15;
        // A manager CC74 sent without a note is retained for a later onset.
        timbre(host.midi, manager, 70); host.tick(frames);
        on(host.midi, member, 60); on(host.midi, other, 65); host.tick(frames);
        const auto witness = find(*host.processor, 60, member);
        check(witness.info.held, "combination witness really held");
        if (sixteen)
            check(witness.engine == &host.processor->getExtensionEngine(), "witness allocated in extension");
        host.expect(60, member, 70, "manager-before-onset establishes timbre floor");
        host.expect(65, other, 0, "onset floor does not leak into opposite zone");
        timbre(host.midi, managerFirst ? manager : member, managerFirst ? 100 : 40);
        timbre(host.midi, managerFirst ? member : manager, managerFirst ? 40 : 100);
        host.tick(frames);
        host.expect(60, member, 100, "maximum is independent of event order");
        host.expect(65, other, 0, "manager update does not leak into opposite zone");
        // CC121 is deliberately not a timbre reset in this product.
        cc(host.midi, manager, 121, 0); host.tick(frames);
        host.expect(60, member, 100, "manager CC121 preserves manager timbre");
        cc(host.midi, member, 121, 0); host.tick(frames);
        host.expect(60, member, 100, "member CC121 preserves combined timbre");
        timbre(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, 40, "manager zero reveals local input retained through CC121");
        timbre(host.midi, member, 100); timbre(host.midi, manager, 40); host.tick(frames);
        host.expect(60, member, 100, "lower manager input cannot reduce member timbre");
        timbre(host.midi, member, 20); host.tick(frames);
        host.expect(60, member, 40, "member decrease reveals retained manager input");
        timbre(host.midi, member, 100); host.tick(frames);
        host.midi.addEvent(juce::MidiMessage::noteOff(member, 60), 0); host.tick();
        const auto released = find(*host.processor, 60, member);
        check(released.info.active && !released.info.held, "accepted member release creates active tail");
        timbre(host.midi, member, 20); on(host.midi, member, 62); host.tick(frames);
        check(find(*host.processor, 62, member).info.held, "reused member channel owns a new held voice");
        host.expect(60, member, 100, "channel reuse preserves released voice local input");
        host.expect(62, member, 40, "reused channel onset combines new local input and manager");
        timbre(host.midi, manager, 110); host.tick(frames);
        host.expect(60, member, 110, "manager reaches released voice");
        host.expect(62, member, 110, "manager reaches reused held voice");
        timbre(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, 100, "manager zero reveals frozen released local input");
        host.expect(62, member, 20, "manager zero reveals new held local input");
        host.expect(65, other, 0, "opposite zone remains isolated after reuse");
    }
}
void testDormantActivation(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (int frames : {0, 32})
    for (bool upper : {false, true}) for (bool managerFirst : {false, true})
    for (bool managerLarger : {false, true}) {
        context = std::string("activation/") + (circuit ? "circuit" : "legacy")
            + (upper ? "/upper" : "/lower") + (managerFirst ? "/manager-first" : "/member-first")
            + (managerLarger ? "/manager-larger" : "/member-larger")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, false);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        const int managerValue = managerLarger ? 100 : 40;
        const int memberValue = managerLarger ? 40 : 100;
        timbre(host.midi, managerFirst ? manager : member, managerFirst ? managerValue : memberValue);
        timbre(host.midi, managerFirst ? member : manager, managerFirst ? memberValue : managerValue);
        host.tick(frames);
        host.processor->setParamValue(P_voiceMode, 1); host.tick(); host.fillPrimary(true);
        on(host.midi, member, 60); host.tick(frames);
        const auto witness = find(*host.processor, 60, member);
        check(witness.info.held && witness.engine == &host.processor->getExtensionEngine(),
              "new extension really owns timbre witness");
        host.expect(60, member, 100, "activation inherits independent raw CC74 inputs");
        timbre(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, memberValue, "activation does not bake manager floor into member cache");
        timbre(host.midi, manager, managerValue); timbre(host.midi, member, 0); host.tick(frames);
        host.expect(60, member, managerValue, "activation preserves separate manager cache");
    }
}
void testLearnedManagerBoundary(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    for (bool upper : {false, true}) for (int frames : {0, 32}) {
        context = std::string("learned-manager/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.fillPrimary(sixteen);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        check(host.processor->getMidiLearn().assign(74, host.processor->hostParameterIndex(P_volume)),
              "CC74 learn assignment succeeds");
        timbre(host.midi, manager, 100); host.tick(frames);
        check(near(host.processor->getParamValue(P_volume), 100.0f / 127),
              "learned manager CC74 really updates volume");
        host.processor->getMidiLearn().clearController(74);
        timbre(host.midi, member, 20); on(host.midi, member, 60); host.tick(frames);
        const auto witness = find(*host.processor, 60, member);
        check(witness.info.held, "learned-manager witness is held");
        if (sixteen)
            check(witness.engine == &host.processor->getExtensionEngine(), "learned-manager witness uses extension");
        host.expect(60, member, 20, "consumed manager CC74 does not seed retained manager timbre");
        timbre(host.midi, manager, 80); host.tick(frames);
        host.expect(60, member, 80, "unlearned manager CC74 resumes ordinary timbre routing");
        check(host.processor->getMidiLearn().assign(74, host.processor->hostParameterIndex(P_volume)),
              "CC74 learn reassignment succeeds");
        timbre(host.midi, manager, 120); host.tick(frames);
        check(near(host.processor->getParamValue(P_volume), 120.0f / 127),
              "learned manager updates volume with a held witness");
        host.expect(60, member, 80, "consumed manager CC74 does not change held timbre");
        host.processor->getMidiLearn().clearController(74);
        timbre(host.midi, member, 30); on(host.midi, member, 62); host.tick(frames);
        host.expect(62, member, 80, "consumed manager CC74 does not replace existing retained floor");
    }
}
void testRibbonBoundary(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    for (bool upper : {false, true}) for (int frames : {0, 32}) {
        context = std::string("CC74-ribbon/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.fillPrimary(sixteen);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        host.processor->setParamValue(P_ribbon, 0);
        host.processor->setParamValue(P_ribbonCc, 74);
        host.processor->setParamValue(P_ribbonCcEnable, 1); host.tick();
        timbre(host.midi, manager, 100); timbre(host.midi, member, 32);
        on(host.midi, member, 60); host.tick(frames);
        const auto witness = find(*host.processor, 60, member);
        check(witness.info.held, "ribbon witness is held");
        if (sixteen)
            check(witness.engine == &host.processor->getExtensionEngine(), "ribbon witness uses extension");
        host.expect(60, member, 100, "low member CC74 retains manager timbre floor with ribbon enabled");
        check(near(witness.info.ribbonRatio, ribbonFrequencyRatio(-.5f)),
              "mapped ribbon onset uses raw member CC74 below manager floor");
        timbre(host.midi, member, 64); host.tick(frames);
        // Circuit ribbon has a 5 ms one-pole dezipper. At 48 kHz, 24
        // blocks of 256 frames exceed 25 time constants: a .5 displacement
        // residual is below the smoother's 1e-9 snap threshold. This also
        // refreshes held-voice telemetry after zero-frame event delivery.
        for (int block = 0; block < 24; ++block) host.tick(256);
        host.expect(60, member, 100, "second low CC74 still retains manager timbre floor");
        check(find(*host.processor, 60, member).info.ribbonRatio == 1.0f,
              "mapped ribbon settles exactly at centre independently of retained timbre maximum");
    }
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-timbre-combination");
    testCombination(fixture.getFile()); testDormantActivation(fixture.getFile());
    testLearnedManagerBoundary(fixture.getFile()); testRibbonBoundary(fixture.getFile());
    std::cout << "MPE timbre combination: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
