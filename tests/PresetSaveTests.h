#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

#include <cmath>
#include <memory>
#include <vector>

namespace cs80::tests
{
// This friend has access only to the storage writer and browser workflow. All
// files belong to a newly allocated temporary fixture; no user preset is read,
// overwritten or removed by these tests.
struct PresetSaveAccess
{
    template <typename Check, typename Flush>
    static void run(Check check, Flush flush)
    {
        using Status = Cs80AudioProcessor::PresetSaveStatus;
        using Overwrite = Cs80AudioProcessor::PresetOverwrite;
        juce::TemporaryFile fixture("-cs80-preset-tests");
        const auto root = fixture.getFile();
        check(root.createDirectory().wasOk(), "preset-save isolated fixture created");
        if (! root.isDirectory())
            return;

        Cs80AudioProcessor host(root.getChildFile("presets"));
        const auto beforeIndex = host.getCurrentPresetIndex();
        check(host.getUserPresetFiles().isEmpty()
                  && ! root.getChildFile("presets").exists(),
              "preset listing does not create the storage folder");

        // Exercise every current host control, including the four memory banks,
        // hardware selectors, performance controls and original SUB extension.
        std::vector<float> expected;
        const auto& parameters = host.getParameters();
        for (int i = 0; i < parameters.size(); ++i)
            parameters[i]->setValueNotifyingHost((float) ((i * 37) % 101) / 100.0f);
        host.setParamValue(cs80::P_subOriginalMode, 1.0f);
        host.setParamValue(cs80::P_subOriginalFunction, 5.0f);
        host.setParamValue(cs80::P_subOriginalSpeed, 0.7f);
        host.setParamValue(cs80::P_subExtLevel, 0.3f);
        host.setParamValue(cs80::P_parityMode, 0.0f);
        for (auto* parameter : parameters)
            expected.push_back(parameter->getValue());
        const auto expectedState = host.getAPVTS().copyState();

        const auto saved = host.saveUserPreset("Round trip");
        check(saved.wasSaved() && saved.status == Status::saved && saved.file.existsAsFile(),
              "preset save reports success only after a readable file exists");
        check(host.getPresetName() == "Round trip" && host.getCurrentPresetIndex() == beforeIndex,
              "successful save updates name without changing factory metadata");
        check(host.getUserPresetFiles().size() == 1,
              "successful save publishes only one visible user preset");
        if (! saved.wasSaved())
            return;

        auto xml = juce::XmlDocument::parse(saved.file);
        check(xml != nullptr, "saved preset is valid XML");
        if (! xml)
            return;
        const auto writtenState = juce::ValueTree::fromXml(*xml);
        check(writtenState.hasType("CS80_PARAMS") && xml->getIntAttribute("subOscStateVersion") == 1,
              "save preserves preset schema and original SUB migration version");
        for (const auto& child : expectedState)
        {
            if (! child.hasProperty("id"))
                continue;
            const auto written = writtenState.getChildWithProperty("id", child["id"]);
            check(written.isValid() && std::abs((double) written["value"] - (double) child["value"]) < 1.0e-6,
                  "saved XML contains every existing control value");
        }
        for (auto* parameter : parameters)
            parameter->setValueNotifyingHost(0.0f);
        host.setParamValue(cs80::P_parityMode, 1.0f);
        host.loadUserPreset(saved.file);
        for (int i = 0; i < parameters.size(); ++i)
        {
            if (parameters[i] == host.getAPVTS().getParameter("parity_mode"))
                continue;
            check(std::abs(parameters[i]->getValue() - expected[(size_t) i]) < 1.0e-5f,
                  "saved user preset restores every sound and performance control");
        }
        check(host.getParamValue(cs80::P_parityMode) == 1.0f,
              "user preset save/load retains existing live isolation policy");

        // All failure paths must leave both bytes and displayed metadata intact.
        const auto originalBytes = saved.file.loadFileAsString();
        host.setPresetName("Unsaved edit");
        const auto unchanged = [&]
        {
            return saved.file.loadFileAsString() == originalBytes
                && host.getPresetName() == "Unsaved edit"
                && host.getCurrentPresetIndex() == beforeIndex;
        };
        const auto duplicate = host.saveUserPreset("Round trip");
        check(duplicate.status == Status::alreadyExists && unchanged(),
              "duplicate filename is not overwritten and does not announce success");
        const auto sanitized = host.saveUserPreset("Round: trip");
        check(sanitized.status == Status::alreadyExists && sanitized.file == saved.file && unchanged(),
              "different names with the same sanitized filename require overwrite permission");

        const auto failedWrite = host.writeUserPreset("Round trip", Overwrite::yes,
            [](const juce::XmlElement&, const juce::File& staging)
            {
                staging.replaceWithText("injected partial write");
                return juce::Result::fail("injected disk-write failure");
            });
        check(failedWrite.status == Status::error && failedWrite.message.contains("injected") && unchanged(),
              "partial staging write failure preserves the original preset and name");
        const auto failedPublish = host.writeUserPreset("Round trip", Overwrite::yes,
            [](const juce::XmlElement&, const juce::File&)
            {
                // Explicit I/O injection: report a write but supply no staged
                // file, forcing the real atomic replacement to fail.
                return juce::Result::ok();
            });
        check(failedPublish.status == Status::error && unchanged(),
              "atomic publication failure preserves the original preset and name");
        check(host.getUserPresetFiles().size() == 1
                  && root.getChildFile("presets").getNumberOfChildFiles(juce::File::findFiles) == 1,
              "failed saves remove only their own staging files");

        const auto blocker = root.getChildFile("not-a-folder");
        check(blocker.replaceWithText("fixture blocker"), "invalid-parent fixture written");
        Cs80AudioProcessor blockedHost(blocker.getChildFile("presets"));
        const auto blockedName = blockedHost.getPresetName();
        const auto failedDirectory = blockedHost.saveUserPreset("Cannot save");
        check(failedDirectory.status == Status::error && failedDirectory.message.isNotEmpty()
                  && blockedHost.getPresetName() == blockedName
                  && blocker.loadFileAsString() == "fixture blocker",
              "directory creation failure is explicit and leaves metadata and parent intact");

        // Simulate a second instance winning after the first existence check.
        const auto racedFile = root.getChildFile("presets/Raced.cs80xml");
        const auto raced = host.writeUserPreset("Raced", Overwrite::no,
            [&](const juce::XmlElement& content, const juce::File& staging)
            {
                if (! content.writeTo(staging) || ! racedFile.replaceWithText("other instance"))
                    return juce::Result::fail("could not prepare concurrent-save fixture");
                return juce::Result::ok();
            });
        check(raced.status == Status::alreadyExists && racedFile.loadFileAsString() == "other instance"
                  && host.getPresetName() == "Unsaved edit",
              "no-replace publication preserves a concurrent instance's new preset");

        // Exercise the actual browser callbacks and asynchronous confirmation,
        // including cancel, repeated Save, overwrite and closing while pending.
        auto& lookAndFeel = juce::LookAndFeel::getDefaultLookAndFeel();
        const bool usedNativeAlerts = lookAndFeel.isUsingNativeAlertWindows();
        lookAndFeel.setUsingNativeAlertWindows(false);
        struct RestoreAlerts
        {
            juce::LookAndFeel& look;
            bool native;
            ~RestoreAlerts() { look.setUsingNativeAlertWindows(native); }
        } restoreAlerts { lookAndFeel, usedNativeAlerts };

        const auto findAlert = []() -> juce::AlertWindow*
        {
            auto* manager = juce::ModalComponentManager::getInstance();
            for (int i = 0; i < manager->getNumModalComponents(); ++i)
                if (auto* alert = dynamic_cast<juce::AlertWindow*>(manager->getModalComponent(i)))
                    if (alert->getName() == "Overwrite user preset?")
                        return alert;
            return nullptr;
        };

        cs80::ui::PresetBrowser browser(host);
        int loadedCallbacks = 0;
        browser.onPresetLoaded = [&] { ++loadedCallbacks; };
        browser.saveName.setText("Round trip", false);
        const int previousSelection = browser.selectedFilteredRow;
        const auto previousDescription = browser.description.getText();
        browser.saveButton.onClick();
        flush(30);
        auto* alert = findAlert();
        check(alert != nullptr && ! browser.saveButton.isEnabled() && loadedCallbacks == 0 && unchanged(),
              "browser asks before overwriting and does not report a save while pending");
        if (alert != nullptr)
        {
            check(alert->getDescription().contains(saved.file.getFullPathName()),
                  "overwrite confirmation identifies the exact sanitized destination");
            browser.saveButton.onClick();
            check(findAlert() == alert, "repeated Save does not create duplicate overwrite dialogs");
            alert->triggerButtonClick("Cancel");
            flush(30);
        }
        check(loadedCallbacks == 0 && unchanged() && browser.saveButton.isEnabled()
                  && browser.selectedFilteredRow == previousSelection
                  && browser.description.getText() == previousDescription
                  && browser.saveStatus.getText().contains("cancelled"),
              "declined overwrite preserves bytes, browser selection, metadata and callbacks");

        browser.saveName.setText("Round: trip", false);
        browser.saveButton.onClick();
        flush(30);
        alert = findAlert();
        check(alert != nullptr, "sanitized-name collision also requires UI confirmation");
        if (alert != nullptr)
        {
            alert->triggerButtonClick("Overwrite");
            flush(30);
        }
        check(loadedCallbacks == 1 && host.getPresetName() == "Round: trip"
                  && browser.saveStatus.getText().startsWith("Saved")
                  && saved.file.loadFileAsString() != originalBytes,
              "confirmed overwrite publishes the new state and reports success once");
        auto overwritten = juce::XmlDocument::parse(saved.file);
        check(overwritten != nullptr && overwritten->getStringAttribute("presetName") == "Round: trip",
              "confirmed overwrite keeps the requested display name inside valid XML");

        const auto beforeNewSaveEntries = browser.entries.size();
        browser.saveName.setText("Browser success", false);
        browser.saveButton.onClick();
        check(loadedCallbacks == 2 && host.getPresetName() == "Browser success"
                  && browser.saveStatus.getText().startsWith("Saved")
                  && browser.entries.size() == beforeNewSaveEntries + 1
                  && browser.selectedEntry() != nullptr
                  && browser.selectedEntry()->name == "Browser success",
              "new browser save reports success once and refreshes the visible library");
        cs80::ui::PresetBrowser blockedBrowser(blockedHost);
        int failedCallbacks = 0;
        blockedBrowser.onPresetLoaded = [&] { ++failedCallbacks; };
        blockedBrowser.saveName.setText("No success", false);
        const auto failedSelection = blockedBrowser.selectedFilteredRow;
        blockedBrowser.saveButton.onClick();
        check(failedCallbacks == 0 && blockedHost.getPresetName() == blockedName
                  && blockedBrowser.selectedFilteredRow == failedSelection
                  && blockedBrowser.saveStatus.getText().startsWith("Not saved:"),
              "browser write failure shows an error without refresh or success callback");

        auto closing = std::make_unique<cs80::ui::PresetBrowser>(host);
        closing->onPresetLoaded = [&] { ++loadedCallbacks; };
        closing->saveName.setText("Round trip", false);
        const auto beforeCloseBytes = saved.file.loadFileAsString();
        const auto beforeCloseName = host.getPresetName();
        closing->saveButton.onClick();
        flush(30);
        juce::Component::SafePointer<juce::AlertWindow> closingAlert(findAlert());
        check(closingAlert != nullptr, "closing-browser test reaches pending confirmation");
        closing.reset();
        flush(30);
        check(loadedCallbacks == 2 && host.getPresetName() == beforeCloseName
                  && saved.file.loadFileAsString() == beforeCloseBytes && findAlert() == nullptr,
              "closing browser before confirmation makes the safe callback a no-op");
        closing = std::make_unique<cs80::ui::PresetBrowser>(host);
        closing->onPresetLoaded = [&] { ++loadedCallbacks; };
        closing->saveName.setText("Round trip", false);
        closing->saveButton.onClick();
        closing.reset(); // Before JUCE's deferred dialog creation runs.
        flush(30);
        check(loadedCallbacks == 2 && host.getPresetName() == beforeCloseName
                  && saved.file.loadFileAsString() == beforeCloseBytes && findAlert() == nullptr,
              "closing browser before dialog creation cancels the queued dialog safely");

        // Keep the original user-preset migration regression, now entirely in
        // this fixture instead of creating/deleting a real per-user preset.
        auto legacyState = juce::ValueTree::fromXml(*xml);
        for (const auto* id : { "sub_original_mode", "sub_original_function", "sub_original_speed", "sub_ext_level" })
            legacyState.removeChild(legacyState.getChildWithProperty("id", id), nullptr);
        legacyState.removeProperty("subOscStateVersion", nullptr);
        const auto legacyFile = root.getChildFile("legacy.cs80xml");
        check(legacyState.createXml()->writeTo(legacyFile), "legacy preset fixture written");
        host.loadUserPreset(legacyFile);
        check(host.getParamValue(cs80::P_subOriginalMode) == 0.0f
                  && host.getParamValue(cs80::P_subOriginalFunction) == 0.0f
                  && std::abs(host.getParamValue(cs80::P_subOriginalSpeed) - 4.2f) < 0.00001f
                  && host.getParamValue(cs80::P_subExtLevel) == 0.0f,
              "legacy user preset clears stale original mode and controls");
    }
};
} // namespace cs80::tests
