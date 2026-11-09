// Exercise the production browser XML reader without a processor, APVTS,
// MessageManager, timer, GUI or file-system side effects.
#include "plugin/PresetFileValues.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

int main()
{
    using namespace cs80;
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const char* message)
    {
        ++checks;
        if (!ok)
        {
            if (failures < 20) std::fprintf(stderr, "FAIL: %s\n", message);
            ++failures;
        }
    };
    ParamValues values;
    std::vector<char> used;
    const auto read = [&](const juce::XmlElement* xml)
    {
        // Preview and edited-marker readers must agree, including repeat calls.
        ParamValues preview;
        ui::readPresetFileValues(xml, preview);
        ui::readPresetFileValues(xml, values, &used);
        check(preview.v == values.v, "preview and edit-reference values agree");
        check(used.size() == (size_t) P_numParams, "used mask has every parameter");
    };
    const auto defaults = [&]
    {
        for (int i = 0; i < P_numParams; ++i)
            {
                if (values[i] != paramDef(i).def || used[(size_t) i] != 0)
                    std::fprintf(stderr, "default mismatch id=%s value=%g def=%g used=%d\n", paramId(i).c_str(), values[i], paramDef(i).def, used[(size_t) i]);
                check(values[i] == paramDef(i).def && used[(size_t) i] == 0,
                      "absent or rejected document clears stale values and usage");
            }
    };
    const auto child = [](juce::XmlElement& xml, int index, const juce::String& value)
    {
        auto* p = xml.createNewChildElement("PARAM");
        p->setAttribute("id", paramId(index));
        p->setAttribute("value", value);
        return p;
    };

    // Reproduce NaN propagation and the infinity/default mismatch for every
    // declared parameter, not only controls used by one current thumbnail.
    for (const char* token : { "nan", "NaN", "-nan", "+NaN", "inf", "-inf",
                               "Infinity", "-Infinity", "1e999", "-1e999",
                               "1e40", "-1e40" })
    {
        juce::XmlElement xml("CS80_PARAMS");
        for (int i = 0; i < P_numParams; ++i)
            child(xml, i, token);
        read(&xml);
        for (int i = 0; i < P_numParams; ++i)
            check(std::isfinite(values[i]) && values[i] == (i == P_subOriginalMode ? 0.0f : paramDef(i).def)
                      && used[(size_t) i] == 1,
                  "non-finite imported value uses finite preview fallback");
    }

    // Preserve finite parsing and range clamping, including exponent notation.
    // XML round trips must not alter valid values or parameter identities.
    for (double fraction : { -2.0, 0.0, 0.123456, 0.5, 1.0, 3.0 })
    {
        juce::XmlElement xml("CS80_PARAMS");
        std::array<float, P_numParams> expected {};
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& def = paramDef(i);
            const double value = def.min + fraction * (double(def.max) - def.min);
            auto* p = child(xml, i, {});
            p->setAttribute("value", value);
            expected[(size_t) i] = clampValue((float) p->getDoubleAttribute("value"), def.min, def.max);
        }
        const auto parsed = juce::XmlDocument::parse(xml.toString());
        check(parsed != nullptr, "valid parameter document round trips as XML");
        read(parsed.get());
        for (int i = 0; i < P_numParams; ++i)
            check(values[i] == expected[(size_t) i] && used[(size_t) i] == 1,
                  "finite preview value retains existing clamping semantics");
    }

    for (const char* root : { "WRONG_ROOT", "cs80_params", "PARAM" })
    {
        juce::XmlElement xml(root);
        child(xml, P_volume, "0.9");
        read(&xml);
        defaults();
    }
    const auto malformed = juce::XmlDocument::parse(juce::String("<CS80_PARAMS>"));
    read(malformed.get());
    defaults();
    read(nullptr);
    defaults();

    juce::XmlElement duplicate("CS80_PARAMS");
    child(duplicate, P_volume, "0.9");
    child(duplicate, P_volume, "nan");
    read(&duplicate);
    check(values[P_volume] == paramDef(P_volume).def && used[(size_t) P_volume],
          "final non-finite duplicate overrides earlier valid value with default");
    auto* missing = child(duplicate, P_volume, {});
    missing->removeAttribute("value");
    read(&duplicate);
    check(values[P_volume] == paramDef(P_volume).def && used[(size_t) P_volume],
          "final missing-value duplicate overrides earlier value with default");
    child(duplicate, P_volume, "  2.5e-1  ");
    read(&duplicate);
    check(values[P_volume] == 0.25f && used[(size_t) P_volume],
          "final valid duplicate wins after malformed earlier values");
    // Restore recognizes known IDs regardless of the direct child's tag.
    auto* future = duplicate.createNewChildElement("FUTURE_PARAMETER");
    future->setAttribute("id", paramId(P_volume));
    future->setAttribute("value", 0.375);
    child(*future, P_volume, "0.8");
    read(&duplicate);
    check(values[P_volume] == 0.375f, "known direct ID accepted, nested values ignored");
    child(duplicate, P_subOriginalMode, {})->removeAttribute("value");
    read(&duplicate);
    check(values[P_subOriginalMode] == 0.0f && used[(size_t) P_subOriginalMode],
          "missing Original SUB value uses restored off fallback, not fresh default");

    juce::XmlElement unknown("CS80_PARAMS");
    auto* p = child(unknown, P_volume, "0.8");
    p->setAttribute("id", "future_unknown_parameter");
    child(unknown, P_volume, "0.8")->removeAttribute("id");
    child(*unknown.createNewChildElement("GROUP"), P_volume, "0.8");
    read(&unknown);
    defaults();

    std::printf("Preset file values: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
