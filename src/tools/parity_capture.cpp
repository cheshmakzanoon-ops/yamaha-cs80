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

// This opt-in fixture has a separate schema; the original export stays unchanged.
int captureLiveAdr(const std::filesystem::path& directory)
{
    std::filesystem::create_directories(directory);
    std::filesystem::remove(directory / "capture.json");
    auto parameters = patch();
    parameters[P_masterSustain] = parameters[P_keyRelease] = 0.0f;
    parameters[P_sustainMode] = parameters[P_sustainAssign] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        parameters[chParam(rank, CP_vA)] = parameters[chParam(rank, CP_fA)] = 1.0f;
        parameters[chParam(rank, CP_vD)] = parameters[chParam(rank, CP_fD)] = 2.0f;
        parameters[chParam(rank, CP_vR)] = parameters[chParam(rank, CP_fR)] = 2.0f;
        parameters[chParam(rank, CP_vS)] = 0.25f;
        parameters[chParam(rank, CP_fIl)] = 0.0f;
        parameters[chParam(rank, CP_fAl)] = 0.8f;
    }
    const auto initial = parameters;
    Engine engine;
    engine.setParams(parameters);
    engine.prepare(sampleRate);
    engine.reset();
    if (!engine.isParityMode()) throw std::runtime_error("Feature isolation not engaged");
    std::vector<float> samples(static_cast<size_t>(sampleCount) * 2);
    struct Change { int parameter; float oldValue, newValue; };
    struct AppliedEvent
    {
        int sequence, sample;
        const char* type;
        std::vector<Change> changes;
        int note = -1, channel = 1;
        float value = 0.0f;
    };
    std::vector<AppliedEvent> applied;
    auto probes = output(directory / "envelope-events.csv");
    probes << "sequence,sample,point,voice,note,rank,envelope,stage,value,elapsed,active,held\n";
    int position = 0;
    const auto renderTo = [&](int end) {
        if (end < position) throw std::runtime_error("Nonmonotonic capture timeline");
        engine.render(samples.data() + static_cast<size_t>(position) * 2, end - position);
        position = end;
    };
    const auto probe = [&](int sequence, const char* point) {
        const auto voice = engine.getVoiceInfo(0);
        for (int rank = 0; rank < 2; ++rank)
        {
            const auto state = engine.getRankEnvelopeState(0, rank);
            const auto row = [&](const char* name, auto envelope) {
                if (!std::isfinite(envelope.value) || !std::isfinite(envelope.elapsed))
                    throw std::runtime_error("Nonfinite software envelope state");
                probes << sequence << ',' << position << ',' << point << ",0," << voice.note
                       << ',' << rank << ',' << name << ',' << static_cast<int>(envelope.stage)
                       << ',' << envelope.value << ',' << envelope.elapsed << ','
                       << voice.active << ',' << voice.held << '\n';
            };
            row("amplitude", state.amplitude);
            row("filter", state.filter);
        }
    };
    constexpr std::array<int, 5> frames {4800, 8640, 28800, 48000, 52800};
    for (int sequence = 0; sequence < static_cast<int>(frames.size()); ++sequence)
    {
        renderTo(frames[static_cast<size_t>(sequence)]);
        if (sequence == 0 || sequence == 3)
        {
            AppliedEvent event {sequence, position, sequence == 0 ? "note_on" : "note_off", {}};
            event.note = 60;
            event.value = sequence == 0 ? 0.75f : 0.0f;
            if (sequence == 0) engine.noteOn(event.note, event.value, event.channel);
            else engine.noteOff(event.note, event.channel);
            applied.push_back(event);
            continue;
        }
        probe(sequence, "before");
        AppliedEvent event {sequence, position, "parameter_batch", {}};
        const int amp = sequence == 1 ? CP_vA : sequence == 2 ? CP_vD : CP_vR;
        const int filter = sequence == 1 ? CP_fA : sequence == 2 ? CP_fD : CP_fR;
        for (int rank = 0; rank < 2; ++rank)
        {
            const float value = sequence == 1 ? (rank == 0 ? 0.2f : 0.25f)
                : sequence == 2 ? (rank == 0 ? 0.8f : 1.2f) : (rank == 0 ? 0.4f : 0.6f);
            for (int local : {amp, filter})
            {
                const int parameter = chParam(rank, local);
                event.changes.push_back({parameter, parameters[parameter], value});
                parameters[parameter] = value;
            }
        }
        engine.setParams(parameters); // One full parameter transaction per batch.
        applied.push_back(event);     // Record only after application, at the actual frame.
        probe(sequence, "after");
        renderTo(position + 1);
        probe(sequence, "after_one_host_frame");
    }
    renderTo(sampleCount);
    probes.close();
    for (float value : samples)
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
    metadata << "{\n  \"schema_version\": 2,\n  \"fixture_id\": \"live-adr-v1\",\n"
             << "  \"capture_kind\": \"engine_candidate_only\",\n  \"analog_evidence\": false,\n"
             << "  \"sample_rate_hz\": " << sampleRate << ",\n  \"sample_count\": " << sampleCount
             << ",\n  \"internal_sample_rate_hz\": " << engine.getInternalSampleRate()
             << ",\n  \"latency_samples\": " << engine.getLatencySamples() << ",\n"
             << "  \"latency_compensated\": false,\n"
             << "  \"channels\": {\"left\": \"left.csv\", \"right\": \"right.csv\"},\n"
             << "  \"envelope_trace\": \"envelope-events.csv\",\n"
             << "  \"event_timing\": \"zero-based host sample; sequence order; applied before rendering that sample\",\n"
             << "  \"render_blocks\": \"split at events, one-host-frame probes and capture end\",\n"
             << "  \"parameter_application\": \"one full setParams per parameter_batch; old/new are submitted core values\",\n"
             << "  \"initial_state\": \"fresh Engine; setParams; prepare; reset; no warmup or discarded samples\",\n"
             << "  \"units\": \"raw engine floats; software envelope state proxies, not calibrated volts\",\n"
             << "  \"postprocessing\": \"none: no clipping, normalization, resampling or trimming\",\n"
             << "  \"engine_output_processing\": \"parity mode bypasses output DC blocker and output soft clip; production decimator and provisional filter saturation retained\",\n"
             << "  \"external_input\": \"disconnected\",\n"
             << "  \"parameter_count\": " << P_numParams << ",\n";
    const auto writeSettings = [&](const char* name, const ParamValues& values) {
        metadata << "  \"" << name << "\": {\n";
        for (int i = 0; i < P_numParams; ++i)
            metadata << "    \"" << paramId(i) << "\": " << values[i]
                     << (i + 1 == P_numParams ? "\n" : ",\n");
        metadata << "  },\n";
    };
    writeSettings("initial_settings", initial);
    writeSettings("final_settings", parameters);
    metadata << "  \"events\": [\n";
    for (size_t i = 0; i < applied.size(); ++i)
    {
        const auto& event = applied[i];
        metadata << "    {\"sequence\": " << event.sequence << ", \"sample\": " << event.sample
                 << ", \"type\": \"" << event.type << "\"";
        if (event.changes.empty())
            metadata << ", \"note\": " << event.note << ", \"channel\": " << event.channel
                     << ", \"value\": " << event.value;
        else
        {
            metadata << ", \"changes\": [";
            for (size_t j = 0; j < event.changes.size(); ++j)
            {
                const auto& change = event.changes[j];
                metadata << "{\"id\": \"" << paramId(change.parameter) << "\", \"old\": "
                         << change.oldValue << ", \"new\": " << change.newValue << "}"
                         << (j + 1 == event.changes.size() ? "" : ", ");
            }
            metadata << ']';
        }
        metadata << '}' << (i + 1 == applied.size() ? "\n" : ",\n");
    }
    metadata << "  ]\n}\n";
    metadata.close();
    std::cout << "Captured live A/D/R candidate trace; no reference comparison or analog evidence.\n";
    return 0;
}

int main(int argc, char** argv)
{
    const bool live = argc == 5 && std::string(argv[3]) == "--fixture"
        && std::string(argv[4]) == "live-adr-v1";
    if ((argc != 3 && !live) || std::string(argv[1]) != "--out" || std::string(argv[2]).empty())
    {
        std::cerr << "Usage: cs80_parity_capture --out DIRECTORY [--fixture live-adr-v1]\n";
        return 2;
    }
    try
    {
        const std::filesystem::path directory(argv[2]);
        if (live) return captureLiveAdr(directory);
        std::filesystem::create_directories(directory);
        // A failed rerun must not leave an older success manifest beside partial CSVs.
        std::filesystem::remove(directory / "capture.json");
        std::filesystem::remove(directory / "envelope-events.csv");
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
