#pragma once

// Handover OE1/OE2 routing contract, preregistered before Engine playback wiring.
// BA617 adjustment p21 + OE57/OE58 name separate controls; independence and
// simultaneous operation remain DECLARED INFERENCES (docs/BBD_CHORUS_MODEL.md).

#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>

namespace cs80::tests
{
template <typename Check>
void testChorusRoutingHost(Check check)
{
    static_assert(P_chorusTremoloRouting == P_numParams - 1,
                  "new OE1/OE2 override must be append-only");
    constexpr auto modes = std::array { "Legacy selector", "Both off", "Chorus only",
                                        "Tremolo only", "Both on" };

    Cs80AudioProcessor host;
    auto* routing = dynamic_cast<juce::AudioParameterChoice*>(
        host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_chorusTremoloRouting)));
    check(routing != nullptr, "independent chorus/tremolo host override is a discrete Choice");
    if (routing == nullptr)
        return;

    const auto& definition = paramDef(P_chorusTremoloRouting);
    check(routing->getVersionHint() >= 16 && host.getParameters().indexOf(routing) == host.getParameters().size() - 1,
          "chorus/tremolo routing is last append-only host parameter with AU hint >=16");
    const juce::AudioProcessorParameter& hostParameter = *routing;
    check(routing->choices.size() == (int) modes.size() && hostParameter.getNumSteps() == (int) modes.size(),
          "chorus/tremolo host Choice expresses neutral, neither, each alone, and both together");
    check(definition.min == 0.0f && definition.max == 4.0f && definition.def == 0.0f
              && routing->getIndex() == 0 && ParamValues {}[P_chorusTremoloRouting] == 0.0f,
          "new routing defaults to neutral legacy selector in core and fresh host");

    for (int mode = 0; mode < (int) modes.size(); ++mode)
    {
        check(routing->choices[mode] == modes[(size_t) mode]
                  && hostParameter.getText(routing->convertTo0to1((float) mode), 48) == modes[(size_t) mode],
              "every explicit routing state has a stable readable host-automation label");
        host.setParamValue(P_chorusTremoloRouting, (float) mode);
        check(host.getParamValue(P_chorusTremoloRouting) == (float) mode,
              "choice accepts every discrete independent routing value");

        juce::MemoryBlock state;
        host.getStateInformation(state);
        Cs80AudioProcessor restored;
        restored.setStateInformation(state.getData(), (int) state.getSize());
        check(restored.getParamValue(P_chorusTremoloRouting) == (float) mode,
              "independent chorus/tremolo switch value survives host session round trip");
    }

    // A pre-extension session contains no new parameter and must not inherit a
    // live opt-in state. Use the production get/setStateInformation code path,
    // which takes the tree lock and syncs its APVTS before replacement.
    juce::MemoryBlock state;
    host.getStateInformation(state);
    auto xml = juce::parseXML(juce::String::fromUTF8(
        static_cast<const char*>(state.getData()), (int) state.getSize()));
    check(xml != nullptr, "host can serialize the routing state as session XML");
    if (xml != nullptr)
    {
        auto old = juce::ValueTree::fromXml(*xml);
        old.removeChild(old.getChildWithProperty("id", Cs80AudioProcessor::parameterId(P_chorusTremoloRouting)), nullptr);
        const auto oldXml = old.createXml()->toString();
        host.setParamValue(P_chorusTremoloRouting, 4.0f);
        host.setStateInformation(oldXml.toRawUTF8(), oldXml.getNumBytesAsUTF8());
        check(host.getParamValue(P_chorusTremoloRouting) == 0.0f,
              "pre-extension host session without the routing parameter restores neutral zero");
    }
}
} // namespace cs80::tests
