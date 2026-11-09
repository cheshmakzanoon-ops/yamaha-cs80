#pragma once

#include "plugin/PluginProcessor.h"
#include "plugin/UndoHistory.h"
#include "SessionUndoHostTests.h"
#include "AbCompareHistoryHostTests.h"
#include "RandomiseHistoryHostTests.h"

#include <cmath>
#include <functional>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace cs80::tests
{
namespace undo_replay_detail
{
struct BareProcessor final : juce::AudioProcessor
{
    const juce::String getName() const override { return "Undo replay fixture"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};
struct Probe final : juce::AudioProcessorParameter::Listener
{
    using Event = std::pair<int, char>;
    explicit Probe(juce::AudioProcessor& p) : owner(p)
    { for (auto* parameter : owner.getParameters()) parameter->addListener(this); }
    ~Probe() override
    { for (auto* parameter : owner.getParameters()) parameter->removeListener(this); }
    void parameterValueChanged(int index, float) override { events.emplace_back(index, 'V'); }
    void parameterGestureChanged(int index, bool start) override
    {
        events.emplace_back(index, start ? 'B' : 'E');
        if (onGesture) onGesture(start);
    }
    bool sequences(std::initializer_list<int> indices) const
    {
        std::vector<Event> expected;
        for (int index : indices)
            for (char kind : {'B', 'V', 'E'}) expected.emplace_back(index, kind);
        return events == expected;
    }
    juce::AudioProcessor& owner;
    std::vector<Event> events;
    std::function<void(bool)> onGesture;
};
struct Fixture
{
    Fixture() : state(owner, history, "STATE", layout()), probe(owner)
    { state.syncNow(); history.clearUndoHistory(); }
    static juce::AudioProcessorValueTreeState::ParameterLayout layout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout result;
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("p", 1), "First", juce::NormalisableRange<float>(0.0f, 1.0f), 0.25f));
        result.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID("q", 1), "Second", juce::NormalisableRange<float>(0.0f, 1.0f), 0.75f));
        return result;
    }
    auto* p(const char* id = "p") { return state.getParameter(id); }
    juce::ValueTree child(const char* id = "p") { return state.state.getChildWithProperty("id", id); }
    void edit(const char* id, float value)
    { auto* parameter = p(id); parameter->beginChangeGesture(); parameter->setValueNotifyingHost(value); parameter->endChangeGesture(); }
    juce::UndoManager history;
    BareProcessor owner;
    UndoableParameterState state;
    Probe probe;
};
struct TreeCallback final : juce::ValueTree::Listener
{
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) override
    { if (property == juce::Identifier("value") && callback) callback(); }
    std::function<void()> callback;
};
}

template<class Check>
void testUndoReplayGestures(Check check)
{
    using namespace undo_replay_detail;
    {
        Fixture f;
        check(!f.state.undo() && !f.state.redo() && f.probe.events.empty(), "empty history produces no host gesture");
        f.edit("p", 0.8f);
        check(f.probe.sequences({0}) && f.history.getUndoDescriptions().size() == 1,
              "recording the initial edit does not add a second host gesture");
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            f.probe.events.clear();
            check(f.state.undo() && f.p()->getValue() == 0.25f && f.probe.sequences({0}),
                  "undo orders one parameter begin, value, end");
            check(!f.history.canUndo() && f.history.canRedo(), "undo gesture cannot add a new history step");
            f.probe.events.clear();
            check(f.state.redo() && f.p()->getValue() == 0.8f && f.probe.sequences({0}),
                  "redo orders one parameter begin, value, end");
            check(f.history.getUndoDescriptions().size() == 1 && !f.history.canRedo(), "redo gesture cannot add a history step");
        }
    }
    {
        Fixture f;
        f.p()->beginChangeGesture();
        int units = 0;
        for (int i = 0; i < 6; ++i)
        {
            f.p()->setValueNotifyingHost(0.4f + (float) i * 0.08f);
            f.state.syncNow(); // exercises the same mid-gesture flush/coalesce path as the timer
            if (i == 0) units = f.history.getNumberOfUnitsTakenUpByStoredCommands();
            check(f.history.getNumberOfUnitsTakenUpByStoredCommands() == units, "adjacent writes coalesce without retaining intermediate actions");
        }
        const auto selected = f.p()->getValue();
        f.p()->endChangeGesture();
        f.probe.events.clear();
        check(f.state.undo() && f.p()->getValue() == 0.25f && f.probe.sequences({0}), "coalesced first undo retains the original value and gesture");
        f.probe.events.clear();
        check(f.state.redo() && f.p()->getValue() == selected && f.probe.sequences({0}), "coalesced first redo retains the final value and gesture");
    }
    {
        Fixture f;
        {
            UndoableParameterState::ScopedUserEdit edit(f.state, "Interleaved");
            f.p()->setValueNotifyingHost(0.4f); f.state.syncNow();
            f.p("q")->setValueNotifyingHost(0.1f); f.state.syncNow();
            f.p()->setValueNotifyingHost(0.9f); f.state.syncNow();
        }
        check(f.history.getUndoDescriptions().size() == 1, "interleaved values remain one user transaction");
        f.probe.events.clear();
        check(f.state.undo() && f.probe.sequences({0, 1, 0}), "interleaved undo preserves reverse action order and balanced per-action gestures");
        check(f.p()->getValue() == 0.25f && f.p("q")->getValue() == 0.75f, "interleaved undo restores both original values");
        f.probe.events.clear();
        check(f.state.redo() && f.probe.sequences({0, 1, 0}), "interleaved redo preserves forward action order");
        check(f.p()->getValue() == 0.9f && f.p("q")->getValue() == 0.1f, "interleaved redo restores final values");
    }
    {
        Fixture f;
        f.edit("p", 0.8f); f.state.undo();
        f.p("q")->beginChangeGesture();
        const auto descriptions = f.history.getRedoDescriptions();
        f.probe.events.clear();
        check(!f.state.undo() && !f.state.redo() && f.probe.events.empty(), "active original gesture rejects undo and redo without nesting");
        check(f.history.getRedoDescriptions() == descriptions && f.p()->getValue() == 0.25f, "active-gesture rejection leaves history and values intact");
        f.p("q")->endChangeGesture();
        check(f.probe.events == std::vector<Probe::Event>{{1, 'E'}}, "original gesture still ends exactly once after rejection");
        f.probe.events.clear();
        check(f.state.redo() && f.probe.sequences({0}), "replay is available once the original gesture ends");
        {
            UndoableParameterState::ScopedUserEdit edit(f.state, "Open scope");
            f.probe.events.clear();
            check(!f.state.undo() && !f.state.redo() && f.probe.events.empty(), "open ScopedUserEdit rejects both replay directions");
        }
        bool undone = true, redone = true;
        std::thread worker([&] { undone = f.state.undo(); redone = f.state.redo(); }); worker.join();
        check(!undone && !redone && f.p()->getValue() == 0.8f && f.history.canUndo(), "off-message-thread replay is rejected without mutation");
        f.probe.events.clear();
        int attempts = 0, admitted = 0;
        f.probe.onGesture = [&](bool begin) { if (begin) { ++attempts; admitted += f.state.undo() ? 1 : 0; admitted += f.state.redo() ? 1 : 0; } };
        check(f.state.undo() && attempts == 1 && admitted == 0 && f.probe.sequences({0}), "reentrant replay from a host gesture callback is rejected");
        f.probe.onGesture = {};
        f.edit("q", 0.2f);
        check(!f.history.canRedo() && f.history.getUndoDescription() == "Second", "first post-undo edit starts a correctly named transaction without waiting for the loop");
        f.probe.events.clear();
        check(f.state.undo() && f.probe.sequences({1}), "replay did not corrupt subsequent user gesture counters");
    }
    {
        Fixture f;
        f.edit("p", 0.4f);
        TreeCallback listener;
        int attempts = 0, admitted = 0;
        listener.callback = [&]
        {
            if (attempts++ == 0)
            { admitted += f.state.undo() ? 1 : 0; admitted += f.state.redo() ? 1 : 0; }
        };
        f.state.state.addListener(&listener);
        f.edit("p", 0.8f);
        f.state.state.removeListener(&listener);
        check(attempts == 1 && admitted == 0 && f.p()->getValue() == 0.8f,
              "a ValueTree callback cannot replay history during its original recording write");
        f.probe.events.clear();
        check(f.state.undo() && f.p()->getValue() == 0.25f && f.probe.sequences({0}),
              "rejecting a recording-write callback preserves coalescing and the first old value");
    }
    {
        Fixture f;
        f.edit("p", 0.8f);
        std::thread automation([&] { f.p("q")->setValueNotifyingHost(0.42f); }); automation.join();
        f.probe.events.clear();
        check(f.state.undo() && f.probe.sequences({0}) && f.p("q")->getValue() == 0.42f,
              "pending external automation stays outside the undo transaction");
        check(!f.history.canUndo() && f.history.canRedo(), "external synchronization adds no replay history");
        f.probe.events.clear();
        check(f.state.redo() && f.probe.sequences({0}) && f.p("q")->getValue() == 0.42f,
              "redo preserves unrelated external automation");
    }
    for (bool approximate : {false, true})
    {
        Fixture f;
        f.edit("p", 0.8f);
        const float external = approximate ? std::nextafter(0.25f, 1.0f) : 0.25f;
        f.p()->setValueNotifyingHost(external); f.state.syncNow(); f.probe.events.clear();
        check(f.state.undo() && f.probe.events.empty() && f.p()->getValue() == external,
              "already-current and approximately-equal replay emits no empty host gesture");
        check((float) f.child().getProperty("value") == 0.25f && f.history.canRedo(), "no-op replay still restores its recorded tree value/history position");
        f.probe.events.clear();
        check(f.state.redo() && f.probe.sequences({0}) && f.p()->getValue() == 0.8f, "redo after no-op undo remains effective and bracketed");
    }
    {
        Fixture f;
        f.child().removeProperty("value", nullptr); f.history.clearUndoHistory(); f.probe.events.clear();
        f.edit("p", 0.8f); f.probe.events.clear();
        check(f.state.undo() && !f.child().hasProperty("value") && f.p()->getValue() == 0.25f && f.probe.sequences({0}),
              "missing old property is removed on undo with the APVTS default fallback");
        f.probe.events.clear();
        check(f.state.redo() && f.child().hasProperty("value") && f.p()->getValue() == 0.8f && f.probe.sequences({0}),
              "redo recreates an originally missing property with a balanced gesture");
    }
    {
        Fixture f;
        f.child().setProperty("value", juce::String("0.25"), nullptr);
        f.edit("p", 0.8f); f.probe.events.clear();
        check(f.state.undo() && f.child().getProperty("value").isString() && f.probe.sequences({0}), "undo preserves original property var type, not just its float projection");
        f.probe.events.clear();
        check(f.state.redo() && f.child().getProperty("value").isDouble() && f.probe.sequences({0}), "redo preserves the new property's numeric representation");
    }
    {
        Fixture f;
        auto original = f.child(); auto duplicate = original.createCopy();
        duplicate.setProperty("value", 0.6f, nullptr); f.state.state.appendChild(duplicate, nullptr);
        f.state.syncNow(); f.history.clearUndoHistory(); f.edit("p", 0.8f); f.probe.events.clear();
        check(f.state.undo() && f.p()->getValue() == 0.6f && f.probe.sequences({0}), "duplicate-id replay follows the actual changed child, not an invented last-child rule");
        check((float) duplicate.getProperty("value") == 0.6f, "duplicate sibling remains unchanged by the recorded action");
        f.state.redo(); f.state.state.removeChild(original, nullptr); f.state.syncNow();
        const auto live = f.p()->getValue(); f.probe.events.clear();
        check(f.state.undo() && f.probe.events.empty() && f.p()->getValue() == live,
              "detached recorded child can replay without inventing a live parameter gesture");
    }
    {
        Fixture f;
        f.state.state.setProperty("label", "before", nullptr);
        f.history.beginNewTransaction("Side state");
        f.state.state.setProperty("label", "after", &f.history);
        check(f.state.undo() && f.probe.events.empty() && f.state.state.getProperty("label") == "before", "side-state-only undo emits no parameter gestures");
        check(f.state.redo() && f.probe.events.empty() && f.state.state.getProperty("label") == "after", "side-state-only redo emits no parameter gestures");
        {
            UndoableParameterState::ScopedUserEdit edit(f.state, "Mixed");
            f.state.state.setProperty("label", "mixed", &f.history); f.p()->setValueNotifyingHost(0.8f);
        }
        f.probe.events.clear();
        check(f.state.undo() && f.probe.sequences({0}) && f.state.state.getProperty("label") == "after", "mixed undo brackets only its parameter action");
        f.probe.events.clear();
        check(f.state.redo() && f.probe.sequences({0}) && f.state.state.getProperty("label") == "mixed", "mixed redo retains metadata and parameter history order");
        auto replacement = f.state.copyStateForSave(); f.state.replaceState(replacement); f.probe.events.clear();
        check(!f.state.undo() && !f.state.redo() && f.probe.events.empty(), "state replacement clears old replay actions");
    }
    {
        juce::UndoManager retained;
        {
            BareProcessor owner;
            auto state = std::make_unique<UndoableParameterState>(owner, retained, "STATE", Fixture::layout());
            auto* p = state->getParameter("p"); p->beginChangeGesture(); p->setValueNotifyingHost(0.8f); p->endChangeGesture();
            check(retained.canUndo(), "lifetime fixture retains a parameter action");
            state.reset(); // retained action destruction must not use the former state/parameter
        }
        retained.clearUndoHistory();
        check(!retained.canUndo(), "history destruction is safe after its parameter-state owner is gone");
    }
    for (bool redo : {false, true})
    {
        juce::UndoManager retained;
        juce::UndoableAction* action = nullptr;
        {
            BareProcessor owner;
            auto state = std::make_unique<UndoableParameterState>(owner, retained, "STATE", Fixture::layout());
            auto* p = state->getParameter("p"); p->beginChangeGesture(); p->setValueNotifyingHost(0.8f); p->endChangeGesture();
            juce::Array<const juce::UndoableAction*> actions;
            retained.getActionsInCurrentTransaction(actions);
            if (actions.size() == 1) action = const_cast<juce::UndoableAction*>(actions[0]);
            if (redo) state->undo();
            check(redo ? retained.canRedo() : retained.canUndo(), "retained-lifetime fixture has the requested replay action");
            state.reset();
        }
        std::unique_ptr<juce::UndoableAction> coalesced(action != nullptr ? action->createCoalescedAction(action) : nullptr);
        check(action != nullptr && coalesced == nullptr, "an expired action cannot coalesce by dereferencing its former owner");
        // JUCE's bool means a command was found, not that every action succeeded.
        // Our expired action refuses execution; UndoManager then clears the failed history.
        const bool handled = redo ? retained.redo() : retained.undo();
        check(handled && !retained.canUndo() && !retained.canRedo(),
              "an expired parameter action refuses retained replay without touching its former owner");
    }
    {
        juce::TemporaryFile storage("-undo-replay-host");
        auto host = std::make_unique<Cs80AudioProcessor>(storage.getFile());
        Probe probe(*host);
        std::vector<juce::String> ids { Cs80AudioProcessor::parameterId(P_volume), Cs80AudioProcessor::parameterId(chParam(0, CP_saw)),
                                      Cs80AudioProcessor::hardwareSourceParameterId(0), Cs80AudioProcessor::hardwareSourceParameterId(1) };
        for (int bank = 0; bank < 4; ++bank)
            for (int local : { CP_saw, CP_pulse }) ids.push_back(Cs80AudioProcessor::memoryParameterId(bank, local));
        for (const auto& id : ids)
        {
            auto* p = host->getAPVTS().getParameter(id);
            check(p != nullptr, "real-host parameter exists"); if (p == nullptr) continue;
            host->getUndoableState().syncNow(); host->getUndoManager().clearUndoHistory();
            const float before = p->getValue(), after = before > 0.5f ? 0.0f : 1.0f;
            const int index = p->getParameterIndex();
            probe.events.clear(); p->beginChangeGesture(); p->setValueNotifyingHost(after); p->endChangeGesture();
            check(probe.sequences({index}), "real-host initial edit has exactly the control's original gesture");
            probe.events.clear();
            check(host->undo() && p->getValue() == before && probe.sequences({index}), "processor undo brackets only the actual host parameter, including memory IDs");
            probe.events.clear();
            check(host->redo() && p->getValue() == after && probe.sequences({index}), "processor redo brackets only the actual host parameter");
            p->beginChangeGesture(); probe.events.clear();
            check(!host->undo() && !host->redo() && probe.events.empty(), "processor entry points reject replay inside an active edit");
            p->endChangeGesture();
        }
    }
    // Existing storage/A-B/randomise regressions remain unchanged and run in the same initialized host.
    testSessionUndoHost(check);
    testAbCompareHistoryHost(check);
    testRandomiseHistoryHost(check);
}
} // namespace cs80::tests
