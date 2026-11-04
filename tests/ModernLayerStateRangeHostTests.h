#pragma once

#include "plugin/ModernLayerState.h"
#include "plugin/PluginProcessor.h"

#include <array>
#include <limits>
#include <memory>
#include <utility>

namespace cs80::tests
{
template <typename Check>
void testModernLayerStateRangeHost(Check&& check)
{
    // Integer-valued modern state fields must be bounded before converting to
    // int. In particular, an index outside the representable range must not
    // wrap to a real MSEG/slot or turn an invalid enum into an active route.
    using namespace modern;
    const ModernConfig defaults;
    const std::array<juce::var, 21> outside {{
        "2147483648", "-2147483649", "4294967296", "4294967297", "4294967298",
        "2147483647.5", "-2147483648.5",
        "-4294967296", "-4294967295", "-4294967294",
        "9223372036854775808", "-9223372036854775809", "1e100", "-1e100",
        (juce::int64) 4294967297LL, (juce::int64) -4294967295LL,
        4294967298.0, -4294967294.0, std::numeric_limits<double>::max(),
        std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()
    }};
    juce::TemporaryFile fixture("-cs80-modern-state-range");
    const auto root = fixture.getFile();
    check(root.createDirectory().wasOk(), "modern state range fixture directory created");
    if (! root.isDirectory()) return;
    struct Cleanup
    {
        juce::File directory;
        ~Cleanup() { directory.deleteRecursively(); }
    } cleanup { root };
    auto host = std::make_unique<Cs80AudioProcessor>(root.getChildFile("exports"));
    auto roundTrip = std::make_unique<Cs80AudioProcessor>();
    host->prepareToPlay(48000.0, 16);
    juce::AudioBuffer<float> audio(2, 16);
    juce::MidiBuffer midi;
    const auto original = host->getAPVTS().copyState();
    const auto file = root.getChildFile("import.cs80xml");
    auto edited = defaults;
    edited.slots[0] = { ModSource::velocity, ModSource::none, ModDestination::pitch, ModTarget::rankII };
    edited.msegs[0].points[1].level = 0.25f;

    auto verify = [&](const juce::ValueTree& layer, const ModernConfig& expected, const juce::String& context)
    {
        const auto checkCase = [&](bool ok, const char* message)
        {
            const auto description = juce::String(message) + " [" + context + "]";
            check(ok, description.toRawUTF8());
        };
        checkCase(ModernLayerState::fromTree(layer) == expected,
                  "typed modern state bounds integers before narrowing");
        for (bool session : { false, true })
        {
            auto state = original.createCopy();
            state.appendChild(layer.createCopy(), nullptr);
            host->getModernLayer().setConfig(edited);
            if (session)
            {
                const auto text = state.createXml()->toString();
                host->setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
            }
            else
            {
                const bool written = state.createXml()->writeTo(file);
                check(written, "modern state range user-preset fixture written");
                if (! written) return;
                host->loadUserPreset(file);
            }
            checkCase(host->getModernLayer().getConfig() == expected,
                      session ? "host restore rejects overflowing modern integers"
                              : "preset restore rejects overflowing modern integers");
            host->processBlock(audio, midi);
            checkCase(host->getEngine().getModernConfig() == expected,
                      "bounded modern configuration reaches the audio engine");
            juce::MemoryBlock saved;
            host->getStateInformation(saved);
            roundTrip->getModernLayer().setConfig(edited);
            roundTrip->setStateInformation(saved.getData(), (int) saved.getSize());
            checkCase(roundTrip->getModernLayer().getConfig() == expected,
                      "bounded modern state survives serialized session round trip");
        }
    };

    for (const auto& value : outside)
    {
        const auto context = "value=" + value.toString();
        for (const auto* type : { "MSEG", "SLOT" })
        {
            auto layer = ModernLayerState::toTree(defaults);
            juce::ValueTree node(type);
            node.setProperty("index", value, nullptr);
            if (node.hasType("MSEG"))
            {
                node.setProperty("points", "0 1 0;1 0.25 0", nullptr);
                layer.appendChild(node, nullptr);
            }
            else
            {
                node.setProperty("source", (int) ModSource::velocity, nullptr);
                node.setProperty("destination", (int) ModDestination::pitch, nullptr);
                layer.getChildWithName("MATRIX").appendChild(node, nullptr);
            }
            verify(layer, defaults, context + ", " + type + " index");
        }
        for (const auto* property : { "loopStart", "loopEnd", "loop", "trigger", "sync", "bipolar" })
        {
            auto layer = ModernLayerState::toTree(defaults);
            layer.getChild(0).setProperty(property, value, nullptr);
            verify(layer, defaults, context + ", MSEG " + property);
        }
        for (const auto* property : { "source", "via", "destination", "target" })
        {
            auto expected = defaults;
            auto& slot = expected.slots[0];
            slot = { ModSource::velocity, ModSource::expression, ModDestination::pitch, ModTarget::rankII };
            auto layer = ModernLayerState::toTree(expected);
            layer.getChildWithName("MATRIX").getChild(0).setProperty(property, value, nullptr);
            const juce::String field(property);
            if (field == "source") slot.source = ModSource::none;
            else if (field == "via") slot.via = ModSource::none;
            else if (field == "destination") slot.destination = ModDestination::none;
            else slot.target = ModTarget::both;
            verify(layer, expected, context + ", SLOT " + property);
        }
    }

    // Preserve the existing nearest-integer rule, enum sanitation and support
    // for later version markers. Valid representable values are unchanged.
    for (const auto& item : std::array<std::pair<double, int>, 10> {{
             { 0.49, 0 }, { 0.5, 1 }, { 1.49, 1 }, { 1.5, 2 },
             { -0.49, 0 }, { -0.5, -1 }, { 2147483647.0, 2147483647 },
             { -2147483648.0, (-2147483647 - 1) },
             { 2147483647.49, 2147483647 }, { -2147483648.49, (-2147483647 - 1) } }})
    {
        auto layer = ModernLayerState::toTree(defaults);
        layer.setProperty("version", 9, nullptr);
        layer.getChild(0).setProperty("loopStart", item.first, nullptr);
        auto expected = defaults;
        expected.msegs[0].loopStart = item.second;
        expected = sanitizeConfig(expected);
        verify(layer, expected, "representable loopStart=" + juce::String(item.first, 2));
    }
}
} // namespace cs80::tests
