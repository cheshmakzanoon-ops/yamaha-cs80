#include "UndoHistory.h"

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
    if (! onMessageThread())
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
    child.setProperty("value", value, undoManager);
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

void UndoableParameterState::timerCallback()
{
    syncExternal();
    if (openGestures > 0 || userScopes > 0)
        flushUser();   // a long drag keeps the tree current; the steps coalesce into its transaction
}

} // namespace cs80
