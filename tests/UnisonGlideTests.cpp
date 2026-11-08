// Unison is a software extension: all circuits assigned by one onset must
// share its glide origin, while keeping their existing detune and ownership.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
void check(bool condition, const std::string& label)
{
    ++checks;
    if (!condition)
    {
        if (failures < 30) std::fprintf(stderr, "FAIL: %s\n", label.c_str());
        ++failures;
    }
}

ParamValues patch(bool source, int mode, int glide = 1, float quality = 0.0f)
{
    ParamValues p;
    p[P_sourcePerformance] = source ? 1.0f : 0.0f;
    p[P_voiceMode] = (float) mode;
    p[P_glideMode] = (float) glide;
    p[P_portamento] = 0.1f;
    p[P_oversampling] = quality;
    p[P_unisonDetune] = p[P_detune] = p[P_initBend] = p[P_masterTune] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = p[P_chorusOn] = 0.0f;
    return p;
}
int countFor(int mode) { return mode == 2 ? 2 : mode == 3 ? 4 : mode == 4 ? 8 : 1; }
void render(Engine& engine, int frames, int block = 127)
{
    std::array<float, 254> audio {};
    bool finite = true;
    for (int offset = 0; offset < frames; offset += block)
    {
        const int count = std::min(block, frames - offset);
        engine.render(audio.data(), count);
        for (int i = 0; i < count * 2; ++i) finite &= std::isfinite(audio[(size_t) i]);
    }
    check(finite, "glide audio is finite");
}

// Independent RC solution, sampled after the rendered interval. Compare pitch
// telemetry, not output audio: free-running circuits intentionally have distinct
// phases. Zero variation is only a deterministic fixture, not a hardware claim.
double expectedPitch(double from, double to, int frames, double rate, float portamento)
{
    return portamento > 0.0001f
        ? to + (from - to) * std::exp(-(double) frames / (rate * (double) portamento)) : to;
}
void checkGroup(const Engine& engine, int note, int channel, int count,
                double pitch, int glide, const std::string& label)
{
    int seen = 0;
    for (int v = 0; v < 8; ++v)
    {
        const auto info = engine.getVoiceInfo(v);
        if (!info.active || !info.held || info.note != note || info.channel != channel) continue;
        ++seen;
        const double expected = glide == 2 ? std::round(pitch) : pitch;
        for (int rank = 0; rank < 2; ++rank)
        {
            const double hz = engine.getRankInfo(v, rank).oscillatorHz;
            const double actual = 69.0 + 12.0 * std::log2(hz / 440.0);
            check(std::abs(actual - expected) < 0.0002,
                  label + " circuit " + std::to_string(v) + " rank " + std::to_string(rank));
        }
    }
    check(seen == count, label + " allocation count");
}

void trajectories()
{
    for (bool source : {false, true})
    for (int mode : {2, 3, 4})
    for (int glide : {1, 2})
    for (double rate : {44100.0, 48000.0, 96000.0})
    for (float quality : {0.0f, 2.0f})
    {
        auto p = patch(source, mode, glide, quality);
        Engine engine; engine.prepare(rate); engine.setParams(p);
        double from = 60.0; // cold-engine glide history
        for (int note : {48, 72, 55, 55})
        {
            engine.noteOn(note, 0.8f, 2);
            int elapsed = 0;
            for (int frames : {1, 31, 1024})
            {
                render(engine, frames, elapsed == 0 ? 1 : 127);
                elapsed += frames;
                const std::string label = std::string(source ? "circuit" : "legacy")
                    + " x" + std::to_string(countFor(mode)) + " glide " + std::to_string(glide)
                    + " rate " + std::to_string(rate) + " quality " + std::to_string(quality)
                    + " note " + std::to_string(note) + " sample " + std::to_string(elapsed);
                checkGroup(engine, note, 2, countFor(mode),
                           expectedPitch(from, note, elapsed, rate, p[P_portamento]), glide, label);
            }
            engine.noteOff(note, 2);
            from = note; // next onset uses previous target, not in-flight pitch
        }
    }
}

void disabledAndDetuned()
{
    for (bool source : {false, true})
    for (int mode : {2, 3, 4})
    for (bool closeGate : {false, true})
    {
        auto p = patch(source, mode);
        if (closeGate) p[P_glideAssign] = p[P_footswitchPlugged] = 1.0f;
        else p[P_portamento] = 0.0f;
        Engine engine; engine.prepare(48000.0); engine.setParams(p);
        engine.noteOn(48, .8f); engine.noteOff(48); engine.noteOn(72, .8f);
        render(engine, 8);
        checkGroup(engine, 72, 1, countFor(mode), 72.0, 1, "disabled glide starts every circuit at target");
    }
    for (bool source : {false, true})
    for (int mode : {2, 3, 4})
    {
        auto p = patch(source, mode); p[P_unisonDetune] = 40.0f;
        Engine engine; engine.prepare(48000.0); engine.setParams(p);
        engine.noteOn(72, .8f); render(engine, 32);
        const int count = countFor(mode);
        const double center = expectedPitch(60.0, 72.0, 32, 48000.0, p[P_portamento]);
        for (int v = 0; v < count; ++v)
        for (int rank = 0; rank < 2; ++rank)
        {
            const double actual = 69.0 + 12.0 * std::log2(engine.getRankInfo(v, rank).oscillatorHz / 440.0);
            const double offset = 0.4 * ((double) v / (count - 1) - 0.5);
            check(std::abs(actual - center - offset) < .0002, "unison detune remains centered on the shared glide");
        }
    }
}

void partialAndRejectedOnsets()
{
    for (bool source : {false, true})
    {
        auto p = patch(source, 0);
        Engine engine; engine.prepare(48000.0); engine.setParams(p);
        for (int note = 40; note < 45; ++note) engine.noteOn(note, .8f, 3);
        p[P_voiceMode] = 3.0f; engine.setParams(p); // only three circuits available
        engine.noteOn(72, .8f, 2); render(engine, 32);
        checkGroup(engine, 72, 2, 3, expectedPitch(44, 72, 32, 48000, p[P_portamento]), 1,
                   "partial group shares onset origin");
        engine.noteOn(96, .8f, 4); // rejected: cannot replace last accepted pitch
        engine.noteOff(96, 4);
        engine.noteOff(72, 2);
        p[P_voiceMode] = 2.0f; engine.setParams(p);
        engine.noteOn(55, .8f, 2); render(engine, 32);
        checkGroup(engine, 55, 2, 2, expectedPitch(72, 55, 32, 48000, p[P_portamento]), 1,
                   "rejected onset leaves glide history and reuse intact");
    }
}

void tunedAndPure()
{
    for (bool source : {false, true})
    {
        auto p = patch(source, 3); p[P_tuningMode] = 2.0f;
        Engine engine; engine.prepare(48000.0); engine.setParams(p);
        auto table = engine.getMtsTuning(); table.pitch[48] = 48.25f; table.pitch[72] = 71.5f;
        engine.setMtsTuning(table);
        engine.noteOn(48, .8f); engine.noteOff(48); engine.noteOn(72, .8f); render(engine, 32);
        checkGroup(engine, 72, 1, 4, expectedPitch(48.25, 71.5, 32, 48000, p[P_portamento]), 1,
                   "glide uses accepted tuned pitches");
        p[P_parityMode] = 1.0f; p[P_voiceMode] = 4.0f; engine.setParams(p);
        engine.noteOn(72, .8f); render(engine, 32);
        checkGroup(engine, 72, 1, 1, expectedPitch(60, 72, 32, 48000, p[P_portamento]), 1,
                   "PURE keeps single-circuit behavior");
    }
}
}
int main()
{
    trajectories();
    disabledAndDetuned();
    partialAndRejectedOnsets();
    tunedAndPure();
    std::printf("Unison glide: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
