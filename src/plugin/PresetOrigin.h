#pragma once

#include <juce_core/juce_core.h>

namespace cs80
{
// Runtime provenance, not a saved-state field or a promise that the file still
// exists/contains the same bytes. File equality follows JUCE's platform policy.
// Unknown session origins must never be guessed from non-unique display names.
struct PresetOrigin
{
    enum class Kind { unknown, factory, userFile };
    Kind kind = Kind::unknown;
    juce::File file;

    static PresetOrigin factory() { return { Kind::factory, {} }; }
    static PresetOrigin userFile(const juce::File& file) { return { Kind::userFile, file }; }
    bool operator==(const PresetOrigin& other) const { return kind == other.kind && file == other.file; }
    bool operator!=(const PresetOrigin& other) const { return !(*this == other); }
};
} // namespace cs80
