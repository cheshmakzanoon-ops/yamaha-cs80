#pragma once

#include <juce_core/juce_core.h>

#include <cstdio>
#include <cstring>

#if JUCE_LINUX && ! JUCE_ANDROID
#include "plugin/PluginProcessor.h"

#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <clocale>
#include <cstdlib>
#include <locale.h>
#include <memory>
#include <string>

namespace cs80::tests::locale_detail
{
struct Category
{
    int value;
    const char* name;
};

// Constant initialization only: never query JUCE or the locale before main.
inline constexpr std::array<Category, 7> categories {{
    { LC_ALL, "LC_ALL" }, { LC_COLLATE, "LC_COLLATE" },
    { LC_CTYPE, "LC_CTYPE" }, { LC_MONETARY, "LC_MONETARY" },
    { LC_NUMERIC, "LC_NUMERIC" }, { LC_TIME, "LC_TIME" },
    { LC_MESSAGES, "LC_MESSAGES" }
}};

struct Snapshot
{
    std::array<std::string, categories.size()> values;
    bool valid = true;

    static Snapshot capture()
    {
        Snapshot result;
        for (std::size_t i = 0; i < categories.size(); ++i)
        {
            // Copy immediately: later setlocale calls can invalidate its pointer.
            const auto* value = std::setlocale(categories[i].value, nullptr);
            result.valid = result.valid && value != nullptr;
            result.values[i] = value != nullptr ? value : "<unavailable>";
        }
        return result;
    }
};

struct Contract
{
    Snapshot expected;
    unsigned failures = 0;
    const char* firstChangedStage = nullptr;

    void check(const char* stage)
    {
        const auto actual = Snapshot::capture();
        bool preserved = expected.valid && actual.valid;
        for (std::size_t i = 0; i < categories.size(); ++i)
        {
            if (expected.values[i] != actual.values[i])
            {
                preserved = false;
                std::fprintf(stderr, "LOCALE CHANGE [%s] %s: before=[%s] after=[%s]\n",
                             stage, categories[i].name, expected.values[i].c_str(),
                             actual.values[i].c_str());
            }
        }
        if (! preserved)
        {
            ++failures;
            if (firstChangedStage == nullptr)
                firstChangedStage = stage;
        }
        std::printf("LOCALE STAGE [%s]: %s\n", stage, preserved ? "preserved" : "FAILED");
        std::fflush(stdout);
    }

    void require(bool condition, const char* message)
    {
        if (! condition)
        {
            ++failures;
            std::fprintf(stderr, "LOCALE FIXTURE FAILURE: %s\n", message);
        }
    }
};

inline void runQueries(Contract& contract)
{
    (void) juce::SystemStats::getUserLanguage();
    contract.check("getUserLanguage");
    (void) juce::SystemStats::getUserRegion();
    contract.check("getUserRegion");
    (void) juce::SystemStats::getDisplayLanguage();
    contract.check("getDisplayLanguage");
}

inline void runEditor(Contract& contract)
{
    {
        juce::ScopedJuceInitialiser_GUI gui;
        contract.check("GUI initialization");
        {
            Cs80AudioProcessor processor;
            contract.check("processor construction");
            processor.prepareToPlay(48000.0, 512);
            contract.check("processor prepareToPlay");
            {
                std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
                contract.require(editor != nullptr, "createEditor returned null");
                contract.check("editor construction");
                if (editor != nullptr)
                {
                    editor->setSize(1120, 700);
                    contract.check("editor layout");
                    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
                    contract.require(image.isValid() && image.getWidth() > 0 && image.getHeight() > 0,
                                     "editor snapshot is empty");
                    contract.check("editor text rendering");
                }
            }
            contract.check("editor destruction");
            processor.releaseResources();
            contract.check("processor releaseResources");
        }
        contract.check("processor destruction");
    }
    contract.check("GUI shutdown");
}
} // namespace cs80::tests::locale_detail
#endif

namespace cs80::tests
{
// Dispatch from main BEFORE ScopedJuceInitialiser_GUI and all ordinary smoke tests.
// Each invocation is one fresh process; modes never restore a changed locale.
inline int runLocaleSmoke(int argc, char** argv)
{
#if JUCE_LINUX && ! JUCE_ANDROID
    using namespace locale_detail;
    const auto processStart = Snapshot::capture();

    if (argc < 3 || argc > 4
        || (std::strcmp(argv[2], "query") != 0 && std::strcmp(argv[2], "editor") != 0)
        || (argc == 4 && std::strcmp(argv[3], "mixed") != 0))
    {
        std::fprintf(stderr, "Usage: CS80Snapshots --locale-smoke query|editor [mixed]\n");
        return 2;
    }

    // C/C++ starts in the C locale, regardless of LANG/LC_ALL environment values.
    // If initialization has already changed it, fail instead of hiding that fact.
    bool pristine = processStart.valid;
    for (const auto& value : processStart.values)
        pristine = pristine && value == "C";
    if (! pristine)
    {
        std::fprintf(stderr, "LOCALE FIXTURE FAILURE: process was not pristine C before JUCE (LC_ALL=[%s])\n",
                     processStart.values[0].c_str());
        return 2;
    }

    const auto* environmentLocale = std::getenv("LC_ALL");
    if (environmentLocale == nullptr || std::strcmp(environmentLocale, "C.UTF-8") != 0)
    {
        std::fprintf(stderr, "LOCALE FIXTURE FAILURE: this fresh-process test requires LC_ALL=C.UTF-8\n");
        return 2;
    }
    // Verify fixture availability using a private locale object, not a global switch.
    const auto available = newlocale(LC_ALL_MASK, "C.UTF-8", nullptr);
    if (available == nullptr)
    {
        std::fprintf(stderr, "LOCALE FIXTURE FAILURE: C.UTF-8 is unavailable\n");
        return 2;
    }
    freelocale(available);

    const bool mixed = argc == 4;
    if (mixed && std::setlocale(LC_CTYPE, "C.UTF-8") == nullptr)
    {
        std::fprintf(stderr, "LOCALE FIXTURE FAILURE: cannot construct mixed-category locale\n");
        return 2;
    }
    const auto baseline = Snapshot::capture();
    Contract contract { baseline };
    if (mixed)
    {
        contract.require(baseline.values[2] != "C" && baseline.values[4] == "C",
                         "mixed fixture must keep LC_NUMERIC=C while changing LC_CTYPE");
        contract.require(baseline.values[0] != processStart.values[0],
                         "mixed fixture must differ from the pristine process locale");
    }
    else
    {
        contract.require(baseline.values == processStart.values,
                         "private locale availability check changed the process locale");
    }
    if (! baseline.valid || contract.failures != 0)
        return 2;

    std::printf("LOCALE SMOKE: JUCE=%s mode=%s fixture=%s baseline=[%s] env=[%s]\n",
                JUCE_STRINGIFY(JUCE_MAJOR_VERSION) "." JUCE_STRINGIFY(JUCE_MINOR_VERSION) "." JUCE_STRINGIFY(JUCE_BUILDNUMBER),
                argv[2], mixed ? "mixed" : "C", baseline.values[0].c_str(), environmentLocale);
    std::fflush(stdout);
    contract.check("before JUCE calls");
    if (std::strcmp(argv[2], "query") == 0)
        runQueries(contract);
    else
        runEditor(contract);
    contract.check("final");

    if (contract.firstChangedStage != nullptr)
        std::fprintf(stderr, "FIRST LOCALE CHANGE: %s\n", contract.firstChangedStage);
    std::printf("LOCALE SMOKE %s: %u failed checks\n", contract.failures == 0 ? "PASS" : "FAIL", contract.failures);
    return contract.failures == 0 ? 0 : 1;
#else
    (void) argc;
    (void) argv;
    std::fprintf(stderr, "The locale preservation smoke test is Linux-only.\n");
    return 2;
#endif
}
} // namespace cs80::tests
