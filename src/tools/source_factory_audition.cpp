// Listen-to-the-model fixture, not a hardware comparison or live-selector test.
#include "core/Engine.h"

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
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace cs80;
namespace fs = std::filesystem;
constexpr int rate = 48000, channels = 2, blockFrames = 256;
constexpr int minimumFrames = rate * 7 / 2, maximumFrames = rate * 25;
constexpr int tailFrames = rate / 10;
constexpr float initialMaster = 0.12f;
constexpr float tailThreshold = 0.5f / 32767.0f;
constexpr std::uint64_t maximumWavBytes = 24ull * 1024 * 1024;
constexpr std::array<int, 4> notes {48, 52, 55, 60};
constexpr std::array<int, 4> onsetFrames {7200, 21600, 36000, 50400};
constexpr int releaseFrame = 100800;
constexpr float velocity = 0.75f;

struct Statistics {
    std::uint64_t samples = 0, nonfinite = 0, outsideUnit = 0;
    double peak = 0.0, power = 0.0;
    void add(float value) {
        ++samples;
        if (!std::isfinite(value)) { ++nonfinite; return; }
        peak = std::max(peak, std::abs(static_cast<double>(value)));
        power += static_cast<double>(value) * value;
        if (std::abs(value) >= 1.0f) ++outsideUnit;
    }
    double rms() const { return samples ? std::sqrt(power / static_cast<double>(samples)) : 0.0; }
};
struct Segment {
    int rank = 0, selector = 0, frames = 0, releaseCompleteFrame = -1;
    std::uint64_t startFrame = 0;
    Engine::SourceToneInfo source;
    Statistics statistics, phrase;
    std::array<Statistics, 2> channel;
    double tailPeak = 0.0;
};
struct Capture {
    std::vector<float> samples;
    std::vector<Segment> segments;
    Statistics statistics;
};

std::string quoted(std::string_view value) {
    std::ostringstream out; out.imbue(std::locale::classic()); out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') out << '\\' << static_cast<char>(c);
        else if (c < 32) out << "\\u" << std::hex << std::setfill('0') << std::setw(4) << static_cast<unsigned>(c) << std::dec;
        else out << static_cast<char>(c);
    }
    out << '"'; return out.str();
}
std::string hex(std::uint64_t value, int width) {
    std::ostringstream out; out.imbue(std::locale::classic());
    out << "0x" << std::hex << std::setfill('0') << std::setw(width) << value; return out.str();
}
std::string nowUtc() {
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    const auto* utc = std::gmtime(&now);
    if (!utc) throw std::runtime_error("Cannot obtain UTC timestamp");
    std::ostringstream out; out.imbue(std::locale::classic());
    out << std::put_time(utc, "%Y-%m-%dT%H:%M:%SZ"); return out.str();
}
std::ofstream output(const fs::path& path) {
    std::ofstream out;
    out.exceptions(std::ios::failbit | std::ios::badbit);
    out.open(path, std::ios::binary | std::ios::trunc);
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10);
    return out;
}
ParamValues commonSettings(float master) {
    ParamValues p;
    p[P_parityMode] = 1.0f;
    p[P_volume] = master;
    p[P_detune] = 0.0f;
    p[P_masterSustain] = p[P_keyRelease] = p[P_sustainMode] = 0.0f;
    p[P_chorusOn] = p[P_ringAmount] = 0.0f;
    p[P_subVco] = p[P_subVcf] = p[P_subVca] = 0.0f;
    p[P_subAtVco] = p[P_subAtVcf] = p[P_subAtSpeed] = p[P_subExtLevel] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_modWheelSub] = p[P_modWheelVcf] = 0.0f;
    p[P_lfo1Depth] = p[P_lfo2Depth] = p[P_lfo3Depth] = 0.0f;
    p[P_fx1Type] = p[P_fx2Type] = p[P_fx3Type] = 0.0f;
    p[P_fx1Mix] = p[P_fx2Mix] = p[P_fx3Mix] = 0.0f;
    p[P_arpOn] = p[P_mpeOn] = p[P_footMode] = 0.0f;
    p[P_outputLevel] = p[P_outputMode] = 1.0f;
    for (int r = 0; r < 2; ++r) {
        p[chParam(r, CP_feet)] = 1.0f; // same 8-foot octave, not a per-tone transpose
        p[chParam(r, CP_pan)] = 0.0f;
    }
    return p;
}
bool released(const Engine& engine, int rank) {
    if (engine.activeVoices() != 0) return false;
    for (int voice = 0; voice < 8; ++voice) {
        const auto s = engine.getRankEnvelopeState(voice, rank);
        if (s.amplitude.stage != Adsr::Stage::Idle || s.filter.stage != FilterEnv::Stage::Idle)
            return false;
    }
    return true;
}
Capture render(float master) {
    Capture capture;
    capture.samples.reserve(static_cast<std::size_t>(rate) * channels * 100);
    const auto common = commonSettings(master);
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector) {
        Engine engine;
        engine.prepare(rate);
        ControlFrame frame;
        frame.effectiveValues = common;
        frame.effectiveValues[P_mix] = static_cast<float>(rank);
        frame.hardwareSources[rank] = selector;
        if (!engine.setControlFrame(frame) || !engine.isParityMode())
            throw std::runtime_error("Source ControlFrame rejected or parity isolation inactive");
        // Source identity is already accepted. Reset only initializes the render
        // fixture and volume smoothing to the chosen uniform master setting.
        engine.reset();
        Segment segment;
        segment.rank = rank; segment.selector = selector;
        segment.startFrame = capture.samples.size() / channels;
        segment.source = engine.getSourceToneInfo(rank);
        if (!source_factory::isSourceDriven(segment.source.route)
            || engine.getSourceToneInfo(1 - rank).route != source_factory::Route::source_memory_panel)
            throw std::runtime_error("Unexpected source route");
        std::array<float, blockFrames * channels> audio {};
        int position = 0, event = 0, quietFrames = 0;
        bool notesReleased = false;
        for (;;) {
            if (event < static_cast<int>(notes.size()) && position == onsetFrames[event]) {
                engine.noteOn(notes[event], velocity); ++event;
            }
            if (!notesReleased && position == releaseFrame) {
                for (int note : notes) engine.noteOff(note);
                notesReleased = true;
            }
            int count = blockFrames;
            if (event < static_cast<int>(notes.size())) count = std::min(count, onsetFrames[event] - position);
            if (!notesReleased) count = std::min(count, releaseFrame - position);
            if (position < minimumFrames) count = std::min(count, minimumFrames - position);
            if (count <= 0 || position + count > maximumFrames)
                throw std::runtime_error("Release/settling did not finish within 25 seconds");
            const auto nextSize = 44ull + (capture.samples.size() + static_cast<std::size_t>(count) * channels) * 2ull;
            if (nextSize > maximumWavBytes)
                throw std::runtime_error("Audition would exceed the 24 MiB WAV budget; no truncated output written");
            engine.render(audio.data(), count);
            for (int n = 0; n < count; ++n) {
                bool quiet = true;
                for (int ch = 0; ch < channels; ++ch) {
                    const float x = audio[n * channels + ch];
                    segment.statistics.add(x); segment.channel[ch].add(x); capture.statistics.add(x);
                    if (position + n >= onsetFrames[0] && position + n < releaseFrame) segment.phrase.add(x);
                    if (!std::isfinite(x)) throw std::runtime_error("Nonfinite engine output; refusing WAV export");
                    quiet = quiet && std::abs(x) < tailThreshold;
                    capture.samples.push_back(x);
                }
                quietFrames = quiet ? quietFrames + 1 : 0;
            }
            position += count;
            if (notesReleased && released(engine, rank)) {
                if (segment.releaseCompleteFrame < 0) segment.releaseCompleteFrame = position;
                if (position >= minimumFrames && quietFrames >= tailFrames) break;
            }
        }
        segment.frames = position;
        for (std::size_t i = capture.samples.size() - tailFrames * channels; i < capture.samples.size(); ++i)
            segment.tailPeak = std::max(segment.tailPeak, std::abs(static_cast<double>(capture.samples[i])));
        capture.segments.push_back(segment);
        const auto* address = source_factory::address(rank, selector);
        std::cout << "Rank " << rank + 1 << " / " << address->name << ": " << static_cast<double>(position) / rate
                  << " s, float peak " << segment.statistics.peak << ", RMS " << segment.statistics.rms()
                  << ", releases idle, tail < half PCM16 LSB\n" << std::flush;
    }
    return capture;
}
void little(std::ostream& out, std::uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) { out.put(static_cast<char>(value & 255u)); value >>= 8; }
}
void writeWav(const fs::path& path, const Capture& capture) {
    if (capture.statistics.nonfinite || capture.statistics.outsideUnit || capture.statistics.peak >= 1.0)
        throw std::runtime_error("Unsafe float capture; refusing PCM conversion");
    const auto dataBytes = static_cast<std::uint32_t>(capture.samples.size() * 2);
    auto out = output(path);
    out.write("RIFF", 4); little(out, 36 + dataBytes, 4); out.write("WAVEfmt ", 8);
    little(out, 16, 4); little(out, 1, 2); little(out, channels, 2); little(out, rate, 4);
    little(out, rate * channels * 2, 4); little(out, channels * 2, 2); little(out, 16, 2);
    out.write("data", 4); little(out, dataBytes, 4);
    for (float x : capture.samples) {
        const long pcm = std::lround(static_cast<double>(x) * 32767.0);
        if (pcm < -32767 || pcm > 32767) throw std::runtime_error("PCM overflow; no clamp permitted");
        little(out, static_cast<std::uint16_t>(static_cast<std::int16_t>(pcm)), 2);
    }
    out.close();
}
void writeStatistics(std::ostream& out, const Statistics& s) {
    out << "{\"float_peak\": " << s.peak << ", \"float_rms\": " << s.rms()
        << ", \"sample_values_checked\": " << s.samples << ", \"nonfinite_count\": " << s.nonfinite
        << ", \"abs_at_or_above_one_count\": " << s.outsideUnit << '}';
}
void writeManifest(const fs::path& path, const Capture& capture, std::string_view commit,
                   float master, const std::vector<float>& attemptedMasters, std::string_view started) {
    auto out = output(path);
    out << "{\n  \"schema_version\": 1,\n  \"fixture_id\": \"source-factory-all22-audition-v1\",\n"
        << "  \"kind\": \"software_model_listening_audition\",\n  \"hardware_comparison\": false,\n"
        << "  \"source_commit\": " << quoted(commit) << ",\n"
        << "  \"source_commit_scope\": \"caller-verified src/core tree; fixture may be added later\",\n"
        << "  \"model_version\": " << source_factory::modelVersion << ",\n"
        << "  \"model_id\": " << quoted(source_factory::modelId) << ",\n"
        << "  \"source_table_hash\": " << quoted(hex(source_factory::sourceFingerprint, 16)) << ",\n"
        << "  \"hypothesis_version\": " << factory_hypotheses::version << ",\n"
        << "  \"hypothesis_hash\": " << quoted(hex(factory_hypotheses::fingerprint, 16)) << ",\n"
        << "  \"hypothesis_policy\": " << quoted(factory_hypotheses::policyId) << ",\n"
        << "  \"render_started_utc\": " << quoted(started) << ",\n  \"render_finished_utc\": " << quoted(nowUtc()) << ",\n"
        << "  \"wav\": \"source-factory-all22.wav\",\n  \"sample_rate_hz\": " << rate << ",\n"
        << "  \"internal_sample_rate_hz\": " << rate * 2 << ",\n  \"channels\": 2,\n  \"encoding\": \"PCM16LE\",\n"
        << "  \"frame_count\": " << capture.samples.size() / channels << ",\n"
        << "  \"duration_seconds\": " << static_cast<double>(capture.samples.size()) / (channels * rate) << ",\n"
        << "  \"wav_bytes\": " << 44 + capture.samples.size() * 2 << ",\n"
        << "  \"compiler\": " << quoted(
#ifdef __VERSION__
            __VERSION__
#else
            "not recorded"
#endif
        ) << ",\n"
        << "  \"latency_host_frames\": " << Decimator::kHostLatency << ",\n  \"latency_compensated\": false,\n"
        << "  \"initial_state\": \"fresh Engine per segment; prepare; setControlFrame; reset; no preroll discarded\",\n"
        << "  \"switching_scope\": \"independent render fixtures, NOT live source-switching evidence\",\n"
        << "  \"isolation\": true,\n  \"oscillator\": \"existing production Engine oscillator; NOT experimental prototype\",\n"
        << "  \"effects\": \"chorus, ring amount, external modulation, global SUB depths and modern FX off; source PWM retained\",\n"
        << "  \"other_rank\": \"PANEL selector 13, fixed common parameters, MIX-muted; modeled shared receiver ports retained\",\n"
        << "  \"performance\": {\"midi_channel\": 1, \"velocity\": " << velocity
        << ", \"pressure\": 0, \"expression\": 1, \"ribbon\": 0, \"sustain_pedal\": false},\n"
        << "  \"phrase\": \"C3 E3 G3 C4 arpeggiated into one held C-major chord; MIDI numbering authoritative\",\n"
        << "  \"events_relative_to_segment\": [\n";
    for (int i = 0; i < 4; ++i)
        out << "    {\"type\": \"note_on\", \"frame\": " << onsetFrames[i] << ", \"seconds\": "
            << static_cast<double>(onsetFrames[i]) / rate << ", \"note\": " << notes[i] << "},\n";
    out << "    {\"type\": \"note_off_all_four\", \"frame\": " << releaseFrame
        << ", \"seconds\": " << static_cast<double>(releaseFrame) / rate << "}\n  ],\n"
        << "  \"timing\": \"zero-based host frames; end_frame exclusive; events applied before rendering their frame\",\n"
        << "  \"release_policy\": \"at least 3.5 seconds; all voices inactive and selected-rank amp/filter envelopes idle; final 100 ms below half PCM16 LSB\",\n"
        << "  \"release_observation_resolution_frames\": " << blockFrames << ",\n"
        << "  \"tail_threshold_float\": " << tailThreshold << ",\n"
        << "  \"postprocessing\": \"none except round(float * 32767) to PCM16; no clamp, limiter, normalization, fade, resampling or dither\",\n"
        << "  \"uniform_master\": " << master << ",\n  \"whole_capture_master_attempts\": [";
    for (std::size_t i = 0; i < attemptedMasters.size(); ++i) out << (i ? ", " : "") << attemptedMasters[i];
    out << "],\n  \"headroom_policy\": \"choose uniform master before rendering; if float peak >= 0.9 rerender ALL 22 segments at lower master\",\n"
        << "  \"float_checks_before_pcm_conversion\": ";
    writeStatistics(out, capture.statistics);
    out << ",\n  \"statistics_scope\": \"RMS over both interleaved channels including silence; phrase RMS over first note-on to common note-off\",\n"
        << "  \"mask_encoding\": \"hex, bit 0 = source output 1; unknown preserves evidence exclusions and overlaps inferred; known does NOT mean hardware-measured\",\n"
        << "  \"common_settings\": {\n";
    const auto common = commonSettings(master);
    for (int i = 0; i < P_numParams; ++i)
        out << "    " << quoted(paramId(i)) << ": " << common[i] << (i + 1 == P_numParams ? "\n" : ",\n");
    out << "  },\n  \"segments\": [\n";
    for (std::size_t i = 0; i < capture.segments.size(); ++i) {
        const auto& s = capture.segments[i];
        const auto& p = s.source.provenance;
        const auto& a = *source_factory::address(s.rank, s.selector);
        out << "    {\"index\": " << i + 1 << ", \"rank\": " << s.rank + 1 << ", \"rank_zero_based\": " << s.rank
            << ", \"selector_zero_based\": " << s.selector << ", \"name\": " << quoted(a.name)
            << ", \"source_name\": " << quoted(a.sourceName) << ", \"board\": " << a.board
            << ", \"column\": " << quoted(std::string(1, a.column)) << ", \"hardware_sources\": ["
            << (s.rank == 0 ? s.selector : 13) << ", " << (s.rank == 1 ? s.selector : 13) << ']'
            << ", \"settings_overrides\": {\"mix\": " << s.rank << "}, \"route\": "
            << quoted(s.source.route == source_factory::Route::source_complete ? "source_complete" : "source_inferred")
            << ", \"known_cells_mask\": " << quoted(hex(p.knownCells, 8))
            << ", \"inferred_cells_mask\": " << quoted(hex(p.inferredCells, 8))
            << ", \"unknown_cells_mask\": " << quoted(hex(p.unknownCells, 8))
            << ", \"hypothesis_version\": " << p.hypothesisVersion
            << ", \"hypothesis_hash\": " << quoted(hex(p.hypothesisHash, 16))
            << ", \"hypothesis_override_mask\": " << quoted(hex(p.hypothesisOverrideMask, 8))
            << ", \"retained_bridge_mask\": " << quoted(hex(p.bridgeMask, 8))
            << ", \"hardware_measured\": " << (p.hardwareMeasured ? "true" : "false")
            << ", \"start_frame\": " << s.startFrame << ", \"end_frame\": " << s.startFrame + s.frames
            << ", \"start_seconds\": " << static_cast<double>(s.startFrame) / rate
            << ", \"end_seconds\": " << static_cast<double>(s.startFrame + s.frames) / rate
            << ", \"duration_seconds\": " << static_cast<double>(s.frames) / rate
            << ", \"release_idle_observed_relative_frame\": " << s.releaseCompleteFrame
            << ", \"active_voices_at_end\": 0, \"selected_rank_envelopes_idle\": true, \"final_100ms_float_peak\": " << s.tailPeak
            << ", \"float_checks\": ";
        writeStatistics(out, s.statistics);
        out << ", \"phrase_float_checks\": "; writeStatistics(out, s.phrase);
        out << ", \"channels\": ["; writeStatistics(out, s.channel[0]); out << ", "; writeStatistics(out, s.channel[1]);
        out << "]}" << (i + 1 == capture.segments.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n"; out.close();
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 5 || std::string_view(argv[1]) != "--out" || std::string_view(argv[3]) != "--source-commit") {
            std::cerr << "Usage: cs80_source_factory_audition --out NEW_DIRECTORY --source-commit VERIFIED_40_HEX_CORE_COMMIT\n";
            return 2;
        }
        const std::string commit = argv[4];
        if (commit.size() != 40 || !std::all_of(commit.begin(), commit.end(), [](char c) {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        })) throw std::runtime_error("source-commit must be a caller-verified lowercase full 40-hex commit");
        const fs::path directory = argv[2];
        if (directory.empty()) throw std::runtime_error("Empty output directory");
        const auto wav = directory / "source-factory-all22.wav", manifest = directory / "source-factory-all22.json";
        const auto wavPart = directory / "source-factory-all22.wav.part", jsonPart = directory / "source-factory-all22.json.part";
        for (const auto& path : {wav, manifest, wavPart, jsonPart})
            if (fs::exists(path)) throw std::runtime_error("Output already exists: " + path.string());
        fs::create_directories(directory);
        const auto started = nowUtc();
        float master = initialMaster;
        Capture capture;
        std::vector<float> attempts;
        for (;;) {
            attempts.push_back(master);
            capture = render(master);
            if (capture.statistics.peak < 0.9) break;
            master *= static_cast<float>(std::min(0.5, 0.8 / capture.statistics.peak));
            if (master < 1.0e-6f) throw std::runtime_error("No usable uniform headroom found");
            std::cerr << "Headroom too small: rerendering every segment with uniform master " << master << '\n';
        }
        if (capture.statistics.peak == 0.0) throw std::runtime_error("Entire capture is silent");
        writeWav(wavPart, capture);
        writeManifest(jsonPart, capture, commit, master, attempts, started);
        fs::rename(wavPart, wav); fs::rename(jsonPart, manifest);
        std::cout << "Saved " << wav << " and " << manifest << "\n"
                  << capture.segments.size() << " segments, " << static_cast<double>(capture.samples.size()) / (channels * rate)
                  << " seconds; " << fs::file_size(wav) << " WAV bytes; peak " << capture.statistics.peak
                  << ", RMS " << capture.statistics.rms() << ", nonfinite 0; uniform master " << master << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Audition failed: " << error.what() << '\n';
        return 1;
    }
}
