#pragma once

#include "FactoryToneRevisionHostTests.h"
#include <clocale>
#include <string>

namespace cs80::tests
{
template <class Check>
void testRenderPolicyLocale(Check check)
{
    using namespace factory_tone_host_detail;
    struct RestoreNumericLocale
    {
        std::string original = std::setlocale(LC_NUMERIC, nullptr);
        ~RestoreNumericLocale() { std::setlocale(LC_NUMERIC, original.c_str()); }
    } restoreLocale;
    constexpr const char* engineId = "source_performance";
    constexpr const char* pureId = "parity_mode";
    constexpr const char* schemaId = "stateSchemaVersion";
    const auto expectedPolicy = [](float engine, float pure)
    {
        // Independent explicit truth table, not the production policy helper.
        return pure == 1.0f ? RenderPolicy::isolation
             : engine == 1.0f ? RenderPolicy::sourcePerformance : RenderPolicy::legacy;
    };
    for (const char* name : {"C", "de_DE.utf8"})
    {
        const char* selected = std::setlocale(LC_NUMERIC, name);
        check(selected != nullptr, "required C/German policy numeric locale exists");
        if (selected == nullptr) continue; // failed above; never a skipped PASS
        const std::string expectedLocale = selected;
        std::printf("Render policy locale fixture: %s decimal=%s\n", name, std::localeconv()->decimal_point);
        juce::TemporaryFile root("-cs80-render-policy-locale");
        auto owned = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        auto& host = *owned;
        configure(host, false);
        host.setParamValue(P_detune, 12.5f);
        host.prepareToPlay(48000, 32);

        const auto verifyRestore = [&](const juce::ValueTree& state, float wantedEngine, float wantedPure)
        {
            host.setParamValue(P_sourcePerformance, 1-wantedEngine);
            host.setParamValue(P_parityMode, 1-wantedPure);
            block(host);
            const auto previousPolicy = host.getEngine().getRenderPolicy();
            restore(host, state); // serialize XML, then use actual public restore
            check(host.getEngine().getRenderPolicy() == previousPolicy,
                  "policy state restore never mutates Engine before its audio transaction");
            check(host.getParamValue(P_sourcePerformance) == wantedEngine,
                  "session ENGINE preserves complete literals and schema fallback across locales");
            check(host.getParamValue(P_parityMode) == wantedPure,
                  "session PURE preserves complete literals and off fallback across locales");
            check(host.getParamValue(P_detune) == 12.5f,
                  "policy decoder does not change unrelated decimal sound parameters");
            block(host);
            check(host.getEngine().getRenderPolicy() == expectedPolicy(wantedEngine, wantedPure),
                  "restored policy reaches the actual Engine at the next audio transaction");
            check(expectedLocale == std::setlocale(LC_NUMERIC, nullptr),
                  "policy restore leaves the caller numeric locale unchanged");
        };

        for (int schema : {0, 1, 2, 3}) // 0 represents an absent schema marker
        {
            const auto withSchema = [&](juce::ValueTree state)
            {
                if (schema == 0) state.removeProperty(schemaId, nullptr);
                else state.setProperty(schemaId, schema, nullptr);
                return state;
            };
            for (float engine : {0.0f, 1.0f})
                for (float pure : {0.0f, 1.0f})
                {
                    host.setParamValue(P_sourcePerformance, engine);
                    host.setParamValue(P_parityMode, pure);
                    auto saved = withSchema(session(host));
                    check(saved.getChildWithProperty("id", engineId).getProperty("value").toString()
                              == (engine == 1 ? "1.0" : "0.0"),
                          "ENGINE fixture is actual JUCE XML decimal serialization");
                    check(saved.getChildWithProperty("id", pureId).getProperty("value").toString()
                              == (pure == 1 ? "1.0" : "0.0"),
                          "PURE fixture is actual JUCE XML decimal serialization");
                    verifyRestore(saved, engine, pure);
                }
            auto base = withSchema(session(host));
            struct Literal { const char* text; float value; };
            for (const Literal literal : {Literal{"0", 0}, {"0.0", 0}, {"0e0", 0}, {" -0.0 ", 0},
                                         {"+0.000E+0", 0}, {"0x0p0", 0}, {"0x0.0p+0", 0},
                                         {"1", 1}, {"1.0", 1}, {"1e0", 1}, {" 1 ", 1},
                                         {"+1.000E+0", 1}, {"0x1p0", 1}, {"0x1.0p+0", 1}})
            {
                auto state = base.createCopy();
                for (const char* id : {engineId, pureId})
                    state.getChildWithProperty("id", id).setProperty("value", literal.text, nullptr);
                verifyRestore(state, literal.value, literal.value);
            }
            for (const char* id : {engineId, pureId})
            {
                const float engineFallback = schema >= 3 ? 1.0f : 0.0f;
                // Keep the other policy field valid and independent of fallback.
                auto independent = base.createCopy();
                independent.getChildWithProperty("id", engineId).setProperty("value", "0", nullptr);
                independent.getChildWithProperty("id", pureId).setProperty("value", "0", nullptr);
                const float expectedEngine = juce::String(id) == engineId ? engineFallback : 0.0f;
                for (const char* malformed : {"", "nan", "inf", "-inf", "-1", "2", "0.5", "1oops", "1,0", "1e", "1e9999"})
                {
                    auto state = independent.createCopy();
                    state.getChildWithProperty("id", id).setProperty("value", malformed, nullptr);
                    verifyRestore(state, expectedEngine, 0);
                }
                auto missingValue = independent.createCopy();
                missingValue.getChildWithProperty("id", id).removeProperty("value", nullptr);
                verifyRestore(missingValue, expectedEngine, 0);
                auto missingChild = independent.createCopy();
                missingChild.removeChild(missingChild.getChildWithProperty("id", id), nullptr);
                verifyRestore(missingChild, expectedEngine, 0);
            }
        }

        // ENGINE belongs to a sound; PURE is a live session-isolation setting.
        // Test both saved PURE values against both opposite/current live values.
        for (float savedEngine : {0.0f, 1.0f})
            for (float savedPure : {0.0f, 1.0f})
            {
                host.setParamValue(P_sourcePerformance, savedEngine);
                host.setParamValue(P_parityMode, savedPure);
                const auto saved = host.saveUserPreset("Policy " + juce::String(savedEngine) + " " + juce::String(savedPure));
                check(saved.wasSaved(), "policy fixture writes a real isolated user preset");
                if (!saved.wasSaved()) continue;
                const auto xml = juce::XmlDocument::parse(saved.file);
                check(xml != nullptr && juce::ValueTree::fromXml(*xml).getChildWithProperty("id", engineId)
                          .getProperty("value").toString() == (savedEngine == 1 ? "1.0" : "0.0"),
                      "user preset ENGINE uses actual JUCE decimal serialization");
                for (float livePure : {0.0f, 1.0f})
                {
                    host.setParamValue(P_sourcePerformance, 1-savedEngine);
                    host.setParamValue(P_parityMode, livePure);
                    block(host);
                    const auto previousPolicy = host.getEngine().getRenderPolicy();
                    host.loadUserPreset(saved.file);
                    check(host.getEngine().getRenderPolicy() == previousPolicy,
                          "user preset restore never mutates Engine before its audio transaction");
                    check(host.getParamValue(P_sourcePerformance) == savedEngine,
                          "user preset recalls its ENGINE independently of host numeric locale");
                    check(host.getParamValue(P_parityMode) == livePure,
                          "user preset preserves live PURE rather than saved sound policy");
                    check(host.getParamValue(P_detune) == 12.5f,
                          "user preset policy parsing preserves unrelated decimal sound values");
                    block(host);
                    check(host.getEngine().getRenderPolicy() == expectedPolicy(savedEngine, livePure),
                          "user preset reaches exact recalled ENGINE and live PURE policy");
                    check(expectedLocale == std::setlocale(LC_NUMERIC, nullptr),
                          "user preset policy parser leaves caller numeric locale unchanged");
                }
            }
    }
}
} // namespace cs80::tests
