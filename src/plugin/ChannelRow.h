#pragma once

#include "PanelWidgets.h"

namespace cs80::panel
{
// One programmable synthesis row (owner index 20-45): VCO, VCF, VCA, INITIAL and AFTER groups of one
// channel, plus its engraved channel numeral.
class ChannelRow final : public PanelSection
{
public:
    ChannelRow(Cs80AudioProcessor& p, int channelIndex);

    int getChannel() const noexcept { return channel; }
    ChannelTag& getTag() noexcept { return tag; }
    // Lights the numeral's pilot while this row is heard, i.e. while its tone selector is on PANEL.
    void refresh();
    // The design rectangle this row occupies (its etched frame, with a margin for rounding).
    juce::Rectangle<float> designArea() const;
    // Static art drawn by the buffered chassis in design space: the etched frame, group brackets, separators.
    void paintArt(juce::Graphics& g) const override;

private:
    int channel;
    std::vector<float> separators;
    ChannelTag tag;
};
} // namespace cs80::panel
