#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

namespace cs80::tests
{
struct PresetSearchAccess
{
    template <typename Check, typename Pump>
    static void run(Check check, Pump pump)
    {
        juce::TemporaryFile fixture("-cs80-preset-search");
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile().getChildFile("presets"));
        const char* names[] { "A Previous", "B Target Birch", "C Target Cedar" };
        for (int i = 0; i < 3; ++i)
        {
            host->setParamValue(P_brilliance, 0.125f + 0.25f * (float) i);
            check(host->saveUserPreset(names[i]).wasSaved(), "search fixture saves isolated real user patches");
        }
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
        check(list != nullptr && browser.getNumRows() == 3, "search regression reaches the production result list");
        if (list == nullptr || browser.getNumRows() != 3) return;
        int loads = 0;
        browser.onPresetLoaded = [&] { ++loads; };
        const auto selectedName = [&]
        {
            const auto* entry = browser.selectedEntry();
            return entry != nullptr ? entry->name : juce::String();
        };
        const auto prepare = [&](bool audition)
        {
            pump(1); // Drain the preceding case's real queued callbacks.
            browser.setAudition(false);
            browser.search.setText("Previous", false);
            browser.rebuildFilter();
            check(browser.getNumRows() == 1 && selectedName() == "A Previous",
                  "old search has one highlighted patch before the edit");
            browser.setAudition(audition);
            host->loadInitPatch();
            host->setPresetName("Unsaved search edit");
            host->setParamValue(P_brilliance, 0.9375f);
            browser.syncLoaded();
            loads = 0;
        };
        const auto typeQuery = [&](const juce::String& query)
        {
            // The real typing/paste path posts onTextChange; do not invoke the
            // callback or pump messages before delivering the following key.
            browser.search.selectAll();
            browser.search.insertTextAtCaret(query);
            check(browser.search.getText() == query, "new search text is already visible before its callback");
        };
        const auto key = [&](int code)
        {
            return browser.search.keyPressed(juce::KeyPress(code));
        };
        for (bool audition : { false, true })
            for (bool delivered : { false, true })
            {
                prepare(audition);
                typeQuery("Birch");
                if (delivered) pump(1);
                check(key(juce::KeyPress::returnKey), "search consumes Enter");
                check(host->getPresetName() == "B Target Birch" && host->getParamValue(P_brilliance) == 0.375f
                          && loads == 1 && selectedName() == "B Target Birch" && browser.getNumRows() == 1,
                      "Enter loads only the visible query's match, whether its callback is pending or delivered");
                pump(1);
                check(host->getPresetName() == "B Target Birch" && loads == 1 && selectedName() == "B Target Birch",
                      "late search callback cannot reload or move the chosen patch");

                prepare(audition);
                const auto before = host->getAPVTS().copyState();
                typeQuery("NoSuchSearchFixture");
                if (delivered) pump(1);
                check(key(juce::KeyPress::returnKey), "empty-result search consumes Enter");
                check(host->getAPVTS().copyState().isEquivalentTo(before) && loads == 0
                          && browser.getNumRows() == 0 && selectedName().isEmpty() && !browser.loadButton.isEnabled(),
                      "Enter on a new no-match query preserves the unsaved sound and cannot load a stale row");
                pump(1);
                check(host->getAPVTS().copyState().isEquivalentTo(before) && loads == 0,
                      "empty-result callback never auditions after Enter");

                for (int code : { juce::KeyPress::upKey, juce::KeyPress::downKey,
                                  juce::KeyPress::pageUpKey, juce::KeyPress::pageDownKey })
                {
                    prepare(audition);
                    const auto soundBeforeNavigation = host->getAPVTS().copyState();
                    typeQuery("Target");
                    if (delivered) pump(1);
                    check(key(code), "search consumes an unmodified result-navigation key");
                    const bool backwards = code == juce::KeyPress::upKey || code == juce::KeyPress::pageUpKey;
                    const juce::String expected = backwards ? "B Target Birch" : "C Target Cedar";
                    check(browser.getNumRows() == 2 && selectedName() == expected,
                          "arrows and page keys navigate only the current query, independent of callback timing");
                    const int expectedLoads = audition && !backwards ? 1 : 0;
                    check(loads == expectedLoads
                              && (expectedLoads == 1 ? host->getPresetName() == expected
                                  : host->getAPVTS().copyState().isEquivalentTo(soundBeforeNavigation)),
                          "navigation retains existing audition policy and never sounds an excluded old row");
                    pump(1);
                    check(selectedName() == expected && loads == expectedLoads,
                          "queued filtering cannot reset keyboard selection or trigger a second audition");
                }
            }
    }
};
} // namespace cs80::tests
