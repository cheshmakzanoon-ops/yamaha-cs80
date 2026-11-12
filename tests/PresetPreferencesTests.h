#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

namespace cs80::tests
{
// The Python driver supplies a disposable preference path and starts each phase
// in a fresh process. No desktop peer, native dialog or real settings are used.
struct PresetPreferencesAccess
{
    template <typename Check, typename Pump>
    static void run(Check check, Pump pump, const juce::String& phase)
    {
        const auto path = juce::SystemStats::getEnvironmentVariable("CS80_FAVOURITES_FILE", {});
        check(path.isNotEmpty(), "preference test requires the driver's isolated storage path");
        if (path.isEmpty()) return;
        const juce::File file(path);
        juce::TemporaryFile fixture("-cs80-preset-preferences");
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        const int initial = cs80::legacyProgramCount;
        host->loadFactoryPreset(initial);
        const auto favourite = "F:" + juce::String(host->getFactoryPresets()[(size_t) initial].name);
        if (phase == "enable")
            check(file.replaceWithText(favourite + "\n#view=compact\n#audition=off\n#facet=style\n"),
                  "seed real favourites and density alongside disabled preference values");
        if (file.existsAsFile())
            check(file.setLastModificationTime(juce::Time(946684800000LL)),
                  "pin fixture timestamp so an identical-content rewrite is detectable");
        const auto beforeOpen = file.loadFileAsString();
        const auto beforeModified = file.getLastModificationTime();
        const auto storageUnchanged = [&]
        {
            return file.loadFileAsString() == beforeOpen && file.getLastModificationTime() == beforeModified;
        };
        const auto savedText = [&] { return file.loadFileAsString().replace("\r\n", "\n"); };
        ui::PresetBrowser browser(*host);
        browser.setSize(1600, 936);
        int loads = 0;
        bool persistedBeforeLoad = false;
        browser.onPresetLoaded = [&]
        {
            ++loads;
            persistedBeforeLoad = savedText().contains("#audition=on\n");
        };
        check(storageUnchanged(), "constructing and laying out a browser never writes preferences");
        const bool read = phase.startsWith("read-");
        const bool expectedOn = phase == "disable" || phase == "read-enabled" || phase == "read-missing";
        check(browser.isAuditioning() == expectedOn && browser.auditionButton.getToggleState() == expectedOn,
              "fresh process restores the saved audition setting and its button");
        check(browser.showTechniques == expectedOn
                  && browser.facetButtons[1]->getToggleState() == expectedOn
                  && browser.facetButtons[0]->getToggleState() != expectedOn,
              "fresh process restores the saved facet and its selected segment");
        const bool existing = phase == "enable" || phase == "disable"
                           || phase == "read-enabled" || phase == "read-disabled";
        if (existing)
            check(browser.compactRows && browser.selectedEntry() != nullptr
                      && browser.isFavourite(*browser.selectedEntry()),
                  "existing slim density and favourite survive preference updates");
        if (read)
        {
            const auto sound = host->capturePresetBrowserSound();
            browser.refresh();
            check(host->getPresetName() == sound.name && host->currentValues().v == sound.values.v && loads == 0,
                  "reopening and refreshing persisted audition never loads over the current sound");
            check(storageUnchanged(), "read and refresh remain read-only after process restart");
            return;
        }

        // Public/internal setters retain their existing session-only semantics.
        // Tests and restoration must not acquire an incidental disk write.
        browser.setAudition(!expectedOn);
        browser.setShowTechniques(!expectedOn);
        browser.setAudition(expectedOn);
        browser.setShowTechniques(expectedOn);
        check(storageUnchanged() && loads == 0,
              "programmatic preference changes do not write storage or reload the sounding selection");

        juce::ListBox* list = nullptr;
        for (auto* child : browser.getChildren())
            if (auto* candidate = dynamic_cast<juce::ListBox*>(child)) list = candidate;
        check(list != nullptr, "preference test reaches the production result list");
        if (list == nullptr) return;
        const bool on = phase != "disable";
        if (on)
        {
            // Audition-on still sounds a distinct highlighted row exactly once.
            list->selectRow(browser.selectedFilteredRow + 1);
            check(browser.selectedEntry() != nullptr && browser.selectedEntry()->factoryIndex != initial,
                  "audition test highlights a different valid factory sound");
        }
        const auto selectedName = browser.selectedEntry()->name;
        const auto soundBeforeClick = host->currentValues();
        browser.auditionButton.triggerClick();
        pump(10);
        check(browser.isAuditioning() == on && browser.auditionButton.getToggleState() == on,
              "real audition button applies the requested setting");
        check(loads == (on ? 1 : 0)
                  && (on ? host->getPresetName() == selectedName : host->currentValues().v == soundBeforeClick.v),
              "persisting audition retains exactly-once selection loading and silent disable");
        const bool blocked = phase == "blocked";
        if (on && !blocked)
            check(persistedBeforeLoad, "audition load callback observes the new choice already persisted");
        if (!blocked)
            check(savedText().contains(on ? "#audition=on\n" : "#audition=off\n"),
                  "audition click persists immediately without another preference or favourite action");

        const auto soundBeforeFacet = host->currentValues();
        const auto loadsBeforeFacet = loads;
        browser.facetButtons[on ? 1u : 0u]->triggerClick();
        pump(10);
        check(browser.showTechniques == on && browser.facetButtons[1]->getToggleState() == on
                  && browser.facetButtons[0]->getToggleState() != on,
              "real facet segment applies the requested style or technique setting");
        check(host->currentValues().v == soundBeforeFacet.v && loads == loadsBeforeFacet,
              "persisting the facet never auditions or changes the sound");
        if (blocked)
        {
            check(!file.exists() && file.getParentDirectory().existsAsFile(),
                  "failed preference writes remain session-only and preserve the blocking file");
            return;
        }
        const auto saved = savedText();
        check(saved.contains(on ? "#facet=technique\n" : "#facet=style\n")
                  && saved.contains(on ? "#audition=on\n" : "#audition=off\n"),
              "facet click persists immediately while preserving the audition setting");
        if (existing)
            check(saved.contains(favourite + "\n") && saved.contains("#view=compact\n"),
                  "preference writes preserve the existing favourite and density");
        browser.facetButtons[on ? 1u : 0u]->triggerClick();
        pump(10);
        check(savedText() == saved && loads == loadsBeforeFacet,
              "repeated active facet clicks retain the persisted choices and sound");
    }
};
} // namespace cs80::tests
