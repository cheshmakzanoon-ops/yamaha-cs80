#pragma once

#include <juce_core/juce_core.h>

namespace cs80::ui
{
// The filename is a storage/favourite key, not the patch's display name.
// Match loadUserPreset's metadata semantics, including an explicitly empty
// name. Only accepted preset documents may supply display metadata.
inline juce::String presetDisplayName(const juce::XmlElement* xml,
                                      const juce::String& filenameStem)
{
    return xml != nullptr && xml->hasTagName("CS80_PARAMS")
        ? xml->getStringAttribute("presetName", filenameStem) : filenameStem;
}
} // namespace cs80::ui
