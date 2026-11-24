#pragma once

// Authored release-colour profiles for the optional piano/EP bodies. These
// are not measured damper constants. The external rank ADSR still owns the
// fundamental and the complete amplitude release, including performance
// sustain, key-release extensions and live timing edits.
//
// A piano damper is spatial contact: modes with a node under it can survive
// longer. Bank discusses this distinction and the calibration limitations in
// Physics-Based Sound Synthesis of the Piano (2000), section 5.2:
// https://home.mit.bme.hu/~bank/thesis/pianomod.pdf
// The finite-pad modal weight below is a bounded voicing approximation; it
// does not claim a measured string/damper geometry or contact-force model.
// The highest piano strings commonly have no dampers:
// https://hub.yamaha.com/pianos/p-acoustic/how-does-a-piano-work/
//
// Electric-piano upper harmonics largely originate in the nonlinear pickup
// response to displacement, rather than separate harmonic beam modes. Their
// release therefore reduces the pickup's excursion control. See Pfeifle,
// DAFx-17, sections 3--5:
// https://www.dafx.de/paper-archive/2017/papers/DAFx17_paper_79.pdf

#include <algorithm>
#include <cmath>

namespace cs80::body
{

enum class DamperFamily { none, piano, tine, reed };

struct DamperProfile
{
    double strength = 0.0;
    double contactSeconds = 0.010;
};

inline DamperProfile captureDamper(DamperFamily family, double soundingNote) noexcept
{
    const double note = std::isfinite(soundingNote)
        ? std::clamp(soundingNote, 0.0, 127.0) : 60.0;
    switch (family)
    {
        case DamperFamily::piano:
        {
            // The exact highest damped key varies by instrument. A smooth
            // authored transition avoids imposing a split-key seam; above
            // G#6 only the existing rank release controls the amplitude.
            const double treble = std::clamp((note - 86.0) / 6.0, 0.0, 1.0);
            return {4.0 * (1.0 - treble * treble * (3.0 - 2.0 * treble)), 0.010};
        }
        case DamperFamily::tine: return {2.2, 0.007};
        case DamperFamily::reed: return {3.0, 0.005};
        default: return {};
    }
}

inline double pianoDamperWeight(double ratio) noexcept
{
    // All members of the fundamental course retain precisely their held
    // loss. Higher partials have a bounded additional colour release, while
    // the rank VCA supplies their common amplitude damping.
    if (!std::isfinite(ratio) || ratio <= 1.05) return 0.0;
    const double mode = std::clamp(ratio, 1.0, 48.0);
    constexpr double pi = 3.1415926535897932384626433832795;
    constexpr double position = 0.14;
    constexpr double width = 0.05;
    const double padPhase = pi * mode * width;
    // Mean sin^2 over a finite contact pad, rather than a strictly monotonic
    // high-cut. Pad width and the floor prevent undamped ideal point nodes.
    const double contact = 0.5 - 0.5 * std::cos(2.0 * pi * mode * position)
                                  * std::sin(padPhase) / padPhase;
    const double upper = 1.0 - std::exp(-0.55 * (mode - 1.0));
    return upper * (0.25 + 0.75 * std::clamp(contact, 0.0, 1.0));
}

inline double damperReleaseSeconds(double seconds) noexcept
{
    // Very short software releases still close the rank VCA immediately;
    // bounding this extra colour response prevents a one-sample modal kill.
    return std::isfinite(seconds) ? std::clamp(seconds, 0.025, 3600.0) : 0.35;
}

inline double damperLoss(double strength, double releaseSeconds,
                         double intervalSeconds) noexcept
{
    return std::exp(-std::max(0.0, strength) * intervalSeconds
                    / damperReleaseSeconds(releaseSeconds));
}

} // namespace cs80::body
