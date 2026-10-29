// W0 candidate capture only: no hardware reference or analog acceptance claim.
#include "core/Engine.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
constexpr int sampleRate = 48000;
constexpr int sampleCount = 96000;
struct Event { int sample; const char* type; int note; float value; };
constexpr std::array<Event, 6> events {{
    {4800, "note_on", 60, 0.75f},
    {19200, "poly_pressure", 60, 0.5f},
    {33600, "poly_pressure", 60, 0.0f},
    {48000, "note_off", 60, 0.0f},
    {57600, "note_on", 67, 0.625f},
    {72000, "note_off", 67, 0.0f}
}};

ParamValues patch()
{
    ParamValues p;
    p[P_parityMode] = 1.0f;
    p[P_volume] = 0.5f;
    p[P_mix] = 0.5f;
    p[P_detune] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_chorusOn] = 0.0f;
    p[P_ringAmount] = 0.0f;
    p[P_subVco] = p[P_subVcf] = p[P_subVca] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_saw)] = 1.0f;
        p[chParam(rank, CP_pulse)] = p[chParam(rank, CP_sine)] = 0.0f;
        p[chParam(rank, CP_noise)] = 0.0f;
        p[chParam(rank, CP_pwmDepth)] = 0.0f;
        p[chParam(rank, CP_hpfCutoff)] = 25.0f;
        p[chParam(rank, CP_lpfCutoff)] = 2400.0f;
        p[chParam(rank, CP_vA)] = 0.02f;
        p[chParam(rank, CP_vD)] = 0.15f;
        p[chParam(rank, CP_vS)] = 0.7f;
        p[chParam(rank, CP_vR)] = 0.12f;
        p[chParam(rank, CP_afterBrill)] = 0.25f;
        p[chParam(rank, CP_afterLevel)] = 0.2f;
    }
    return p;
}

std::ofstream output(const std::filesystem::path& path)
{
    std::ofstream stream;
    stream.exceptions(std::ios::failbit | std::ios::badbit);
    stream.open(path, std::ios::binary | std::ios::trunc);
    stream.imbue(std::locale::classic());
    stream << std::setprecision(std::numeric_limits<float>::max_digits10);
    return stream;
}
}

int main(int argc, char** argv)
{
    if (argc != 3 || std::string(argv[1]) != "--out" || std::string(argv[2]).empty())
    {
        std::cerr << "Usage: cs80_parity_capture --out DIRECTORY\n";
        return 2;
    }
    try
    {
        const std::filesystem::path directory(argv[2]);
        std::filesystem::create_directories(directory);
        // A failed rerun must not leave an older success manifest beside partial CSVs.
        std::filesystem::remove(directory / "capture.json");
        Engine engine;
        const auto parameters = patch();
        engine.setParams(parameters);
        engine.prepare(sampleRate);
        engine.reset();
        if (!engine.isParityMode()) throw std::runtime_error("Feature isolation not engaged");
        std::vector<float> samples(static_cast<size_t>(sampleCount) * 2);
        int position = 0;
        for (const auto& event : events)
        {
            engine.render(samples.data() + static_cast<size_t>(position) * 2, event.sample - position);
            position = event.sample;
            const std::string type(event.type);
            if (type == "note_on") engine.noteOn(event.note, event.value, 1);
            else if (type == "poly_pressure") engine.polyPressure(event.note, event.value, 1);
            else engine.noteOff(event.note, 1);
        }
        engine.render(samples.data() + static_cast<size_t>(position) * 2, sampleCount - position);
        for (const auto value : samples)
            if (!std::isfinite(value)) throw std::runtime_error("Nonfinite engine output");
        for (int channel = 0; channel < 2; ++channel)
        {
            auto stream = output(directory / (channel == 0 ? "left.csv" : "right.csv"));
            stream << "sample,value\n";
            for (int i = 0; i < sampleCount; ++i)
                stream << i << ',' << samples[static_cast<size_t>(i) * 2 + channel] << '\n';
            stream.close();
        }
        auto metadata = output(directory / "capture.json");
        metadata << "{\n  \"schema_version\": 1,\n"
                 << "  \"capture_kind\": \"engine_candidate_only\",\n"
                 << "  \"analog_evidence\": false,\n"
                 << "  \"fixture_id\": \"w0-engine-stimulus-v1\",\n"
                 << "  \"sample_rate_hz\": " << sampleRate << ",\n"
                 << "  \"internal_sample_rate_hz\": " << engine.getInternalSampleRate() << ",\n"
                 << "  \"sample_count\": " << sampleCount << ",\n"
                 << "  \"latency_samples\": " << engine.getLatencySamples() << ",\n"
                 << "  \"latency_compensated\": false,\n"
                 << "  \"channels\": {\"left\": \"left.csv\", \"right\": \"right.csv\"},\n"
                 << "  \"units\": \"raw engine floating-point output (not calibrated volts)\",\n"
                 << "  \"postprocessing\": \"none: no clipping, normalization, resampling or trimming\",\n"
                 << "  \"engine_output_processing\": \"parity mode bypasses output DC blocker and output soft clip; production decimator and provisional filter saturation retained\",\n"
                 << "  \"initial_state\": \"fresh Engine; setParams; prepare; reset; no warmup or discarded samples\",\n"
                 << "  \"transients\": \"startup, note-start phase reset and release tails retained; existing deterministic engine seed\",\n"
                 << "  \"external_input\": \"disconnected\",\n"
                 << "  \"event_timing\": \"zero-based host sample; applied before rendering that sample\",\n"
                 << "  \"render_blocks\": \"split only at event samples and capture end\",\n"
                 << "  \"settings_scope\": \"all submitted core ParamValues; no factory preset or host state; parity mode overrides extensions internally\",\n"
                 << "  \"parameter_count\": " << P_numParams << ",\n"
                 << "  \"settings\": {\n";
        for (int i = 0; i < P_numParams; ++i)
            metadata << "    \"" << paramId(i) << "\": " << parameters[i]
                     << (i + 1 == P_numParams ? "\n" : ",\n");
        metadata << "  },\n  \"events\": [\n";
        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& event = events[i];
            metadata << "    {\"sample\": " << event.sample << ", \"type\": \"" << event.type
                     << "\", \"note\": " << event.note << ", \"channel\": 1, \"value\": " << event.value << "}"
                     << (i + 1 == events.size() ? "\n" : ",\n");
        }
        metadata << "  ]\n}\n";
        metadata.close();
        std::cout << "Captured " << sampleCount << " engine candidate frames; no reference comparison or analog evidence.\n";
        return 0;
    }
    catch (const std::exception& exception)
    {
        std::cerr << "Capture failed: " << exception.what() << '\n';
        return 2;
    }
}
