#include "plugin/PresetDisplayName.h"

#include <iostream>

int main()
{
    int failures = 0;
    const auto check = [&](const char* text, const char* stem, const char* expected)
    {
        const auto xml = juce::XmlDocument::parse(juce::String(text));
        if (cs80::ui::presetDisplayName(xml.get(), stem) != expected)
        {
            std::cerr << "Unexpected preset display name: " << text << '\n';
            ++failures;
        }
    };
    check("<CS80_PARAMS presetName=\"Round: trip\"/>", "Round trip", "Round: trip");
    check("<CS80_PARAMS presetName=\"Round trip\"/>", "Round trip", "Round trip");
    check("<CS80_PARAMS presetName=\"External display\"/>", "Renamed file", "External display");
    check("<CS80_PARAMS/>", "Legacy name", "Legacy name");
    check("<CS80_PARAMS presetName=\"\"/>", "Empty metadata", "");
    check("<CS80_PARAMS presetName=\"  \"/>", "Whitespace metadata", "  ");
    check("<WRONG_ROOT presetName=\"Ignore me\"/>", "Wrong root", "Wrong root");
    check("<cs80_params presetName=\"Ignore me\"/>", "Wrong case", "Wrong case");
    check("<not valid XML", "Malformed", "Malformed");
    // Duplicate names are display labels, never inferred file identities.
    check("<CS80_PARAMS presetName=\"Init Patch\"/>", "User copy", "Init Patch");
    check("<CS80_PARAMS presetName=\"Same name\"/>", "First file", "Same name");
    check("<CS80_PARAMS presetName=\"Same name\"/>", "Second file", "Same name");
    return failures == 0 ? 0 : 1;
}
