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

// Only the message thread classifies callbacks as user edits. A worker writing
// an external value must not change that thread's recording-suppression flag:
// the message thread may simultaneously receive an unrelated user edit.
class ScopedMessageThreadWrite
{
public:
    explicit ScopedMessageThreadWrite(bool& writing)
    {
        if (onMessageThread())
        {
            flag = &writing;
            previous = writing;
            writing = true;
        }
    }
    ~ScopedMessageThreadWrite() { if (flag != nullptr) *flag = previous; }

private:
    bool* flag = nullptr;
    bool previous = false;
};
} // namespace

// A ValueTree property action alone cannot tell the host that replay is an edit. Retaining the
// affected parameter here lets the gesture begin before the APVTS's synchronous value callback,
// and avoids broadcasting gestures for every parameter in the instrument on every undo.
class UndoableParameterState::ParameterChangeAction final : public juce::UndoableAction
{
public:
    ParameterChangeAction(UndoableParameterState& s, int i, juce::ValueTree t,
                          juce::var before, juce::var after)
        : state(s), index(i), tree(std::move(t)), oldValue(std::move(before)), newValue(std::move(after)) {}

    bool perform() override { return apply(newValue); }
    bool undo() override { return apply(oldValue); }
    int getSizeInUnits() override { return 64; }
    juce::UndoableAction* createCoalescedAction(juce::UndoableAction* next) override
    {
        auto* other = dynamic_cast<ParameterChangeAction*>(next);
        if (other != nullptr && &state == &other->state && index == other->index && tree == other->tree)
            return new ParameterChangeAction(state, index, tree, oldValue, other->newValue);
        return nullptr;
    }

private:
    bool apply(const juce::var& value)
    {
        const juce::ScopedLock scoped(state.lock);
        auto* parameter = state.parameters[(size_t) index];
        const bool gesture = state.history.isPerformingUndoRedo() && parameter != nullptr
            && parameter->getValue() != parameter->convertTo0to1((float) value);
        const ScopedMessageThreadWrite guard(state.writing);
        if (gesture) parameter->beginChangeGesture();
        tree.setProperty("value", value, nullptr);
        if (gesture) parameter->endChangeGesture();
        return true;
    }

    UndoableParameterState& state;
    int index;
    juce::ValueTree tree;
    juce::var oldValue, newValue;
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
    if (! onMessageThread() || writing || history.isPerformingUndoRedo())
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
    openTransactionUsing(name, nullptr, [](void*, juce::UndoManager& manager, const juce::String& title) {
        manager.beginNewTransaction(title);
    });
}

void UndoableParameterState::openTransactionUsing(const juce::String& name, void* context, TransactionWriter writer)
{
    const juce::ScopedLock scoped(lock);
    // Gestures begun in the same message-loop pass are one action (for example a switch that sets two
    // parameters): only the first opens a transaction.
    if (groupOpen)
        return;
    syncExternal();   // what changed outside the history before this edit stays outside it
    writer(context, history, name);
    groupOpen = true;
    juce::MessageManager::callAsync([this, flag = std::weak_ptr<bool>(alive)]
    {
        if (auto live = flag.lock(); live != nullptr && *live)
            groupOpen = false;
    });
}

UndoableParameterState::HistorySnapshot UndoableParameterState::getHistorySnapshot() const
{
    return readHistoryUsing(nullptr, [](void*, const juce::UndoManager& manager) {
        return HistorySnapshot { manager.canUndo(), manager.canRedo(),
                                 manager.getUndoDescription(), manager.getRedoDescription() };
    });
}

UndoableParameterState::HistorySnapshot UndoableParameterState::readHistoryUsing(void* context, HistoryReader reader) const
{
    const juce::ScopedLock scoped(lock);
    return reader(context, history);
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
    const auto* raw = getRawParameterValue(p->getParameterID());
    if (raw == nullptr)
        return;
    const float value = raw->load(std::memory_order_relaxed);
    if (const auto* current = child.getPropertyPointer("value"); current != nullptr && (float) *current == value)
        return;
    const ScopedMessageThreadWrite guard(writing);
    if (undoManager != nullptr)
        undoManager->perform(new ParameterChangeAction(*this, index, child, child.getProperty("value"), value));
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
    syncExternal();
    if (onMessageThread())
        flushUser();
}

juce::ValueTree UndoableParameterState::copyState()
{
    const juce::ScopedLock scoped(lock);
    syncNow();
    auto snapshot = state.createCopy();
    // A worker must not flush a still-open user gesture into the live tree or
    // UndoManager, but the host snapshot must include its latest parameter value.
    for (auto* parameter : parameters)
    {
        if (parameter == nullptr) continue;
        auto child = snapshot.getChildWithProperty("id", parameter->getParameterID());
        if (auto* value = getRawParameterValue(parameter->getParameterID()); child.isValid() && value != nullptr)
            child.setProperty("value", value->load(std::memory_order_relaxed), nullptr);
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
