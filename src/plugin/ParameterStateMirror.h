#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <cmath>

namespace juce { class AudioProcessorValueTreeState; }

namespace cs80
{
// A custom undo-state mirror must not be interpreted as a new parameter edit.
// The generated, reviewed JUCE callback consults this scope. It changes no JUCE
// class layout, and scopes are shared across translation units on this thread.
class ParameterStateMirrorScope final
{
public:
    ParameterStateMirrorScope(const juce::AudioProcessorValueTreeState* state,
                              const juce::ValueTree& target, float snapshot) noexcept
        : owner(state), child(target), parent(target.getParent()), expected(snapshot),
          previous(current), active(std::isfinite(snapshot))
    {
        if (active) current = this;
    }
    ~ParameterStateMirrorScope() noexcept { if (active) current = previous; }
    ParameterStateMirrorScope(const ParameterStateMirrorScope&) = delete;
    ParameterStateMirrorScope& operator=(const ParameterStateMirrorScope&) = delete;

    static bool suppresses(const juce::AudioProcessorValueTreeState* state,
                           const juce::ValueTree& target, const juce::Identifier& property) noexcept
    {
        if (current == nullptr || property.toString() != "value") return false;
        for (auto* scope = current; scope != nullptr; scope = scope->previous)
            if (scope->owner == state && scope->child == target && scope->parent == target.getParent())
                if (const auto* value = target.getPropertyPointer(property);
                    value != nullptr && (float) *value == scope->expected)
                    return true;
        return false;
    }

private:
    const juce::AudioProcessorValueTreeState* owner;
    juce::ValueTree child;
    juce::ValueTree parent;
    float expected;
    const ParameterStateMirrorScope* previous;
    bool active;
    static inline thread_local const ParameterStateMirrorScope* current = nullptr;
};
} // namespace cs80
