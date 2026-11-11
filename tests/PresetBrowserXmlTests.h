#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

#include <algorithm>
#include <string>
#include <vector>

namespace cs80::tests
{
// Exercise the production catalogue and dirty-reference readers, not a second
// XML implementation. Every file, including external DTD sentinels, is owned
// by this disposable fixture. The sentinel is behavioral evidence, not a trace
// of arbitrary filesystem access.
struct PresetBrowserXmlAccess
{
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-browser-xml");
        const auto directory = fixture.getFile().getChildFile("presets");
        auto host = std::make_unique<Cs80AudioProcessor>(directory);
        const auto saved = host->saveUserPreset("Admission");
        check(saved.wasSaved(), "browser XML fixture saves an isolated user file");
        if (! saved.wasSaved()) return;
        const auto empty = directory.getChildFile("Default.cs80xml");
        check(empty.replaceWithText("<CS80_PARAMS/>"), "create default-signature reference row");
        const auto sentinel = directory.getChildFile("owned-sentinel.dtd");
        check(sentinel.replaceWithText("<!ENTITY label 'Flute_CS80_OWNED_SENTINEL'>\n<!ENTITY value '1'>"),
              "create harmless fixture-local DTD sentinel");

        const auto param = [](int id, const juce::String& value)
        {
            return "<PARAM id='" + juce::String(paramId(id)) + "' value='" + value + "'/>";
        };
        const auto content = param(P_brilliance, "0.375") + param(P_arpOn, "1");
        const juce::String canonical = "<CS80_PARAMS presetName='Flute &amp; valid'>"
            + content + "</CS80_PARAMS>";
        const juce::String changed = "<CS80_PARAMS presetName='Flute partial'>"
            + param(P_brilliance, "-0.875") + param(P_arpOn, "1") + "</CS80_PARAMS>";
        const juce::String externalBody = "<CS80_PARAMS presetName='&label;'>"
            + param(P_brilliance, "&value;") + param(P_arpOn, "&value;") + "</CS80_PARAMS>";
        const std::vector<juce::String> malformed {
            changed + "trailing",
            changed.replace("</CS80_PARAMS>", "</OTHER>"),
            "<CS80_PARAMS presetName='Flute partial'",
            changed + "<CS80_PARAMS/>",
            changed + "<!-- unfinished",
            "<!DOCTYPE CS80_PARAMS SYSTEM 'owned-sentinel.dtd'>" + externalBody,
            "<!DOCTYPE CS80_PARAMS [<!ENTITY % local SYSTEM 'owned-sentinel.dtd'> %local; ]>" + externalBody,
        };
        juce::int64 modification = juce::Time::getCurrentTime().toMilliseconds();
        const auto writeBytes = [&](const std::string& bytes)
        {
            check(saved.file.replaceWithData(bytes.data(), bytes.size()), "replace browser XML fixture");
            // JUCE's Linux setter truncates to seconds. Advance by whole
            // seconds and verify the stamp rather than relying on sleep timing.
            modification += 2000;
            check(saved.file.setLastModificationTime(juce::Time(modification))
                      && saved.file.getLastModificationTime().toMilliseconds() / 1000 == modification / 1000,
                  "advance fixture timestamp for refreshed dirty-reference cache");
        };
        const auto write = [&](const juce::String& text) { writeBytes(text.toStdString()); };

        host->loadFactoryPreset(legacyProgramCount);
        ui::PresetBrowser browser(*host);
        const bool previousAudition = browser.isAuditioning();
        struct RestoreAudition
        {
            ui::PresetBrowser& browser; bool previous;
            ~RestoreAudition() { browser.setAudition(previous); }
        } restore { browser, previousAudition };
        browser.setAudition(false);
        browser.scopeFilter = 4;
        browser.rebuildFilter();
        juce::ListBox* list = nullptr;
        for (auto* child : browser.getChildren())
            if (auto* candidate = dynamic_cast<juce::ListBox*>(child)) list = candidate;
        check(list != nullptr, "browser XML suite reaches the real result list");
        if (list == nullptr) return;
        const auto entry = [&](const juce::File& file) -> const ui::PresetBrowser::Entry*
        {
            for (const auto& row : browser.entries)
                if (row.userFile == file) return &row;
            return nullptr;
        };
        const auto select = [&]
        {
            for (int row = 0; row < browser.getNumRows(); ++row)
                if (browser.entries[(size_t) browser.entryIndexForRow(row)].userFile == saved.file)
                {
                    list->selectRow(row);
                    return true;
                }
            return false;
        };
        int callbacks = 0;
        browser.onPresetLoaded = [&] { ++callbacks; };
        const auto sound = host->getAPVTS().copyState();
        const auto metadata = host->getPresetMetadata();
        const auto unchanged = [&]
        {
            const auto now = host->getPresetMetadata();
            return callbacks == 0 && host->getAPVTS().copyState().isEquivalentTo(sound)
                && now.name == metadata.name && now.factoryIndex == metadata.factoryIndex
                && now.origin == metadata.origin;
        };

        for (size_t i = 0; i < malformed.size(); ++i)
            for (const bool audition : { false, true })
            {
                browser.setAudition(false);
                list->deselectAllRows();
                write(malformed[i]);
                browser.setAudition(audition);
                browser.refresh();
                const auto* row = entry(saved.file);
                const auto* fallback = entry(empty);
                std::printf("Browser XML case %zu, audition=%d: name=%s, styles=%u, fallback=%u\n", i, (int) audition,
                    row != nullptr ? row->name.toRawUTF8() : "<missing>", row != nullptr ? row->styles : 0,
                    fallback != nullptr ? fallback->styles : 0);
                check(row != nullptr && fallback != nullptr && browser.getNumRows() == 2
                          && row->name == "Admission" && row->type == fallback->type
                          && row->category == "User" && row->signature != nullptr
                          && row->styles == fallback->styles && ! row->haystack.contains("flute"),
                      "invalid refresh retains a filename/default-preview row without unadmitted metadata or styles");
                check(unchanged(), "invalid catalogue refresh never mutates sound or emits a success callback");
                check(select() && browser.loadButton.isEnabled(), "invalid row remains visible and loadable for diagnosis");
                browser.loadButton.onClick();
                check(unchanged() && browser.saveStatus.getText().startsWith("Not loaded:")
                          && browser.saveStatus.getTooltip() == saved.file.getFullPathName(),
                      "invalid refreshed row reports the existing LOAD error without success or mutation");
            }

        browser.setAudition(false);

        // Built-in/numeric entities and all supported Unicode byte encodings
        // must still supply the same metadata, category and style signature.
        write(canonical);
        browser.refresh();
        const auto* validRow = entry(saved.file);
        const auto* defaultRow = entry(empty);
        check(validRow != nullptr && defaultRow != nullptr && validRow->styles != defaultRow->styles
                  && validRow->type != defaultRow->type, "valid control differentiates category and preview from fallback");
        if (validRow == nullptr || defaultRow == nullptr) return;
        const auto styles = validRow->styles;
        const auto type = validRow->type;
        const auto verifyValid = [&](const juce::String& name)
        {
            browser.refresh();
            const auto* row = entry(saved.file);
            check(row != nullptr && row->name == name && row->styles == styles && row->type == type
                      && row->haystack.contains("flute") && unchanged(),
                  "valid XML metadata, instrument category and preview styles survive browser refresh");
        };
        for (const auto& text : { canonical,
              canonical.replace("&amp;", "&#38;"), canonical.replace("&amp;", "&#x26;"),
              "<?xml version='1.0' encoding='UTF-8'?><!-- before -->" + canonical + "<?after ok?>",
              "<!DOCTYPE CS80_PARAMS>" + canonical })
        {
            write(text);
            verifyValid("Flute & valid");
        }
        const auto unicode = canonical.replace("valid", juce::String::fromUTF8("é 🎹"));
        writeBytes(std::string("\xef\xbb\xbf") + unicode.toStdString());
        verifyValid(juce::String::fromUTF8("Flute & é 🎹"));
        for (const bool bigEndian : { false, true })
        {
            std::string bytes;
            bytes += bigEndian ? char(0xfe) : char(0xff);
            bytes += bigEndian ? char(0xff) : char(0xfe);
            const auto text = "<?xml version='1.0' encoding='UTF-16'?>" + unicode;
            const auto utf16 = text.toUTF16();
            for (auto* c = utf16.getAddress(); *c != 0; ++c)
            {
                bytes += char(bigEndian ? (*c >> 8) : (*c & 0xff));
                bytes += char(bigEndian ? (*c & 0xff) : (*c >> 8));
            }
            writeBytes(bytes);
            verifyValid(juce::String::fromUTF8("Flute & é 🎹"));
        }

        // Exercise the other file reader after a real load: the source may be
        // replaced externally, then repaired while the same browser stays open.
        write(canonical);
        check(host->loadUserPreset(saved.file).wasOk(), "load valid dirty-reference fixture");
        browser.refresh();
        check(! browser.patchEdited() && browser.editFileUsed[(size_t) P_brilliance] != 0
                  && browser.editFileValues[P_brilliance] == 0.375f,
              "valid file contributes dirty-reference values without a false edit");
        for (size_t i = 0; i < malformed.size(); ++i)
        {
            write(malformed[i]);
            const auto loadedSound = host->getAPVTS().copyState();
            const auto loadedMetadata = host->getPresetMetadata();
            browser.refresh();
            std::printf("Browser dirty XML case %zu: brilliance=%g, used=%d, edited=%d\n", i,
                browser.editFileValues[P_brilliance], (int) browser.editFileUsed[(size_t) P_brilliance],
                (int) browser.patchEdited());
            check(std::none_of(browser.editFileUsed.begin(), browser.editFileUsed.end(), [](char used) { return used != 0; })
                      && browser.editFileValues.v == ParamValues{}.v && ! browser.patchEdited(),
                  "malformed replacement cannot seed dirty-reference values or fabricate a live edit");
            check(host->getAPVTS().copyState().isEquivalentTo(loadedSound)
                      && host->getPresetMetadata().origin == loadedMetadata.origin
                      && host->getPresetName() == loadedMetadata.name && callbacks == 0,
                  "dirty-reference refresh leaves the loaded sound and identity intact");
        }
        host->setParamValue(P_brilliance, 0.625f);
        check(browser.patchEdited(), "live edits still compare against the baseline when the file is inadmissible");
        write(canonical);
        browser.refresh();
        check(browser.editFileUsed[(size_t) P_brilliance] != 0
                  && browser.editFileValues[P_brilliance] == 0.375f && browser.patchEdited(),
              "repair repopulates valid dirty-reference values and preserves a real edit");
        check(select(), "repaired row remains selectable");
        browser.loadButton.onClick();
        check(callbacks == 1 && ! browser.patchEdited() && host->getPresetName() == "Flute & valid"
                  && host->getParamValue(P_brilliance) == 0.375f
                  && browser.saveStatus.getText().startsWith("Loaded"),
              "repaired-file retry succeeds once and restores the clean loaded state");
    }
};
} // namespace cs80::tests
