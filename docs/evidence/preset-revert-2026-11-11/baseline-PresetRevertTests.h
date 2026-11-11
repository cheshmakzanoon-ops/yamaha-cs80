#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/PresetBrowser.h"

#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace cs80::tests
{
struct PresetRevertAccess
{
    static void source(Cs80AudioProcessor& p, int channel, int value)
    {
        auto* parameter = p.getAPVTS().getParameter(Cs80AudioProcessor::hardwareSourceParameterId(channel));
        parameter->setValueNotifyingHost(parameter->convertTo0to1((float) value));
    }
    template <typename Check>
    static void run(Check check)
    {
        juce::TemporaryFile fixture("-cs80-preset-revert");
        auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile().getChildFile("presets"));
        host->loadFactoryPreset(legacyProgramCount + 3);
        host->setPresetName("Opening unsaved sound");
        ui::PresetBrowser browser(*host);
        int callbacks = 0;
        browser.onPresetLoaded = [&] { ++callbacks; };

        // Direct host edits deliberately preserve name/index: metadata must not
        // accidentally make these source-only changes detectable.
        for (int channel = 0; channel < 2; ++channel)
            for (int selected = 0; selected < 14; ++selected)
            {
                source(*host, channel, selected);
                browser.captureOpening();
                check(!browser.canRevert(), "new opening snapshot is clean for every hardware source");
                source(*host, channel, (selected + 1) % 14);
                check(browser.canRevert(), "source-only host edit enables REVERT for either rank and every source");
                browser.revertToOpening();
                check(host->getHardwareSource(channel) == selected,
                      "REVERT restores every factory, MEMORY and PANEL hardware source");
                check(!browser.canRevert(), "source REVERT returns to its complete opening baseline");
            }

        source(*host, 0, 13);
        source(*host, 1, 13);
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < CP_count; ++local)
            {
                if (!Cs80AudioProcessor::isMemoryControl(local)) continue;
                const auto& def = kChannelDefs[local];
                host->setMemoryValue(bank, local, def.min);
                browser.captureOpening();
                host->setMemoryValue(bank, local, def.max);
                check(browser.canRevert(), "hidden MEMORY-only edit enables REVERT for every bank/control");
                browser.revertToOpening();
                check(host->getMemoryValue(bank, local) == def.min,
                      "REVERT restores every valid MEMORY value even when neither rank uses it");
                check(!browser.canRevert(), "MEMORY REVERT returns to its complete opening baseline");
            }

        host->loadFactoryPreset(legacyProgramCount + 5);
        host->setParamValue(P_brilliance, 0.375f);
        host->setPresetName("Edited custom opening");
        const auto meta = host->getPresetMetadata();
        const float brilliance = host->getParamValue(P_brilliance);
        source(*host, 0, 11);
        source(*host, 1, 12);
        browser.captureOpening();
        host->loadFactoryPreset(22);
        browser.revertToOpening();
        check(host->getPresetMetadata().name == meta.name
                  && host->getPresetMetadata().factoryIndex == meta.factoryIndex
                  && host->getCurrentProgram() == meta.factoryIndex
                  && host->getParamValue(P_brilliance) == brilliance,
              "REVERT restores custom name, original program index and unsaved core values together");
        check(host->getHardwareSource(0) == 11 && host->getHardwareSource(1) == 12,
              "mixed MEMORY A/B rank selection survives preset audition and REVERT");
    }
};
} // namespace cs80::tests
