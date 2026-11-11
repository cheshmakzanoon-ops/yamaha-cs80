// Direct processor regression for MIDI program recall while both instruments
// are active. This does not qualify a native wrapper or a DAW.
#include "plugin/PluginProcessor.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cs80::tests
{
// Reuse the existing test friendship; no production instrumentation is added.
struct PresetSaveAccess
{
    static Engine& extension(Cs80AudioProcessor& p) { return *p.extensionEngine; }
    static bool active(const Cs80AudioProcessor& p) { return p.extensionActive; }
    static void publishExpectedExtension(Cs80AudioProcessor& p)
    {
        const auto prior = p.extensionEngine->getRenderPolicy();
        p.extensionEngine->setControlFrame({p.cached, p.cachedHardwareSources});
        if (p.extensionEngine->getRenderPolicy() != prior)
            p.extensionEngine->setExpression(p.getExpressionValue());
    }
    static void freshNotes(Cs80AudioProcessor& p)
    {
        for (int i = 0; i < 16; ++i) p.routeNoteOn(60 + i, 0.75f, 1);
    }
};
}
namespace
{
using Access = cs80::tests::PresetSaveAccess;
int checks = 0, failures = 0;
void check(bool condition, const std::string& label)
{
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << label << '\n'; }
}
void require(bool condition, const char* label)
{
    if (!condition) throw std::runtime_error(label);
}
double energy(const std::vector<float>& audio)
{
    double sum = 0;
    for (float v : audio) { require(std::isfinite(v), "nonfinite audio"); sum += double(v) * v; }
    return sum;
}
std::vector<float> process(Cs80AudioProcessor& p, int frames, juce::MidiBuffer midi = {}, bool tagged = false)
{
    juce::AudioBuffer<float> buffer(2, frames);
    buffer.clear();
    cs80::HostAutomationBlock scope(tagged ? &p : nullptr, frames, true);
    p.processBlock(buffer, midi);
    std::vector<float> result;
    for (int i = 0; i < frames; ++i)
        for (int ch = 0; ch < 2; ++ch) result.push_back(buffer.getSample(ch, i));
    energy(result);
    return result;
}
void recall(juce::MidiBuffer& midi, int sample, int bank)
{
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, bank), sample);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), sample);
    midi.addEvent(juce::MidiMessage::programChange(1, 0), sample);
}
std::vector<float> render(cs80::Engine& engine, int frames)
{
    std::vector<float> audio(static_cast<size_t>(2 * frames));
    engine.render(audio.data(), frames);
    energy(audio);
    return audio;
}
struct Fixture
{
    juce::TemporaryFile root {"-cs80-program-extension-recall"};
    std::unique_ptr<Cs80AudioProcessor> owned;
    std::vector<float> warmAudio;
    Fixture() : owned(std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets")))
    {
        auto& p = *owned;
        auto values = cs80::defaultValues();
        values[cs80::P_parityMode] = 0.0f;
        values[cs80::P_sourcePerformance] = 1.0f;
        values[cs80::P_voiceMode] = 1.0f;
        values[cs80::P_volume] = 0.11f;
        values[cs80::P_drift] = values[cs80::P_voiceDispersion] = values[cs80::P_analogNoise] = 0.0f;
        p.applyParameterValues(values);
        for (int i = 0; i < cs80::P_numParams; ++i) p.setParamValue(i, values[i]);
        p.setExpressionFromUI(0.63f); // Policy resets must restore the physical pedal.
        p.prepareToPlay(48000.0, 2048);
        juce::MidiBuffer notes;
        for (int i = 0; i < 16; ++i)
            notes.addEvent(juce::MidiMessage::noteOn(1, 36 + i, static_cast<juce::uint8>(96)), i);
        warmAudio = process(p, 2048, notes);
        require(Access::active(p) && p.getEngine().activeVoices() == 8
                && Access::extension(p).activeVoices() == 8, "fixture must have sixteen active voices");
        // Measure each instrument separately, advancing every fixture identically.
        const auto primary = render(p.getEngine(), 128);
        const auto extension = render(Access::extension(p), 128);
        require(energy(primary) > 1.0e-12 && energy(extension) > 1.0e-12,
                "both eight-voice instruments must actually sound before recall");
        warmAudio.insert(warmAudio.end(), primary.begin(), primary.end());
        warmAudio.insert(warmAudio.end(), extension.begin(), extension.end());
    }
    ~Fixture() { owned->releaseResources(); }
};
struct State
{
    cs80::ParamValues primary, extension, stored;
    cs80::RenderPolicy primaryPolicy, extensionPolicy;
    bool active;
    int program;
};
State inspect(Cs80AudioProcessor& p)
{
    return {p.getEngine().getParams(), Access::extension(p).getParams(), p.currentValues(),
            p.getEngine().getRenderPolicy(), Access::extension(p).getRenderPolicy(),
            Access::active(p), p.getCurrentProgram()};
}
bool equal(const State& a, const State& b)
{
    return a.primary.v == b.primary.v && a.extension.v == b.extension.v && a.stored.v == b.stored.v
        && a.primaryPolicy == b.primaryPolicy && a.extensionPolicy == b.extensionPolicy
        && a.active == b.active && a.program == b.program;
}
State expectedState(int bank)
{
    Fixture reference;
    reference.owned->loadFactoryPreset(cs80::programIndexForMidi(bank, 0, 0));
    // The independently loaded preset supplies physical values, not the path
    // under test. Its engine state is intentionally not used as an audio oracle.
    auto state = inspect(*reference.owned);
    state.primary = state.extension = state.stored;
    state.primaryPolicy = state.extensionPolicy = cs80::renderPolicy(
        state.stored[cs80::P_parityMode], state.stored[cs80::P_sourcePerformance]);
    return state;
}
void checkRecall(const State& state, const State& expected, const std::string& label)
{
    check(state.primary.v == expected.stored.v, label + " primary physical frame");
    check(state.extension.v == expected.stored.v, label + " extension physical frame");
    check(state.stored.v == expected.stored.v, label + " stored physical values");
    check(state.primaryPolicy == expected.primaryPolicy && state.extensionPolicy == expected.extensionPolicy,
          label + " both policies");
    check(state.program == expected.program, label + " program identity");
    check(state.active, label + " extension remains active until next callback");
}
std::vector<float> directTail(Cs80AudioProcessor& p)
{
    require(Access::active(p), "direct tail requires the still-active extension");
    auto audio = render(p.getEngine(), 1024);
    const auto extension = render(Access::extension(p), 1024);
    for (size_t i = 0; i < audio.size(); ++i) audio[i] += extension[i];
    return audio;
}
void zeroFrameAudioWitness(int bank, bool tagged, const State& expected)
{
    Fixture actual, reference;
    const std::string label = "zero-frame bank=" + std::to_string(bank) + " tagged=" + std::to_string(tagged);
    check(actual.warmAudio == reference.warmAudio, label + " exact pre-event twin audio");
    const auto initial = inspect(*actual.owned);
    require(initial.primary.v != expected.stored.v, "initial and final patches must differ");
    juce::MidiBuffer midi;
    recall(midi, 0, bank);
    process(*actual.owned, 0, midi, tagged);
    process(*reference.owned, 0, midi, tagged);
    checkRecall(inspect(*actual.owned), expected, label);
    // Bounded intervention oracle: after the SAME real zero-frame MIDI recall,
    // explicitly publish the expected extension frame. Never call processBlock
    // between this event and the tail: an eight-voice recall intentionally
    // retires the extension at the next callback boundary.
    Access::publishExpectedExtension(*reference.owned);
    if (bank == 1)
    {
        // Cross-policy recalls reset voices. Inject identical fresh notes only
        // AFTER the reference publication, so its reset cannot erase them.
        Access::freshNotes(*actual.owned);
        Access::freshNotes(*reference.owned);
    }
    const auto audio = directTail(*actual.owned);
    const auto referenceAudio = directTail(*reference.owned);
    check(energy(referenceAudio) > 1.0e-12, label + " non-silent reference tail");
    check(audio == referenceAudio, label + " exact direct-engine tail after expected extension publication");
    std::cout << "DIRECT_TAIL " << label << " exact=" << (audio == referenceAudio) << '\n';
}
}
int main()
{
    try
    {
        juce::ScopedJuceInitialiser_GUI init;
        for (int bank : {0, 1})
        {
            const auto expected = expectedState(bank);
            require(expected.stored[cs80::P_voiceMode] == 0.0f, "coverage requires an eight-voice destination");
            for (bool tagged : {false, true}) zeroFrameAudioWitness(bank, tagged, expected);
            for (int position : {0, 64, 700, 1023})
            {
                Fixture ordinary, tagged;
                check(ordinary.warmAudio == tagged.warmAudio, "ordinary/tagged exact warmup");
                juce::MidiBuffer midi;
                recall(midi, position, bank);
                for (int i = 0; i < 16; ++i)
                    midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, static_cast<juce::uint8>(96)), position);
                const auto a = process(*ordinary.owned, 1024, midi);
                const auto b = process(*tagged.owned, 1024, midi, true);
                const std::string label = "bank=" + std::to_string(bank) + " sample=" + std::to_string(position);
                checkRecall(inspect(*ordinary.owned), expected, label + " ordinary");
                checkRecall(inspect(*tagged.owned), expected, label + " tagged-empty");
                check(a == b && equal(inspect(*ordinary.owned), inspect(*tagged.owned)),
                      label + " ordinary/tagged-empty exact audio and state");
                process(*ordinary.owned, 64);
                process(*tagged.owned, 64);
                check(!Access::active(*ordinary.owned) && !Access::active(*tagged.owned),
                      label + " next ordinary callback retires the extension");
            }
        }
        // Unassigned bank/program must not publish or otherwise alter a frame.
        for (bool tagged : {false, true})
        {
            Fixture invalid;
            const auto before = inspect(*invalid.owned);
            juce::MidiBuffer midi;
            recall(midi, 0, 127);
            process(*invalid.owned, 0, midi, tagged);
            check(equal(before, inspect(*invalid.owned)), "invalid recall preserves both engine frames");
        }
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    std::cout << "Program extension recall: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
