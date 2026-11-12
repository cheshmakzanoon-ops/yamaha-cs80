#pragma once

#include "plugin/StudioView.h"
#include "plugin/PluginProcessor.h"

#include <array>
#include <memory>
#include <functional>
#include <vector>

namespace cs80::tests
{
namespace ring_curve_detail
{
constexpr std::array<int, 3> parameters { P_ringAttack, P_ringDepth, P_ringDecay };
struct Probe final : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, values = 0;
    std::function<void()> onBegin, onValue, onEnd;
    void parameterValueChanged(int, float) override
    {
        ++values;
        if (onValue) onValue();
    }
    void parameterGestureChanged(int, bool starting) override
    {
        starting ? ++begins : ++ends;
        if (starting && onBegin) onBegin();
        if (!starting && onEnd) onEnd();
    }
};
inline juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p, int buttons)
{
    return { juce::Desktop::getInstance().getMainMouseSource(), p, juce::ModifierKeys(buttons),
             1.f, 0.f, 0.f, 0.f, 0.f, &c, &c, juce::Time::getCurrentTime(),
             p, juce::Time::getCurrentTime(), 1, false };
}
struct Fixture
{
    juce::TemporaryFile file { "-cs80-ring-curve-gesture" };
    std::unique_ptr<Cs80AudioProcessor> host = std::make_unique<Cs80AudioProcessor>(file.getFile());
    juce::Component root;
    std::unique_ptr<ui::StudioView> studio;
    juce::Component* curve = nullptr;
    std::array<Probe, 3> probes;
    Fixture()
    {
        host->setParamValue(P_ringAttack, .4f);
        host->setParamValue(P_ringDepth, .5f);
        host->setParamValue(P_ringDecay, .7f);
        host->setParamValue(P_ringSpeed, 100.f);
        host->getUndoableState().syncNow();
        host->getUndoManager().clearUndoHistory();
        studio = std::make_unique<ui::StudioView>(*host);
        root.setVisible(true);
        root.addAndMakeVisible(*studio);
        studio->setSize(1600, 936);
        studio->showPage(1);
        // Locate the existing production plot by its authored page rectangle.
        // No test-only access hook or new component ID is needed on the baseline.
        for (int i = 0; i < studio->getNumSubPages(ui::StudioTab::modulation); ++i)
            if (auto* page = studio->getSubPage(ui::StudioTab::modulation, i))
                for (auto* child : page->getChildren())
                    if (child->getBounds() == juce::Rectangle<int>(854, 148, 668, 376)) curve = child;
        for (size_t i = 0; i < parameters.size(); ++i) parameter(i)->addListener(&probes[i]);
    }
    ~Fixture()
    {
        studio.reset();
        for (size_t i = 0; i < parameters.size(); ++i) parameter(i)->removeListener(&probes[i]);
    }
    juce::RangedAudioParameter* parameter(size_t i)
    { return host->getAPVTS().getParameter(Cs80AudioProcessor::parameterId(parameters[i])); }
    std::array<float, 3> values() const
    { return { host->getParamValue(P_ringAttack), host->getParamValue(P_ringDepth), host->getParamValue(P_ringDecay) }; }
    juce::Point<float> node(int which) const
    {
        const float ta = 1.151f * host->getParamValue(P_ringAttack);
        const float td = 1.267f * host->getParamValue(P_ringDecay);
        const float depth = host->getParamValue(P_ringDepth);
        const float ed = depth <= .001f ? 1.f : depth;
        return which == 0 ? juce::Point<float>(84.f + 562.f * ta / (ta + td), 284.f - 226.f / 1.1f)
                          : juce::Point<float>(646.f, 284.f - 226.f / ((1.f + ed) * 1.1f));
    }
    bool balanced(int which, int count = 1) const
    {
        for (size_t i = 0; i < probes.size(); ++i)
        {
            const int expected = (which == 0 ? i < 2 : i == 2) ? count : 0;
            if (probes[i].begins != expected || probes[i].ends != expected) return false;
        }
        return true;
    }
};
}

template<class Check>
void testRingCurveGestures(Check check)
{
    using namespace ring_curve_detail;
    constexpr int left = juce::ModifierKeys::leftButtonModifier;
    // Exercise the actual Studio plot without a desktop peer: both nodes, normal
    // release, destruction, direct/inherited disable, tab and ancestor visibility.
    for (int which : {0, 1})
        for (int end = 0; end < 9; ++end)
    {
        Fixture f;
        check(f.curve != nullptr, "real Studio SUB / RING page exposes its authored plot");
        if (f.curve == nullptr) continue;
        const auto before = f.values();
        const auto start = f.node(which);
        const auto moved = start + juce::Point<float>(-40.f, 14.f);
        f.curve->mouseDown(mouse(*f.curve, start, left));
        check(f.host->getUndoableState().hasActiveParameterGesture(), "node press begins a real host parameter gesture");
        f.curve->mouseDrag(mouse(*f.curve, moved, left));
        const auto edited = f.values();
        check(edited != before && (which == 0 ? edited[2] == before[2] : edited[0] == before[0] && edited[1] == before[1]),
              "A changes attack/depth and D changes only decay");
        if (end == 0) f.curve->mouseUp(mouse(*f.curve, moved, 0));
        if (end == 1) f.studio.reset();
        if (end == 2) f.curve->setEnabled(false);
        if (end == 3) f.root.setEnabled(false);
        if (end == 4) f.studio->showTab(ui::StudioTab::keyboard);
        if (end == 5) f.studio->setVisible(false);
        if (end == 6) f.root.setVisible(false);
        if (end == 7) f.root.removeChildComponent(f.studio.get());
        if (end == 8) f.curve->setVisible(false);
        check(f.balanced(which) && !f.host->getUndoableState().hasActiveParameterGesture(),
              "release or interrupted lifecycle ends each owned gesture exactly once");
        check(f.values() == edited, "cancellation retains the last admitted drag value without another write");
        check(f.host->undo() && f.values() == before, "Undo remains usable and restores the complete interrupted node drag");
        check(f.host->redo() && f.values() == edited, "Redo restores the complete interrupted node drag");
        if (end != 1)
        {
            const auto latest = f.values();
            const int oldValues = f.probes[0].values + f.probes[1].values + f.probes[2].values;
            f.curve->mouseDrag(mouse(*f.curve, start + juce::Point<float>(-70.f, 24.f), left));
            f.curve->mouseUp(mouse(*f.curve, start, 0));
            check(f.values() == latest && f.probes[0].values + f.probes[1].values + f.probes[2].values == oldValues,
                  "late drag and up cannot restart a cancelled gesture");
            f.curve->setEnabled(true); f.curve->setVisible(true);
            f.root.setEnabled(true); f.root.setVisible(true);
            f.root.addAndMakeVisible(*f.studio); f.studio->showPage(1);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
            const auto fresh = f.node(which);
            const auto beforeFresh = f.values();
            f.curve->mouseDown(mouse(*f.curve, fresh, left));
            f.curve->mouseDrag(mouse(*f.curve, fresh + juce::Point<float>(-25.f, 9.f), left));
            f.curve->mouseUp(mouse(*f.curve, fresh, 0));
            check(f.values() != beforeFresh && !f.host->getUndoableState().hasActiveParameterGesture(),
                  "a fresh drag works after visibility, enablement or hierarchy recovery");
            check(f.host->undo() && f.values() == beforeFresh, "the recovered drag is its own undoable edit");
        }
    }
    // Host listeners are synchronous. Cancelling during a begin/value must
    // wait until all listeners saw that notification; deleting Studio may also
    // delete the graph before its initiating mouse event has returned.
    for (int which : {0, 1})
        for (int target = 0; target < (which == 0 ? 2 : 1); ++target)
            for (int phase = 0; phase < 3; ++phase)
                for (bool destroy : {false, true})
    {
        Fixture f;
        if (f.curve == nullptr) { check(false, "reentrant fixture contains the production graph"); continue; }
        const auto before = f.values();
        const auto start = f.node(which), moved = start + juce::Point<float>(-40.f, 14.f);
        const size_t primary = which == 0 ? (size_t)target : 2;
        int cancellations = 0;
        const auto cancel = [&] {
            if (++cancellations != 1) return;
            if (destroy) f.studio.reset(); else f.root.setVisible(false);
        };
        if (phase == 0) f.probes[primary].onBegin = cancel;
        if (phase == 1) f.probes[primary].onValue = cancel;
        if (phase == 2) f.probes[primary].onEnd = cancel;
        f.curve->mouseDown(mouse(*f.curve, start, left));
        if (phase != 0)
        {
            f.curve->mouseDrag(mouse(*f.curve, moved, left));
            if (phase == 2) f.curve->mouseUp(mouse(*f.curve, moved, 0));
        }
        check(cancellations == 1 && !f.host->getUndoableState().hasActiveParameterGesture(),
              "synchronous begin/value/end hide or destruction completes without leaving an open host gesture");
        bool balanced = true;
        for (size_t i = 0; i < f.probes.size(); ++i)
        {
            const int expected = which == 0 ? (i <= (phase == 0 ? primary : 1) ? 1 : 0) : (i == 2 ? 1 : 0);
            balanced &= f.probes[i].begins == expected && f.probes[i].ends == expected;
            f.probes[i].onBegin = {}; f.probes[i].onValue = {}; f.probes[i].onEnd = {};
        }
        check(balanced, "reentrant cancellation ends only parameters whose begin notification actually ran");
        const auto edited = f.values();
        if (phase == 0)
            check(edited == before && !f.host->getUndoManager().canUndo(), "begin cancellation writes no parameter and creates no edit");
        else
        {
            check(edited != before && (phase != 1 || primary != 0 || edited[1] == before[1]),
                  "value cancellation retains its first write and never continues to the A node's depth write");
            check(f.host->undo() && f.values() == before && f.host->redo() && f.values() == edited,
                  "deferred gesture completion preserves Undo/Redo after a synchronous host cancellation");
        }
        if (!destroy)
        {
            const auto prior = f.values();
            f.curve->mouseDrag(mouse(*f.curve, start + juce::Point<float>(-60.f, 20.f), left));
            f.curve->mouseUp(mouse(*f.curve, start, 0));
            check(f.values() == prior && !f.host->getUndoableState().hasActiveParameterGesture(),
                  "late events after synchronous hiding cannot resume the cancelled edit");
            f.root.setVisible(true);
            juce::MessageManager::getInstance()->runDispatchLoopUntil(5);
            const auto fresh = f.node(which);
            f.curve->mouseDown(mouse(*f.curve, fresh, left));
            f.curve->mouseDrag(mouse(*f.curve, fresh + juce::Point<float>(-25.f, 9.f), left));
            f.curve->mouseUp(mouse(*f.curve, fresh, 0));
            check(f.values() != prior && !f.host->getUndoableState().hasActiveParameterGesture()
                      && f.host->undo() && f.values() == prior,
                  "fresh editing and Undo recover after synchronous cancellation finished dispatching");
        }
    }
    for (int which : {0, 1})
        for (int buttons : { 0, (int)juce::ModifierKeys::rightButtonModifier, (int)juce::ModifierKeys::middleButtonModifier,
                             left | juce::ModifierKeys::rightButtonModifier })
    {
        Fixture f;
        if (f.curve == nullptr) continue;
        const auto before = f.values();
        const auto start = f.node(which);
        f.curve->mouseDown(mouse(*f.curve, start, buttons));
        f.curve->mouseDrag(mouse(*f.curve, start + juce::Point<float>(-40.f, 15.f), buttons));
        f.curve->mouseUp(mouse(*f.curve, start, 0));
        check(f.values() == before && f.balanced(which, 0) && !f.host->getUndoManager().canUndo(),
              "non-left and popup pointer sequences do not edit a node or open host gestures");
    }
    for (int disabled = 0; disabled < 2; ++disabled)
    {
        Fixture f;
        if (f.curve == nullptr) continue;
        if (disabled == 0) f.curve->setEnabled(false); else f.root.setEnabled(false);
        const auto before = f.values();
        const auto start = f.node(0);
        f.curve->mouseDown(mouse(*f.curve, start, left));
        f.curve->mouseDrag(mouse(*f.curve, start + juce::Point<float>(-40.f, 15.f), left));
        f.curve->mouseUp(mouse(*f.curve, start, 0));
        check(f.values() == before && f.balanced(0, 0), "direct or inherited disable rejects a fresh pointer sequence");
    }
    for (int which : {0, 1})
    {
        Fixture f;
        if (f.curve == nullptr) continue;
        const auto start = f.node(which), other = f.node(1 - which);
        f.curve->mouseDown(mouse(*f.curve, start, left));
        f.curve->mouseDown(mouse(*f.curve, other, left));
        f.curve->mouseDrag(mouse(*f.curve, start + juce::Point<float>(-40.f, 14.f), left));
        f.curve->mouseUp(mouse(*f.curve, start, 0));
        check(f.balanced(which) && !f.host->getUndoableState().hasActiveParameterGesture(),
              "repeated mouseDown cannot replace the owning node or strand its host gestures");
    }
}
}
