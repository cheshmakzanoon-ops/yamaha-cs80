#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// Drawable multi-segment envelopes (IMPROVEMENT_PLAN Phase 6). A modern software
// modulation source with no CS-80 counterpart; inert under PURE CS-80.
//
// A shape is a list of points (time, level) joined by curved segments. Times are absolute
// from the shape start, in seconds when free-running or in beats (quarter notes) when
// tempo-synced. Levels are 0..1; a bipolar shape is read as 2 * level - 1 by the matrix.
// Two point indices mark a loop region; how it is used depends on the loop mode.
namespace cs80::modern
{

inline constexpr int kMsegCount = 3;
inline constexpr int kMsegMaxPoints = 32;
inline constexpr float kMsegMaxTime = 64.0f; // seconds, or beats when synced

// Segment curvature law. c = 0 is a straight line. c = +1 is a single-RC charge that
// settles to -60 dB of its span at the segment end (exponent ln 1000, the same time law
// as the circuit-engine envelopes, docs/ENVELOPE_RC_MODEL.md); c = -1 is its mirror,
// slow at the start. Intermediate values scale the exponent. A design choice for a
// modern control, not CS-80 evidence.
inline constexpr double kMsegCurveExponent = 6.907755278982137; // ln(1000)

inline double msegCurve(double t, double curve) noexcept
{
    t = std::clamp(t, 0.0, 1.0);
    if (std::abs(curve) < 1.0e-4)
        return t;
    const double a = kMsegCurveExponent * curve;
    return std::expm1(-a * t) / std::expm1(-a);
}

enum class MsegLoop : int
{
    off = 0,     // play once, hold the final level
    sustain = 1, // loop the region while the gate is held; release continues past it
    always = 2,  // loop the region for ever, ignoring the gate
};

enum class MsegTrigger : int
{
    poly = 0,  // one envelope per voice, restarted at each note-on
    mono = 1,  // one shared envelope, restarted when a key goes down with none held
    free = 2,  // one shared envelope that never restarts; transport-locked when synced
};

struct MsegPoint
{
    float time = 0.0f;  // absolute, from the shape start
    float level = 0.0f; // 0..1
    float curve = 0.0f; // -1..1, of the segment that STARTS at this point
    bool operator==(const MsegPoint&) const = default;
};

struct MsegShape
{
    std::array<MsegPoint, kMsegMaxPoints> points {};
    int count = 2;
    int loopStart = 0; // point index
    int loopEnd = 0;   // point index, >= loopStart; equal indices are a sustain point
    MsegLoop loop = MsegLoop::off;
    MsegTrigger trigger = MsegTrigger::poly;
    bool sync = false;
    bool bipolar = false;

    float endTime() const noexcept { return points[(std::size_t) (count - 1)].time; }
    bool operator==(const MsegShape&) const = default;
};

// Bring any shape (restored state, UI edit) into the valid domain: 2..32 points, the first
// at time 0, times non-decreasing and bounded, levels 0..1, curves -1..1, ordered loop
// indices and known enum values. Idempotent.
inline MsegShape sanitizeShape(MsegShape s) noexcept
{
    auto finite = [](float x, float fallback) { return std::isfinite(x) ? x : fallback; };
    s.count = std::clamp(s.count, 2, kMsegMaxPoints);
    float previous = 0.0f;
    for (int i = 0; i < s.count; ++i)
    {
        auto& p = s.points[(std::size_t) i];
        p.time = i == 0 ? 0.0f : std::clamp(finite(p.time, previous), previous, kMsegMaxTime);
        p.level = std::clamp(finite(p.level, 0.0f), 0.0f, 1.0f);
        p.curve = std::clamp(finite(p.curve, 0.0f), -1.0f, 1.0f);
        previous = p.time;
    }
    for (int i = s.count; i < kMsegMaxPoints; ++i)
        s.points[(std::size_t) i] = {};
    s.loopStart = std::clamp(s.loopStart, 0, s.count - 1);
    s.loopEnd = std::clamp(s.loopEnd, s.loopStart, s.count - 1);
    if ((int) s.loop < 0 || (int) s.loop > 2) s.loop = MsegLoop::off;
    if ((int) s.trigger < 0 || (int) s.trigger > 2) s.trigger = MsegTrigger::poly;
    return s;
}

// Starting shapes. They only matter once a matrix slot routes them, so they are free to
// be useful rather than neutral: an envelope with a sustain point, a looping rise/fall and
// a one-shot ramp.
inline MsegShape defaultShape(int index) noexcept
{
    MsegShape s;
    auto set = [&s](int i, float t, float l, float c) { s.points[(std::size_t) i] = { t, l, c }; };
    if (index == 1)
    {
        s.count = 3;
        set(0, 0.0f, 0.0f, 0.4f);
        set(1, 0.5f, 1.0f, -0.4f);
        set(2, 1.0f, 0.0f, 0.0f);
        s.loopStart = 0;
        s.loopEnd = 2;
        s.loop = MsegLoop::always;
        s.bipolar = true;
    }
    else if (index == 2)
    {
        s.count = 2;
        set(0, 0.0f, 0.0f, 0.0f);
        set(1, 2.0f, 1.0f, 0.0f);
    }
    else
    {
        s.count = 4;
        set(0, 0.0f, 0.0f, 0.5f);
        set(1, 0.08f, 1.0f, 0.6f);
        set(2, 0.45f, 0.6f, 0.0f);
        set(3, 0.95f, 0.0f, 0.5f);
        s.loopStart = s.loopEnd = 2;
        s.loop = MsegLoop::sustain;
    }
    return s;
}

// Level of segment `segment` at `position`, optionally starting from `startLevel` instead
// of the segment's own first point (the release segment after an early gate-off).
inline double msegSegmentLevel(const MsegShape& s, int segment, double position, double startLevel) noexcept
{
    const auto& a = s.points[(std::size_t) segment];
    const auto& b = s.points[(std::size_t) std::min(segment + 1, s.count - 1)];
    const double span = (double) b.time - (double) a.time;
    const double t = span > 0.0 ? (position - (double) a.time) / span : 1.0;
    return startLevel + ((double) b.level - startLevel) * msegCurve(t, a.curve);
}

// Static level at a position along the shape, ignoring loops (UI drawing, hit tests).
inline double msegLevelAt(const MsegShape& s, double position) noexcept
{
    if (position <= 0.0)
        return s.points[0].level;
    for (int i = 0; i + 1 < s.count; ++i)
        if (position < (double) s.points[(std::size_t) i + 1].time)
            return msegSegmentLevel(s, i, position, s.points[(std::size_t) i].level);
    return s.points[(std::size_t) (s.count - 1)].level;
}

// Tempo used when the host reports none (or an unusable one).
inline constexpr double kMsegFallbackBpm = 120.0;

inline double effectiveBpm(double hostBpm) noexcept
{
    return std::isfinite(hostBpm) && hostBpm >= 1.0 && hostBpm <= 999.0 ? hostBpm : kMsegFallbackBpm;
}

// Shape units per sample: seconds per second when free, beats per second when synced.
inline double msegAdvance(double rate, bool sync, double hostBpm, double sampleRate) noexcept
{
    const double unitsPerSecond = sync ? effectiveBpm(hostBpm) / 60.0 : 1.0;
    return std::max(0.0, rate) * unitsPerSecond / sampleRate;
}

// One running envelope. tick() takes the gate for this sample: a gate edge acts on the
// current position first, then the position advances by `advance` shape units and the
// level at the new position is returned. `advance` = rate * (sync ? bpm / 60 : 1) / fs.
class MsegRunner
{
public:
    void trigger(const MsegShape& s) noexcept
    {
        position = 0.0;
        segment = 0;
        running = true;
        released = false;
        overrideSegment = -1;
        value = (float) s.points[0].level;
    }

    // Gate fell. In sustain-loop mode the envelope leaves its loop at once: it jumps to the
    // loop end and the following segment starts from the level reached, so a release
    // during the attack or decay never plays the rest of them first.
    void release(const MsegShape& s) noexcept
    {
        if (released || ! running)
            return;
        released = true;
        if (s.loop != MsegLoop::sustain)
            return;
        const double loopEndTime = s.points[(std::size_t) s.loopEnd].time;
        if (position <= loopEndTime)
        {
            position = loopEndTime;
            segment = s.loopEnd;
            overrideSegment = s.loopEnd;
            overrideLevel = value;
            if (s.loopEnd >= s.count - 1)
                running = false; // nothing after the loop: hold the level reached
        }
    }

    float tick(const MsegShape& s, double advance, bool gate) noexcept
    {
        if (! gate)
            release(s);
        if (! running)
            return value;
        position += std::max(0.0, advance);
        wrapAndLocate(s);
        return value = (float) levelHere(s);
    }

    // Jump to an absolute position (transport lock), keeping loop semantics.
    void seek(const MsegShape& s, double newPosition) noexcept
    {
        running = true;
        released = false;
        overrideSegment = -1;
        position = std::max(0.0, newPosition);
        segment = 0;
        wrapAndLocate(s);
        value = (float) levelHere(s);
    }

    float getValue() const noexcept { return value; }
    double getPosition() const noexcept { return position; }
    bool isRunning() const noexcept { return running; }
    bool isReleased() const noexcept { return released; }
    void clear() noexcept { *this = MsegRunner {}; }

private:
    void wrapAndLocate(const MsegShape& s) noexcept
    {
        // A live shape edit may have removed or moved the current segment: search again.
        if (segment < 0 || segment >= s.count || position < (double) s.points[(std::size_t) segment].time)
            segment = 0;
        if (overrideSegment >= s.count)
            overrideSegment = -1;
        const double end = s.points[(std::size_t) (s.count - 1)].time;
        const bool looping = s.loop == MsegLoop::always || (s.loop == MsegLoop::sustain && ! released);
        if (looping)
        {
            const double loopStartTime = s.points[(std::size_t) s.loopStart].time;
            const double loopEndTime = s.points[(std::size_t) s.loopEnd].time;
            const double length = loopEndTime - loopStartTime;
            if (position >= loopEndTime)
            {
                if (length > 0.0)
                {
                    position = loopStartTime + std::fmod(position - loopEndTime, length);
                    segment = s.loopStart;
                }
                else
                {
                    position = loopEndTime; // sustain point: hold
                    segment = s.loopEnd;
                }
            }
        }
        if (position >= end)
        {
            position = end;
            segment = s.count - 1;
            if (! looping)
                running = false;
        }
        while (segment < s.count - 1 && position >= (double) s.points[(std::size_t) segment + 1].time)
            ++segment;
    }

    double levelHere(const MsegShape& s) const noexcept
    {
        if (segment >= s.count - 1)
            return overrideSegment == s.count - 1 ? overrideLevel : s.points[(std::size_t) (s.count - 1)].level;
        const double start = segment == overrideSegment ? overrideLevel : (double) s.points[(std::size_t) segment].level;
        return msegSegmentLevel(s, segment, position, start);
    }

    double position = 0.0;
    int segment = 0;
    bool running = false;
    bool released = false;
    int overrideSegment = -1;
    double overrideLevel = 0.0;
    float value = 0.0f;
};

} // namespace cs80::modern
