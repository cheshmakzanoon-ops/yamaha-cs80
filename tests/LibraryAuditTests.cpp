// Library audit for the curated circuit-engine library and the frozen Legacy
// Library (docs/workstreams/ws6-library.md). Software checks of the shipped
// program data and of rendered level on this engine; not hardware evidence.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace cs80;

namespace
{
// ---------------------------------------------------------------------------
// Acceptance criteria, fixed before the library was measured.
// ---------------------------------------------------------------------------
// "About 300" curated programs.
constexpr int kMinCurated = 280, kMaxCurated = 320;
// Musical test phrase (4-note chord or category phrase, velocity 0.9, full
// pressure): peak at least 1 dB under full scale, so default volume never clips.
constexpr double kMaxPhrasePeakDbfs = -1.0;
// Worst case (8-note chord, full velocity, full pressure) must stay below 0 dBFS.
constexpr double kMaxStressPeakDbfs = 0.0;
// Not silent: the loudest 50 ms window of the phrase must reach this RMS.
constexpr double kMinWindowRmsDbfs = -42.0;
// Distinct: the normalized L1 distance between any two curated programs must be
// at least the equivalent of an 8 % change of one full parameter range.
constexpr double kMinPairDistance = 0.08;
// The legacy bank: 222 programs whose names, categories, descriptions and
// values (indexes 0..P_sourcePerformance) hash to this FNV-1a digest, computed
// from the bank as it shipped before the curated library (commit 6719ce4).
constexpr std::uint64_t kLegacyDigest = 0x3628dd9acc109b2aull;
constexpr int kLegacyDigestLastIndex = 138;
// Level matching: the loudest-window RMS of every program lies in this window.
constexpr double kLevelWindowLowDbfs = -30.0, kLevelWindowHighDbfs = -12.0;
// Calibration targets used by --calibrate (CuratedLibraryLevels.inc).
constexpr double kTargetRmsDbfs = -18.0, kTargetSingleLineRmsDbfs = -22.0;
constexpr double kCalibratePhrasePeakDbfs = -3.0, kCalibrateStressPeakDbfs = -1.5;
constexpr double kMinVolume = 0.05, kMaxVolume = 1.0;
constexpr double kSampleRate = 44100.0;

int failures = 0;
void check(bool ok, const std::string& message)
{
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", message.c_str());
    }
}

std::uint64_t legacyDigest()
{
    const auto& bank = legacyLibrary();
    std::uint64_t h = 14695981039346656037ull;
    auto byte = [&](unsigned char b) { h ^= b; h *= 1099511628211ull; };
    auto u64 = [&](std::uint64_t n) { for (int i = 0; i < 8; ++i) { byte((unsigned char) (n & 255u)); n >>= 8; } };
    auto str = [&](const std::string& s) { u64(s.size()); for (char c : s) byte((unsigned char) c); };
    u64(bank.size());
    for (const auto& p : bank)
    {
        str(p.name); str(p.category); str(p.description);
        for (int i = 0; i <= kLegacyDigestLastIndex; ++i)
        {
            std::uint32_t bits;
            const float f = p.values[i];
            std::memcpy(&bits, &f, 4);
            u64(bits);
        }
    }
    return h;
}

std::string lower(std::string s)
{
    for (auto& c : s) c = (char) std::tolower((unsigned char) c);
    return s;
}

bool isSwitch(int i)
{
    for (int ch = 0; ch < 2; ++ch)
        if (i == chParam(ch, CP_saw) || i == chParam(ch, CP_pulse)
            || i == chParam(ch, CP_hpfOn) || i == chParam(ch, CP_lpfOn))
            return true;
    return i == P_chorusOn || i == P_chorusMode || i == P_outputMode || i == P_mpeOn || i == P_arpOn
        || i == P_subOriginalMode || i == P_footswitchPlugged || i == P_footswitchPressed
        || i == P_sustainAssign || i == P_glideAssign || i == P_footMode || i == P_outputLevel
        || i == P_parityMode || i == P_sourcePerformance || i == P_sustainMode;
}

bool isDiscrete(int i)
{
    if (i == chParam(0, CP_feet) || i == chParam(1, CP_feet)) return true;
    return i == P_glideMode || i == P_subWave || i == P_subOriginalFunction || i == P_lfo1Dest
        || i == P_lfo2Dest || i == P_lfo3Dest || i == P_fx1Type || i == P_fx2Type || i == P_fx3Type
        || i == P_arpMode || i == P_arpOctaves;
}

bool usesModernLayer(const ParamValues& v)
{
    return v[P_arpOn] >= 0.5f
        || (v[P_fx1Type] >= 0.5f && v[P_fx1Mix] > 0.0f) || (v[P_fx2Type] >= 0.5f && v[P_fx2Mix] > 0.0f)
        || (v[P_fx3Type] >= 0.5f && v[P_fx3Mix] > 0.0f)
        || (v[P_lfo1Depth] > 0.0f && v[P_lfo1Dest] >= 0.5f) || (v[P_lfo2Depth] > 0.0f && v[P_lfo2Dest] >= 0.5f)
        || (v[P_lfo3Depth] > 0.0f && v[P_lfo3Dest] >= 0.5f)
        || v[P_macro1] != 0.5f || v[P_macro2] != 0.5f || v[P_macro3] != 0.5f || v[P_macro4] != 0.5f
        || v[P_mpeOn] >= 0.5f;
}

struct Level
{
    double phrasePeak = 0.0, windowRms = 0.0, stressPeak = 0.0;
    bool finite = true;
    bool circuit = false;
};

std::vector<int> phraseNotes(const std::string& category)
{
    if (category == "Bass") return { 36, 43 };
    if (category == "Leads" || category == "Winds") return { 67 };
    if (category == "Bells") return { 72, 76, 79 };
    return { 48, 55, 60, 64 };
}

double dbfs(double x) { return 20.0 * std::log10(std::max(x, 1.0e-12)); }

// Renders one program as a musician would audition it, then a worst-case chord.
Level measure(const FactoryPreset& preset)
{
    Level out;
    std::vector<float> block(512 * 2);
    const int window = (int) (0.05 * kSampleRate);
    {
        Engine e;
        e.prepare(kSampleRate);
        e.setParams(preset.values);
        out.circuit = e.getRenderPolicy() == RenderPolicy::sourcePerformance;
        const auto notes = phraseNotes(preset.category);
        for (int n : notes) e.noteOn(n, 0.9f, 1);
        const int total = (int) (1.9 * kSampleRate);
        double windowSum = 0.0;
        int windowCount = 0;
        bool released = false;
        for (int done = 0; done < total; done += 256)
        {
            const double t = done / kSampleRate;
            // Pressure ramps in after the attack, as a player leans into a chord.
            const float pressure = (float) std::clamp((t - 0.3) / 0.3, 0.0, 1.0);
            if (!released) e.channelPressure(1, pressure);
            if (!released && t >= 1.2)
            {
                for (int n : notes) e.noteOff(n, 1);
                released = true;
            }
            e.render(block.data(), 256);
            for (int i = 0; i < 256; ++i)
            {
                const double l = block[(size_t) i * 2], r = block[(size_t) i * 2 + 1];
                if (!std::isfinite(l) || !std::isfinite(r)) out.finite = false;
                out.phrasePeak = std::max({ out.phrasePeak, std::abs(l), std::abs(r) });
                windowSum += 0.5 * (l * l + r * r);
                if (++windowCount == window)
                {
                    out.windowRms = std::max(out.windowRms, std::sqrt(windowSum / window));
                    windowSum = 0.0;
                    windowCount = 0;
                }
            }
        }
    }
    {
        Engine e;
        e.prepare(kSampleRate);
        e.setParams(preset.values);
        const int notes[8] = { 36, 43, 48, 55, 60, 64, 67, 72 };
        for (int n : notes) e.noteOn(n, 1.0f, 1);
        e.channelPressure(1, 1.0f);
        for (int done = 0; done < (int) (1.0 * kSampleRate); done += 256)
        {
            e.render(block.data(), 256);
            for (int i = 0; i < 512; ++i)
            {
                if (!std::isfinite(block[(size_t) i])) out.finite = false;
                out.stressPeak = std::max(out.stressPeak, (double) std::abs(block[(size_t) i]));
            }
        }
    }
    return out;
}

void testLegacyLibrary()
{
    const auto& legacy = legacyLibrary();
    check((int) legacy.size() == legacyProgramCount, "legacy library keeps 222 programs");
    check(&legacy == &factoryPresets(), "factoryPresets() is the frozen legacy bank");
    check(legacyDigest() == kLegacyDigest, "legacy names, categories, descriptions and values are byte-identical");
    const ParamValues defaults;
    for (const auto& p : legacy)
    {
        check(p.library == ProgramLibrary::legacy && p.tags.empty(), "legacy entry marked legacy: " + p.name);
        check(p.values[P_sourcePerformance] == 0.0f && p.values[P_subOriginalMode] == 0.0f,
              "legacy program keeps the legacy engine and SUB: " + p.name);
        for (int i = kLegacyDigestLastIndex + 1; i < P_numParams; ++i)
            check(p.values[i] == defaults[i], "parameters appended after the bank stay neutral in legacy programs");
    }
}

void testProgramList()
{
    const auto& curated = curatedLibrary();
    const auto& all = programLibrary();
    const int c = curatedProgramCount();
    const int L = legacyProgramCount;
    check(c == (int) curated.size() && (int) all.size() == L + c, "program list is legacy + curated");
    // Host program numbers 0..221 keep their shipped meaning: same name, same values.
    for (int i = 0; i < (int) all.size(); ++i)
    {
        const auto& expected = i < L ? legacyLibrary()[(size_t) i] : curated[(size_t) (i - L)];
        check(all[(size_t) i].name == expected.name && all[(size_t) i].values.v == expected.values.v
                  && all[(size_t) i].library == expected.library,
              "program list order");
    }
    for (int j = 0; j < c; ++j)
        check(programIndexForCurated(j) == L + j && curatedIndexForProgram(L + j) == j, "curated index round trip");
    check(programIndexForCurated(-1) == -1 && programIndexForCurated(c) == -1, "curated index bounds");
    check(curatedIndexForProgram(0) == -1 && curatedIndexForProgram(L - 1) == -1 && curatedIndexForProgram(L + c) == -1,
          "legacy programs have no curated index");
    // MIDI: MSB 0 curated, MSB 1 legacy, LSB pages of 128.
    check(programIndexForMidi(0, 0, 0) == L && programIndexForMidi(0, 0, 127) == L + 127, "MIDI bank 0/0");
    check(programIndexForMidi(0, 1, 0) == L + 128 && programIndexForMidi(0, 2, c - 257) == L + c - 1, "MIDI curated pages");
    check(programIndexForMidi(0, 2, c - 256) == -1, "MIDI past the curated library is unassigned");
    check(programIndexForMidi(1, 0, 5) == 5 && programIndexForMidi(1, 1, 221 - 128) == 221, "MIDI legacy pages");
    check(programIndexForMidi(1, 1, 222 - 128) == -1 && programIndexForMidi(2, 0, 0) == -1, "MIDI unassigned banks");
    check(programIndexForMidi(0, -1, 0) == -1 && programIndexForMidi(0, 0, 128) == -1, "MIDI invalid data");
}

const std::set<std::string> kCategories { "Brass", "Strings", "Pads", "Polysynth", "Keys", "Organ", "Winds",
                                          "Plucked", "Bass", "Leads", "Bells", "FX", "Sequences", "Init" };
const std::set<std::string> kTags { "aftertouch", "velocity", "brilliance-swell", "resonance", "glide", "init-bend",
                                    "ribbon", "ring-mod", "sub-mod", "pwm", "feet-layer", "dual-rank", "chorus",
                                    "tremolo", "kbd-contour", "noise", "sine", "detune", "percussive", "lfo", "modern" };
// Product, artist and film names that must never appear in a program name.
const char* const kForbidden[] = { "blade", "runner", "vangelis", "replicant", "yamaha", "cs-80", "cs80", "rhodes",
                                   "wurlitzer", "clavinet", "mellotron", "hammond", "leslie", "moog", "prophet",
                                   "oberheim", "juno", "jupiter", "arturia", "roland", "korg", "solina", "tears" };

void testCuratedData()
{
    const auto& curated = curatedLibrary();
    check((int) curated.size() >= kMinCurated && (int) curated.size() <= kMaxCurated,
          "about 300 curated programs (" + std::to_string(curated.size()) + ")");
    std::set<std::string> names, legacyNames;
    for (const auto& p : legacyLibrary()) legacyNames.insert(lower(p.name));
    std::set<std::string> categoriesSeen;
    for (const auto& p : curated)
    {
        const auto n = lower(p.name);
        check(!p.name.empty() && p.name.size() <= 28, "program name length: " + p.name);
        check(names.insert(n).second, "unique curated name: " + p.name);
        check(legacyNames.count(n) == 0, "curated name does not shadow a legacy name: " + p.name);
        for (const char* word : kForbidden)
            check(n.find(word) == std::string::npos, "no trademarked/film/artist name in: " + p.name);
        check(kCategories.count(p.category) == 1, "known category: " + p.category);
        categoriesSeen.insert(p.category);
        check(p.description.size() >= 12, "program has a technique description: " + p.name);
        check(p.library == ProgramLibrary::curated, "curated entry marked curated: " + p.name);
        std::set<std::string> tags;
        for (const auto& t : p.tags)
        {
            check(kTags.count(t) == 1, "tag in vocabulary: " + t + " (" + p.name + ")");
            check(tags.insert(t).second, "no duplicate tag: " + p.name);
        }
        const auto& v = p.values;
        // Per-program engine policy: every curated program is a circuit-engine program.
        check(v[P_sourcePerformance] == 1.0f && v[P_parityMode] == 0.0f, "circuit-engine policy set: " + p.name);
        check(v[P_subOriginalMode] == 1.0f, "original SUB oscillator: " + p.name);
        const ParamValues defaults;
        for (int i : { (int) P_footswitchPlugged, (int) P_footswitchPressed, (int) P_footMode, (int) P_outputLevel,
                       (int) P_ribbon, (int) P_masterTune, (int) P_tuningRef, (int) P_mpeOn })
            check(v[i] == defaults[i], "physical setup and tuning stay at defaults: " + p.name);
        for (int i = 0; i < P_numParams; ++i)
        {
            const auto& d = paramDef(i);
            const float x = v[i];
            check(std::isfinite(x) && x >= d.min && x <= d.max,
                  "parameter in range: " + p.name + " " + paramId(i) + "=" + std::to_string(x));
            if (isSwitch(i)) check(x == 0.0f || x == 1.0f, "switch is 0/1: " + p.name + " " + paramId(i));
            if (isDiscrete(i)) check(x == std::round(x), "discrete parameter is integral: " + p.name + " " + paramId(i));
        }
        // At least one audible source on a rank that the MIX lets through.
        bool audible = false;
        for (int ch = 0; ch < 2; ++ch)
        {
            const float weight = ch == 0 ? 1.0f - v[P_mix] : v[P_mix];
            const bool osc = v[chParam(ch, CP_saw)] >= 0.5f || v[chParam(ch, CP_pulse)] >= 0.5f
                             || v[chParam(ch, CP_noise)] > 0.05f;
            const bool source = (osc && v[chParam(ch, CP_vcfLevel)] > 0.05f) || v[chParam(ch, CP_sine)] > 0.05f;
            audible = audible || (weight > 0.05f && source
                                  && v[chParam(ch, CP_vLevel)] + v[chParam(ch, CP_initLevel)]
                                         + v[chParam(ch, CP_afterLevel)] > 0.05f);
        }
        check(audible, "program has an audible source: " + p.name);
        const bool modern = std::find(p.tags.begin(), p.tags.end(), "modern") != p.tags.end();
        check(modern == usesModernLayer(v), "modern-layer use is tagged 'modern' (and only then): " + p.name);
    }
    for (const auto& c : kCategories)
        check(categoriesSeen.count(c) == 1, "category is populated: " + c);
}

void testDistinct()
{
    const auto& curated = curatedLibrary();
    double closest = 1.0e9;
    std::string closestPair;
    for (size_t a = 0; a < curated.size(); ++a)
        for (size_t b = a + 1; b < curated.size(); ++b)
        {
            double distance = 0.0;
            for (int i = 0; i < P_numParams; ++i)
                distance += std::abs(normaliseParam(i, curated[a].values[i]) - normaliseParam(i, curated[b].values[i]));
            if (distance < closest)
            {
                closest = distance;
                closestPair = curated[a].name + " / " + curated[b].name;
            }
            check(distance >= kMinPairDistance,
                  "distinct programs: " + curated[a].name + " / " + curated[b].name + " d=" + std::to_string(distance));
        }
    std::printf("closest curated pair: %s (normalized L1 %.3f)\n", closestPair.c_str(), closest);
}

std::vector<Level> measureAll()
{
    const auto& curated = curatedLibrary();
    std::vector<Level> levels(curated.size());
    std::atomic<size_t> next { 0 };
    auto worker = [&] {
        for (size_t i; (i = next.fetch_add(1)) < curated.size();)
            levels[i] = measure(curated[i]);
    };
    std::thread helper(worker);
    worker();
    helper.join();
    return levels;
}

bool singleLine(const std::string& category)
{
    return category == "Leads" || category == "Winds" || category == "Bass" || category == "Sequences";
}

void testLevels()
{
    const auto& curated = curatedLibrary();
    const auto levels = measureAll();
    const bool verbose = std::getenv("CS80_LIBRARY_AUDIT_VERBOSE") != nullptr;
    double loudest = -1.0e9, quietest = 1.0e9, worstStress = -1.0e9, loudestRms = -1.0e9;
    for (size_t i = 0; i < curated.size(); ++i)
    {
        const auto& p = curated[i];
        const auto& l = levels[i];
        const double peak = dbfs(l.phrasePeak), rms = dbfs(l.windowRms), stress = dbfs(l.stressPeak);
        loudest = std::max(loudest, peak);
        quietest = std::min(quietest, rms);
        loudestRms = std::max(loudestRms, rms);
        worstStress = std::max(worstStress, stress);
        if (verbose)
            std::printf("%-28s %-10s vol %.3f peak %6.1f  rms %6.1f  stress %6.1f dBFS\n", p.name.c_str(),
                        p.category.c_str(), p.values[P_volume], peak, rms, stress);
        check(l.circuit, "program renders on the circuit engine: " + p.name);
        check(l.finite, "finite audio: " + p.name);
        check(peak <= kMaxPhrasePeakDbfs, "phrase peak below -1 dBFS: " + p.name + " " + std::to_string(peak));
        check(stress < kMaxStressPeakDbfs, "8-voice stress peak below 0 dBFS: " + p.name + " " + std::to_string(stress));
        check(rms >= kMinWindowRmsDbfs, "not silent: " + p.name + " " + std::to_string(rms));
        check(rms >= kLevelWindowLowDbfs && rms <= kLevelWindowHighDbfs,
              "level-matched within the library window: " + p.name + " " + std::to_string(rms));
    }
    std::printf("phrase peak max %.1f dBFS, loudest-window RMS %.1f..%.1f dBFS, stress peak max %.1f dBFS\n",
                loudest, quietest, loudestRms, worstStress);
}

// Writes CuratedLibraryLevels.inc: VOLUME per program so the audit phrase's
// loudest window meets the level target within the peak limits.
int calibrate(const char* path)
{
    const auto& curated = curatedLibrary();
    const auto levels = measureAll();
    FILE* file = std::fopen(path, "w");
    if (file == nullptr) return 2;
    std::fprintf(file,
        "// Calibrated VOLUME per curated program (generated; see below). Included by\n"
        "// FactoryPresets.cpp inside the kCalibratedLevels initializer.\n"
        "//\n"
        "// Level matching, not voicing: each program's sound is authored in\n"
        "// CuratedLibrary.inc; this table only sets its VOLUME so the library plays at\n"
        "// a consistent level. Regenerate after any voicing or engine gain change with\n"
        "//   build-core/cs80_library_audit_tests --calibrate src/core/CuratedLibraryLevels.inc\n"
        "// and run it twice (the output driver is mildly nonlinear). Targets: loudest\n"
        "// 50 ms RMS of the audit phrase %.0f dBFS (%.0f dBFS for single-line Leads/Winds/Bass/Sequences),\n"
        "// limited so the phrase peak stays <= %.1f dBFS and the 8-voice stress peak\n"
        "// <= %.1f dBFS, VOLUME %.2f..%.2f.\n",
        kTargetRmsDbfs, kTargetSingleLineRmsDbfs, kCalibratePhrasePeakDbfs, kCalibrateStressPeakDbfs,
        kMinVolume, kMaxVolume);
    for (size_t i = 0; i < curated.size(); ++i)
    {
        const auto& p = curated[i];
        const auto& l = levels[i];
        const double volume = p.values[P_volume];
        const double target = singleLine(p.category) ? kTargetSingleLineRmsDbfs : kTargetRmsDbfs;
        double gainDb = target - dbfs(l.windowRms);
        gainDb = std::min(gainDb, kCalibratePhrasePeakDbfs - dbfs(l.phrasePeak));
        gainDb = std::min(gainDb, kCalibrateStressPeakDbfs - dbfs(l.stressPeak));
        const double next = std::clamp(volume * std::pow(10.0, gainDb / 20.0), kMinVolume, kMaxVolume);
        std::fprintf(file, "    { \"%s\", %.3ff },\n", p.name.c_str(), next);
    }
    return std::fclose(file) == 0 ? 0 : 3;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--calibrate") == 0)
        return calibrate(argv[2]);
    testLegacyLibrary();
    testProgramList();
    testCuratedData();
    testDistinct();
    // Data-only runs (mutation checks of the data gates) may skip rendering.
    if (std::getenv("CS80_LIBRARY_AUDIT_DATA_ONLY") == nullptr)
        testLevels();
    if (failures != 0)
    {
        std::printf("%d library audit failures\n", failures);
        return 1;
    }
    std::printf("library audit passed: %zu curated + %d legacy programs\n", curatedLibrary().size(), legacyProgramCount);
    return 0;
}
