#include "UndoHistory.h"
#include "ParameterStateChildIndex.h"

namespace cs80
{

namespace
{
bool onMessageThread()
{
    auto* mm = juce::MessageManager::getInstanceWithoutCreating();
    return mm != nullptr && mm->isThisTheMessageThread();
}
} // namespace

// A ValueTree property action with the same first-value/final-value coalescing as JUCE's own
// action. Only replay adds host gestures: the first perform records an edit already delivered
// by the control. Resolve the live child exactly as APVTS does, rather than assuming a stored
// core index is also a host parameter index (memory controls have their own host IDs).
struct UndoableParameterState::ParameterValueAction final : juce::UndoableAction
{
    ParameterValueAction(UndoableParameterState& state, juce::ValueTree child,
                         juce::var oldValue, juce::var newValue, bool hadValue)
        : owner(state), lifetime(state.alive), target(std::move(child)), before(std::move(oldValue)),
          after(std::move(newValue)), hadProperty(hadValue) {}

    bool perform() override { return apply(after, true); }
    bool undo() override { return apply(before, hadProperty); }
    int getSizeInUnits() override { return (int) sizeof(*this); }

    juce::UndoableAction* createCoalescedAction(juce::UndoableAction* nextAction) override
    {
        const auto live = lifetime.lock();
        if (live == nullptr || !*live)
            return nullptr;
        if (auto* next = dynamic_cast<ParameterValueAction*>(nextAction);
            next != nullptr && hadProperty && next->hadProperty
                && &next->owner == &owner && next->target == target)
            return new ParameterValueAction(owner, target, before, next->after, true);
        return nullptr;
    }

private:
    bool apply(const juce::var& value, bool present)
    {
        const auto live = lifetime.lock();
        if (live == nullptr || !*live)
            return false; // an externally retained action must never dereference its former owner
        if (target.hasProperty("value") == present
            && (!present || target.getProperty("value") == value))
            return true; // no ValueTree notification means no APVTS update either
        juce::RangedAudioParameter* changed = nullptr;
        if (owner.history.isPerformingUndoRedo() && target.hasType("PARAM")
            && target.getParent() == owner.state)
        {
            const auto id = target.getProperty("id").toString();
            if (auto* parameter = owner.getParameter(id))
                if (const auto* raw = owner.getRawParameterValue(id))
                {
                    // Match APVTS ParameterAdapter::setDenormalisedValue: an already-current
                    // or approximately-equal physical value emits no host update, so no gesture.
                    const float desired = present ? (float) value
                        : parameter->convertFrom0to1(parameter->getDefaultValue());
                    if (!juce::approximatelyEqual(desired, raw->load(std::memory_order_relaxed)))
                        changed = parameter;
                }
        }
        struct Gesture
        {
            explicit Gesture(juce::RangedAudioParameter* p) : parameter(p)
            { if (parameter != nullptr) parameter->beginChangeGesture(); }
            ~Gesture() { if (parameter != nullptr) parameter->endChangeGesture(); }
            juce::RangedAudioParameter* parameter;
        } gesture(changed);
        if (present)
            target.setProperty("value", value, nullptr);
        else
            target.removeProperty("value", nullptr);
        return true;
    }

    UndoableParameterState& owner;
    const std::weak_ptr<bool> lifetime;
    juce::ValueTree target;
    const juce::var before, after;
    const bool hadProperty;
};

UndoableParameterState::UndoableParameterState(juce::AudioProcessor& processor, juce::UndoManager& undoManager,
                                               const juce::Identifier& valueTreeType, ParameterLayout layout)
    : juce::AudioProcessorValueTreeState(processor, &undoManager, valueTreeType, std::move(layout)),
      owner(processor), history(undoManager)
{
    const auto& all = owner.getParameters();
    parameters.reserve((size_t) all.size());
    externalPending = std::make_unique<std::atomic<bool>[]>((size_t) all.size());
    userPending = std::make_unique<std::atomic<bool>[]>((size_t) all.size());
    for (int i = 0; i < all.size(); ++i)
    {
        parameters.push_back(dynamic_cast<juce::RangedAudioParameter*>(all.getUnchecked(i)));
        externalPending[(size_t) i] = false;
        userPending[(size_t) i] = false;
        all.getUnchecked(i)->addListener(this);
    }
    // One transaction per gesture; keep a long history but bound its memory.
    history.setMaxNumberOfStoredUnits(400000, 64);
}

UndoableParameterState::~UndoableParameterState()
{
    *alive = false;
    for (auto* p : owner.getParameters())
        p->removeListener(this);
}

void UndoableParameterState::parameterValueChanged(int index, float)
{
    if (index < 0 || index >= (int) parameters.size())
        return;
    if (onMessageThread())
    {
        if (writing || history.isPerformingUndoRedo())
            return;   // our own tree writes, or undo/redo replaying the tree: nothing new to record
        if (openGestures > 0 || userScopes > 0)
        {
            userPending[(size_t) index].store(true, std::memory_order_relaxed);
            return;
        }
    }
    // Host automation, MIDI learn, preset loads, programmatic setters: never history.
    externalPending[(size_t) index].store(true, std::memory_order_release);
}

void UndoableParameterState::parameterGestureChanged(int index, bool starting)
{
    if (! onMessageThread() || history.isPerformingUndoRedo())
        return;
    if (starting)
    {
        if (openGestures++ == 0 && userScopes == 0)
            openTransaction(index >= 0 && index < (int) parameters.size() && parameters[(size_t) index] != nullptr
                                ? parameters[(size_t) index]->getName(40) : juce::String("edit"));
    }
    else
    {
        openGestures = juce::jmax(0, openGestures - 1);
        flushUser();   // the gesture's final values belong to its own transaction
    }
}

void UndoableParameterState::openTransaction(const juce::String& name)
{
    // Gestures begun in the same message-loop pass are one action (for example a switch that sets two
    // parameters): only the first opens a transaction.
    if (groupOpen)
        return;
    syncExternal();   // what changed outside the history before this edit stays outside it
    history.beginNewTransaction(name);
    groupOpen = true;
    juce::MessageManager::callAsync([this, flag = std::weak_ptr<bool>(alive)]
    {
        if (auto live = flag.lock(); live != nullptr && *live)
            groupOpen = false;
    });
}

bool UndoableParameterState::hasActiveUserEdit() const
{
    return onMessageThread() && userScopes > 0 && ! history.isPerformingUndoRedo();
}

void UndoableParameterState::beginUserEdit(const juce::String& name)
{
    jassert(onMessageThread());
    if (userScopes++ == 0 && openGestures == 0)
    {
        groupOpen = false;
        openTransaction(name);
    }
}

void UndoableParameterState::endUserEdit()
{
    jassert(onMessageThread());
    userScopes = juce::jmax(0, userScopes - 1);
    flushUser();
}

void UndoableParameterState::write(int index, juce::UndoManager* undoManager)
{
    auto* p = parameters[(size_t) index];
    if (p == nullptr)
        return;
    auto child = state.getChildWithProperty("id", p->getParameterID());
    if (! child.isValid())
        return;
    const float value = p->convertFrom0to1(p->getValue());
    if (const auto* current = child.getPropertyPointer("value"); current != nullptr && (float) *current == value)
        return;
    const juce::ScopedValueSetter<bool> guard(writing, true);
    if (undoManager != nullptr)
        undoManager->perform(new ParameterValueAction(*this, child, child.getProperty("value"),
                                                       value, child.hasProperty("value")));
    else
        child.setProperty("value", value, nullptr);
}

void UndoableParameterState::syncExternal()
{
    const juce::ScopedLock scoped(lock);
    for (int i = 0; i < (int) parameters.size(); ++i)
        if (externalPending[(size_t) i].exchange(false, std::memory_order_acq_rel))
            write(i, nullptr);
}

void UndoableParameterState::flushUser()
{
    const juce::ScopedLock scoped(lock);
    for (int i = 0; i < (int) parameters.size(); ++i)
        if (userPending[(size_t) i].exchange(false, std::memory_order_acq_rel))
            write(i, &history);
}

void UndoableParameterState::syncNow()
{
    if (! onMessageThread())
        return;
    syncExternal();
    flushUser();
}

bool UndoableParameterState::undo() { return replayHistory(false); }
bool UndoableParameterState::redo() { return replayHistory(true); }

bool UndoableParameterState::replayHistory(bool redo)
{
    // Never nest a host gesture inside an original edit or a callback from a tree write/replay.
    // These counters are message-thread-owned, so check the thread before reading them.
    if (!onMessageThread() || openGestures != 0 || userScopes != 0 || writing
        || replayingHistory || history.isPerformingUndoRedo())
        return false;
    const juce::ScopedValueSetter<bool> replayGuard(replayingHistory, true);
    const juce::ScopedLock scoped(lock);
    syncNow();
    if (!(redo ? history.canRedo() : history.canUndo()))
        return false;
    groupOpen = false; // the next edit must name its new transaction, even without a loop turn
    return redo ? history.redo() : history.undo();
}

juce::ValueTree UndoableParameterState::copyStateForSave()
{
    const juce::ScopedLock scoped(lock);
    auto snapshot = state.createCopy();
    detail::ParameterStateChildIndex index(snapshot);
    for (auto* parameter : parameters)
    {
        if (parameter == nullptr)
            continue;
        const auto& id = parameter->getParameterID();
        const auto* raw = getRawParameterValue(id);
        if (raw == nullptr)
            continue;
        // This is the same denormalised atomic that JUCE flushes to its tree.
        // Write only to the detached copy: pending live edits retain their
        // original user/automation classification until the normal flush.
        const float value = raw->load(std::memory_order_relaxed);
        auto& matches = index.matches(id);
        const bool found = ! matches.empty();
        for (auto child : matches)
            child.setProperty("value", value, nullptr);
        // JUCE accepts all direct children with a known id, with the last
        // duplicate winning. Keep every duplicate current in a saved state.
        if (!found)
        {
            juce::ValueTree child("PARAM");
            child.setProperty("id", id, nullptr);
            child.setProperty("value", value, nullptr);
            snapshot.appendChild(child, nullptr);
            matches.push_back(child);
        }
    }
    return snapshot;
}

void UndoableParameterState::timerCallback()
{
    syncExternal();
    if (openGestures > 0 || userScopes > 0)
        flushUser();   // a long drag keeps the tree current; the steps coalesce into its transaction
}

} // namespace cs80
