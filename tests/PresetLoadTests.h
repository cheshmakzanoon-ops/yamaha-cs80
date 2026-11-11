#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

namespace cs80::tests
{
// All filesystem changes use a fresh disposable fixture, never the user's library.
struct PresetLoadAccess
{
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-preset-load");
        const auto root = fixture.getFile();
        auto host = std::make_unique<Cs80AudioProcessor>(root.getChildFile("presets"));
        host->setParamValue(P_brilliance, 0.1875f);
        const auto saved = host->saveUserPreset("Load retry");
        check(saved.wasSaved(), "load failure fixture saves a real user patch");
        if (! saved.wasSaved()) return;
        const auto bytes = saved.file.loadFileAsString();
        ui::PresetBrowser browser(*host);
        const bool previousAudition = browser.isAuditioning();
        struct RestoreAudition
        {
            ui::PresetBrowser& browser;
            bool previous;
            ~RestoreAudition() { browser.setAudition(previous); }
        } restoreAudition { browser, previousAudition };
        browser.setAudition(false);
        browser.scopeFilter = 4;
        browser.rebuildFilter();
        juce::ListBox* list = nullptr;
        for (auto* child : browser.getChildren())
            if (auto* candidate = dynamic_cast<juce::ListBox*>(child)) list = candidate;
        check(list != nullptr && browser.getNumRows() == 1,
              "load failure fixture reaches the real browser result list");
        if (list == nullptr || browser.getNumRows() != 1) return;
        int callbacks = 0;
        browser.onPresetLoaded = [&] { ++callbacks; };

        // The catalogue was built from a valid file. It can disappear or be
        // replaced externally while the browser remains open, before LOAD.
        const char* malformed[] = {
            "<CS80_PARAMS><", "<NOT_A_PRESET/>", "<cs80_params/>", "",
            "<CS80_PARAMS", "<CS80_PARAMS ", "<CS80_PARAMS presetName='truncated'",
            "<CS80_PARAMS>", "<CS80_PARAMS></CS80_PARAMS", "<CS80_PARAMS></OTHER>",
            "<CS80_PARAMS><PARAM/></OTHER>", "<CS80_PARAMS><PARAM></CS80_PARAMS>",
            "<CS80_PARAMS/><CS80_PARAMS/>", "<CS80_PARAMS/>junk", "<CS80_PARAMS/><!--",
            "<CS80_PARAMS/><?unfinished", "<CS80_PARAMS/> <", "<CS80_PARAMS/> </CS80_PARAMS>",
            "<CS80_PARAMS presetName='unclosed>", "<CS80_PARAMS presetName=bare/>",
            "<CS80_PARAMS presetName='a' presetName='b'/>",
            "<CS80_PARAMS presetName='&unknown;'/>", "<CS80_PARAMS presetName='&#0;'/>",
            "<CS80_PARAMS><!--unfinished</CS80_PARAMS>",
            "<CS80_PARAMS><![CDATA[unfinished</CS80_PARAMS>",
            "<CS80_PARAMS><PARAM id='volume' value='0.2'></OTHER></CS80_PARAMS>",
            "<CS80_PARAMS><PARAM id='volume' value='0.2'/></CS80_PARAMS>trailing",
        };
        for (int failure = 0; failure <= (int) std::size(malformed); ++failure)
            for (bool audition : { false, true })
            {
                browser.setAudition(false);
                check(saved.file.replaceWithText(bytes), "restore valid fixture before failed-load case");
                browser.refresh();
                list->deselectAllRows();
                host->loadFactoryPreset(cs80::legacyProgramCount);
                host->setPresetName("Unsaved live edit");
                host->setParamValue(P_brilliance, 0.8125f);
                host->setParamValue(P_parityMode, 1.0f);
                host->setParamValue(P_oversampling, 2.0f);
                host->setParamValue(P_outputJack, 2.0f);
                host->setParamValue(P_tuningMode, 1.0f);
                browser.syncLoaded();
                const auto metadata = host->getPresetMetadata();
                const auto before = host->getAPVTS().copyState();
                const auto loadedKey = browser.loadedKey;
                callbacks = 0;
                if (failure == 0)
                    check(saved.file.moveFileTo(root.getChildFile("missing.cs80xml")),
                          "move fixture out of catalogue without refreshing its stale row");
                else
                    check(saved.file.replaceWithText(malformed[failure - 1]),
                          "replace fixture with malformed, wrong-root, wrong-case or empty content");
                std::printf("Preset rejection case %d, audition=%d: %s\n", failure, (int) audition,
                            failure == 0 ? "missing file" : malformed[failure - 1]);
                if (audition) browser.setAudition(true);
                list->selectRow(0); // AUDITION goes through selectedRowsChanged.
                if (! audition) browser.loadButton.onClick();
                const auto unchanged = [&]
                {
                    const auto now = host->getPresetMetadata();
                    return now.name == metadata.name && now.factoryIndex == metadata.factoryIndex
                        && now.origin == metadata.origin
                        && host->getAPVTS().copyState().isEquivalentTo(before);
                };
                check(unchanged(), "failed LOAD or audition preserves the entire live parameter state and metadata");
                check(callbacks == 0, "failed LOAD or audition does not emit a success callback");
                check(browser.saveStatus.getText().startsWith("Not loaded:")
                          && browser.saveStatus.getText().contains("Load retry")
                          && browser.saveStatus.getTooltip() == saved.file.getFullPathName(),
                      "failed LOAD or audition identifies the rejected patch and exact file");
                check(browser.selectedEntry() != nullptr && browser.selectedEntry()->userFile == saved.file
                          && list->getSelectedRow() == 0 && browser.loadedKey == loadedKey
                          && browser.loadButton.isEnabled(),
                      "failed load keeps its selected row, old loaded identity and retry action");
                browser.returnKeyPressed(0);
                const juce::MouseEvent doubleClick(juce::Desktop::getInstance().getMainMouseSource(),
                    {}, {}, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, list, list,
                    juce::Time(), {}, juce::Time(), 2, false);
                browser.listBoxItemDoubleClicked(0, doubleClick);
                check(callbacks == 0 && unchanged(),
                      "repeated keyboard and double-click loads cannot announce success or alter sound");

                check(saved.file.replaceWithText(bytes), "repair fixture without closing or refreshing browser");
                callbacks = 0;
                browser.loadButton.onClick();
                check(callbacks == 1 && host->getPresetName() == "Load retry"
                          && host->getParamValue(P_brilliance) == 0.1875f
                          && browser.loadedKey == "U:Load retry.cs80xml"
                          && browser.loadButton.getButtonText() == "RELOAD",
                      "retry after file repair loads exactly once and restores the normal loaded marker");
                check(browser.saveStatus.getText().startsWith("Loaded")
                          && ! browser.saveStatus.getText().contains("Not loaded"),
                      "successful retry replaces the stale failure status");
                check(host->getParamValue(P_parityMode) == 1.0f
                          && host->getParamValue(P_oversampling) == 2.0f
                          && host->getParamValue(P_outputJack) == 2.0f
                          && host->getParamValue(P_tuningMode) == 1.0f,
                      "successful retry preserves existing live PURE and physical-setup recall policy");
            }

        // Complete documents stay schema-permissive. XML syntax admission must
        // not become a new preset schema or reject ordinary XML decorations.
        const juce::String canonical = "<CS80_PARAMS presetName=\"XML &amp; valid\">"
            "<PARAM id=\"" + juce::String(paramId(P_brilliance)) + "\" value=\"0.375\"/>"
            "</CS80_PARAMS>";
        check(saved.file.replaceWithText(canonical) && host->loadUserPreset(saved.file).wasOk(),
              "canonical complete user document loads");
        const juce::String valid[] = {
            canonical,
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" + canonical,
            " \r\n\t<!-- before --><?application test?>" + canonical + "\n<!-- after --><?after ok?> ",
            canonical.replace("XML &amp; valid", "XML &#38; valid"),
            canonical.replace("XML &amp; valid", "XML &#x26; valid"),
            canonical.replace("<PARAM", "<!-- quoted > < markers --><?inner ok?><PARAM"),
            canonical.replace("</CS80_PARAMS>", "<![CDATA[ignored <tag> & literal]]></CS80_PARAMS>"),
            canonical.replace(" value=\"0.375\"/>", " value='0.375'></PARAM >"),
            "<!DOCTYPE CS80_PARAMS>" + canonical,
        };
        for (const auto& text : valid)
        {
            host->setParamValue(P_brilliance, 0.8125f);
            check(saved.file.replaceWithText(text), "write complete decorated XML fixture");
            check(host->loadUserPreset(saved.file).wasOk()
                      && host->getParamValue(P_brilliance) == 0.375f
                      && host->getPresetName() == "XML & valid",
                  "valid declarations, comments, PI, CDATA, entities and whitespace preserve recall");
        }
        for (const char* text : { "<CS80_PARAMS/>", "<CS80_PARAMS></CS80_PARAMS>" })
        {
            check(saved.file.replaceWithText(text) && host->loadUserPreset(saved.file).wasOk(),
                  "complete empty parameter documents retain existing default normalization");
        }
    }
};
} // namespace cs80::tests
