// Direct processor witness retaining the original lanes, blocks and recall pairs.
// Circuit factory recalls preserve attached host curves until wrapper scope exit.
// Existing PresetSaveAccess friendship supplies read-only state observation.
// This is not a native VST3 wrapper or AU/DAW qualification substitute.
#include "plugin/PluginProcessor.h"
#include "core/FactoryPresets.h"
#include "Ws1Tolerances.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace cs80::tests
{
// This executable does not include PresetSaveTests.h; its existing production
// friend declaration is reused without adding or changing instrumentation.
struct PresetSaveAccess
{
    static const Engine& extension(const Cs80AudioProcessor& host) { return *host.extensionEngine; }
    static bool extensionIsActive(const Cs80AudioProcessor& host) { return host.extensionActive; }
    static int activeCurveCount(const Cs80AudioProcessor& host)
    {
        return static_cast<int>(std::count_if(host.automationCurves.begin(), host.automationCurves.end(),
            [](const auto& curve) { return curve.active(); }));
    }
    static bool automationBlockActive(const Cs80AudioProcessor& host) { return host.hostAutomationBlock; }
};
}

namespace
{
// Keep this frozen pre-extension witness at its original eighteen lanes.
// New pan lanes are covered by the actual-module dynamic boundary fixtures.
constexpr size_t frozenParameterCount = 18;
static_assert(cs80::kSampleAccurateParameters.size() >= frozenParameterCount);
int checks = 0, failures = 0;
void check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition) { ++failures; std::cerr << "FAIL " << message << '\n'; }
}
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
using Access = cs80::tests::PresetSaveAccess;

struct State
{
    cs80::ParamValues primary, extension, stored;
    cs80::RenderPolicy primaryPolicy, extensionPolicy;
    bool extensionActive, automationActive;
    int activeCurves, program, primaryVoices, extensionVoices;
};
State inspect(Cs80AudioProcessor& host)
{
    const auto& extension = Access::extension(host);
    return {host.getEngine().getParams(), extension.getParams(), host.currentValues(),
            host.getEngine().getRenderPolicy(), extension.getRenderPolicy(),
            Access::extensionIsActive(host), Access::automationBlockActive(host),
            Access::activeCurveCount(host), host.getCurrentProgram(),
            host.getEngine().activeVoices(), extension.activeVoices()};
}
bool sameState(const State& a, const State& b)
{
    return a.primary.v == b.primary.v && a.extension.v == b.extension.v && a.stored.v == b.stored.v
        && a.primaryPolicy == b.primaryPolicy && a.extensionPolicy == b.extensionPolicy
        && a.extensionActive == b.extensionActive && a.automationActive == b.automationActive
        && a.activeCurves == b.activeCurves && a.program == b.program
        && a.primaryVoices == b.primaryVoices && a.extensionVoices == b.extensionVoices;
}
double error(const cs80::ParamValues& a, const cs80::ParamValues& b)
{
    double maximum = 0;
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        if (!std::isfinite(a[i]) || !std::isfinite(b[i])) return std::numeric_limits<double>::infinity();
        maximum = std::max(maximum, std::abs(double(a[i]) - double(b[i])));
    }
    return maximum;
}
void initialise(Cs80AudioProcessor& host)
{
    auto values = cs80::defaultValues();
    values[cs80::P_parityMode] = 0.0f;
    values[cs80::P_sourcePerformance] = 1.0f;
    values[cs80::P_voiceMode] = 1.0f;
    values[cs80::P_volume] = 0.11f;
    values[cs80::P_drift] = values[cs80::P_voiceDispersion] = values[cs80::P_analogNoise] = 0.0f;
    host.applyParameterValues(values);
    // Include physical/session parameters and parity explicitly in both
    // independent fixtures; preset recall deliberately preserves these.
    for (int i = 0; i < cs80::P_numParams; ++i) host.setParamValue(i, values[i]);
}
void process(Cs80AudioProcessor& host, int frames, juce::MidiBuffer midi, std::vector<float>& audio)
{
    juce::AudioBuffer<float> buffer(2, frames);
    buffer.clear();
    host.processBlock(buffer, midi);
    for (int i = 0; i < frames; ++i)
        for (int channel = 0; channel < 2; ++channel)
        {
            const float sample = buffer.getSample(channel, i);
            require(std::isfinite(sample), "direct multi-recall render must remain finite");
            audio.push_back(sample);
        }
}
void recall(juce::MidiBuffer& midi, int sample, int bank)
{
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, bank), sample);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), sample);
    midi.addEvent(juce::MidiMessage::programChange(1, 0), sample);
}
struct Queue
{
    juce::RangedAudioParameter* parameter = nullptr;
    std::array<std::pair<int, double>, 2> points {};
    int commits = 0;
    static bool read(void* context, int index, int& sample, double& value) noexcept
    {
        if (index < 0 || index >= 2) return false;
        const auto& point = static_cast<Queue*>(context)->points[static_cast<size_t>(index)];
        sample = point.first; value = point.second;
        return true;
    }
    static void commit(void* context, float value) noexcept
    {
        auto& queue = *static_cast<Queue*>(context);
        ++queue.commits;
        queue.parameter->setValueNotifyingHost(value);
    }
};
enum class Mode { ordinary, vst3Empty, vst3Curves };
struct Result
{
    State warm, insideScope, afterScope, afterRepeatedEnd, nextBlock, afterFreshEdit;
    std::vector<float> audio;
    std::array<int, frozenParameterCount> commitsInside {}, commitsAfter {},
        commitsAfterRepeatedEnd {}, commitsNext {}, commitsAfterFreshEdit {};
};
Result run(int first, int second, Mode mode, int frames, int notes)
{
    juce::TemporaryFile root("-cs80-vst3-multirecall-state");
    auto owned = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
    auto& host = *owned;
    initialise(host);
    host.prepareToPlay(48000.0, frames);
    Result result;
    juce::MidiBuffer warm;
    for (int i = 0; i < notes; ++i)
        warm.addEvent(juce::MidiMessage::noteOn(1, 36 + i, static_cast<juce::uint8>(96)), i);
    process(host, 128, warm, result.audio);
    result.warm = inspect(host);
    std::array<Queue, frozenParameterCount> queues;
    for (size_t lane = 0; lane < queues.size(); ++lane)
    {
        auto& queue = queues[lane];
        queue.parameter = host.getAPVTS().getParameter(juce::String(cs80::paramId(cs80::kSampleAccurateParameters[lane])));
        require(queue.parameter != nullptr, "every frozen automation parameter must exist");
        queue.points = {{{0, queue.parameter->getValue()}, {frames - 1, 0.18 + 0.02 * double(lane)}}};
    }
    juce::MidiBuffer midi;
    recall(midi, first, 1);
    recall(midi, second, 0);
    // Insert after both same-sample recalls. Existing extension lifetime lets
    // these notes reach both already-active instruments even if the program
    // now selects eight voices; no immediate extension shutdown is allowed.
    for (int i = 0; i < notes; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, 60 + i, static_cast<juce::uint8>(96)), second);
    {
        cs80::HostAutomationBlock scope(mode == Mode::ordinary ? nullptr : &host, frames, true);
        if (mode == Mode::vst3Curves)
            for (auto& queue : queues)
                require(host.attachHostAutomationQueue(*queue.parameter,
                    {&queue, 2, &Queue::read, &queue, &Queue::commit}), "supported curve must attach");
        process(host, frames, midi, result.audio);
        result.insideScope = inspect(host);
        for (size_t lane = 0; lane < queues.size(); ++lane) result.commitsInside[lane] = queues[lane].commits;
    }
    result.afterScope = inspect(host);
    for (size_t lane = 0; lane < queues.size(); ++lane) result.commitsAfter[lane] = queues[lane].commits;
    // A repeated wrapper flush must not publish borrowed queues twice.
    host.endHostAutomationBlock();
    result.afterRepeatedEnd = inspect(host);
    for (size_t lane = 0; lane < queues.size(); ++lane)
        result.commitsAfterRepeatedEnd[lane] = queues[lane].commits;
    process(host, 64, {}, result.audio);
    result.nextBlock = inspect(host);
    for (size_t lane = 0; lane < queues.size(); ++lane) result.commitsNext[lane] = queues[lane].commits;
    // A new unqueued edit must supersede the completed curve's endpoint.
    host.setParamValue(cs80::P_volume, 0.73f);
    process(host, 64, {}, result.audio);
    result.afterFreshEdit = inspect(host);
    for (size_t lane = 0; lane < queues.size(); ++lane)
        result.commitsAfterFreshEdit[lane] = queues[lane].commits;
    host.releaseResources();
    return result;
}
}

int main(int argc, char** argv)
{
    try
    {
        require(argc == 2, "usage: cs80_main_vst3_multirecall_state_tests CONTRACT");
        const ws1::Tolerances limits(argv[1], "main_vst3_multirecall_state");
        require(limits["allowed_failures"] == 0 && limits["maximum_physical_parameter_error"] == 0
                && limits["maximum_stored_parameter_error"] == 0, "state contract must retain exact comparisons");
        require(limits["key_parameter_count"] == frozenParameterCount, "frozen parameter roster");
        const int frames = static_cast<int>(limits["block_frames"]);
        const int notes = static_cast<int>(limits["warm_note_count"]);
        require(frames == 1024 && notes == 16, "frozen block and warm-note shape");
        const std::array<std::pair<int, int>, 5> pairs {{{0, 1}, {63, 64}, {64, 64}, {64, 700}, {700, 1023}}};
        require(limits["recall_pair_count"] == pairs.size(), "frozen recall coverage");
        juce::ScopedJuceInitialiser_GUI juceInitialiser;
        juce::TemporaryFile referenceRoot("-cs80-vst3-multirecall-reference");
        auto reference = std::make_unique<Cs80AudioProcessor>(referenceRoot.getFile().getChildFile("presets"));
        initialise(*reference);
        const int finalProgram = cs80::programIndexForMidi(0, 0, 0);
        require(finalProgram >= 0, "independent final MIDI program must exist");
        reference->loadFactoryPreset(finalProgram);
        const auto expected = reference->currentValues();
        require(expected[cs80::P_sourcePerformance] == 1.0f && expected[cs80::P_voiceMode] == 0.0f,
                "final independent program must use Circuit and eight voices");
        // The queue generator above remains unchanged. Independently enumerate
        // its final normalized values, then use the reference processor's public
        // parameter mapping; do not derive expectations from the rendered host
        // or from HostAutomationCurve.
        constexpr std::array<float, frozenParameterCount> normalizedEndpoints {{
            0.18f, 0.20f, 0.22f, 0.24f, 0.26f, 0.28f,
            0.30f, 0.32f, 0.34f, 0.36f, 0.38f, 0.40f,
            0.42f, 0.44f, 0.46f, 0.48f, 0.50f, 0.52f }};
        auto expectedEndpoint = expected;
        for (size_t lane = 0; lane < normalizedEndpoints.size(); ++lane)
        {
            const int key = cs80::kSampleAccurateParameters[lane];
            auto* parameter = reference->getAPVTS().getParameter(juce::String(cs80::paramId(key)));
            require(parameter != nullptr, "independent endpoint parameter must exist");
            expectedEndpoint[key] = parameter->convertFrom0to1(normalizedEndpoints[lane]);
        }
        require(error(expectedEndpoint, expected) > 0.0,
                "the prescribed queue endpoints must distinguish preserved curves from cancelled curves");
        auto expectedFreshEdit = expectedEndpoint;
        expectedFreshEdit[cs80::P_volume] = 0.73f;
        for (const auto [first, second] : pairs)
        {
            const std::string label = "first=" + std::to_string(first) + " second=" + std::to_string(second);
            const auto actual = run(first, second, Mode::vst3Curves, frames, notes);
            check(actual.warm.extensionActive && actual.warm.primaryVoices > 0 && actual.warm.extensionVoices > 0,
                  label + " warmup reaches both active instruments");
            const auto& final = actual.insideScope;
            const double primaryError = error(final.primary, expectedEndpoint);
            const double extensionError = error(final.extension, expectedEndpoint);
            const double storedError = error(final.stored, expected);
            std::cout << "CURVED " << label << " primary_policy=" << static_cast<int>(final.primaryPolicy)
                      << " extension_policy=" << static_cast<int>(final.extensionPolicy)
                      << " primary_error=" << primaryError << " extension_error=" << extensionError
                      << " stored_error=" << storedError << " extension_active=" << final.extensionActive
                      << " primary_voices=" << final.primaryVoices << " extension_voices=" << final.extensionVoices
                      << " active_curves=" << final.activeCurves << '\n';
            check(final.program == finalProgram, label + " final MIDI program identity owns the result");
            check(final.primaryPolicy == cs80::RenderPolicy::sourcePerformance
                  && final.extensionPolicy == cs80::RenderPolicy::sourcePerformance,
                  label + " both active engines retain the final Circuit policy");
            check(primaryError <= limits["maximum_physical_parameter_error"], label + " primary physical frame overlays prescribed endpoints on the final program");
            check(extensionError <= limits["maximum_physical_parameter_error"], label + " extension physical frame overlays prescribed endpoints on the final program");
            check(storedError <= limits["maximum_stored_parameter_error"], label + " stored physical values remain the final program until wrapper publication");
            check(final.activeCurves == static_cast<int>(frozenParameterCount) && final.automationActive,
                  label + " both recalls retain every trajectory through the complete process call");
            check(std::all_of(actual.commitsInside.begin(), actual.commitsInside.end(), [](int n) { return n == 0; }),
                  label + " no queue publishes before wrapper-scope exit");
            auto expectedAfterScope = final;
            expectedAfterScope.stored = expectedEndpoint;
            expectedAfterScope.activeCurves = 0;
            expectedAfterScope.automationActive = false;
            check(std::all_of(actual.commitsAfter.begin(), actual.commitsAfter.end(), [](int n) { return n == 1; })
                      && sameState(expectedAfterScope, actual.afterScope),
                  label + " wrapper-scope exit publishes each endpoint once and changes only stored values and queue ownership");
            check(actual.commitsAfterRepeatedEnd == actual.commitsAfter
                      && sameState(actual.afterScope, actual.afterRepeatedEnd),
                  label + " repeated wrapper flush neither recommits nor changes final state");
            check(final.extensionActive && actual.afterScope.extensionActive,
                  label + " existing extension lifetime survives the complete recalled block");
            check(!actual.nextBlock.extensionActive, label + " eight-voice recall retires extension at next ordinary block");
            check(actual.nextBlock.stored.v == expectedEndpoint.v
                      && actual.nextBlock.primary.v == expectedEndpoint.v
                      && actual.nextBlock.extension.v == expectedEndpoint.v
                      && actual.nextBlock.program == finalProgram
                      && actual.nextBlock.activeCurves == 0 && !actual.nextBlock.automationActive
                      && actual.commitsNext == actual.commitsAfter,
                  label + " following no-queue block holds committed endpoints without reviving or republishing queues");
            check(actual.afterFreshEdit.stored.v == expectedFreshEdit.v
                      && actual.afterFreshEdit.primary.v == expectedFreshEdit.v
                      && actual.afterFreshEdit.program == finalProgram
                      && !actual.afterFreshEdit.extensionActive
                      && actual.afterFreshEdit.activeCurves == 0 && !actual.afterFreshEdit.automationActive
                      && actual.commitsAfterFreshEdit == actual.commitsAfter,
                  label + " a later unqueued control edit supersedes the completed endpoint without replay");

            const auto ordinary = run(first, second, Mode::ordinary, frames, notes);
            const auto empty = run(first, second, Mode::vst3Empty, frames, notes);
            const bool sameAudio = ordinary.audio.size() == empty.audio.size()
                && std::memcmp(ordinary.audio.data(), empty.audio.data(), ordinary.audio.size() * sizeof(float)) == 0;
            check(sameAudio && sameState(ordinary.warm, empty.warm)
                  && sameState(ordinary.afterScope, empty.afterScope) && sameState(ordinary.nextBlock, empty.nextBlock)
                  && sameState(ordinary.afterFreshEdit, empty.afterFreshEdit),
                  label + " tagged empty queues preserve ordinary main PCM and state exactly");
            std::cout << "NO_CURVE_CHARACTERIZATION " << label << " same_pcm=" << sameAudio
                      << " ordinary_extension_policy=" << static_cast<int>(ordinary.afterScope.extensionPolicy)
                      << " ordinary_extension_error=" << error(ordinary.afterScope.extension, expected)
                      << " extension_active=" << ordinary.afterScope.extensionActive << '\n';
        }
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 2; }
    std::cout << "Main VST3 multi-recall state: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
