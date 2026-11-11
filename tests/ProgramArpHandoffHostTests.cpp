// Real processor regression: a same-policy MIDI Program Change must update
// keyboard admission/pressure decisions at the event, not the next callback.
#include "plugin/PluginProcessor.h"
#include "core/FactoryPresets.h"

#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace cs80::tests
{
// Existing test friendship supplies only deterministic producer timestamps.
// Admission, token creation, epochs, ordering and processor dispatch are real.
struct PresetSaveAccess
{
    static KeyboardEventQueue<>& keyboardQueue(Cs80AudioProcessor& processor)
    { return processor.keyboardEvents; }
};
}

namespace
{
using namespace cs80;
int checks = 0, failures = 0;
std::string context;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL: " << message << " [" << context << "]\n"; }
}
int namedProgram(const char* name)
{
    const auto& programs = programLibrary();
    for (size_t i = 0; i < programs.size(); ++i)
        if (programs[i].name == name) return (int) i;
    return -1;
}
struct Host
{
    std::unique_ptr<Cs80AudioProcessor> processor;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio { 2, 128 };
    bool deterministicMidblock = false;
    explicit Host(const juce::File& fixture, int program, bool initialArp, bool sixteen)
        : processor(std::make_unique<Cs80AudioProcessor>(fixture))
    {
        processor->setCurrentProgram(program);
        processor->setParamValue(P_parityMode, 0);
        processor->setParamValue(P_arpOn, initialArp ? 1 : 0);
        processor->setParamValue(P_voiceMode, sixteen ? 1 : 0);
        processor->setParamValue(P_kasScanTiming, 0);
        processor->setParamValue(P_mpeOn, 0);
        processor->setParamValue(P_oversampling, 0);
        processor->prepareToPlay(48000, 128);
        tick();
        processor->serviceKeyboardUI();
        check(processor->getParamValue(P_voiceMode) == (sixteen ? 1 : 0), "requested pre-recall voice mode is active");
    }
    float tick(int frames = 128)
    {
        audio.setSize(2, frames);
        audio.clear(); processor->processBlock(audio, midi); midi.clear();
        float peak = 0;
        bool finite = true;
        for (int c = 0; c < audio.getNumChannels(); ++c)
            for (int s = 0; s < audio.getNumSamples(); ++s)
            {
                const float value = audio.getSample(c, s);
                finite = finite && std::isfinite(value);
                peak = std::max(peak, std::abs(value));
            }
        check(finite, "output remains finite");
        return peak;
    }
    float recallTick(int frames, bool volumeCurve)
    {
        if (!volumeCurve) return tick(frames);
        // Exercise the real supported continuous lane alongside MIDI recall.
        // ARP itself is deliberately not a sample-accurate automation lane.
        struct Point { int sample; double value; };
        std::array<Point, 3> points {{{0, 0.25}, {63, 0.5}, {127, 0.35}}};
        auto* parameter = processor->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_volume));
        auto read = +[](void* context, int index, int& sample, double& value) noexcept
        {
            const auto& queue = *static_cast<const std::array<Point, 3>*>(context);
            if (index < 0 || index >= (int) queue.size()) return false;
            sample = queue[(size_t) index].sample;
            value = queue[(size_t) index].value;
            return true;
        };
        float peak = 0;
        {
            HostAutomationBlock block(processor.get(), frames, true);
            check(processor->usesHostAutomationTiming(), "Circuit fixture enables real host-curve timing");
            check(parameter != nullptr && processor->attachHostAutomationQueue(*parameter,
                      {&points, (int) points.size(), read, parameter,
                       +[](void* target, float value) noexcept
                       { static_cast<juce::AudioProcessorParameter*>(target)->setValue(value); }}),
                  "real volume automation queue attaches during program recall");
            peak = tick(frames);
        }
        check(parameter != nullptr && std::abs(parameter->getValue() - 0.35f) < 1.0e-5f,
              "real volume curve survives recall and commits its final endpoint");
        return peak;
    }
    int held() const
    {
        int result = 0;
        for (int i = 0; i < 8; ++i)
        {
            const auto a = processor->getEngine().getVoiceInfo(i);
            const auto b = processor->getExtensionEngine().getVoiceInfo(i);
            result += a.active && a.held;
            result += b.active && b.held;
        }
        return result;
    }
    void program(int index, int offset)
    {
        const bool legacy = index < legacyProgramCount;
        const int local = legacy ? index : index - legacyProgramCount;
        check(programIndexForMidi(legacy ? 1 : 0, local / 128, local % 128) == index,
              "factory program has the expected MIDI bank address");
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, legacy ? 1 : 0), offset);
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, local / 128), offset);
        midi.addEvent(juce::MidiMessage::programChange(1, local % 128), offset);
    }
    KeyPressureToken strike(bool midblock)
    {
        deterministicMidblock = midblock;
        if (midblock)
        {
            auto& queue = tests::PresetSaveAccess::keyboardQueue(*processor);
            // With first=100 and last=101, the production queue maps the
            // anchor to sample 0 and onset to round(1*128/(1+1)) = 64.
            // Pressure uses the same 101 timestamp, so scheduler pauses cannot
            // stretch the batch and move the onset ahead of the program event.
            check(queue.push(1, 12, 0, false, 100), "timestamp anchor enters the real keyboard FIFO");
            KeyPressureToken token;
            check(queue.push(1, 60, 0.8f, true, 101, &token) && token,
                  "deterministic UI onset receives a production queue token");
            return token;
        }
        // Public JUCE gesture capture stays covered by same-offset and
        // zero-frame cases; only the midblock timestamps are test-controlled.
        return processor->captureKeyboardGesture(1, 60, [&]
            { processor->getKeyboardState().noteOn(1, 60, 0.8f); });
    }
    bool pressure(KeyPressureToken token, float value)
    {
        return deterministicMidblock
            ? tests::PresetSaveAccess::keyboardQueue(*processor).pushPressure(token, value, 101)
            : processor->setKeyPressureFromUI(token, value);
    }
    void release()
    {
        if (deterministicMidblock)
            check(tests::PresetSaveAccess::keyboardQueue(*processor).push(1, 60, 0, false, 102),
                  "deterministic UI release enters the real keyboard FIFO");
        else
            processor->getKeyboardState().noteOff(1, 60, 0);
    }
};
void testEnable(const juce::File& fixture, int program, bool midblock, bool sixteen,
                int recallFrames = 128, bool volumeCurve = false)
{
    Host host(fixture, program, false, sixteen);
    const auto policy = host.processor->getEngine().getRenderPolicy();
    const auto token = host.strike(midblock);
    host.program(program, midblock ? 32 : 0);
    float peak = host.recallTick(recallFrames, volumeCurve);
    check(token && host.processor->getCurrentProgram() == program
              && host.processor->getParamValue(P_arpOn) == 1,
          "host-first recall enables ARP before the captured UI onset");
    check(host.processor->getEngine().getRenderPolicy() == policy, "recall keeps the render policy and UI epoch");
    bool sawGate = host.held() > 0;
    // Positive control spans several gates before release. An empty or dropped
    // UI batch cannot pass merely because it has nothing left to release.
    for (int block = 0; block < 160; ++block)
    {
        peak = std::max(peak, host.tick());
        sawGate = sawGate || host.held() > 0;
    }
    check(sawGate && peak > 1.0e-7f, "accepted UI source produces held ARP gates and non-silent samples");
    host.release();
    host.tick(recallFrames == 0 ? 0 : 128);
    bool noNewGate = host.held() == 0;
    // Observe > two full periods of the actual recalled rate. Release tails
    // may continue sounding; renewed held gates are the stuck-source oracle.
    const float rate = host.processor->getParamValue(P_arpRate);
    check(rate > 0, "factory ARP rate is positive");
    const int blocks = (int) std::ceil(3.0 * 48000.0 / (128.0 * std::max(0.1f, rate)));
    for (int block = 0; block < blocks; ++block)
    {
        host.tick(); noNewGate = noNewGate && host.held() == 0;
    }
    check(noNewGate, "UI release removes its ARP source and no gate reappears");
}
void testDisable(const juce::File& fixture, int arpProgram, bool midblock, bool sixteen,
                 int recallFrames = 128, bool volumeCurve = false, bool rapid = false)
{
    const auto& programs = programLibrary();
    int directProgram = -1;
    for (size_t i = 0; i < programs.size(); ++i)
        if (programs[i].values[P_arpOn] < 0.5f
            && programs[i].values[P_sourcePerformance] == programs[(size_t) arpProgram].values[P_sourcePerformance])
        { directProgram = (int) i; break; }
    check(directProgram >= 0, "same-policy factory direct-note program exists");
    if (directProgram < 0) return;
    Host host(fixture, directProgram, true, sixteen);
    const auto policy = host.processor->getEngine().getRenderPolicy();
    const auto token = host.strike(midblock);
    check(host.pressure(token, 0.8f), "captured pressure enters the real keyboard FIFO");
    if (rapid)
    {
        // Each same-policy recall publishes a real ARP edge before the later
        // timestamped UI onset. No unsupported ARP automation lane is invented.
        host.program(directProgram, 16);
        host.program(arpProgram, 32);
        host.program(directProgram, 48);
    }
    else
        host.program(directProgram, midblock ? 32 : 0);
    host.recallTick(recallFrames, volumeCurve);
    check(host.processor->getEngine().getRenderPolicy() == policy
              && host.processor->getParamValue(P_arpOn) == 0 && host.held() == 1,
          "ARP-off recall allocates the UI onset without a policy reset");
    bool pressured = false;
    for (int i = 0; i < 8; ++i)
    {
        const auto voice = host.processor->getEngine().getVoiceInfo(i);
        pressured = pressured || (voice.active && voice.held && voice.note == 60 && voice.pressure > 0.7f);
    }
    check(pressured, "ARP-off recall admits captured per-key pressure in the same block");
    host.release();
    host.tick(recallFrames == 0 ? 0 : 128);
    check(host.held() == 0, "direct-note UI release after ARP-off recall reaches its voice");
}
} // namespace
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-program-arp-handoff");
    for (const char* name : { "Arp Motion", "Arp Poly" })
    {
        const int program = namedProgram(name);
        context = name;
        check(program >= 0, "named factory ARP program exists");
        if (program < 0) continue;
        check(programLibrary()[(size_t) program].values[P_arpOn] == 1, "factory program really enables ARP");
        for (bool midblock : { false, true }) for (bool sixteen : { false, true })
        {
            context = std::string(name) + (midblock ? "/midblock" : "/same-offset")
                + (sixteen ? "/initial-16" : "/initial-8");
            // Voice mode is a recalled sound parameter: these exercise both
            // pre-recall extension lifetimes, not a claim that recall keeps 16.
            testEnable(fixture.getFile(), program, midblock, sixteen);
            testDisable(fixture.getFile(), program, midblock, sixteen);
        }
        context = std::string(name) + "/rapid-off-on-off/midblock";
        testDisable(fixture.getFile(), program, true, false, 128, false, true);
        context = std::string(name) + "/same-offset/zero-frame";
        testEnable(fixture.getFile(), program, false, false, 0);
        testDisable(fixture.getFile(), program, false, false, 0);
        if (programLibrary()[(size_t) program].values[P_sourcePerformance] >= 0.5f)
            for (bool midblock : { false, true })
            {
                context = std::string(name) + "/real-volume-curve"
                    + (midblock ? "/midblock" : "/same-offset");
                testEnable(fixture.getFile(), program, midblock, false, 128, true);
                testDisable(fixture.getFile(), program, midblock, false, 128, true);
            }
    }
    std::cout << "Program ARP handoff: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
