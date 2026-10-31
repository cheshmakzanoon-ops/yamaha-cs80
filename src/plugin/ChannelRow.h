#pragma once

#include "PanelWidgets.h"

namespace cs80::panel
{
// One programmable synthesis row (owner index 20-45): VCO, VCF, VCA, INITIAL and AFTER groups of one
// channel, plus its engraved channel tag.
class ChannelRow final : public PanelSection
{
public:
    ChannelRow(Cs80AudioProcessor& p, int channelIndex);

    int getChannel() const noexcept { return channel; }
    ChannelTag& getTag() noexcept { return tag; }
    // The design rectangle this row occupies (its etched frame, with a margin for rounding).
    juce::Rectangle<float> designArea() const;
    // Static art drawn by the buffered chassis in design space: the etched frame, group brackets, separators.
    static void paintArt(juce::Graphics& g, int channel);

private:
    int channel;
    ChannelTag tag;
};
} // namespace cs80::panel
