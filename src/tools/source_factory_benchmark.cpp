// Bounded offline core timing. Not a DAW, audio-driver or hard-real-time test.
#include "core/Engine.h"
#include "core/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__linux__)
#include <time.h>
#endif
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif

namespace {
using namespace cs80;
using Clock = std::chrono::steady_clock;
namespace fs = std::filesystem;
constexpr std::array<int, 8> notes {36, 43, 48, 55, 60, 64, 67, 72};
constexpr double warmupSeconds = 1.0, minimumTailSeconds = 1.0, maximumTailSeconds = 40.0;
constexpr double pedalHoldSeconds = 0.25, fxTailSeconds = 3.0;
constexpr unsigned orderSeed = 803010;

struct ScopedDenormals {
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    unsigned before = _mm_getcsr();
    ScopedDenormals() { _mm_setcsr(before | 0x8040u); }
    ~ScopedDenormals() { _mm_setcsr(before); }
    static constexpr const char* description = "x86 MXCSR FTZ/DAZ enabled locally, restored at exit";
#else
    static constexpr const char* description = "unchanged; no platform-specific denormal setup";
#endif
};

struct Case {
    int rate = 48000, block = 128, selector = 0;
    bool isolated = true, moving = false, idle = false, noise = false;
    bool sourcePerformance = false;
    int performanceProfile = 0; // 0 dry, 1 MPE+chorus/reverb, 2 MPE+phaser/delay/drive
};

const char* modeName(const Case& c) {
    return c.isolated ? "isolation" : c.sourcePerformance ? "source_performance" : "legacy";
}

double threadCpuSeconds() {
#if defined(__linux__)
    timespec time {};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &time) == 0)
        return static_cast<double>(time.tv_sec) + static_cast<double>(time.tv_nsec) * 1.0e-9;
#endif
    return -1.0;
}

struct Stats {
    std::vector<double> micros, eventMicros;
    std::uint64_t frames = 0, overBudget = 0, nonfinite = 0, outsideUnit = 0;
    double checksum = 0.0, peak = 0.0, power = 0.0, cpu = 0.0;
    int minimumVoices = 8, maximumVoices = 0, finalVoices = 0;
    std::uint64_t compileDelta = 0;
    std::uint64_t controllerEvents = 0, memberChecks = 0, pedalHeldCallbacks = 0;
    double threadCpu = -1.0, lastBlockPeak = 0.0;
    double pedalTargetMin = 1.0e30, pedalTargetMax = 0.0;
    bool tailComplete = false;
    void observe(const std::vector<float>& audio, int voices) {
        lastBlockPeak = 0.0;
        minimumVoices = std::min(minimumVoices, voices);
        maximumVoices = std::max(maximumVoices, voices);
        finalVoices = voices;
        for (float x : audio) {
            if (!std::isfinite(x)) { ++nonfinite; continue; }
            const double value = x;
            checksum += value;
            peak = std::max(peak, std::abs(value));
            lastBlockPeak = std::max(lastBlockPeak, std::abs(value));
            power += value * value;
            if (std::abs(value) >= 1.0) ++outsideUnit;
        }
    }
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::string jsonString(const std::string& text) {
    std::string result = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') result += '\\';
        require(c >= 32, "Control character in metadata");
        result += static_cast<char>(c);
    }
    return result + '"';
}

std::ofstream output(const fs::path& path) {
    std::ofstream file;
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file.open(path);
    file.imbue(std::locale::classic());
    file << std::setprecision(12);
    return file;
}

ControlFrame makeFrame(const Case& c, int selector) {
    ControlFrame frame;
    auto& p = frame.effectiveValues;
    p[P_parityMode] = c.isolated ? 1.0f : 0.0f;
    p[P_sourcePerformance] = c.sourcePerformance ? 1.0f : 0.0f;
    p[P_mix] = 0.5f;
    p[P_volume] = 0.12f; // fixed headroom, never per-tone normalization
    p[P_masterSustain] = p[P_keyRelease] = p[P_sustainMode] = 0.0f;
    p[P_chorusOn] = p[P_ringAmount] = 0.0f;
    p[P_subOriginalMode] = 1.0f;
    p[P_subVco] = p[P_subVcf] = p[P_subVca] = 0.0f;
    p[P_subAtVco] = p[P_subAtVcf] = p[P_subAtSpeed] = p[P_subExtLevel] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = 0.0f;
    p[P_analogNoise] = c.noise ? 0.12f : 0.0f;
    p[P_lfo1Depth] = p[P_lfo2Depth] = p[P_lfo3Depth] = 0.0f;
    p[P_fx1Type] = p[P_fx2Type] = p[P_fx3Type] = 0.0f;
    p[P_fx1Mix] = p[P_fx2Mix] = p[P_fx3Mix] = 0.0f;
    p[P_mpeOn] = p[P_arpOn] = 0.0f;
    p[P_footMode] = c.moving ? 1.0f : 0.0f;
    p[P_outputLevel] = p[P_outputMode] = 1.0f;
    if (c.performanceProfile != 0) {
        p[P_mpeOn] = 1.0f;
        p[P_mpeBendRange] = 12.0f;
        p[P_pitchBendRange] = 2.0f;
        p[P_footMode] = 1.0f;
        p[P_modWheelSub] = 0.15f;
        p[P_modWheelVcf] = 0.20f;
        if (c.performanceProfile == 1) {
            p[P_fx1Type] = 1.0f; p[P_fx1Mix] = 0.20f;
            p[P_fx2Type] = 4.0f; p[P_fx2Mix] = 0.15f;
        } else {
            p[P_fx1Type] = 2.0f; p[P_fx1Mix] = 0.20f;
            p[P_fx2Type] = 3.0f; p[P_fx2Mix] = 0.15f;
            p[P_fx3Type] = 5.0f; p[P_fx3Mix] = 0.10f;
        }
    }
    for (int rank = 0; rank < 2; ++rank) {
        frame.hardwareSources[rank] = selector;
        ParamValues legacy;
        applyHardwareTone(legacy, rank, selector);
        // Match the processor's physical selector adapter. Footage, pan and
        // software bypasses are not among the 26 replaced programmer controls.
        for (int local = 0; local < CP_count; ++local)
            if (local != CP_feet && local != CP_pan && local != CP_hpfOn && local != CP_lpfOn)
                p[chParam(rank, local)] = legacy[chParam(rank, local)];
        p[chParam(rank, CP_feet)] = 1.0f;
        p[chParam(rank, CP_pan)] = 0.0f;
        p[chParam(rank, CP_hpfOn)] = p[chParam(rank, CP_lpfOn)] = 1.0f;
    }
    return frame;
}

double quantile(const std::vector<double>& sorted, double q) {
    return sorted[std::min(sorted.size() - 1,
                          static_cast<std::size_t>(std::ceil(q * static_cast<double>(sorted.size() - 1))))];
}

void writeRow(std::ostream& out, const Case& c, const char* phase, Stats& s) {
    require(!s.micros.empty(), "Empty timing distribution");
    const double budget = 1.0e6 * c.block / c.rate;
    const double sum = std::accumulate(s.micros.begin(), s.micros.end(), 0.0);
    std::sort(s.micros.begin(), s.micros.end());
    std::sort(s.eventMicros.begin(), s.eventMicros.end());
    const char* scenario = c.idle ? (c.noise ? "idle_noise" : "idle_quiet")
                                  : c.moving ? "moving_tabs_pressure_wah"
                                  : c.performanceProfile == 1 ? "mpe_chorus_reverb"
                                  : c.performanceProfile == 2 ? "mpe_phaser_delay_drive" : "paired_tones";
    out << c.rate << ',' << c.block << ',' << c.isolated << ',' << scenario << ','
        << c.selector << ',' << hardwareToneName(c.selector, 0) << ',' << hardwareToneName(c.selector, 1)
        << ',' << phase << ',' << s.micros.size() << ',' << static_cast<double>(s.frames) / c.rate
        << ',' << budget << ',' << s.micros.front() << ',' << quantile(s.micros, 0.5)
        << ',' << quantile(s.micros, 0.95) << ',' << quantile(s.micros, 0.99) << ',' << s.micros.back()
        << ',' << sum / (s.micros.size() * budget) << ',' << quantile(s.micros, 0.5) / budget
        << ',' << quantile(s.micros, 0.99) / budget << ',' << s.micros.back() / budget
        << ',' << s.overBudget << ',' << s.eventMicros.size()
        << ',' << (s.eventMicros.empty() ? 0.0 : quantile(s.eventMicros, 0.5))
        << ',' << (s.eventMicros.empty() ? 0.0 : s.eventMicros.back())
        << ',' << s.minimumVoices << ',' << s.maximumVoices << ',' << s.finalVoices
        << ',' << s.tailComplete << ',' << s.compileDelta << ',' << s.nonfinite
        << ',' << s.outsideUnit << ',' << s.peak << ',' << std::sqrt(s.power / (2.0 * s.frames))
        << ',' << s.checksum << ',' << s.cpu << ',' << modeName(c) << ',' << c.sourcePerformance
        << ',' << s.threadCpu << ',' << s.controllerEvents << ',' << s.memberChecks
        << ',' << s.pedalHeldCallbacks << ',' << s.lastBlockPeak
        << ',' << (s.pedalTargetMin == 1.0e30 ? 0.0 : s.pedalTargetMin) << ',' << s.pedalTargetMax << '\n';
    out.flush();
    std::cout << c.rate << '/' << c.block << " mode=" << modeName(c) << ' ' << scenario
              << " pair=" << c.selector << ' ' << phase << " medianRTF=" << quantile(s.micros, 0.5) / budget
              << " p99RTF=" << quantile(s.micros, 0.99) / budget << " misses=" << s.overBudget
              << '/' << s.micros.size() << '\n' << std::flush;
}

int run(const Case& c, int callbacks, std::ostream& csv) {
    Engine engine;
    engine.prepare(c.rate);
    std::array<ControlFrame, 11> frames;
    for (int selector = 0; selector < 11; ++selector) frames[selector] = makeFrame(c, selector);
    require(engine.setControlFrame(frames[c.selector]), "Initial ControlFrame rejected");
    engine.reset();
    engine.setExpression(0.8f);
    const bool expressive = c.performanceProfile != 0;
    if (!c.idle) for (int i = 0; i < static_cast<int>(notes.size()); ++i)
        engine.noteOn(notes[i], 0.8f, expressive ? i + 2 : 1);
    require(engine.getRenderPolicy() == renderPolicy(c.isolated ? 1.0f : 0.0f,
                                                   c.sourcePerformance ? 1.0f : 0.0f),
            "Requested render policy was not accepted");
    for (int rank = 0; rank < 2; ++rank) {
        const auto info = engine.getSourceToneInfo(rank);
        require(info.selector == c.selector
                    && info.route == source_factory::route(rank, c.selector, c.isolated || c.sourcePerformance),
                "Requested route was not accepted by Engine");
    }
    std::vector<float> audio(static_cast<std::size_t>(c.block) * 2);
    const int warmup = static_cast<int>(std::ceil(warmupSeconds * c.rate / c.block));
    for (int i = 0; i < warmup; ++i) {
        require(engine.setControlFrame(frames[c.selector]), "Warmup frame rejected");
        engine.render(audio.data(), c.block);
    }
    require(engine.activeVoices() == (c.idle ? 0 : 8), "Unexpected warmup voice count");

    int currentSelector = c.selector;
    enum class Phase { held, sustain, release };
    auto measure = [&](Phase measurementPhase) {
        const bool tail = measurementPhase == Phase::release;
        const bool sustain = measurementPhase == Phase::sustain;
        Stats stats;
        const int maxCallbacks = tail ? static_cast<int>(std::ceil(maximumTailSeconds * c.rate / c.block))
                               : sustain ? static_cast<int>(std::ceil(pedalHoldSeconds * c.rate / c.block)) : callbacks;
        stats.micros.reserve(static_cast<std::size_t>(maxCallbacks));
        stats.eventMicros.reserve(static_cast<std::size_t>(maxCallbacks / 32 + 2));
        const auto compilationStart = engine.getSourceCompileCount();
        const auto cpuStart = std::clock();
        const double threadCpuStart = threadCpuSeconds();
        for (int i = 0; i < maxCallbacks; ++i) {
            const bool event = c.moving && !tail && !sustain && i % 32 == 0;
            if (event) currentSelector = (currentSelector + 1) % 11;
            const double phase = std::fmod(static_cast<double>(i) * c.block / c.rate * 0.75, 1.0);
            const float expression = 0.15f + 0.8f * static_cast<float>(1.0 - std::abs(2.0 * phase - 1.0));
            const float pressure = 0.1f + 0.8f * static_cast<float>(phase);
            std::array<float, 8> memberPressure {}, memberBend {}, memberTimbre {};
            if (expressive && !tail) for (int member = 0; member < 8; ++member) {
                const float offset = static_cast<float>(std::fmod(phase + member / 8.0, 1.0));
                memberPressure[member] = 0.1f + 0.7f * offset;
                memberBend[member] = 0.1f * (2.0f * offset - 1.0f);
                memberTimbre[member] = 0.15f + 0.35f * offset;
            }
            const auto before = Clock::now();
            const bool accepted = engine.setControlFrame(frames[currentSelector]);
            if (c.moving && !tail) {
                engine.setExpression(expression);
                for (int note : notes) engine.polyPressure(note, pressure);
            }
            if (expressive && !tail) {
                engine.controlChange(11, expression, 1);
                engine.controlChange(1, 0.15f + 0.15f * static_cast<float>(phase), 1);
                engine.pitchBend(1, 0.05f * (2.0f * static_cast<float>(phase) - 1.0f));
                for (int member = 0; member < 8; ++member) {
                    engine.channelPressure(member + 2, memberPressure[member]);
                    engine.pitchBend(member + 2, memberBend[member]);
                    engine.controlChange(74, memberTimbre[member], member + 2);
                }
                if (!sustain && i % 64 == 0)
                    engine.controlChange(64, (i / 64) % 2 == 0 ? 1.0f : 0.0f, 1);
            }
            if (sustain && i == 0) {
                engine.controlChange(64, 1.0f, 1);
                for (int member = 0; member < 8; ++member) engine.noteOff(notes[member], member + 2);
            }
            if (tail && i == 0) {
                if (expressive) engine.controlChange(64, 0.0f, 1);
                else for (int note : notes) engine.noteOff(note);
            }
            engine.render(audio.data(), c.block);
            const auto after = Clock::now();
            require(accepted, "Timed ControlFrame rejected");
            const double elapsed = std::chrono::duration<double, std::micro>(after - before).count();
            stats.micros.push_back(elapsed);
            if (event) stats.eventMicros.push_back(elapsed);
            if (elapsed > 1.0e6 * c.block / c.rate) ++stats.overBudget;
            stats.frames += static_cast<unsigned>(c.block);
            stats.observe(audio, engine.activeVoices());
            if (expressive && !tail) {
                stats.controllerEvents += 27 + (!sustain && i % 64 == 0 ? 1 : 0) + (sustain && i == 0 ? 1 : 0);
                for (int member = 0; member < 8; ++member) {
                    const auto voice = engine.getVoiceInfo(member);
                    require(voice.channel == member + 2
                                && std::abs(voice.pressure - memberPressure[member]) < 1.0e-6f
                                && std::abs(voice.bend - memberBend[member] * 12.0f) < 1.0e-5f,
                            "Live MPE member identity/pressure/bend did not reach voice state");
                    if (sustain) require(voice.pedalHeld && !voice.held, "CC64 did not hold released MPE notes");
                    ++stats.memberChecks;
                }
                if (sustain) ++stats.pedalHeldCallbacks;
                const double target = engine.getPraWahState(0).targetOhms;
                stats.pedalTargetMin = std::min(stats.pedalTargetMin, target);
                stats.pedalTargetMax = std::max(stats.pedalTargetMax, target);
            }
            if (expressive && tail && i == 0) ++stats.controllerEvents;
            require(stats.nonfinite == 0, "Nonfinite output in measured capture");
            if (!tail) require(engine.activeVoices() == (c.idle ? 0 : 8), "Measured voice count changed");
            if (tail && engine.activeVoices() == 0
                && stats.frames >= (expressive ? fxTailSeconds : minimumTailSeconds) * c.rate) {
                stats.tailComplete = true;
                break;
            }
        }
        stats.cpu = static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
        const double threadCpuEnd = threadCpuSeconds();
        if (threadCpuStart >= 0.0 && threadCpuEnd >= 0.0) stats.threadCpu = threadCpuEnd - threadCpuStart;
        stats.compileDelta = engine.getSourceCompileCount() - compilationStart;
        require(tail || c.moving || stats.compileDelta == 0, "Unchanged selectors unexpectedly recompiled");
        require(!c.moving || tail || !(c.isolated || c.sourcePerformance)
                    || stats.compileDelta == 2 * stats.eventMicros.size(),
                "Moving source fixture did not compile both ranks at every selector event");
        if (!c.idle && !tail) require(stats.peak > 0.0, "Silent held-note fixture");
        if (expressive && !tail)
            require(stats.pedalTargetMax > stats.pedalTargetMin, "CC11 did not move the source PRA target");
        writeRow(csv, c, tail ? "release_tail" : sustain ? "sustain_pedal_hold" : "warmed_held", stats);
    };
    measure(Phase::held);
    if (expressive) measure(Phase::sustain);
    if (!c.idle) measure(Phase::release);
    return c.idle ? 1 : expressive ? 3 : 2;
}

void usage() {
    std::cout << "Usage: cs80_source_factory_benchmark --out NEW_DIRECTORY --source-commit HEX "
                 "[--callbacks 1024] [--suite baseline|performance] [--include-64] [--quick] "
                 "[--rate 44100|48000] [--block 64|128|256]\n"
                 "--quick defaults to 48 kHz / 128 frames; explicit --rate and --block "
                 "override those quick-run defaults.\n"
                 "Full sweep: 44.1/48 kHz, 64/128/256 frames, all 22 tabs as 11 two-rank pairs, "
                 "8 notes, source/legacy held and release, moving controls, idle/noise.\n"
                 "Performance suite: 128/256 frames, Source Performance all22 dry/live MPE+FX profiles, "
                 "pedal-held release and FX windows; explicit legacy/isolation dry anchors.\n"
                 "Timing is diagnostic; budget misses are reported, never a platform pass/fail.\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        fs::path directory;
        std::string commit;
        int callbacks = 1024;
        bool quick = false, include64 = false;
        int onlyRate = 0, onlyBlock = 0; // optional sweep filters; 0 keeps every value
        std::string suite = "baseline";
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help") { usage(); return 0; }
            if (arg == "--quick") { quick = true; continue; }
            if (arg == "--include-64") { include64 = true; continue; }
            require(i + 1 < argc, "Missing option value; use --help");
            if (arg == "--out") directory = argv[++i];
            else if (arg == "--source-commit") commit = argv[++i];
            else if (arg == "--suite") suite = argv[++i];
            else if (arg == "--rate") onlyRate = std::stoi(argv[++i]);
            else if (arg == "--block") onlyBlock = std::stoi(argv[++i]);
            else if (arg == "--callbacks") {
                const std::string value = argv[++i];
                require(!value.empty() && value.find_first_not_of("0123456789") == std::string::npos,
                        "Callbacks must be an integer");
                callbacks = std::stoi(value);
            }
            else throw std::runtime_error("Unknown option: " + arg);
        }
        require(!directory.empty() && commit.size() >= 7 && commit.size() <= 40
                    && commit.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos,
                "A new output directory and caller-verified source commit are required; use --help");
        require(callbacks >= 16 && callbacks <= 1000000, "Callbacks must be in 16..1000000");
        require(suite == "baseline" || suite == "performance", "Suite must be baseline or performance");
        require(!fs::exists(directory), "Output already exists; refusing to overwrite");
        require(fs::create_directories(directory), "Cannot create output directory");
        ScopedDenormals denormals;
        const auto started = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto meta = output(directory / "metadata.json");
        meta << "{\n  \"schema_version\": 2,\n  \"fixture_id\": \"source-core-timing-v2\",\n"
             << "  \"suite\": " << jsonString(suite) << ",\n"
             << "  \"source_commit_caller_verified\": " << jsonString(commit) << ",\n"
             << "  \"started_unix_seconds\": " << started << ",\n"
#ifdef __VERSION__
             << "  \"compiler\": " << jsonString(__VERSION__) << ",\n"
#else
             << "  \"compiler\": \"record compiler externally\",\n"
#endif
             << "  \"denormals\": " << jsonString(ScopedDenormals::description) << ",\n"
             << "  \"clock_is_steady\": " << (Clock::is_steady ? "true" : "false") << ",\n"
             << "  \"quick_subset\": " << (quick ? "true" : "false") << ",\n"
             << "  \"callbacks_per_held_phase\": " << callbacks << ",\n"
             << "  \"warmup_seconds\": " << warmupSeconds << ",\n"
             << "  \"tail_minimum_seconds\": " << minimumTailSeconds << ",\n"
             << "  \"tail_cap_seconds\": " << maximumTailSeconds << ",\n"
             << "  \"performance_fx_tail_seconds\": " << fxTailSeconds << ",\n"
             << "  \"performance_pedal_hold_seconds\": " << pedalHoldSeconds << ",\n"
             << "  \"performance_include_64\": " << (include64 ? "true" : "false") << ",\n"
             << "  \"thread_cpu_clock_available\": " << (threadCpuSeconds() >= 0.0 ? "true" : "false") << ",\n"
             << "  \"order_seed\": " << orderSeed << ",\n"
             << "  \"timed_scope\": \"setControlFrame, active control events, render; no JUCE/DAW/driver\",\n"
             << "  \"source_model_version\": " << source_factory::modelVersion << ",\n"
             << "  \"source_fingerprint\": \"" << std::hex << source_factory::sourceFingerprint << std::dec << "\",\n"
             << "  \"hardware_equivalence_or_realtime_qualification\": false\n}\n";
        meta.close();
        auto csv = output(directory / "results.csv");
        csv << "rate,block,isolation,scenario,initial_selector,rank_i_name,rank_ii_name,phase,callbacks,audio_seconds,"
               "budget_us,min_us,median_us,p95_us,p99_us,max_us,mean_rtf,median_rtf,p99_rtf,max_rtf,"
               "budget_exceedances,selector_events,event_median_us,event_max_us,min_active_voices,max_active_voices,"
               "final_active_voices,tail_voices_complete,source_compilations,nonfinite_count,abs_at_or_above_one,"
               "peak,rms,checksum,process_cpu_seconds_including_checks,mode,source_performance,"
               "thread_cpu_seconds_including_checks,controller_events,mpe_member_state_checks,"
               "pedal_held_callbacks,last_block_peak,pra_target_min_ohms,pra_target_max_ohms\n";
        std::vector<Case> cases;
        for (int rate : {44100, 48000}) for (int block : {64, 128, 256}) {
            // A quick target-block benchmark must not silently return an empty
            // sweep: the prior hard-coded 128-frame filter made
            // `--quick --rate 48000 --block 256` record zero cases.
            if (quick && (rate != (onlyRate ? onlyRate : 48000)
                       || block != (onlyBlock ? onlyBlock : 128))) continue;
            if ((onlyRate && rate != onlyRate) || (onlyBlock && block != onlyBlock)) continue;
            if (suite == "performance") {
                if (block == 64 && !include64) continue;
                for (int selector = 0; selector < 11; ++selector) {
                    if (quick && selector != 0 && selector != 1 && selector != 10) continue;
                    for (int profile = 0; profile < 3; ++profile)
                        cases.push_back({rate, block, selector, false, false, false, false, true, profile});
                    if (selector == 0 || selector == 1 || selector == 10) {
                        cases.push_back({rate, block, selector, true});
                        cases.push_back({rate, block, selector, false});
                    }
                }
                continue;
            }
            for (int selector = 0; selector < 11; ++selector) {
                if (quick && selector != 0 && selector != 1 && selector != 10) continue;
                cases.push_back({rate, block, selector, true});
                cases.push_back({rate, block, selector, false});
            }
            cases.push_back({rate, block, 0, true, true});
            cases.push_back({rate, block, 0, false, true});
            cases.push_back({rate, block, 0, true, false, true, false});
            cases.push_back({rate, block, 0, false, false, true, false});
            cases.push_back({rate, block, 0, false, false, true, true});
        }
        std::mt19937 random(orderSeed);
        std::shuffle(cases.begin(), cases.end(), random);
        int rows = 0;
        for (const auto& c : cases) rows += run(c, callbacks, csv);
        csv.close();
        auto complete = output(directory / "completed.json");
        complete << "{\"completed\":true,\"cases\":" << cases.size() << ",\"rows\":" << rows << "}\n";
        complete.close();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Benchmark error: " << error.what() << '\n';
        return 2;
    }
}
