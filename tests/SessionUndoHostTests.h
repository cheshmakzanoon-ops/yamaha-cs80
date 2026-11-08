#pragma once

#include "plugin/PluginProcessor.h"

#include <cmath>
#include <memory>
#include <thread>

namespace cs80::tests
{
template <typename Check>
void testSessionUndoHost(Check&& check)
{
    // Hosts may save on a background thread before the message-thread timer
    // has flushed recent automation. Saving must not turn those changes into
    // user edits or append them to the most recent gesture's undo transaction.
    for (bool userPreset : { false, true })
      for (bool backgroundSave : { false, true })
        for (bool userEdit : { false, true })
        {
            juce::TemporaryFile root("-cs80-session-undo");
            auto host = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
            const int edited = chParam(0, CP_lpfCutoff);
            const float original = host->getParamValue(edited);
            const float selected = original > 3000.0f ? 1200.0f : 6000.0f;
            if (userEdit)
            {
                auto* parameter = host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(edited));
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost(parameter->convertTo0to1(selected));
                parameter->endChangeGesture();
            }
            // No message loop is dispatched between automation and saving.
            // This proves the host state callback, not a lucky timer, handles it.
            std::thread automation([&] { host->setParamValue(P_volume, 0.37f); });
            automation.join();
            const float automated = host->getParamValue(P_volume);
            juce::MemoryBlock saved;
            bool savedOk = false;
            const auto save = [&]
            {
                if (userPreset)
                {
                    const auto result = host->saveUserPreset("Snapshot", Cs80AudioProcessor::PresetOverwrite::yes);
                    saved.reset(); // File::loadFileAsData appends to an existing block.
                    savedOk = result.wasSaved() && result.file.loadFileAsData(saved);
                }
                else
                {
                    host->getStateInformation(saved);
                    savedOk = saved.getSize() != 0;
                }
            };
            if (backgroundSave)
            {
                std::thread saver(save);
                saver.join();
            }
            else
                save();
            check(savedOk, "session and user-preset snapshots serialize successfully on either thread");

            auto restored = std::make_unique<Cs80AudioProcessor>();
            restored->setStateInformation(saved.getData(), (int) saved.getSize());
            check(std::abs(restored->getParamValue(P_volume) - automated) < 1.0e-6f,
                  "session save captures pending automation on either host thread");
            check(std::abs(restored->getParamValue(edited) - (userEdit ? selected : original)) < 0.01f,
                  "session save captures the current user-edited sound");
            check(host->getUndoManager().canUndo() == userEdit,
                  "session save never creates an undo step for host automation");
            if (userEdit)
            {
                check(host->undo(), "the user gesture remains undoable after session save");
                check(std::abs(host->getParamValue(edited) - original) < 0.01f,
                      "undo after session save restores the user gesture");
                check(std::abs(host->getParamValue(P_volume) - automated) < 1.0e-6f,
                      "undo after session save leaves unrelated host automation alone");
                check(!host->getUndoManager().canUndo(),
                      "session save keeps exactly the original user undo transaction");
                check(host->redo(), "the user gesture remains redoable after session save");
                check(std::abs(host->getParamValue(edited) - selected) < 0.01f
                          && std::abs(host->getParamValue(P_volume) - automated) < 1.0e-6f,
                      "redo after session save restores only the user gesture");
                check(host->undo(), "the user gesture can be undone again before saving");
                host->setParamValue(P_volume, 0.61f);
                const float newerAutomation = host->getParamValue(P_volume);
                if (backgroundSave)
                {
                    std::thread saver(save);
                    saver.join();
                }
                else
                    save();
                check(savedOk, "repeated snapshots save successfully with a redo branch");
                restored->setStateInformation(saved.getData(), (int) saved.getSize());
                check(std::abs(restored->getParamValue(P_volume) - newerAutomation) < 1.0e-6f,
                      "repeated snapshots serialize the newest automation");
                check(!host->getUndoManager().canUndo() && host->getUndoManager().canRedo(),
                      "session saving pending automation preserves an existing redo branch");
                check(host->redo(), "the prior user gesture is still redoable after another save");
                check(std::abs(host->getParamValue(edited) - selected) < 0.01f
                          && std::abs(host->getParamValue(P_volume) - newerAutomation) < 1.0e-6f,
                      "redo after repeated session saves preserves the newest automation");
            }
        }

    // A restored file may contain duplicated ids. JUCE binds the last one;
    // every saved duplicate must carry the same current value, not a stale
    // trailing value that could override an edited parameter on the next load.
    auto host = std::make_unique<Cs80AudioProcessor>();
    const auto id = Cs80AudioProcessor::parameterId(P_volume);
    juce::ValueTree duplicate("PARAM");
    duplicate.setProperty("id", id, nullptr);
    duplicate.setProperty("value", 0.8f, nullptr);
    duplicate.setProperty("futureProperty", "preserved", nullptr);
    host->getAPVTS().state.appendChild(duplicate, nullptr);
    host->setParamValue(P_volume, 0.53f);
    const float current = host->getParamValue(P_volume);
    juce::MemoryBlock saved;
    std::thread saver([&] { host->getStateInformation(saved); });
    saver.join();
    const auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
        static_cast<const char*>(saved.getData()), (int) saved.getSize()));
    check(xml != nullptr, "snapshot with duplicate parameter ids is valid XML");
    if (xml != nullptr)
    {
        const auto tree = juce::ValueTree::fromXml(*xml);
        int matches = 0;
        bool currentValues = true, retainedProperty = false;
        for (const auto child : tree)
            if (child.getProperty("id").toString() == id)
            {
                ++matches;
                currentValues = currentValues && std::abs((float) child.getProperty("value") - current) < 1.0e-6f;
                retainedProperty = retainedProperty || child.getProperty("futureProperty").toString() == "preserved";
            }
        check(matches == 2 && currentValues,
              "saved duplicate parameter ids carry the same current atomic value");
        check(retainedProperty, "detached snapshots preserve unknown parameter properties");
    }
    host->setStateInformation(saved.getData(), (int) saved.getSize());
    check(std::abs(host->getParamValue(P_volume) - current) < 1.0e-6f,
          "the last duplicate cannot restore a stale parameter value");
}
} // namespace cs80::tests
