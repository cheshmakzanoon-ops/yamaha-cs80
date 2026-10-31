#include "EditorPreferences.h"

#include <juce_data_structures/juce_data_structures.h>

#include <algorithm>
#include <memory>

namespace cs80::panel
{
namespace
{
juce::File& testStorage()
{
    static juce::File file;
    return file;
}

std::unique_ptr<juce::PropertiesFile> openStore()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "CS-80 Reference";
    options.filenameSuffix = ".settings";
    options.folderName = "CS-80 Reference";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    if (testStorage() != juce::File())
        return std::make_unique<juce::PropertiesFile>(testStorage(), options);
    return std::make_unique<juce::PropertiesFile>(options);
}

constexpr const char* zoomKey = "editorZoomPercent";
} // namespace

bool EditorPreferences::isZoomPreset(int percent) noexcept
{
    return std::find(zoomPresets.begin(), zoomPresets.end(), percent) != zoomPresets.end();
}

int EditorPreferences::loadZoomPercent()
{
    const int value = openStore()->getIntValue(zoomKey, 0);
    return isZoomPreset(value) ? value : 0;
}

void EditorPreferences::saveZoomPercent(int percent)
{
    if (! isZoomPreset(percent))
        return;
    auto store = openStore();
    store->setValue(zoomKey, percent);
    store->saveIfNeeded();
}

void EditorPreferences::setStorageFileForTesting(const juce::File& file)
{
    testStorage() = file;
}

juce::File EditorPreferences::getStorageFileForTesting()
{
    return testStorage();
}
} // namespace cs80::panel
