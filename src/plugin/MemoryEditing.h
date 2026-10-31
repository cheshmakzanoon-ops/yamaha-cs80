#pragma once

// Memory-lid editing: store a rank's panel into a memory, copy one memory to another, copy one rank's panel row
// to the other, and load a memory into a rank's panel. Each operates on the 26 programmer controls
// (Cs80AudioProcessor::isMemoryControl) through the APVTS host parameters, one balanced gesture per parameter
// that actually changes, so hosts record it as automation and an undo manager can revert it.
//
// None of this exists on the original: the four memories are miniature panels set by hand. These helpers only
// produce settings an owner could reach by moving those sliders, so they write no non-original state and are
// available in every engine mode (PURE CS-80 included). Message thread only.

#include "PluginProcessor.h"

namespace cs80::memory_editing
{
// Memory banks 0..3 are MEMORY 1..4. Banks 0 and 2 belong to rank I, 1 and 3 to rank II.
inline constexpr int rankOfBank(int bank) noexcept { return bank % 2; }

namespace detail
{
inline bool validBank(int bank) noexcept { return bank >= 0 && bank < 4; }
inline bool validChannel(int channel) noexcept { return channel == 0 || channel == 1; }

// Writes one host parameter as a complete gesture, only when the value changes.
inline void write(Cs80AudioProcessor& processor, const juce::String& id, float physicalValue)
{
    auto* parameter = processor.getAPVTS().getParameter(id);
    if (parameter == nullptr || ! std::isfinite(physicalValue))
        return;
    const float normalised = parameter->convertTo0to1(physicalValue);
    if (std::abs(parameter->getValue() - normalised) <= 1.0e-7f)
        return;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(normalised);
    parameter->endChangeGesture();
}

inline juce::String panelId(int channel, int local)
{
    return Cs80AudioProcessor::parameterId(cs80::chParam(channel, local));
}
} // namespace detail

// Rank `channel`'s main-panel row into memory `bank`. Returns false (and changes nothing) for invalid arguments.
inline bool storePanelToMemory(Cs80AudioProcessor& processor, int channel, int bank)
{
    if (! detail::validChannel(channel) || ! detail::validBank(bank))
        return false;
    for (int local = 0; local < cs80::CP_count; ++local)
        if (Cs80AudioProcessor::isMemoryControl(local))
            detail::write(processor, Cs80AudioProcessor::memoryParameterId(bank, local),
                          processor.getParamValue(cs80::chParam(channel, local)));
    return true;
}

// Memory `fromBank` into memory `toBank` (any pair of distinct banks).
inline bool copyMemory(Cs80AudioProcessor& processor, int fromBank, int toBank)
{
    if (! detail::validBank(fromBank) || ! detail::validBank(toBank) || fromBank == toBank)
        return false;
    for (int local = 0; local < cs80::CP_count; ++local)
        if (Cs80AudioProcessor::isMemoryControl(local))
            detail::write(processor, Cs80AudioProcessor::memoryParameterId(toBank, local),
                          processor.getMemoryValue(fromBank, local));
    return true;
}

// Rank `fromChannel`'s main-panel row onto the other rank's row. FEET, pan and the filter on/off extensions are
// not part of the row and stay as they are.
inline bool copyRank(Cs80AudioProcessor& processor, int fromChannel, int toChannel)
{
    if (! detail::validChannel(fromChannel) || ! detail::validChannel(toChannel) || fromChannel == toChannel)
        return false;
    for (int local = 0; local < cs80::CP_count; ++local)
        if (Cs80AudioProcessor::isMemoryControl(local))
            detail::write(processor, detail::panelId(toChannel, local),
                          processor.getParamValue(cs80::chParam(fromChannel, local)));
    return true;
}

// Memory `bank` into rank `channel`'s main-panel row.
inline bool recallMemoryToPanel(Cs80AudioProcessor& processor, int bank, int channel)
{
    if (! detail::validChannel(channel) || ! detail::validBank(bank))
        return false;
    for (int local = 0; local < cs80::CP_count; ++local)
        if (Cs80AudioProcessor::isMemoryControl(local))
            detail::write(processor, detail::panelId(channel, local), processor.getMemoryValue(bank, local));
    return true;
}
} // namespace cs80::memory_editing
