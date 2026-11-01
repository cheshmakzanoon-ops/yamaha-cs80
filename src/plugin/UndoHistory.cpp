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

// APVTS's ordinary ValueTree SetPropertyAction restores the parameter value
// during UndoManager replay, but never brackets that host notification in a
// begin/end gesture. Use the same property/UndoManager mechanism with a
// parameter-specific action: the initial perform is already inside the UI's
// own gesture, and only subsequent undo/redo replays need a new host gesture.
// This does not wrap unrelated MSEG/matrix ValueTree transactions, and it
// preserves JUCE's consecutive-property-action coalescing for long drags.
class ParameterValueAction final : public juce::UndoableAction
{
public:
    ParameterValueAction(juce::ValueTree target, juce::RangedAudioParameter& parameter,
                         juce::var previous, juce::var next,
                         bool addingProperty = false, bool alreadyPerformed = false)
        : child(std::move(target)), hostParameter(parameter), before(std::move(previous)),
          after(std::move(next)), wasAddingProperty(addingProperty), firstPerform(! alreadyPerformed)
    {
    }

    bool perform() override
    {
        const bool isReplay = ! firstPerform;
        firstPerform = false;
        return apply(after, isReplay);
    }

    bool undo() override { return apply(before, true, wasAddingProperty); }

    int getSizeInUnits() override { return (int) sizeof(*this); }

    juce::UndoableAction* createCoalescedAction(juce::UndoableAction* next) override
    {
        if (! wasAddingProperty)
            if (auto* candidate = dynamic_cast<ParameterValueAction*>(next))
                if (! candidate->wasAddingProperty && candidate->child == child
                    && &candidate->hostParameter == &hostParameter)
                    return new ParameterValueAction(child, hostParameter, before, candidate->after, false, true);
        return nullptr;
    }

private:
    bool apply(const juce::var& value, bool replay, bool remove = false)
    {
        struct ScopedHostGesture
        {
            explicit ScopedHostGesture(juce::RangedAudioParameter& p) : parameter(p) { parameter.beginChangeGesture(); }
            ~ScopedHostGesture() { parameter.endChangeGesture(); }
            juce::RangedAudioParameter& parameter;
        };
        if (replay)
        {
            ScopedHostGesture gesture(hostParameter);
            if (remove) child.removeProperty("value", nullptr);
            else child.setProperty("value", value, nullptr);
        }
        else
        {
            if (remove) child.removeProperty("value", nullptr);
            else child.setProperty("value", value, nullptr);
        }
        return true;
    }

    juce::ValueTree child;
    juce::RangedAudioParameter& hostParameter;
    juce::var before, after;
    bool wasAddingProperty = false;
    bool firstPerform = true;
};
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
    // A replay action itself opens a host gesture. It must not open a second
    // user transaction or record the same undo operation back into history.
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
    const auto* currentProperty = child.getPropertyPointer("value");
    if (currentProperty != nullptr && (float) *currentProperty == value)
        return;
    const juce::ScopedValueSetter<bool> guard(writing, true);
    if (undoManager != nullptr)
        undoManager->perform(new ParameterValueAction(child, *p,
            currentProperty != nullptr ? *currentProperty : juce::var(), value, currentProperty == nullptr));
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

void UndoableParameterState::timerCallback()
{
    syncExternal();
    if (openGestures > 0 || userScopes > 0)
        flushUser();   // a long drag keeps the tree current; the steps coalesce into its transaction
}

} // namespace cs80
