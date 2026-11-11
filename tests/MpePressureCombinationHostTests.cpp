// Exercise raw member/manager pressure combination through the real processor.
// In particular, activation must transfer independent inputs, not their maximum.
#include "plugin/PluginProcessor.h"
#include "core/ControllerCurves.h"
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
void pressure(juce::MidiBuffer& midi, int channel, int value)
{ midi.addEvent(juce::MidiMessage::channelPressureChange(channel, value), 0); }
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
        for (int p : std::array<int, 9>{P_parityMode, P_kasScanTiming, P_arpOn, P_voiceDispersion, P_drift,
                      P_analogNoise, P_velocityCurve, P_pressureCurve, P_pressureThreshold})
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
        check(v.info.active && near(v.info.pressure, value), label);
    }
};
void testCombination(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    for (int frames : {0, 32}) for (bool upper : {false, true}) for (bool managerFirst : {false, true})
    for (bool curved : {false, true}) {
        context = std::string("combination/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + (managerFirst ? "/manager-first" : "/member-first")
            + (curved ? "/curved" : "/linear") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.fillPrimary(sixteen);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2, other = upper ? 2 : 15;
        const float curve = curved ? .65f : 0, threshold = curved ? .15f : 0;
        host.processor->setParamValue(P_pressureCurve, curve);
        host.processor->setParamValue(P_pressureThreshold, threshold); host.tick();
        auto mapped = [&](int value) { return applyPressureCurve(value / 127.0f, curve, threshold); };
        on(host.midi, member, 60); on(host.midi, other, 65); host.tick(frames);
        const auto witness = find(*host.processor, 60, member);
        check(witness.info.held, "combination witness really held");
        if (sixteen) check(witness.engine == &host.processor->getExtensionEngine(), "witness allocated in extension");
        pressure(host.midi, managerFirst ? manager : member, managerFirst ? 100 : 40);
        pressure(host.midi, managerFirst ? member : manager, managerFirst ? 40 : 100); host.tick(frames);
        host.expect(60, member, mapped(100), "manager floor is independent of event order");
        host.expect(65, other, 0, "manager pressure does not leak into opposite zone");
        cc(host.midi, manager, 121, 0); host.tick(frames);
        host.expect(60, member, mapped(40), "manager CC121 reveals retained member input");
        pressure(host.midi, manager, 70); host.tick(frames);
        cc(host.midi, member, 121, 0); host.tick(frames);
        host.expect(60, member, mapped(70), "member CC121 retains manager floor");
        pressure(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, 0, "manager zero reveals reset local input");
        pressure(host.midi, member, 100); pressure(host.midi, manager, 40); host.tick(frames);
        host.expect(60, member, mapped(100), "lower manager input cannot reduce member pressure");
        host.midi.addEvent(juce::MidiMessage::noteOff(member, 60), 0); host.tick();
        check(!find(*host.processor, 60, member).info.held, "accepted member release creates a tail");
        pressure(host.midi, member, 20); on(host.midi, member, 62); host.tick(frames);
        host.expect(60, member, mapped(100), "channel reuse preserves released voice local input");
        host.expect(62, member, mapped(40), "reused channel onset combines new member input and manager");
        pressure(host.midi, manager, 110); host.tick(frames);
        host.expect(60, member, mapped(110), "manager still reaches released voice");
        pressure(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, mapped(100), "manager zero reveals frozen released local input");
        host.expect(62, member, mapped(20), "manager zero reveals new held voice local input");
    }
}
void testDormantActivation(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (int frames : {0, 32})
    for (bool upper : {false, true}) for (bool managerFirst : {false, true}) {
        context = std::string("activation/") + (circuit ? "circuit" : "legacy")
            + (upper ? "/upper" : "/lower") + (managerFirst ? "/manager-first" : "/member-first")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, false);
        const int manager = upper ? 16 : 1, member = upper ? 15 : 2;
        pressure(host.midi, managerFirst ? manager : member, managerFirst ? 100 : 40);
        pressure(host.midi, managerFirst ? member : manager, managerFirst ? 40 : 100); host.tick(frames);
        host.processor->setParamValue(P_voiceMode, 1); host.tick(); host.fillPrimary(true);
        on(host.midi, member, 60); host.tick(frames);
        const auto witness = find(*host.processor, 60, member);
        check(witness.info.held && witness.engine == &host.processor->getExtensionEngine(), "new extension owns pressure witness");
        host.expect(60, member, 100.0f / 127, "activation inherits both raw pressure inputs");
        pressure(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, 40.0f / 127, "activation does not bake manager floor into member cache");
    }
}
void testUiPressure(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    for (int frames : {0, 32}) for (bool upper : {false, true}) {
        context = std::string("UI/") + (circuit ? "circuit" : "legacy") + (sixteen ? "/16" : "/8")
            + (upper ? "/upper" : "/lower") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.processor->serviceKeyboardUI(); host.fillPrimary(sixteen);
        const int member = upper ? 15 : 2, manager = upper ? 16 : 1;
        pressure(host.midi, manager, 100); host.tick(frames);
        auto& keyboard = host.processor->getKeyboardState();
        const auto token = host.processor->captureKeyboardGesture(member, 60, [&] { keyboard.noteOn(member, 60, .8f); });
        host.tick(frames);
        check(token && find(*host.processor, 60, member).info.held, "real UI gesture allocates held note");
        check(!host.processor->isHostKeyDown(60), "UI witness is not host MIDI");
        if (sixteen) check(find(*host.processor, 60, member).engine == &host.processor->getExtensionEngine(), "UI witness uses extension");
        check(host.processor->setKeyPressureFromUI(token, .2f), "live UI token pressure accepted"); host.tick(frames);
        host.expect(60, member, 100.0f / 127, "manager floors UI token local pressure");
        pressure(host.midi, manager, 0); host.tick(frames);
        host.expect(60, member, .2f, "manager zero reveals UI token input");
        keyboard.noteOff(member, 60, 0); host.tick();
        check(!find(*host.processor, 60, member).info.held, "UI release works after combined pressure");
    }
}
void testCapturedManager(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    for (int frames : {0, 32}) {
        context = std::string("captured-manager/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen);
        const auto& zones = host.processor->getEngine().getMpeZones();
        check(zones.role(1) == MpeZoneLayout::Role::lowerMaster
                  && zones.role(16) == MpeZoneLayout::Role::upperMaster,
              "both zones are configured while MPE is enabled before captured-manager test");
        host.processor->setParamValue(P_mpeOn, 0); host.tick(); host.fillPrimary(sixteen);
        host.processor->serviceKeyboardUI();
        auto& keyboard = host.processor->getKeyboardState();
        const auto token = host.processor->captureKeyboardGesture(16, 60,
            [&] { keyboard.noteOn(16, 60, .8f); });
        host.tick(frames);
        check(token && find(*host.processor, 60, 16).info.held, "MPE-off channel 16 UI witness is held");
        check(host.processor->setKeyPressureFromUI(token, .2f), "captured voice receives independent UI local input");
        host.tick(frames);
        host.processor->setParamValue(P_mpeOn, 1); host.tick();
        check(find(*host.processor, 60, 16).info.held, "enabling MPE preserves captured voice");
        pressure(host.midi, 16, 100); pressure(host.midi, 1, 60); host.tick(frames);
        host.expect(60, 16, 100.0f / 127, "own upper and captured lower managers combine independently");
        pressure(host.midi, 16, 0); host.tick(frames);
        host.expect(60, 16, 60.0f / 127, "own upper manager zero reveals captured lower floor");
        pressure(host.midi, 1, 0); host.tick(frames);
        host.expect(60, 16, .2f, "both manager zeros reveal independent UI local pressure");
        on(host.midi, 16, 62); host.tick(frames);
        pressure(host.midi, 16, 100); host.tick(frames);
        host.expect(62, 16, 100.0f / 127, "MPE-origin manager note receives manager pressure");
        pressure(host.midi, 16, 0); host.tick(frames);
        host.expect(62, 16, 0, "MPE-origin manager note does not store manager as local");
    }
}
void testControls(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool pure : {false, true}) {
        context = std::string(pure ? "PURE" : "MPE-off") + (circuit ? "/circuit" : "/legacy");
        Host host(fixture, circuit, false);
        host.processor->setParamValue(pure ? int(P_parityMode) : int(P_mpeOn), pure ? 1 : 0); host.tick();
        check(!host.processor->getEngine().isMpeEnabled(), "control disables effective MPE");
        on(host.midi, 2, 60); host.tick(256);
        check(find(*host.processor, 60, 2).info.held, "conventional control witness is held");
        pressure(host.midi, 1, 100); host.tick(32);
        host.expect(60, 2, 100.0f / 127, "conventional channel 1 pressure retains global broadcast");
        pressure(host.midi, 2, 40); host.tick(32);
        host.expect(60, 2, 40.0f / 127, "conventional own-channel pressure replaces broadcast without manager floor");
    }
    for (bool circuit : {false, true}) for (bool sixteen : {false, true}) {
        context = std::string("matrix/") + (circuit ? "circuit" : "legacy") + (sixteen ? "/16" : "/8");
        Host host(fixture, circuit, sixteen); host.fillPrimary(sixteen);
        host.processor->setParamValue(P_mod1Amount, 1);
        host.processor->getModernLayer().setSlot(0, {modern::ModSource::channelPressure,
            modern::ModSource::none, modern::ModDestination::level, modern::ModTarget::both});
        on(host.midi, 2, 60); pressure(host.midi, 1, 100); pressure(host.midi, 2, 40); host.tick(32);
        const auto v = find(*host.processor, 60, 2);
        check(v.info.held, "matrix witness is held");
        host.expect(60, 2, 100.0f / 127, "voice pressure uses maximum while matrix does not");
        if (v.engine) for (int rank = 0; rank < 2; ++rank)
            check(near(v.engine->getModernRankOffsets(v.index, rank).level, 40.0f / 127), "matrix remains last-event channel pressure");
    }
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-pressure-combination");
    testCombination(fixture.getFile()); testDormantActivation(fixture.getFile());
    testUiPressure(fixture.getFile()); testControls(fixture.getFile());
    testCapturedManager(fixture.getFile());
    std::cout << "MPE pressure combination: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
