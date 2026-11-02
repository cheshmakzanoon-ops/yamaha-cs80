#pragma once

// Registered in host-automation-state-boundary.json before evaluation. This
// fixture uses the actual processor and APVTS. The native VST3 host test covers
// the additional zero-frame wrapper path; neither fixture substitutes for it.
#include "plugin/PluginProcessor.h"
#include "Ws1Tolerances.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace cs80::tests
{
struct AutomationStateAccess
{
    // Inspect the real processor's per-sample control target without rendering
    // an independent voice for every scalar assertion. No test mode enters DSP.
    static void applyAt(Cs80AudioProcessor& host, int sample)
    { host.applyHostAutomationAt(sample, false); }
};

namespace automation_boundary_detail
{
struct Point { int sample = 0; double value = 0; };
inline bool readPoint(void* context, int index, int& sample, double& value) noexcept
{
    if (index != 0) return false;
    const auto& point = *static_cast<const Point*>(context);
    sample = point.sample;
    value = point.value;
    return true;
}
inline void commitParameter(void* parameter, float value) noexcept
{ static_cast<juce::AudioProcessorParameter*>(parameter)->setValueNotifyingHost(value); }
} // namespace automation_boundary_detail

template <typename Check>
void testHostAutomationStateBoundary(Check check)
{
    using namespace automation_boundary_detail;
    const ws1::Tolerances limits((std::filesystem::path(__FILE__).parent_path()
        / "parity/tolerances-v2/host-automation-state-boundary.json").string(),
        "host_automation_state_boundary");
    check(kSampleAccurateParameters.size() == limits["key_parameter_count"],
          "state-boundary gate exercises the registered key roster");
    juce::TemporaryFile root("-cs80-automation-state-boundary");
    auto ownedHost = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
    auto& host = *ownedHost;
    auto values = defaultValues();
    values[P_parityMode] = 0.0f;
    values[P_sourcePerformance] = 1.0f;
    values[P_drift] = values[P_voiceDispersion] = values[P_analogNoise] = 0.0f;
    host.applyParameterValues(values);
    host.prepareToPlay(48000.0, 256);
    std::array<juce::RangedAudioParameter*, kSampleAccurateParameters.size()> parameters {};
    for (size_t lane = 0; lane < parameters.size(); ++lane)
    {
        parameters[lane] = host.getAPVTS().getParameter(juce::String(paramId(kSampleAccurateParameters[lane])));
        check(parameters[lane] != nullptr, "state-boundary key has an actual APVTS parameter");
        if (parameters[lane] == nullptr) return;
    }
    std::array<Point, kSampleAccurateParameters.size()> points {};
    const auto attach = [&] {
        for (size_t lane = 0; lane < points.size(); ++lane)
            check(host.attachHostAutomationQueue(*parameters[lane],
                {&points[lane], 1, readPoint, parameters[lane], commitParameter}),
                "state-boundary fixture attaches the native bridge queue");
    };
    const auto completePriorCurve = [&] {
        host.beginHostAutomationBlock(1);
        for (size_t lane = 0; lane < points.size(); ++lane)
            points[lane] = {0, 0.234567890123 + double(lane) * 0.007654321987};
        attach();
        AutomationStateAccess::applyAt(host, 0);
        host.endHostAutomationBlock();
    };

    double largestCurveError = 0.0;
    const auto checkDelayedCurve = [&](int frames, int firstSample, bool exactUnchangedEndpoint) {
        std::array<double, kSampleAccurateParameters.size()> initial {};
        for (size_t lane = 0; lane < initial.size(); ++lane)
            initial[lane] = exactUnchangedEndpoint ? points[lane].value : parameters[lane]->getValue();
        host.beginHostAutomationBlock(frames);
        for (size_t lane = 0; lane < points.size(); ++lane)
        {
            const double tolerance = exactUnchangedEndpoint ? limits["unchanged_endpoint_error"]
                                                            : limits["normalized_curve_error"];
            check(std::abs(host.sampleAccurateParameterValue(lane) - initial[lane]) <= tolerance,
                exactUnchangedEndpoint ? "unchanged double host endpoint survives the render boundary exactly"
                                       : "intervening parameter state is authoritative before attaching a delayed queue");
            points[lane] = {firstSample, 0.789012345678 - double(lane) * 0.006543219876};
        }
        attach();
        bool everySampleCorrect = true;
        for (int sample = 0; sample < frames; ++sample)
        {
            AutomationStateAccess::applyAt(host, sample);
            for (size_t lane = 0; lane < points.size(); ++lane)
            {
                // Independent scalar equation, with the authoritative state at
                // -1 and the first transmitted knot at its unchanged offset.
                const double fraction = std::min(1.0, double(sample + 1) / double(firstSample + 1));
                const double expected = initial[lane] * (1.0 - fraction) + points[lane].value * fraction;
                const double error = std::abs(host.sampleAccurateParameterValue(lane) - expected);
                largestCurveError = std::max(largestCurveError, error);
                everySampleCorrect = everySampleCorrect && error <= limits["normalized_curve_error"];
            }
        }
        check(everySampleCorrect, "delayed queue follows the latest authoritative state at every sample");
        host.endHostAutomationBlock();
    };

    for (int frames : {7, 64, 256})
        for (int firstSample : {1, frames - 1})
        {
            completePriorCurve();
            checkDelayedCurve(frames, firstSample, true);

            completePriorCurve();
            {
                cs80::HostAutomationBlock block(&host, 8);
                juce::AudioBuffer<float> audio(2, 8);
                juce::MidiBuffer midi;
                host.processBlock(audio, midi);
            }
            checkDelayedCurve(frames, firstSample, true);

            completePriorCurve();
            for (size_t lane = 0; lane < parameters.size(); ++lane)
                parameters[lane]->setValueNotifyingHost(0.69f - float(lane) * 0.009f);
            checkDelayedCurve(frames, firstSample, false);

            // Save and restore through the processor's locked/synchronised
            // state API. This fixture never copies/replaces the live tree itself.
            for (size_t lane = 0; lane < parameters.size(); ++lane)
                parameters[lane]->setValueNotifyingHost(0.57f - float(lane) * 0.008f);
            juce::MemoryBlock state;
            host.getStateInformation(state);
            completePriorCurve();
            host.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
            checkDelayedCurve(frames, firstSample, false);
        }
    host.releaseResources();
    std::printf("Automation state-boundary maximum normalized error=%.17g\n", largestCurveError);
}
} // namespace cs80::tests
