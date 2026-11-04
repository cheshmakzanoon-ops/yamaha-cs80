#pragma once

#include "ModMatrix.h"
#include "Mseg.h"
#include "PerformanceControls.h"

#include <array>
#include <cstdint>

// The modern modulation layer (IMPROVEMENT_PLAN Phase 6): three MSEGs, the 16-slot matrix
// and the keyboard response curves, evaluated in one place. The engine feeds it the
// current controller/circuit values and reads back one offsets struct per voice and rank
// plus one global struct; nothing else in the voice path knows the matrix exists.
//
// Inert by construction: with no live slot (the default) or under PURE CS-80 nothing is
// evaluated and every offset is the identity (0 or a ratio of exactly 1).
namespace cs80::modern
{

// Session data that is not a host parameter: MSEG shapes and matrix routing. Saved in the
// plugin state (src/plugin/ModernLayerState.*); amounts and rates are host parameters.
struct ModernConfig
{
    std::array<MsegShape, kMsegCount> msegs { defaultShape(0), defaultShape(1), defaultShape(2) };
    std::array<ModSlot, kModSlots> slots {};
    bool operator==(const ModernConfig&) const = default;
};

inline ModernConfig sanitizeConfig(ModernConfig c) noexcept
{
    for (auto& shape : c.msegs)
        shape = sanitizeShape(shape);
    for (auto& slot : c.slots)
        slot = sanitizeSlot(slot);
    return c;
}

// Host-parameter side, refreshed whenever the engine applies parameters.
struct ModernControls
{
    bool enabled = false; // false under PURE CS-80
    std::array<float, kMsegCount> msegRate { 1.0f, 1.0f, 1.0f };
    std::array<float, kModSlots> amount {};
    float velocityCurve = 0.0f, pressureCurve = 0.0f, wheelCurve = 0.0f, keyCurve = 0.0f;
};

// Host transport, once per host block. bpm <= 0 means "not reported" (120 BPM fallback).
struct Transport
{
    double bpm = 0.0;
    double ppq = 0.0;
    bool hasPosition = false;
    bool playing = false;
};

struct GlobalInputs
{
    float modWheel = 0.0f;
    float ribbon = 0.0f;
    float channelPressure = 0.0f;
    float sharedPressure = 0.0f;
    float expression = 1.0f;
    float pitchBend = 0.0f;
    float sub = 0.0f;
    float ringEnvelope = 0.0f;
    std::array<float, 3> lfo {};
};

struct VoiceInputs
{
    std::uint64_t age = 0; // changes at every note-on of this voice
    bool gate = false;     // key or pedal still holding the note
    int note = 60;
    float velocity = 0.0f;
    float pressure = 0.0f;
    float timbre = 0.0f;
    KeyboardContourWeights contour;
    std::array<float, 2> filterEnvelope {}; // per rank, octaves (kProvisionalFilterEnvelopeOctaves full scale)
    std::array<float, 2> ampEnvelope {};
};

class ModernLayer
{
public:
    static constexpr int kVoices = 8;

    void prepare(double internalRate) noexcept
    {
        sampleRate = internalRate > 0.0 ? internalRate : 96000.0;
        reset();
    }

    // Clears envelopes, held values and offsets; keeps configuration and controls.
    void reset() noexcept
    {
        for (auto& voice : voices)
            voice = VoiceState {};
        for (auto& runner : globalRunners)
            runner.clear();
        newest = newestCandidate = SourceValues {};
        newestThisSample = 0;
        anyGateLast = anyGateNow = false;
        monoTriggerPending = false;
        lockPending = transport.hasPosition && transport.playing;
        freeStarted = {};
        globalRate = {};
        noteRng = Rng(0x7f4a7c15u);
        identity();
    }

    void setConfig(const ModernConfig& newConfig) noexcept
    {
        const auto previous = config;
        config = sanitizeConfig(newConfig);
        for (int m = 0; m < kMsegCount; ++m)
            if (config.msegs[(std::size_t) m].trigger != previous.msegs[(std::size_t) m].trigger)
            {
                // A trigger-mode change restarts that envelope cleanly.
                freeStarted[(std::size_t) m] = false;
                globalRunners[(std::size_t) m].clear();
                for (auto& voice : voices)
                    voice.runners[(std::size_t) m].clear();
            }
        rebuild();
    }
    const ModernConfig& getConfig() const noexcept { return config; }

    void setControls(const ModernControls& newControls) noexcept
    {
        controls = newControls;
        rebuild();
    }

    const Transport& getTransport() const noexcept { return transport; }
    void setTransport(const Transport& t) noexcept
    {
        transport = t;
        lockPending = t.hasPosition && t.playing;
    }

    bool isActive() const noexcept { return active; }
    bool hasPanDestinations() const noexcept { return panDestinations; }

    // Once per internal sample, before the voices: advance shared envelopes and evaluate
    // the global destinations. Per-voice sources come from the newest voice of the
    // previous sample (last-note priority).
    void beginSample(const GlobalInputs& in) noexcept
    {
        if (! active)
            return;
        newest = newestCandidate;
        newestThisSample = 0;
        anyGateLast = anyGateNow;
        anyGateNow = false;

        for (int m = 0; m < kMsegCount; ++m)
        {
            if (! msegUsed[(std::size_t) m])
                continue;
            const auto& shape = config.msegs[(std::size_t) m];
            if (shape.trigger == MsegTrigger::poly)
                continue;
            auto& runner = globalRunners[(std::size_t) m];
            const double rate = (double) controls.msegRate[(std::size_t) m];
            if (shape.trigger == MsegTrigger::free)
            {
                // Free: started once, never restarted; a synced one follows the song
                // position at every host block while the transport plays.
                if (lockPending && shape.sync)
                    runner.seek(shape, transport.ppq * rate);
                else if (! freeStarted[(std::size_t) m])
                    runner.trigger(shape);
                freeStarted[(std::size_t) m] = true;
            }
            else if (monoTriggerPending)
                runner.trigger(shape);
            const bool gate = shape.trigger == MsegTrigger::free || anyGateLast;
            runner.tick(shape, advanceFor(m, globalRate[(std::size_t) m]), gate);
        }
        lockPending = false;
        monoTriggerPending = false;

        globalValues = newest;
        globalValues[ModSource::modWheel] = responseCurve(in.modWheel, controls.wheelCurve);
        globalValues[ModSource::ribbon] = clampf(in.ribbon, -1.0f, 1.0f);
        globalValues[ModSource::channelPressure] = responseCurve(in.channelPressure, controls.pressureCurve);
        globalValues[ModSource::sharedPressure] = responseCurve(in.sharedPressure, controls.pressureCurve);
        globalValues[ModSource::expression] = clampf(in.expression, 0.0f, 1.0f);
        globalValues[ModSource::pitchBend] = clampf(in.pitchBend, -1.0f, 1.0f);
        globalValues[ModSource::sub] = in.sub;
        globalValues[ModSource::ringEnvelope] = in.ringEnvelope;
        globalValues[ModSource::lfo1] = in.lfo[0];
        globalValues[ModSource::lfo2] = in.lfo[1];
        globalValues[ModSource::lfo3] = in.lfo[2];
        globalValues[ModSource::constant] = 1.0f;
        for (int m = 0; m < kMsegCount; ++m)
            if (config.msegs[(std::size_t) m].trigger != MsegTrigger::poly)
                globalValues[msegSource(m)] = msegOutput(m, globalRunners[(std::size_t) m].getValue());

        if (! globalDestinations)
            return;
        float subOct = 0.0f, subDepth = 0.0f;
        std::array<float, 3> fx {};
        std::array<float, kMsegCount> rate {};
        for (int i = 0; i < liveCount; ++i)
        {
            const int s = live[(std::size_t) i];
            const auto& slot = config.slots[(std::size_t) s];
            const bool global = isGlobalDestination(slot.destination);
            const bool globalRateSlot = isMsegRateDestination(slot.destination)
                && config.msegs[(std::size_t) msegIndex(slot.destination)].trigger != MsegTrigger::poly;
            if (! global && ! globalRateSlot)
                continue;
            const float v = controls.amount[(std::size_t) s] * globalValues[slot.source]
                            * (slot.via == ModSource::none ? 1.0f : globalValues[slot.via])
                            * destinationScale(slot.destination);
            switch (slot.destination)
            {
                case ModDestination::subSpeed: subOct += v; break;
                case ModDestination::subDepth: subDepth += v; break;
                case ModDestination::fx1Mix: fx[0] += v; break;
                case ModDestination::fx2Mix: fx[1] += v; break;
                case ModDestination::fx3Mix: fx[2] += v; break;
                default: rate[(std::size_t) msegIndex(slot.destination)] += v; break;
            }
        }
        globalOffsets.subSpeedRatio = std::exp2(subOct);
        globalOffsets.subDepthGain = clampf(1.0f + subDepth, 0.0f, 2.0f);
        globalOffsets.fxMix = fx;
        globalRate = rate;
    }

    // Once per internal sample for each active voice, before its ranks render.
    // Extra closed-tail updates advance modulation without changing the original
    // note-latching or global-source publication schedule.
    void processVoice(int voiceIndex, const VoiceInputs& in, bool scheduled = true) noexcept
    {
        if (! active || voiceIndex < 0 || voiceIndex >= kVoices)
            return;
        auto& voice = voices[(std::size_t) voiceIndex];
        if (scheduled && in.age != voice.age)
        {
            // New note on this voice: restart poly envelopes and latch per-note values.
            voice.age = in.age;
            voice.random = noteRng.nextBipolar();
            voice.velocity = responseCurve(in.velocity, controls.velocityCurve);
            const float key = ((float) (in.note - kKeyboardLowestNote) - 0.5f * (float) (kKeyboardKeyCount - 1))
                              / (0.5f * (float) (kKeyboardKeyCount - 1));
            voice.key = bipolarResponseCurve(clampf(key, -1.0f, 1.0f), controls.keyCurve);
            for (int m = 0; m < kMsegCount; ++m)
                if (config.msegs[(std::size_t) m].trigger == MsegTrigger::poly)
                    voice.runners[(std::size_t) m].trigger(config.msegs[(std::size_t) m]);
            if (! anyGateLast)
                monoTriggerPending = true;
        }
        if (scheduled)
            anyGateNow = anyGateNow || in.gate;

        for (int m = 0; m < kMsegCount; ++m)
            if (msegUsed[(std::size_t) m] && config.msegs[(std::size_t) m].trigger == MsegTrigger::poly)
                voice.runners[(std::size_t) m].tick(config.msegs[(std::size_t) m],
                                                    advanceFor(m, voice.rate[(std::size_t) m]), in.gate);

        SourceValues values = globalValues;
        values[ModSource::velocity] = voice.velocity;
        values[ModSource::polyPressure] = responseCurve(in.pressure, controls.pressureCurve);
        values[ModSource::keyFollow] = voice.key;
        values[ModSource::keyboardContour] = in.contour.high - in.contour.low;
        values[ModSource::timbre] = clampf(in.timbre, 0.0f, 1.0f);
        values[ModSource::noteRandom] = voice.random;
        for (int m = 0; m < kMsegCount; ++m)
            if (config.msegs[(std::size_t) m].trigger == MsegTrigger::poly)
                values[msegSource(m)] = msegOutput(m, voice.runners[(std::size_t) m].getValue());
        values[ModSource::filterEnvelope] = in.filterEnvelope[0] / kEnvelopeOctaves;
        values[ModSource::ampEnvelope] = in.ampEnvelope[0];
        if (scheduled && in.age >= newestThisSample)
        {
            newestThisSample = in.age;
            newestCandidate = values;
        }

        std::array<float, 2> pitch {}, pw {}, hp {}, lp {}, hpQ {}, lpQ {}, level {}, pan {};
        std::array<float, kMsegCount> rate {};
        for (int i = 0; i < liveCount; ++i)
        {
            const int s = live[(std::size_t) i];
            const auto& slot = config.slots[(std::size_t) s];
            const bool rank = isRankDestination(slot.destination);
            const bool voiceRate = isMsegRateDestination(slot.destination)
                && config.msegs[(std::size_t) msegIndex(slot.destination)].trigger == MsegTrigger::poly;
            if (! rank && ! voiceRate)
                continue;
            const float scaled = controls.amount[(std::size_t) s] * destinationScale(slot.destination);
            for (int ch = 0; ch < 2; ++ch)
            {
                if (! rank && ch == 1)
                    break; // MSEG rate is per voice: rank I's EG values drive it
                if (rank && ((slot.target == ModTarget::rankI && ch != 0) || (slot.target == ModTarget::rankII && ch != 1)))
                    continue;
                const float v = scaled * sourceValue(values, slot.source, in, ch)
                                * (slot.via == ModSource::none ? 1.0f : sourceValue(values, slot.via, in, ch));
                switch (slot.destination)
                {
                    case ModDestination::pitch: pitch[(std::size_t) ch] += v; break;
                    case ModDestination::pulseWidth: pw[(std::size_t) ch] += v; break;
                    case ModDestination::cutoff: hp[(std::size_t) ch] += v; lp[(std::size_t) ch] += v; break;
                    case ModDestination::hpCutoff: hp[(std::size_t) ch] += v; break;
                    case ModDestination::lpCutoff: lp[(std::size_t) ch] += v; break;
                    case ModDestination::hpResonance: hpQ[(std::size_t) ch] += v; break;
                    case ModDestination::lpResonance: lpQ[(std::size_t) ch] += v; break;
                    case ModDestination::level: level[(std::size_t) ch] += v; break;
                    case ModDestination::pan: pan[(std::size_t) ch] += v; break;
                    default: rate[(std::size_t) msegIndex(slot.destination)] += v; break;
                }
            }
        }
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& o = voice.offsets[(std::size_t) ch];
            o.pitch = pitch[(std::size_t) ch];
            o.pulseWidth = pw[(std::size_t) ch];
            o.hpCutoff = hp[(std::size_t) ch];
            o.lpCutoff = lp[(std::size_t) ch];
            o.hpQ = hpQ[(std::size_t) ch] == 0.0f ? 1.0f : std::exp2(hpQ[(std::size_t) ch]);
            o.lpQ = lpQ[(std::size_t) ch] == 0.0f ? 1.0f : std::exp2(lpQ[(std::size_t) ch]);
            o.level = level[(std::size_t) ch];
            o.pan = pan[(std::size_t) ch];
        }
        voice.rate = rate;
    }

    const RankOffsets& rankOffsets(int voiceIndex, int rank) const noexcept
    {
        return voices[(std::size_t) std::clamp(voiceIndex, 0, kVoices - 1)].offsets[(std::size_t) (rank == 1 ? 1 : 0)];
    }
    const GlobalOffsets& getGlobalOffsets() const noexcept { return globalOffsets; }

    // Diagnostics and UI telemetry (render thread). voice < 0 reads the shared runner.
    float msegValue(int voiceIndex, int index) const noexcept
    {
        if (index < 0 || index >= kMsegCount) return 0.0f;
        const bool poly = config.msegs[(std::size_t) index].trigger == MsegTrigger::poly;
        const auto& runner = poly && voiceIndex >= 0 && voiceIndex < kVoices
            ? voices[(std::size_t) voiceIndex].runners[(std::size_t) index] : globalRunners[(std::size_t) index];
        return runner.getValue();
    }
    // Position of the envelope a player is most likely watching: the newest voice for a
    // poly MSEG, the shared runner otherwise. Negative when nothing runs.
    double displayPosition(int index) const noexcept
    {
        if (index < 0 || index >= kMsegCount || ! active) return -1.0;
        if (config.msegs[(std::size_t) index].trigger != MsegTrigger::poly)
            return globalRunners[(std::size_t) index].isRunning() || globalRunners[(std::size_t) index].getPosition() > 0.0
                ? globalRunners[(std::size_t) index].getPosition() : -1.0;
        const VoiceState* best = nullptr;
        for (const auto& voice : voices)
            if (voice.age != 0 && (best == nullptr || voice.age > best->age))
                best = &voice;
        return best != nullptr ? best->runners[(std::size_t) index].getPosition() : -1.0;
    }

private:
    static constexpr float kEnvelopeOctaves = 3.0f; // kProvisionalFilterEnvelopeOctaves (Dsp.h)

    struct SourceValues
    {
        std::array<float, kModSourceCount> v {};
        float& operator[](ModSource s) noexcept { return v[(std::size_t) s]; }
        float operator[](ModSource s) const noexcept { return v[(std::size_t) s]; }
    };

    struct VoiceState
    {
        std::uint64_t age = 0;
        float random = 0.0f, velocity = 0.0f, key = 0.0f;
        std::array<MsegRunner, kMsegCount> runners {};
        std::array<float, kMsegCount> rate {};
        std::array<RankOffsets, 2> offsets {};
    };

    static ModSource msegSource(int m) noexcept { return (ModSource) ((int) ModSource::mseg1 + m); }
    static int msegIndex(ModDestination d) noexcept { return (int) d - (int) ModDestination::mseg1Rate; }

    float msegOutput(int m, float level) const noexcept
    {
        return config.msegs[(std::size_t) m].bipolar ? 2.0f * level - 1.0f : level;
    }

    static float sourceValue(const SourceValues& values, ModSource s, const VoiceInputs& in, int rank) noexcept
    {
        if (s == ModSource::filterEnvelope)
            return in.filterEnvelope[(std::size_t) rank] / kEnvelopeOctaves;
        if (s == ModSource::ampEnvelope)
            return in.ampEnvelope[(std::size_t) rank];
        return values[s];
    }

    double advanceFor(int m, float rateOctaves) const noexcept
    {
        const auto& shape = config.msegs[(std::size_t) m];
        double rate = (double) controls.msegRate[(std::size_t) m];
        if (rateOctaves != 0.0f)
            rate *= std::exp2((double) clampf(rateOctaves, -8.0f, 8.0f));
        return msegAdvance(rate, shape.sync, transport.bpm, sampleRate);
    }

    void identity() noexcept
    {
        for (auto& voice : voices)
        {
            voice.offsets = {};
            voice.rate = {};
        }
        globalOffsets = GlobalOffsets {};
        globalRate = {};
    }

    // Live-slot list and usage flags; called whenever routing, amounts or PURE change.
    void rebuild() noexcept
    {
        const bool wasActive = active;
        liveCount = 0;
        msegUsed = {};
        globalDestinations = false;
        panDestinations = false;
        if (controls.enabled)
            for (int s = 0; s < kModSlots; ++s)
            {
                const auto& slot = config.slots[(std::size_t) s];
                if (slot.source == ModSource::none || slot.destination == ModDestination::none
                    || controls.amount[(std::size_t) s] == 0.0f)
                    continue;
                live[(std::size_t) liveCount++] = s;
                if (slot.destination == ModDestination::pan)
                    panDestinations = true;
                for (auto source : { slot.source, slot.via })
                    if (source >= ModSource::mseg1 && source <= ModSource::mseg3)
                        msegUsed[(std::size_t) ((int) source - (int) ModSource::mseg1)] = true;
                if (isGlobalDestination(slot.destination) || isMsegRateDestination(slot.destination))
                    globalDestinations = true;
            }
        active = liveCount > 0;
        // Leaving the active state returns every consumer to the identity at once.
        if (wasActive && ! active)
            identity();
        if (! wasActive && active)
        {
            identity();
            newestCandidate = newest = SourceValues {};
        }
        // processVoice rewrites every rank offset each sample; global offsets are only
        // written while a global destination is routed, so clear them when none is.
        if (active && ! globalDestinations)
            globalOffsets = GlobalOffsets {};
    }

    ModernConfig config {};
    ModernControls controls {};
    Transport transport {};
    double sampleRate = 96000.0;
    bool active = false;
    bool globalDestinations = false;
    bool panDestinations = false;
    std::array<int, kModSlots> live {};
    int liveCount = 0;
    std::array<bool, kMsegCount> msegUsed {};

    std::array<VoiceState, kVoices> voices {};
    std::array<MsegRunner, kMsegCount> globalRunners {};
    std::array<float, kMsegCount> globalRate {};
    SourceValues globalValues {}, newest {}, newestCandidate {};
    std::uint64_t newestThisSample = 0;
    bool anyGateLast = false, anyGateNow = false, monoTriggerPending = false;
    bool lockPending = false;
    std::array<bool, kMsegCount> freeStarted {};
    Rng noteRng { 0x7f4a7c15u };
    GlobalOffsets globalOffsets {};
};

} // namespace cs80::modern
