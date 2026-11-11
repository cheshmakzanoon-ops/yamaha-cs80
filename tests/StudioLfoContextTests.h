#pragma once

#include "StudioSwitchContextTests.h" // Shared direct-event, gesture and dispatch helpers.
#include "plugin/AdvancedPanel.h"
#include <array>

namespace cs80::tests
{
template<class Check>
void testStudioLfoContext(Check check)
{
    using namespace studio_switch_detail;
    juce::TemporaryFile fixture("-cs80-studio-lfo-context");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    host->setParamValue(P_parityMode, 0.0f);
    auto editor = std::make_unique<Cs80AudioProcessorEditor>(*host);
    editor->setSize(1600, 1000); editor->setVisible(true); editor->showAdvancedPanel(true);
    ui::StudioView* studio = nullptr;
    std::array<std::array<ui::ParameterContextButton*, 6>, 3> chips {};
    const std::array<juce::String, 6> names { "OFF", "PITCH", "VCF", "VCA", "PAN", "PWM" };
    int found = 0;
    std::function<void(juce::Component&)> collect = [&](juce::Component& parent) {
        if (auto* view = dynamic_cast<ui::StudioView*>(&parent)) studio = view;
        for (auto* child : parent.getChildren())
        {
            for (int group = 0; group < 3; ++group)
                for (int dest = 0; dest < 6; ++dest)
                    if (child->getTitle() == "LFO " + juce::String(group + 1) + " destination " + names[(size_t)dest])
                    {
                        ++found;
                        auto* chip = dynamic_cast<ui::ParameterContextButton*>(child);
                        check(chip != nullptr && chips[(size_t)group][(size_t)dest] == nullptr,
                              "each actual destination chip inherits the shared context helper exactly once");
                        chips[(size_t)group][(size_t)dest] = chip;
                    }
            collect(*child);
        }
    };
    collect(*editor);
    check(found == 18 && studio != nullptr, "actual editor contains three groups of six destination chips");
    if (studio == nullptr) return;
    studio->showTab(ui::StudioTab::modulation, 0);
    host->prepareToPlay(48000.0, 32);
    const juce::KeyPress shortcut(juce::KeyPress::F10Key, juce::ModifierKeys::shiftModifier, 0);
    std::vector<std::function<void()>> retained;
    std::array<float, 3> beforeDestruction {};
    for (int group = 0; group < 3; ++group)
    {
        const int core = P_lfo1Dest + group * 3;
        const auto id = Cs80AudioProcessor::parameterId(core);
        auto* parameter = host->getAPVTS().getParameter(id);
        check(parameter != nullptr, "each destination group has a real APVTS parameter");
        if (parameter == nullptr) continue;
        const int target = parameter->getParameterIndex();
        auto& siblings = chips[(size_t)group];
        const auto matchesLights = [&](int dest) {
            return std::all_of(siblings.begin(), siblings.end(), [&](auto* c) {
                return c != nullptr && c->getToggleState() == (c->getName() == names[(size_t)dest]);
            });
        };
        for (int otherGroup = 0; otherGroup < 3; ++otherGroup)
            if (otherGroup != group)
                check(target != host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_lfo1Dest + otherGroup * 3))->getParameterIndex(),
                      "different LFO groups have distinct host parameter identities");
        for (int dest = 0; dest < 6; ++dest)
        {
            auto* chip = siblings[(size_t)dest];
            if (chip == nullptr) continue;
            check(chip->getContextParameterId() == id && chip->getContextHostParameterIndex() == target && (bool)chip->onContextMenu,
                  "all six chips bind the same group's exact APVTS ID and host index");
            const int initial = dest == 0 ? 3 : 0;
            host->setParamValue(core, (float)initial);
            check(pump([&] { return matchesLights(initial); }), "external group value refresh lights exactly its selected chip");
            const auto menu = chip->buildContextMenu();
            juce::PopupMenu::MenuItemIterator first(menu);
            check(first.next() && first.getItem().isSectionHeader
                      && first.getItem().text == parameter->getName(64) + "   " + parameter->getCurrentValueAsText(),
                  "every chip's menu names the whole destination parameter and current value");
            check(chip->getTooltip().contains("whole LFO " + juce::String(group + 1) + " destination")
                      && chip->getTooltip().contains(dest == 0 ? "no destination" : "Click the lit chip again")
                      && chip->getDescription().contains("Shift+F10"),
                  "custom selection explanation and explicit shared-group context scope are preserved");
            Probe events; parameter->addListener(&events);
            int menus = 0;
            // Observe routing only. Helper presentation/lifetime guards are exercised
            // by the existing Studio switch gate, without opening native OS menus.
            chip->onContextMenu = [&] { ++menus; };
            chip->mouseDown(mouse(*chip, juce::ModifierKeys::rightButtonModifier));
            chip->mouseDrag(mouse(*chip, juce::ModifierKeys::rightButtonModifier)); chip->mouseUp(mouse(*chip, 0));
            check(menus == 1 && host->getParamValue(core) == initial && events.events.empty(),
                  "context pointer dispatch does not select this chip or emit a gesture");
            check(chip->keyPressed(shortcut) && menus == 2 && host->getParamValue(core) == initial && events.events.empty(),
                  "chip Shift+F10 opens group context without selection");
            click(*chip);
            check(host->getParamValue(core) == dest && chip->getToggleState() && events.events == std::vector<int>({1, 0, -1}),
                  "ordinary click selects this chip with its original balanced gesture");
            events.events.clear(); click(*chip);
            check(host->getParamValue(core) == 0.0f && events.events == std::vector<int>({1, 0, -1}),
                  "repeated lit click selects OFF; OFF retains its original explicit write gesture");
            check(pump([&] { return matchesLights(0); }), "all siblings refresh to OFF after ordinary deselection");
            events.events.clear();
            check(!chip->keyPressed(juce::KeyPress(juce::KeyPress::spaceKey))
                      && !chip->keyPressed(juce::KeyPress(juce::KeyPress::leftKey))
                      && !chip->keyPressed(juce::KeyPress(juce::KeyPress::rightKey)),
                  "chip keeps unhandled JUCE Space and arrow behavior");
            check(chip->keyPressed(juce::KeyPress(juce::KeyPress::returnKey)), "chip retains ordinary queued Return activation");
            check(!chip->keyPressed(shortcut) && menus == 2 && events.events.empty(),
                  "inherited pending-key guard blocks context until the chip's queued click runs");
            check(pump([&] { paint(*chip); return host->getParamValue(core) == dest && !chip->isDown() && !events.events.empty(); })
                      && events.events == std::vector<int>({1, 0, -1}), "queued Return selects this destination once");
            events.events.clear();
            auto reset = action(chip->buildContextMenu(), "Reset to default");
            auto learn = action(chip->buildContextMenu(), "MIDI learn");
            for (bool ancestor : {false, true})
            {
                if (ancestor) editor->setEnabled(false); else chip->setEnabled(false);
                if (reset) reset(); if (learn) learn();
                chip->mouseDown(mouse(*chip, juce::ModifierKeys::rightButtonModifier)); chip->mouseUp(mouse(*chip, 0));
                click(*chip);
                check(!chip->keyPressed(shortcut) && !chip->keyPressed(juce::KeyPress(juce::KeyPress::returnKey))
                          && menus == 2 && host->getParamValue(core) == dest && events.events.empty()
                          && host->getMidiLearn().learningParameter() == -1,
                      "actual disabled chip or ancestor blocks context, ordinary input and delayed actions");
                if (ancestor) editor->setEnabled(true); else chip->setEnabled(true);
            }
            // Reset acts on the group default, even when invoked from a different
            // chip. It never activates the face under the context menu.
            host->setParamValue(core, (float)(dest == 5 ? 4 : 5));
            const float beforeReset = parameter->getValue();
            const float defaultValue = parameter->convertTo0to1(parameter->convertFrom0to1(parameter->getDefaultValue()));
            events.events.clear(); host->getUndoableState().syncNow(); host->getUndoManager().clearUndoHistory();
            reset = action(chip->buildContextMenu(), "Reset to default");
            check((bool)reset, "every chip exposes group reset"); if (reset) reset();
            check(parameter->getValue() == defaultValue && events.events == std::vector<int>({1, 0, -1})
                      && pump([&] { return matchesLights(juce::roundToInt(parameter->convertFrom0to1(defaultValue))); }),
                  "reset uses the actual group default, balanced host gesture and sibling refresh");
            if (reset) reset();
            check(events.events == std::vector<int>({1, 0, -1}), "group reset at default is a no-op from every chip");
            parameter->removeListener(&events);
            check(host->undo() && parameter->getValue() == beforeReset, "one undo restores group value before reset");
            check(host->redo() && parameter->getValue() == defaultValue, "one redo restores group default");
            chip->onContextMenu = {};
            check(chip->bindParameterContext(*host, id), "chip can restore its ordinary context opener after routed event observation");
        }
        if (std::any_of(siblings.begin(), siblings.end(), [](auto* c) { return c == nullptr; })) continue;
        host->getMidiLearn().clearAll();
        auto arm = action(siblings[1]->buildContextMenu(), "MIDI learn"); if (arm) arm();
        check(host->getMidiLearn().learningParameter() == target, "one chip arms the group's actual host index");
        for (auto* chip : siblings)
            check(chip->getTooltip().contains("Waiting for a MIDI controller") && (bool)action(chip->buildContextMenu(), "Cancel MIDI learn"),
                  "pending learn is shared visibly by all six chips");
        auto staleCancel = action(siblings[2]->buildContextMenu(), "Cancel MIDI learn");
        if (auto* otherChip = chips[(size_t)((group + 1) % 3)][1])
        {
            auto otherArm = action(otherChip->buildContextMenu(), "MIDI learn"); if (otherArm) otherArm();
            const int otherTarget = otherChip->getContextHostParameterIndex();
            if (staleCancel) staleCancel();
            check(otherTarget != target && host->getMidiLearn().learningParameter() == otherTarget,
                  "a stale sibling cancel preserves a different group's newer learn intent");
            arm = action(siblings[1]->buildContextMenu(), "MIDI learn"); if (arm) arm();
        }
        auto cancel = action(siblings[5]->buildContextMenu(), "Cancel MIDI learn"); if (cancel) cancel();
        check(host->getMidiLearn().learningParameter() == -1, "a sibling chip cancels the shared pending learn");
        arm = action(siblings[0]->buildContextMenu(), "MIDI learn"); if (arm) arm();
        std::vector<float> original;
        for (auto* p : host->getParameters()) original.push_back(p->getValue());
        juce::AudioBuffer<float> audio(2, 32); juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 127), 0); host->processBlock(audio, midi);
        bool isolated = true;
        for (int i = 0; i < host->getParameters().size(); ++i)
            if (i != target) isolated &= original[(size_t)i] == host->getParameters()[i]->getValue();
        check(host->getParamValue(core) == 5.0f && isolated && host->getMidiLearn().parameterFor(74) == target,
              "real learned CC changes only its LFO group, not another group or any other parameter");
        check(pump([&] { return matchesLights(5); }), "learned MIDI value refreshes all sibling lights");
        for (auto* chip : siblings)
            check(chip->getTooltip().contains("MIDI CC 74") && (bool)action(chip->buildContextMenu(), "Forget MIDI CC 74"),
                  "every sibling observes the same learned controller");
        auto staleForget = action(siblings[1]->buildContextMenu(), "Forget MIDI CC 74");
        host->getMidiLearn().assign(75, target); if (staleForget) staleForget();
        check(host->getMidiLearn().controllerFor(target) == 75, "stale sibling forget preserves a newer group mapping");
        host->setParamValue(P_parityMode, 1.0f);
        for (auto* chip : siblings)
            check(chip->isEnabled() && chip->getTooltip().contains("inactive in PURE CS-80")
                      && !action(chip->buildContextMenu(), "MIDI learn"),
                  "PURE preserves editable chip policy and reports masked group mapping");
        midi.clear(); midi.addEvent(juce::MidiMessage::controllerEvent(1, 75, 0), 0); host->processBlock(audio, midi);
        check(host->getParamValue(core) == 5.0f, "PURE leaves the mapped destination unchanged by incoming MIDI");
        click(*siblings[2]);
        check(host->getParamValue(core) == 2.0f, "PURE keeps ordinary stored destination selection available");
        auto pureReset = action(siblings[4]->buildContextMenu(), "Reset to default"); if (pureReset) pureReset();
        check(parameter->getValue() == parameter->getDefaultValue(), "PURE still permits stored group reset");
        auto forget = action(siblings[5]->buildContextMenu(), "Forget MIDI CC 75"); if (forget) forget();
        check(host->getMidiLearn().controllerFor(target) == -1, "a different sibling can forget the group mapping in PURE");
        for (auto* chip : siblings)
            check(!chip->getTooltip().contains("MIDI CC"), "forget is immediately reflected across all siblings");
        host->setParamValue(P_parityMode, 0.0f);
        host->setParamValue(core, 3.0f);
        beforeDestruction[(size_t)group] = parameter->getValue();
        retained.push_back(action(siblings[3]->buildContextMenu(), "Reset to default"));
    }
    editor.reset();
    for (const auto& reset : retained) if (reset) reset();
    for (int group = 0; group < 3; ++group)
        check(host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(P_lfo1Dest + group * 3))->getValue() == beforeDestruction[(size_t)group],
              "actual editor destruction invalidates retained destination actions");
}
}
