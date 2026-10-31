#include "PluginProcessor.h"

void Cs80AudioProcessor::resetHostAutomationState() noexcept
{
    hostAutomationBlock = false;
    hostAutomationFrames = hostAutomationPosition = 0;
    for (size_t i = 0; i < automationParameters.size(); ++i)
    {
        automationParameters[i] = apvts.getParameter(parameterId(cs80::kSampleAccurateParameters[i]));
        automationCurrent[i] = automationParameters[i] != nullptr ? automationParameters[i]->getValue() : 0.0;
        automationCurves[i] = {};
    }
}

bool Cs80AudioProcessor::usesHostAutomationTiming() const noexcept
{
    const auto policy = cs80::renderPolicy(
        parameterValues[cs80::P_parityMode]->load(std::memory_order_relaxed),
        parameterValues[cs80::P_sourcePerformance]->load(std::memory_order_relaxed));
    return !cs80::isPureCs80(policy) && cs80::usesSourceModels(policy);
}

int Cs80AudioProcessor::sampleAccurateParameterIndex(const juce::AudioProcessorParameter& parameter) const noexcept
{
    for (size_t i = 0; i < automationParameters.size(); ++i)
        if (automationParameters[i] == &parameter) return static_cast<int>(i);
    return -1;
}

double Cs80AudioProcessor::sampleAccurateParameterValue(size_t lane) const noexcept
{
    return lane < automationCurrent.size() ? automationCurrent[lane] : 0.0;
}

void Cs80AudioProcessor::beginHostAutomationBlock(int frameCount) noexcept
{
    endHostAutomationBlock();
    hostAutomationBlock = true;
    hostAutomationFrames = std::max(0, frameCount);
    hostAutomationPosition = 0;
}

bool Cs80AudioProcessor::attachHostAutomationQueue(juce::AudioProcessorParameter& parameter,
                                                  cs80::HostAutomationQueue queue) noexcept
{
    if (!hostAutomationBlock || queue.count <= 0 || queue.readPoint == nullptr) return false;
    for (size_t i = 0; i < automationParameters.size(); ++i)
        if (automationParameters[i] == &parameter)
        {
            automationCurves[i].begin(queue, automationCurrent[i]);
            return true;
        }
    return false;
}

void Cs80AudioProcessor::endHostAutomationBlock() noexcept
{
    if (!hostAutomationBlock) return;
    for (size_t i = 0; i < automationCurves.size(); ++i)
    {
        auto& curve = automationCurves[i];
        if (curve.active())
        {
            automationCurrent[i] = curve.finalValue();
            curve.commit();
            curve = {};
        }
    }
    hostAutomationBlock = false;
}

void Cs80AudioProcessor::prepareHostAutomation() noexcept
{
    if (!hostAutomationBlock) return;
    // Modern host timing is inert in both protected audio paths. Committing
    // before MIDI learn/readParameters restores JUCE's historical final-point
    // at block start order, including host notifications and final APVTS state.
    if (!usesHostAutomationTiming()) endHostAutomationBlock();
}

void Cs80AudioProcessor::syncHostAutomationParameters() noexcept
{
    for (size_t i = 0; i < automationParameters.size(); ++i)
        if (!automationCurves[i].active() && automationParameters[i] != nullptr)
            automationCurrent[i] = automationParameters[i]->getValue();
    if (hostAutomationBlock) applyHostAutomationAt(hostAutomationPosition, false);
}

void Cs80AudioProcessor::applyHostAutomationAt(int position, bool publish) noexcept
{
    if (!hostAutomationBlock || position < 0 || position >= hostAutomationFrames) return;
    hostAutomationPosition = position;
    bool changed = false;
    for (size_t i = 0; i < automationCurves.size(); ++i)
    {
        if (!automationCurves[i].active()) continue;
        const auto normalized = automationCurves[i].valueAt(position);
        automationCurrent[i] = normalized;
        const int parameter = cs80::kSampleAccurateParameters[i];
        // The programmer, including MEMORY, remains the source of effective
        // rank controls. Hidden panel automation may update its stored value
        // but cannot override a selected factory tab or memory bank.
        if (parameter >= cs80::P_ch1Start && parameter < cs80::P_legacyNumParams)
        {
            const int rank = parameter >= cs80::P_ch2Start ? 1 : 0;
            if (cachedHardwareSources[(size_t) rank] != 13) continue;
        }
        const float physical = automationParameters[i]->convertFrom0to1(static_cast<float>(normalized));
        changed = changed || cached[parameter] != physical;
        cached[parameter] = physical;
    }
    if (changed && publish)
    {
        engine.setControlFrame(cs80::ControlFrame::programmer(cached, cachedHardwareSources));
        if (extensionActive) extensionEngine->setControlFrame(cs80::ControlFrame::programmer(cached, cachedHardwareSources));
    }
}

int Cs80AudioProcessor::hostAutomationSpan(int position, int remaining) const noexcept
{
    if (!hostAutomationBlock) return remaining;
    for (const auto& curve : automationCurves)
        if (curve.active()) remaining = curve.samplesUntilChange(position, remaining);
    return remaining;
}
