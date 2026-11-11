#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

namespace cs80::tests
{
// All catalogue mutations stay inside a disposable fixture. Moving a file out
// of its preset folder models the refresh after DELETE without using real trash.
struct PresetRefreshAccess
{
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-preset-refresh");
        const auto root = fixture.getFile();
        const auto directory = root.getChildFile("presets");
        auto host = std::make_unique<Cs80AudioProcessor>(directory);
        const auto b = host->saveUserPreset("B Refresh");
        const auto c = host->saveUserPreset("C Refresh");
        check(b.wasSaved() && c.wasSaved(), "refresh fixture saves two isolated user patches");
        if (! b.wasSaved() || ! c.wasSaved())
            return;

        const int factory = cs80::legacyProgramCount;
        host->loadFactoryPreset(factory);
        ui::PresetBrowser browser(*host);
        const bool previousAudition = browser.isAuditioning();
        struct RestoreAudition
        {
            ui::PresetBrowser& browser;
            bool previous;
            ~RestoreAudition() { browser.setAudition(previous); }
        } restoreAudition { browser, previousAudition };
        browser.setAudition(false);
        browser.scopeFilter = 4; // USER: the sounding factory patch is excluded.
        browser.rebuildFilter();
        browser.setSize(1600, 936); // Exercise layout callbacks during refresh, too.

        juce::ListBox* list = nullptr;
        for (auto* child : browser.getChildren())
            if (auto* candidate = dynamic_cast<juce::ListBox*>(child))
                list = candidate;
        check(list != nullptr, "refresh regression reaches the production result list");
        if (list == nullptr)
            return;
        const auto selectFile = [&](const juce::File& file)
        {
            for (int row = 0; row < browser.getNumRows(); ++row)
                if (browser.entries[(size_t) browser.entryIndexForRow(row)].userFile == file)
                {
                    list->selectRow(row);
                    return true;
                }
            return false;
        };
        const auto selectedIs = [&](const juce::File& file)
        {
            const auto* selected = browser.selectedEntry();
            return selected != nullptr && selected->userFile == file
                && browser.selectedFilteredRow == list->getSelectedRow();
        };
        int callbacks = 0;
        browser.onPresetLoaded = [&] { ++callbacks; };

        // A custom/edited sound has only a factory-slot fallback in the list.
        // Refresh may select that slot, but must not audition it over the edit.
        check(selectFile(b.file), "select a user row before testing custom-sound refresh");
        browser.setAudition(true);
        host->loadFactoryPreset(factory);
        host->setPresetName("Unsaved refresh edit");
        host->setParamValue(cs80::P_brilliance, 0.375f);
        const auto customValues = host->currentValues();
        callbacks = 0;
        browser.scopeFilter = 0;
        browser.rebuildFilter();
        browser.refresh();
        check(browser.selectedEntry() != nullptr
                  && browser.selectedEntry()->factoryIndex == factory,
              "custom sound restores its factory-slot fallback selection");
        check(host->getPresetName() == "Unsaved refresh edit"
                  && host->currentValues().v == customValues.v && callbacks == 0,
              "programmatic refresh selection does not audition over an unsaved sound");
        browser.setAudition(false);
        host->loadFactoryPreset(factory);
        browser.scopeFilter = 4;
        browser.rebuildFilter();
        callbacks = 0;

        check(selectFile(c.file) && selectedIs(c.file), "select the final user patch before catalogue insertion");
        const auto a = host->saveUserPreset("A Refresh");
        check(a.wasSaved(), "earlier-sorting user patch is added outside the browser");
        host->loadFactoryPreset(factory);
        const auto sounding = host->getPresetMetadata();
        const auto sound = host->currentValues();
        const auto unchanged = [&]
        {
            const auto now = host->getPresetMetadata();
            return now.name == sounding.name && now.factoryIndex == sounding.factoryIndex
                && host->currentValues().v == sound.v && callbacks == 0;
        };
        browser.refresh();
        const bool insertionKeptIdentity = selectedIs(c.file);
        check(insertionKeptIdentity,
              "catalogue insertion preserves the selected file, not its former vector index");
        // The unfixed implementation fails this defined identity assertion.
        // Stop before removal exercises its separate destroyed-entry access.
        if (! insertionKeptIdentity)
            return;
        check(browser.getNumRows() == 3 && browser.loadButton.isEnabled()
                  && browser.deleteButton.isEnabled() && unchanged(),
              "insertion updates rows and actions without loading another sound");

        check(a.file.moveFileTo(root.getChildFile("removed-a.cs80xml")),
              "remove an earlier catalogue file inside the fixture");
        browser.refresh();
        check(selectedIs(c.file) && browser.getNumRows() == 2 && unchanged(),
              "removing an earlier file preserves the surviving selected identity");

        browser.deleteSelected();
        check(browser.deleteArmedKey.isNotEmpty(), "selected fixture patch can arm DELETE");
        check(c.file.moveFileTo(root.getChildFile("removed-c.cs80xml")),
              "remove the selected final catalogue file inside the fixture");
        browser.refresh();
        check(browser.getNumRows() == 1 && browser.selectedEntry() == nullptr
                  && list->getSelectedRow() == -1 && ! browser.loadButton.isEnabled()
                  && ! browser.deleteButton.isEnabled() && browser.deleteArmedKey.isEmpty()
                  && unchanged(),
              "removed final selection becomes unselected and disarmed without stale-entry access");

        check(selectFile(b.file), "select the last remaining saved patch");
        check(b.file.moveFileTo(root.getChildFile("removed-b.cs80xml")),
              "empty the fixture preset folder");
        browser.refresh();
        check(browser.getNumRows() == 0 && browser.selectedEntry() == nullptr
                  && list->getSelectedRow() == -1 && browser.entryIndexForRow(0) == -1
                  && browser.entryIndexForRow(-1) == -1 && ! browser.loadButton.isEnabled()
                  && ! browser.deleteButton.isEnabled() && ! browser.favouriteButton.isEnabled()
                  && unchanged(),
              "refresh to an empty user library leaves no selectable or actionable stale row");

        check(root.getChildFile("removed-b.cs80xml").moveFileTo(b.file)
                  && root.getChildFile("removed-c.cs80xml").moveFileTo(c.file),
              "restore two fixture files for search and audition refresh");
        browser.refresh();
        check(selectFile(c.file), "select a restored user patch");
        browser.setAudition(true);
        host->loadFactoryPreset(factory);
        callbacks = 0;
        for (int sort = 0; sort < 5; ++sort)
            for (const bool reverse : { false, true })
            {
                browser.sortMode = sort;
                browser.sortReverse = reverse;
                browser.rebuildFilter();
                browser.refresh();
                check(selectedIs(c.file) && unchanged(),
                      "refresh preserves selection and sound across sort modes with audition enabled");
            }
        browser.search.setText("Refresh", false);
        browser.rebuildFilter();
        check(c.file.moveFileTo(root.getChildFile("removed-c-again.cs80xml")),
              "remove selected search hit inside fixture");
        browser.refresh();
        check(selectedIs(b.file) && unchanged(),
              "search chooses the surviving top hit without auditioning during refresh");

        browser.scopeFilter = 0;
        browser.search.setText({}, false);
        browser.refresh();
        check(browser.selectedEntry() != nullptr
                  && browser.selectedEntry()->factoryIndex == factory && unchanged(),
              "refresh still selects the sounding patch when it belongs to the filter");
    }
};
} // namespace cs80::tests
