#pragma once
#include "FactoryToneRevisionHostTests.h"
#include <clocale>
#include <string>

namespace cs80::tests
{
template <class Check>
void testFactoryToneRevisionLocale(Check check)
{
    using namespace factory_tone_host_detail;
    struct RestoreNumericLocale
    {
        std::string original = std::setlocale(LC_NUMERIC, nullptr);
        ~RestoreNumericLocale() { std::setlocale(LC_NUMERIC, original.c_str()); }
    } restoreLocale;
    // The existing Linux locale CI already provisions this German fixture.
    // A missing required locale is a failure, never a skipped PASS.
    for (const char* name : {"C", "de_DE.utf8"})
    {
        const char* selected = std::setlocale(LC_NUMERIC, name);
        check(selected != nullptr, "required C/German numeric locale exists");
        if (selected == nullptr) continue;
        const std::string expectedLocale = selected;
        std::printf("Factory tone locale fixture: %s decimal=%s\n", name, std::localeconv()->decimal_point);
        juce::TemporaryFile root("-cs80-tone-revision-locale");
        auto owned = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        auto& host = *owned;
        configure(host, false);
        for (float wanted : {0.0f, 1.0f})
        {
            setChoice(host, wanted);
            const auto saved = session(host);
            check(saved.getChildWithProperty("id", revisionId).getProperty("value").toString()
                      == (wanted == 1 ? "1.0" : "0.0"),
                  "locale fixture uses actual JUCE XML decimal serialization");
            setChoice(host, 1-wanted);
            restore(host, saved);
            check(choice(host) == wanted, "actual session choice round-trips independently of numeric locale");
            host.prepareToPlay(48000, 32);
            engineMatches(host.getEngine(), wanted == 1, check, "locale-restored choice reaches actual source engine");
            const auto preset = host.saveUserPreset(wanted == 1 ? "Locale V2" : "Locale V1");
            check(preset.wasSaved(), "locale fixture saves an isolated user preset");
            setChoice(host, 1-wanted);
            host.loadUserPreset(preset.file);
            check(choice(host) == wanted, "user preset choice round-trips independently of numeric locale");
            check(expectedLocale == std::setlocale(LC_NUMERIC, nullptr),
                  "session/preset decoder never changes host numeric locale");
        }
        setChoice(host, 1);
        const auto savedV2 = session(host);
        for (const char* encoded : {"1", "1.0", "1e0", " 1 ", "+1.000E+0", "0x1p0", "0x1.0p+0"})
        {
            auto literal = savedV2.createCopy();
            literal.getChildWithProperty("id", revisionId).setProperty("value", encoded, nullptr);
            setChoice(host, 0);
            restore(host, literal);
            check(choice(host) == 1, "complete decimal/exponent/hex one retains prior literal semantics");
        }
        for (const char* encoded : {"", "nan", "inf", "-inf", "-1", "2", "0.5", "1oops", "1,0", "1e", "1e9999"})
        {
            auto malformed = savedV2.createCopy();
            malformed.getChildWithProperty("id", revisionId).setProperty("value", encoded, nullptr);
            setChoice(host, 1);
            restore(host, malformed);
            check(choice(host) == 0, "malformed/non-one locale input cannot become an opt-in");
        }
        auto missing = savedV2.createCopy();
        missing.removeChild(missing.getChildWithProperty("id", revisionId), nullptr);
        setChoice(host, 1);
        restore(host, missing);
        check(choice(host) == 0, "missing revision still restores frozen V1 under every locale");

        setChoice(host, 1);
        host.abCopyCurrentTo(1);
        setChoice(host, 0);
        const auto savedAb = session(host);
        restore(host, savedAb);
        host.abToggle();
        check(choice(host) == 1, "inactive A/B V2 survives actual decimal serialization in every locale");
        check(expectedLocale == std::setlocale(LC_NUMERIC, nullptr),
              "A/B and literal decoding leave the caller numeric locale unchanged");
    }
}
} // namespace cs80::tests
