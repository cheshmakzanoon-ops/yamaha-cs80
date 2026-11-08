#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <map>
#include <vector>

namespace cs80::detail
{
// A per-call index for a detached state copy. Match the same stringified IDs
// as the save/restore loops, including non-PARAM children and duplicates.
// The caller keeps each ordered match list current when adding/removing a
// child; changing an indexed child's ID requires rebuilding the index.
class ParameterStateChildIndex
{
public:
    explicit ParameterStateChildIndex(const juce::ValueTree& state)
    {
        for (auto child : state)
            childrenById[child.getProperty("id").toString()].push_back(child);
    }

    std::vector<juce::ValueTree>& matches(const juce::String& id)
    {
        return childrenById[id];
    }

private:
    std::map<juce::String, std::vector<juce::ValueTree>> childrenById;
};
} // namespace cs80::detail
