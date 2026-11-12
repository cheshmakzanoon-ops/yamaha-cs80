#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

namespace cs80::tests
{
struct PresetClickAccess
{
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-preset-click");
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile().getChildFile("presets"));
        host->setParamValue(P_brilliance, 0.125f);
        const auto first = host->saveUserPreset("A Click Birch");
        host->setParamValue(P_brilliance, 0.875f);
        const auto second = host->saveUserPreset("B Click Cedar");
        check(first.wasSaved() && second.wasSaved(), "click fixture saves two valid distinct patches");
        if (! first.wasSaved() || ! second.wasSaved()) return;

        ui::PresetBrowser browser(*host);
        browser.setSize(1600, 936);
        const bool previousAudition = browser.isAuditioning();
        struct RestoreAudition
        {
            ui::PresetBrowser& browser;
            bool previous;
            ~RestoreAudition() { browser.setAudition(previous); }
        } restore { browser, previousAudition };
        browser.setAudition(false);
        browser.scopeFilter = 4;
        browser.sortMode = 1;
        browser.sortReverse = false;
        browser.rebuildFilter();
        juce::ListBox* list = nullptr;
        for (auto* child : browser.getChildren())
            if (auto* candidate = dynamic_cast<juce::ListBox*>(child)) list = candidate;
        check(list != nullptr && browser.getNumRows() == 2, "click test reaches the real result list");
        if (list == nullptr || browser.getNumRows() != 2) return;

        // The custom component is the star hit area. Its parent is JUCE's real
        // RowComponent: drive that row's mouse handlers, not a model callback.
        const auto click = [&](int row)
        {
            auto* star = list->getComponentForRowNumber(row);
            auto* component = star != nullptr ? star->getParentComponent() : nullptr;
            check(component != nullptr, "real JUCE row exists for pointer selection");
            if (component == nullptr) return;
            const juce::Point<float> point(20.0f, 20.0f);
            const auto now = juce::Time::getCurrentTime();
            const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(), point,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier),
                1.0f, 0.0f, 0.0f, 0.0f, 0.0f, component, component, now, point, now, 1, false);
            component->mouseDown(event);
            component->mouseUp(event);
        };
        int loads = 0;
        browser.onPresetLoaded = [&] { ++loads; };
        const auto query = [&](const juce::String& text)
        {
            browser.search.setText(text, false);
            browser.applyPendingSearch();
        };
        const auto prepare = [&](bool audition)
        {
            browser.setAudition(false);
            query("No matching click fixture");
            check(browser.selectedEntry() == nullptr && list->getSelectedRow() == -1
                      && browser.silentlySelectedKey.isEmpty(),
                  "empty results retire the former selection and its silent click identity");
            browser.setAudition(audition); // No highlighted row to audition.
            host->loadInitPatch();
            host->setPresetName("Unsaved click edit");
            host->setParamValue(P_brilliance, 0.5f);
            browser.syncLoaded();
            loads = 0;
        };

        for (bool audition : { false, true })
        {
            prepare(audition);
            const auto before = host->getAPVTS().copyState();
            query("Birch");
            check(browser.getNumRows() == 1 && list->getSelectedRow() == 0
                      && browser.selectedEntry()->userFile == first.file,
                  "search highlights the matching first row");
            check(loads == 0 && host->getAPVTS().copyState().isEquivalentTo(before),
                  "search restoration never auditions or changes the unsaved sound");
            click(0);
            check(audition ? (loads == 1 && host->getPresetName() == "A Click Birch"
                                 && host->getParamValue(P_brilliance) == 0.125f)
                           : (loads == 0 && host->getAPVTS().copyState().isEquivalentTo(before)),
                  "single-clicking the highlighted search result auditions exactly when enabled");
            if (! audition)
            {
                browser.setAudition(true);
                check(loads == 1 && host->getPresetName() == "A Click Birch",
                      "enabling audition still sounds the selected row exactly once");
            }
            // An already sounding/edited patch remains protected from automatic reload.
            host->setParamValue(P_brilliance, 0.25f);
            browser.syncLoaded();
            loads = 0;
            click(0);
            check(loads == 0 && host->getParamValue(P_brilliance) == 0.25f,
                  "repeat click does not reload over edits to the sounding patch");

            query("Click");
            click(1);
            check(loads == 1 && host->getPresetName() == "B Click Cedar"
                      && host->getParamValue(P_brilliance) == 0.875f && list->getSelectedRow() == 1,
                  "clicking a different row auditions once, not once for selection and again for the click");
            check(browser.silentlySelectedKey.isEmpty(),
                  "ordinary selection retires the silent identity before its subsequent click callback");
            check(list->keyPressed(juce::KeyPress(juce::KeyPress::upKey)), "result list consumes Up");
            check(loads == 2 && host->getPresetName() == "A Click Birch"
                      && host->getParamValue(P_brilliance) == 0.125f,
                  "keyboard selection retains its existing single-load audition behavior");
        }

        prepare(true);
        query("Birch");
        query("Click");
        browser.sortButtons[2]->onClick(); // Z-A moves Birch without changing its identity.
        check(list->getSelectedRow() == 1 && browser.selectedEntry()->userFile == first.file && loads == 0,
              "sorting preserves the silent selected identity without audition");
        click(1);
        check(loads == 1 && host->getPresetName() == "A Click Birch",
              "explicit click still chooses the restored file after its row moves");

        prepare(true);
        query("Birch");
        const auto beforeMissing = host->getAPVTS().copyState();
        const auto held = fixture.getFile().getChildFile("held-birch.cs80xml");
        check(first.file.moveFileTo(held), "move valid fixture out of the catalogue without corrupting it");
        click(0);
        check(loads == 0 && host->getAPVTS().copyState().isEquivalentTo(beforeMissing)
                  && browser.saveStatus.getText().startsWith("Not loaded:"),
              "missing silently selected file reports failure without replacing the live sound");
        check(held.moveFileTo(first.file), "restore the unchanged valid file to its original path");
        click(0);
        check(loads == 1 && host->getPresetName() == "A Click Birch"
                  && host->getParamValue(P_brilliance) == 0.125f,
              "another explicit click retries the same restored selection after its file returns");

        // A normal selection's load callback may change the sounding state.
        // The following JUCE click callback must not replay the chosen file.
        prepare(true);
        browser.sortMode = 1;
        browser.sortReverse = false;
        query("Click");
        browser.onPresetLoaded = [&]
        {
            ++loads;
            host->loadInitPatch();
            host->setParamValue(P_brilliance, 0.625f);
        };
        click(1);
        check(loads == 1 && host->getParamValue(P_brilliance) == 0.625f
                  && browser.silentlySelectedKey.isEmpty(),
              "ordinary pointer selection is never replayed after a reentrant successful-load callback");
    }
};
} // namespace cs80::tests
