// Offline evidence helper only. Build twice with A45_AUDIT_SOURCE selecting
// the frozen and repaired audit sources, and link identical frozen core objects.
// Both runs use ordinary process settings. Hexadecimal floats retain every bit
// of the three actual measure() results without changing its audio operations.
#ifndef A45_AUDIT_SOURCE
#error A45_AUDIT_SOURCE must name the audited source
#endif
#define main a45_unused_original_audit_main
#include A45_AUDIT_SOURCE
#undef main

int main()
{
    const auto& programs = programLibrary();
    std::set<std::size_t> selected {
        0, static_cast<std::size_t>(legacyProgramCount - 1),
        static_cast<std::size_t>(legacyProgramCount), programs.size() - 1
    };
    for (const int model : { body::piano, body::clav, body::bowed,
                             body::flute, body::organ })
    {
        const auto found = std::find_if(programs.begin(), programs.end(),
            [model](const auto& preset) {
                return preset.values[P_body1Model] == model
                    || preset.values[P_body2Model] == model;
            });
        if (found == programs.end()) return 2;
        selected.insert(static_cast<std::size_t>(found - programs.begin()));
    }
    for (const auto index : selected)
    {
        const auto& preset = programs[index];
        const auto result = measure(preset);
        std::printf("%zu\t%s\t%s\t%a\t%a\t%a\t%d\t%d\n",
                    index, preset.name.c_str(), preset.category.c_str(),
                    result.phrasePeak, result.windowRms, result.stressPeak,
                    result.finite, result.circuit);
        if (!result.finite || !result.circuit) return 3;
    }
    return 0;
}
