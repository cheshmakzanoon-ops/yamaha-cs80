#include "core/FactoryPresets.h"
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace cs80;

void u64(std::ostream& f, std::uint64_t n) {
    for (int i = 0; i < 8; ++i) f.put(static_cast<char>((n >> (8 * i)) & 255));
}
void string(std::ostream& f, const std::string& s) {
    u64(f, s.size()); f.write(s.data(), static_cast<std::streamsize>(s.size()));
}
bool bank(const std::filesystem::path& path, const std::vector<FactoryPreset>& presets) {
    std::ofstream f(path, std::ios::binary);
    u64(f, presets.size()); u64(f, P_numParams);
    for (const auto& p : presets) {
        string(f, p.name); string(f, p.category); string(f, p.description);
        u64(f, static_cast<int>(p.library)); u64(f, p.tags.size());
        for (const auto& tag : p.tags) string(f, tag);
        for (float value : p.values.v) {
            auto bits = std::bit_cast<std::uint32_t>(value);
            for (int i = 0; i < 4; ++i) f.put(static_cast<char>((bits >> (8 * i)) & 255));
        }
    }
    return static_cast<bool>(f);
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path out(argv[1]); std::filesystem::create_directories(out);
    if (!bank(out / "legacy.bin", legacyLibrary()) ||
        !bank(out / "curated.bin", curatedLibrary()) ||
        !bank(out / "host.bin", programLibrary())) return 1;
    std::ofstream midi(out / "midi.bin", std::ios::binary);
    for (int msb = 0; msb < 3; ++msb)
        for (int lsb = 0; lsb < 128; ++lsb)
            for (int pc = 0; pc < 128; ++pc)
                u64(midi, static_cast<std::int64_t>(programIndexForMidi(msb, lsb, pc)));
    std::ofstream index(out / "index.bin", std::ios::binary);
    for (int i = -1; i <= 302; ++i) u64(index, static_cast<std::int64_t>(programIndexForCurated(i)));
    for (int i = -1; i <= 524; ++i) u64(index, static_cast<std::int64_t>(curatedIndexForProgram(i)));
    int memory = 0;
    for (int local = 0; local < CP_count; ++local)
        if (local != CP_feet && local != CP_pan && local != CP_hpfOn && local != CP_lpfOn) ++memory;
    std::cout << "{\"legacy_count\":" << legacyLibrary().size()
              << ",\"curated_count\":" << curatedLibrary().size()
              << ",\"host_count\":" << programLibrary().size()
              << ",\"core_parameters\":" << P_numParams
              << ",\"memory_controls\":" << memory
              << ",\"host_parameter_count_by_layout\":" << P_numParams + 2 + 4 * memory
              << ",\"midi_cases\":49152,\"index_cases\":830}\n";
    return midi && index ? 0 : 1;
}
