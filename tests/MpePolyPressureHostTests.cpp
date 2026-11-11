// MPE v1.1 Table 7 (p. 30): native MIDI Polyphonic Key Pressure is
// ignored on member channels. Internal keyboard pressure remains per-key.
#include "plugin/PluginProcessor.h"

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
void on(juce::MidiBuffer& midi, int channel, int note, int offset = 0)
{
    midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 80), offset);
}
void pressure(juce::MidiBuffer& midi, int channel, int note, int value, int offset = 0)
{
    midi.addEvent(juce::MidiMessage::aftertouchChange(channel, note, value), offset);
}
struct FoundVoice
{
    const Engine* engine = nullptr;
    int index = -1;
    Engine::VoiceInfo info;
};
FoundVoice find(Cs80AudioProcessor& host, int note, int channel)
{
    for (const auto* engine : std::array<const Engine*, 2>{&host.getEngine(), &host.getExtensionEngine()})
        for (int i = 0; i < 8; ++i)
        {
            const auto v = engine->getVoiceInfo(i);
            if (v.active && v.note == note && v.channel == channel) return {engine, i, v};
        }
    return {};
}
bool near(float a, float b) { return std::abs(a - b) < 1.0e-6f; }
struct Host
{
    std::unique_ptr<Cs80AudioProcessor> processor;
    juce::AudioBuffer<float> audio {2, 0};
    juce::MidiBuffer midi;
    Host(const juce::File& fixture, bool circuit, bool sixteen)
    {
        processor = std::make_unique<Cs80AudioProcessor>(fixture);
        processor->setEngineMode(circuit ? Cs80AudioProcessor::EngineMode::circuit
                                         : Cs80AudioProcessor::EngineMode::legacy);
        for (int p : std::array<int, 9>{P_parityMode, P_kasScanTiming, P_arpOn, P_voiceDispersion,
                                       P_drift, P_analogNoise, P_velocityCurve, P_pressureCurve,
                                       P_pressureThreshold})
            processor->setParamValue(p, 0);
        processor->setParamValue(P_mpeOn, 1);
        processor->setParamValue(P_voiceMode, sixteen ? 1 : 0);
        processor->setParamValue(P_keyRelease, 5);
        processor->prepareToPlay(48000, 256);
        tick();
    }
    void tick(int frames = 0)
    {
        audio.setSize(2, frames);
        processor->processBlock(audio, midi);
        midi.clear();
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
    void fillPrimary(bool sixteen)
    {
        if (sixteen) { for (int n = 40; n < 48; ++n) on(midi, 10, n); tick(); }
    }
};
struct Scenario { const char* name; int channel, lower, upper; bool enabled, reject; };
constexpr std::array<Scenario, 9> scenarios {{
    {"channel16-lower-member", 16, 15, 0, true, true},
    {"channel1-upper-member", 1, 0, 15, true, true},
    {"lower-member", 2, 2, 2, true, true},
    {"upper-member", 15, 2, 2, true, true},
    {"lower-manager", 1, 2, 2, true, false},
    {"upper-manager", 16, 2, 2, true, false},
    {"conventional-gap", 8, 2, 2, true, false},
    {"zero-zones", 2, 0, 0, true, false},
    {"MPE-off", 2, 2, 2, false, false}
}};
void testNativeRouting(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (int frames : {0, 32}) for (const auto& s : scenarios)
    {
        context = std::string(s.name) + (circuit ? "/circuit" : "/legacy")
            + (sixteen ? "/16" : "/8") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.layout(s.lower, s.upper);
        host.processor->setParamValue(P_mpeOn, s.enabled ? 1 : 0); host.tick();
        host.fillPrimary(sixteen);
        on(host.midi, s.channel, 60); on(host.midi, s.channel, 62); on(host.midi, 9, 60);
        host.tick(frames);
        const auto witness = find(*host.processor, 60, s.channel);
        check(witness.info.held, "target note is really held");
        if (sixteen) check(witness.engine == &host.processor->getExtensionEngine(),
                           "target pressure witness reaches extension engine");
        // Valid member pressure must still work, including the cached onset
        // value for a later note. Poly pressure must not overwrite either.
        if (s.reject)
        {
            host.midi.addEvent(juce::MidiMessage::channelPressureChange(s.channel, 31), 0);
            host.tick(frames);
            check(near(find(*host.processor, 60, s.channel).info.pressure, 31.0f / 127),
                  "member channel pressure is accepted as the positive control");
        }
        for (int value : {127, 0, 89})
        {
            pressure(host.midi, s.channel, 60, value); host.tick(frames);
            const auto now = find(*host.processor, 60, s.channel);
            check(now.engine == witness.engine && now.index == witness.index && now.info.held,
                  "pressure leaves voice allocation and held state intact");
            check(near(now.info.pressure, (s.reject ? 31 : value) / 127.0f),
                  s.reject ? "native member poly pressure is ignored" : "non-member poly pressure is retained");
            check(near(find(*host.processor, 62, s.channel).info.pressure, s.reject ? 31.0f / 127 : 0),
                  "different pitch on same channel is unchanged");
            check(find(*host.processor, 60, 9).info.pressure == 0,
                  "same pitch on another channel is unchanged");
        }
        on(host.midi, s.channel, 64); host.tick(frames);
        check(near(find(*host.processor, 64, s.channel).info.pressure, s.reject ? 31.0f / 127 : 0),
              "poly pressure does not poison a later note's onset cache");
        host.midi.addEvent(juce::MidiMessage::noteOff(s.channel, 60), 0); host.tick();
        const auto released = find(*host.processor, 60, s.channel);
        check(released.info.active && !released.info.held, "released pressure witness remains an active tail");
        pressure(host.midi, s.channel, 60, 57); host.tick(frames);
        const auto tail = find(*host.processor, 60, s.channel);
        check(tail.info.active && !tail.info.held
                  && near(tail.info.pressure, (s.reject ? 31 : 57) / 127.0f),
              s.reject ? "native member poly pressure also ignores released tails"
                       : "conventional poly pressure retains released-tail pitch-group semantics");
    }
}
void testSameOffsetRoleChanges(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (int frames : {0, 32}) for (bool upper : {false, true}) for (bool entering : {false, true})
    {
        context = std::string("same-offset/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + (entering ? "/enter" : "/leave") + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen);
        const int channel = upper ? 13 : 4, manager = upper ? 16 : 1;
        host.layout(upper ? 0 : (entering ? 2 : 3), upper ? (entering ? 2 : 3) : 0);
        host.fillPrimary(sixteen);
        // Deliberately insert all events at one offset: parsing must consult
        // the role after the preceding MCM, not a block-start role snapshot.
        mcm(host.midi, manager, entering ? 3 : 2, 11);
        on(host.midi, channel, 60, 11);
        pressure(host.midi, channel, 60, 103, 11); host.tick(frames);
        const auto now = find(*host.processor, 60, channel);
        check(now.info.held, "post-MCM same-offset note is held");
        if (sixteen) check(now.engine == &host.processor->getExtensionEngine(),
                           "same-offset witness reaches extension");
        check(near(now.info.pressure, entering ? 0 : 103.0f / 127),
              "same-offset poly pressure follows the new effective channel role");
    }
}
void testPurePressure(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (int frames : {0, 32})
    {
        context = std::string("PURE/") + (circuit ? "circuit" : "legacy")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, false); host.layout(2, 2);
        host.processor->setParamValue(P_parityMode, 1); host.tick();
        check(!host.processor->getEngine().isMpeEnabled(), "PURE disables effective MPE despite stored MPE toggle");
        on(host.midi, 2, 60); host.tick(256); // Allow PURE's physical scanner to allocate.
        check(find(*host.processor, 60, 2).info.held, "PURE pressure witness is held");
        pressure(host.midi, 2, 60, 79); host.tick(frames);
        check(near(find(*host.processor, 60, 2).info.pressure, 79.0f / 127),
              "PURE preserves conventional native per-key pressure");
    }
}
void testInternalUiPressure(const juce::File& fixture)
{
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
        for (int frames : {0, 32}) for (bool upper : {false, true})
    {
        context = std::string("internal-UI/") + (circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8") + (upper ? "/upper" : "/lower")
            + "/frames=" + std::to_string(frames);
        Host host(fixture, circuit, sixteen); host.layout(2, 2);
        host.processor->serviceKeyboardUI(); host.fillPrimary(sixteen);
        const int channel = upper ? 15 : 2;
        auto& keyboard = host.processor->getKeyboardState();
        const auto token = host.processor->captureKeyboardGesture(channel, 60,
            [&] { keyboard.noteOn(channel, 60, .8f); });
        host.tick(frames);
        check(token && find(*host.processor, 60, channel).info.held,
              "real UI gesture allocates member-channel note");
        check(!host.processor->isHostKeyDown(60), "UI witness is not substituted host MIDI");
        if (sixteen) check(find(*host.processor, 60, channel).engine == &host.processor->getExtensionEngine(),
                           "UI pressure witness reaches extension");
        check(host.processor->setKeyPressureFromUI(token, .6f), "live UI pressure token is accepted");
        host.tick(frames);
        check(near(find(*host.processor, 60, channel).info.pressure, .6f),
              "internal UI per-key pressure still works on MPE members");
        pressure(host.midi, channel, 60, 127); host.tick(frames);
        check(near(find(*host.processor, 60, channel).info.pressure, .6f),
              "native member poly pressure cannot overwrite internal UI pressure");
        host.processor->setKeyPressureFromUI(token, .2f); host.tick(frames);
        check(near(find(*host.processor, 60, channel).info.pressure, .2f),
              "internal UI pressure remains live after ignored native event");
        keyboard.noteOff(channel, 60, 0); host.tick(frames);
        check(!find(*host.processor, 60, channel).info.held, "UI release remains functional");
    }
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-mpe-poly-pressure");
    testNativeRouting(fixture.getFile());
    testSameOffsetRoleChanges(fixture.getFile());
    testInternalUiPressure(fixture.getFile());
    testPurePressure(fixture.getFile());
    std::cout << "MPE poly pressure: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
