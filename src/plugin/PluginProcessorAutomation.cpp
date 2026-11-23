#include "PluginProcessor.h"

void Cs80AudioProcessor::resetHostAutomationState() noexcept
{
    hostAutomationBlock = false;
    hostAutomationVst3 = false;
    vst3CurveRecallInProcess = false;
    hostAutomationFrames = hostAutomationPosition = 0;
    for (size_t i = 0; i < automationParameters.size(); ++i)
    {
        automationParameters[i] = apvts.getParameter(parameterId(cs80::kSampleAccurateParameters[i]));
        automationParameterReadback[i] = automationParameters[i] != nullptr ? automationParameters[i]->getValue() : 0.0f;
        automationCurrent[i] = automationParameterReadback[i];
        automationCurves[i] = {};
        markHostAutomationLane(i, false);
    }
}

void Cs80AudioProcessor::refreshHostAutomationParameter(size_t lane) noexcept
{
    if (automationParameters[lane] == nullptr) return;
    const float current = automationParameters[lane]->getValue();
    // A native zero-frame flush, UI edit or state restore can supersede the
    // preceding curve between blocks. Compare with the readback associated
    // with our own last publication, rather than with its double endpoint:
    // JUCE's physical Float32 storage need not invert to that endpoint exactly.
    if (current != automationParameterReadback[lane])
        automationCurrent[lane] = current;
    automationParameterReadback[lane] = current;
}

void Cs80AudioProcessor::refreshAllHostAutomationParameters() noexcept
{
    for (size_t i = 0; i < automationParameters.size(); ++i)
        refreshHostAutomationParameter(i);
}

bool Cs80AudioProcessor::usesHostAutomationTiming() const noexcept
{
    // There is one production engine; all of its attached curves keep their
    // sample positions regardless of obsolete saved mode fields.
    return true;
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
    // Resolve the implicit sample -1 value before any wrapper attaches a queue.
    refreshAllHostAutomationParameters();
    hostAutomationBlock = true;
    hostAutomationVst3 = false;
    hostAutomationFrames = std::max(0, frameCount);
    hostAutomationPosition = 0;
}

void Cs80AudioProcessor::beginVst3HostAutomationBlock(int frameCount) noexcept
{
    beginHostAutomationBlock(frameCount);
    hostAutomationVst3 = true;
}

bool Cs80AudioProcessor::attachHostAutomationQueue(juce::AudioProcessorParameter& parameter,
                                                  cs80::HostAutomationQueue queue) noexcept
{
    if (!hostAutomationBlock || queue.count <= 0 || queue.readPoint == nullptr) return false;
    // Hosts can submit queues in any order. A contiguous prefix often
    // predicts the next parameter; trust this hint only after checking its
    // exact object identity. Duplicates, gaps and foreign parameters retain
    // the full lookup and the same borrowed queue callbacks.
    auto lane = size_t{0};
    if (automationParameters[0] != &parameter) lane = static_cast<size_t>(activeHostCurveCount);
    if (lane >= automationParameters.size() || automationParameters[lane] != &parameter)
    {
        const auto found = std::find(automationParameters.begin() + 1, automationParameters.end(), &parameter);
        if (found == automationParameters.end()) return false;
        lane = static_cast<size_t>(found - automationParameters.begin());
    }
    markHostAutomationLane(lane, true); // begin publishes the queue before calling readPoint.
    automationCurves[lane].begin(queue, automationCurrent[lane]);
    return true;
}

void Cs80AudioProcessor::endHostAutomationBlock() noexcept
{
    if (!hostAutomationBlock) return;
    // The live ascending traversal rereads the mask after each borrowed
    // callback, matching the original scan if a later lane changes.
    visitActiveHostAutomationLanes([&](size_t i)
    {
        auto& curve = automationCurves[i];
        automationCurrent[i] = curve.finalValue();
        curve.commitFinalValue(automationCurrent[i]);
        if (automationParameters[i] != nullptr)
            automationParameterReadback[i] = automationParameters[i]->getValue();
        curve = {};
        markHostAutomationLane(i, false);
    });
    hostAutomationBlock = false;
    hostAutomationVst3 = false;
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
    // With no borrowed queues every lane is eligible. These are the processor's
    // own JUCE parameter readbacks (no borrowed host callback or attachment).
    // Keep all normalized reads in order, but don't touch 120 inactive curves.
    if (activeHostCurveCount == 0)
        refreshAllHostAutomationParameters();
    else
        for (size_t i = 0; i < automationParameters.size(); ++i)
            if (!automationCurves[i].active()) refreshHostAutomationParameter(i);
    if (hostAutomationBlock) applyHostAutomationAt(hostAutomationPosition, false);
}

void Cs80AudioProcessor::applyHostAutomationAt(int position, bool publish) noexcept
{
    if (!hostAutomationBlock || position < 0 || position >= hostAutomationFrames) return;
    hostAutomationPosition = position;
    bool changed = false;
    visitActiveHostAutomationLanes([&](size_t i)
    {
        const auto normalized = automationCurves[i].valueAt(position);
        automationCurrent[i] = normalized;
        const int parameter = cs80::kSampleAccurateParameters[i];
        // The programmer, including MEMORY, remains the source of effective
        // rank controls. Hidden panel automation may update its stored value
        // but cannot override a selected factory tab or memory bank. Rank pan
        // is an independent software control, as in readParameters().
        if (parameter >= cs80::P_ch1Start && parameter < cs80::P_legacyNumParams)
        {
            const int rank = parameter >= cs80::P_ch2Start ? 1 : 0;
            if (cachedFrame.hardwareSources[(size_t) rank] != 13
                && parameter != cs80::chParam(rank, cs80::CP_pan)) return;
        }
        const float physical = automationParameters[i]->convertFrom0to1(static_cast<float>(normalized));
        changed = changed || cachedFrame.effectiveValues[parameter] != physical;
        cachedFrame.effectiveValues[parameter] = physical;
    });
    if (changed && publish)
    {
        engine.setControlFrame(cachedFrame);
        if (extensionActive) extensionEngine->setControlFrame(cachedFrame);
    }
}

int Cs80AudioProcessor::hostAutomationSpan(int position, int remaining) const noexcept
{
    if (!hostAutomationBlock) return remaining;
    visitActiveHostAutomationLanes([&](size_t i) {
        remaining = automationCurves[i].samplesUntilChange(position, remaining);
    });
    return remaining;
}
