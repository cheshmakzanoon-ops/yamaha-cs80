#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace cs80::panel
{
// Per-user editor preferences: presentation settings that are neither host parameters nor session state, so
// changing them never marks a song as edited and a project opens the same on any machine. Stored in a
// properties file in the user's application-data folder ("CS-80 Reference/CS-80 Reference.settings").
// Message thread only.
class EditorPreferences
{
public:
    // The zoom presets, as a percentage of the 1600 x 1000 design size.
    static constexpr std::array<int, 5> zoomPresets { 75, 100, 125, 150, 200 };
    static bool isZoomPreset(int percent) noexcept;

    // The last zoom preset the user chose, or 0 when none is stored (or the stored value is not a preset).
    static int loadZoomPercent();
    static void saveZoomPercent(int percent);

    // Tests and the snapshot tool point the store at a scratch file so they never read or write the user's
    // real preferences. An empty file restores the normal location.
    static void setStorageFileForTesting(const juce::File& file);
    static juce::File getStorageFileForTesting();
};
} // namespace cs80::panel
