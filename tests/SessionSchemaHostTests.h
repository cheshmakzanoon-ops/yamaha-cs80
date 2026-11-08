#pragma once

#include "plugin/PluginProcessor.h"

#include <array>
#include <memory>

namespace cs80::tests
{
template <typename Check>
void testSessionSchemaHost(Check&& check)
{
    // Original SUB mode is an opt-in for saved patches, independent of the
    // fresh-instance default. A malformed XML number must not select it just
    // because its prefix, or its rounded float representation, happens to be 1.
    juce::TemporaryFile fixture("-cs80-session-schema");
    const auto root = fixture.getFile();
    check(root.createDirectory().wasOk(), "session-schema fixture directory created");
    if (!root.isDirectory()) return;
    auto host = std::make_unique<Cs80AudioProcessor>(root.getChildFile("exports"));
    auto roundTrip = std::make_unique<Cs80AudioProcessor>();
    const auto original = host->getAPVTS().copyState();
    const auto file = root.getChildFile("import.cs80xml");
    struct Case { const char* value; float expected; };
    const std::array<Case, 29> cases {{
        { "0", 0.0f }, { "1", 1.0f }, { "0.0", 0.0f }, { "1.0", 1.0f },
        { " 1 ", 1.0f }, { "+1", 1.0f }, { "1e0", 1.0f }, { "1e-0", 1.0f },
        { "1oops", 0.0f }, { "1.0suffix", 0.0f }, { "1f", 0.0f }, { "1 0", 0.0f },
        { "1.0e", 0.0f }, { "1e+", 0.0f },
        { "0.99999999", 0.0f }, { "1.00000001", 0.0f },
        { "0.5", 0.0f }, { "2", 0.0f }, { "-1", 0.0f }, { "nan", 0.0f },
        { "inf", 0.0f }, { "-inf", 0.0f }, { "1e999", 0.0f },
        { "true", 0.0f }, { "garbage", 0.0f }, { "", 0.0f }, { "  ", 0.0f },
        { nullptr, 0.0f }, // Present PARAM with no value property.
        { "missing-child", 0.0f }
    }};
    for (int schema : { 0, 1, 2, Cs80AudioProcessor::stateSchemaVersion })
        for (bool session : { false, true })
            for (float live : { 0.0f, 1.0f })
                for (const auto& item : cases)
                {
                    const auto context = juce::String(" [schema=") + juce::String(schema)
                        + (session ? ", session" : ", preset") + ", live=" + juce::String(live)
                        + ", value='" + (item.value != nullptr ? item.value : "missing-value") + "']";
                    const auto checkCase = [&](bool ok, const char* message)
                    {
                        const auto description = juce::String(message) + context;
                        check(ok, description.toRawUTF8());
                    };
                    auto state = original.createCopy();
                    if (schema != 0)
                        state.setProperty(Cs80AudioProcessor::stateSchemaKey, schema, nullptr);
                    // Legacy files need not carry the diagnostic SUB marker;
                    // valid explicit mode values still retain their meaning.
                    if (schema >= 2) state.setProperty("subOscStateVersion", 1, nullptr);
                    auto mode = state.getChildWithProperty("id", "sub_original_mode");
                    if (item.value == nullptr) mode.removeProperty("value", nullptr);
                    else if (juce::String(item.value) == "missing-child") state.removeChild(mode, nullptr);
                    else mode.setProperty("value", item.value, nullptr);
                    host->setParamValue(P_subOriginalMode, live);
                    if (session)
                    {
                        const auto text = state.createXml()->toString();
                        host->setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
                    }
                    else
                    {
                        const bool written = state.createXml()->writeTo(file);
                        check(written, "session-schema user-preset fixture written");
                        if (!written) return;
                        host->loadUserPreset(file);
                    }
                    checkCase(host->getParamValue(P_subOriginalMode) == item.expected,
                          "session/preset SUB mode requires complete numeric 0/1, otherwise legacy off");
                    const auto* parameter = host->getAPVTS().getParameter("sub_original_mode");
                    checkCase(parameter != nullptr && parameter->getValue() == item.expected,
                          "restored SUB mode agrees between host-facing value and APVTS");
                    juce::MemoryBlock saved;
                    host->getStateInformation(saved);
                    roundTrip->setParamValue(P_subOriginalMode, 1.0f - item.expected);
                    roundTrip->setStateInformation(saved.getData(), (int) saved.getSize());
                    checkCase(roundTrip->getParamValue(P_subOriginalMode) == item.expected,
                          "normalized SUB policy survives a real serialized session round trip");
                }

    // Keep the final-duplicate authority established by parameter migration:
    // an earlier valid value must not shield a malformed later SUB value.
    for (bool session : { false, true })
    {
        auto state = original.createCopy();
        state.getChildWithProperty("id", "sub_original_mode").setProperty("value", 1, nullptr);
        juce::ValueTree finalMode("PARAM");
        finalMode.setProperty("id", "sub_original_mode", nullptr);
        finalMode.setProperty("value", "1oops", nullptr);
        state.appendChild(finalMode, nullptr);
        host->setParamValue(P_subOriginalMode, 1.0f);
        if (session)
        {
            const auto text = state.createXml()->toString();
            host->setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
        }
        else
        {
            const bool written = state.createXml()->writeTo(file);
            check(written, "duplicate SUB migration fixture written");
            if (!written) return;
            host->loadUserPreset(file);
        }
        check(host->getParamValue(P_subOriginalMode) == 0.0f,
              "malformed final SUB value restores legacy despite an earlier valid duplicate");
    }
}
} // namespace cs80::tests
