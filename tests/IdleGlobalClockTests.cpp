// Truth-first gate: numerical identity is required for CPU-only global-clock
// optimizations. The independent PWM recurrence is the pre-optimization
// Engine::processGlobalSample law. SUB's log-node oracle is the original
// OriginalSubOscillator::circuitRate law. See the PRE-COMMITTED limits in
// tests/parity/tolerances-v2/idle-global-clock.json (2026-11-01).
// A change in audible circuit numerics is NOT permitted by this gate.
#include "core/Engine.h"
#include "core/IdleGlobalClock.h"
#include "core/OriginalSubOscillator.h"
#include "experimental/ContinuousVoiceCircuit.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
int checks = 0, failures = 0;
constexpr int kVoices = 8; // Yamaha's eight hardware voice slots.

void check(bool pass, const std::string& message)
{
    ++checks;
    if (! pass)
    {
        ++failures;
        if (failures <= 10)
            std::cerr << "FAIL: " << message << '\n';
        else if (failures == 11)
            std::cerr << "... additional exact-gate failures suppressed\n";
    }
}

bool sameBits(float a, float b) noexcept
{
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}

bool sameBits(double a, double b) noexcept
{
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// Exhaustive truth table. In particular all LEGACY samples must continue
// requesting the old sine, and one idle voice's staggered clock suffices.
void decisionTruthTable()
{
    int skipped = 0;
    for (int policy = 0; policy <= 1; ++policy)
        for (int active = 0; active <= 1; ++active)
            for (int due = 0; due <= 1; ++due)
            {
                const bool wanted = policy == 0 || active != 0 || due != 0;
                const bool actual = cs80::idle_global_clock::shouldReadPwm(
                    policy != 0, active != 0, due != 0);
                check(actual == wanted, "PWM policy exhaustive truth-table entry "
                                        + std::to_string(policy) + '/'
                                        + std::to_string(active) + '/'
                                        + std::to_string(due));
                if (! actual) ++skipped;
            }
    check(skipped == 1, "only source-policy fully unobserved sample skips sine");
}

// An idle voice's clock starts staggered by its index; every ~3 kHz rank
// evaluation consumes the current *shared* PWM sine. A new note consumes it on
// the very first active sample. There is no right to jump float phase by N*dt.
void pwmTrajectory()
{
    constexpr std::array<int, 3> hostRates { 44100, 48000, 96000 };
    constexpr std::array<int, 2> factors { 2, 4 };
    constexpr std::array<int, 10> gaps { 0, 1, 31, 32, 33, 127, 128, 1024, 9600, 96000 };
    constexpr std::array<float, 4> speeds { 0.0f, 0.03125f, 0.71f, 8.0f };
    std::uint64_t skippedSamples = 0, observedSamples = 0;
    bool mutationFreezeDetected = false;
    bool mutationMissedIdleStepDetected = false;
    for (const int hostRate : hostRates)
        for (const int factor : factors)
        {
            const double internalRate = (double) hostRate * factor;
            const float invSr = 1.0f / (float) internalRate;
            const int idleSamples = cs80::voice_continuity::idleStepSamples(internalRate);
            for (const float speed : speeds)
                for (const int gap : gaps)
                {
                    std::array<int, kVoices> clocks {};
                    for (int vi = 0; vi < kVoices; ++vi)
                        clocks[(std::size_t) vi] = vi * idleSamples / kVoices;
                    float truthPhase = 0.0f, optimizedPhase = 0.0f, frozenPhase = 0.0f;
                    for (int sample = 0; sample < gap + 64; ++sample)
                    {
                        // Voice zero becomes active at the wake boundary. All
                        // other seven keep their free-running staggered clocks.
                        const bool anyActive = sample >= gap;
                        bool due = false;
                        for (int vi = 0; vi < kVoices; ++vi)
                            if (! (vi == 0 && anyActive)
                                && clocks[(std::size_t) vi] + 1 >= idleSamples)
                                due = true;
                        const bool read = cs80::idle_global_clock::shouldReadPwm(true, anyActive, due);
                        check(read == (anyActive || due), "PWM consumption truth at idle/wake");

                        // Original engine arithmetic, independently evaluated
                        // for both trajectories on EVERY internal sample.
                        truthPhase += speed * invSr;
                        truthPhase -= std::floor(truthPhase);
                        const float truthSine = std::sin(cs80::kTwoPi * truthPhase);
                        optimizedPhase += speed * invSr;
                        optimizedPhase -= std::floor(optimizedPhase);
                        check(sameBits(optimizedPhase, truthPhase), "PWM float phase identity");
                        if (read)
                        {
                            const float value = std::sin(cs80::kTwoPi * optimizedPhase);
                            check(sameBits(value, truthSine), "PWM sine exact at rank consumption/wake");
                            ++observedSamples;
                        }
                        else
                            ++skippedSamples;

                        // Negative controls: a frozen idle phase and a sine
                        // incorrectly skipped at a staggered idle read must
                        // each be observable under this reference trajectory.
                        if (read)
                        {
                            mutationFreezeDetected |= ! sameBits(frozenPhase, truthPhase);
                            mutationMissedIdleStepDetected |= due && ! anyActive;
                            frozenPhase += speed * invSr;
                            frozenPhase -= std::floor(frozenPhase);
                        }

                        for (int vi = 0; vi < kVoices; ++vi)
                        {
                            int& clock = clocks[(std::size_t) vi];
                            if (vi == 0 && anyActive)
                                clock = 0;
                            else if (++clock >= idleSamples)
                                clock = 0;
                        }
                    }
                }
        }
    check(skippedSamples > 0 && observedSamples > 0,
          "fixture exercises both skipped and observed sine reads");
    check(mutationFreezeDetected, "negative control detects frozen idle PWM phase");
    check(mutationMissedIdleStepDetected, "negative control exercises staggered idle read");
}

struct OriginalRateOracle
{
    bool primed = false;
    double logRate = 0.0;
    double tick(double targetHz, double smoothing) noexcept
    {
        const double target = std::log(targetHz);
        if (! primed)
        {
            logRate = target;
            primed = true;
        }
        logRate += (target - logRate) * smoothing;
        return std::exp(logRate);
    }
};

// The oracle uses the unoptimized exp(log-rate) law exactly, not a differently
// rounded analytic shortcut. An uncaught cache invalidation or a premature
// convergence skip changes at least one double result.
void subRateMemoTruth()
{
    constexpr std::array<double, 5> targets { 0.7, 1.0, 13.75, 60.0, 200.0 };
    constexpr std::array<double, 5> smoothing { 0.0, 0.000001, 0.001, 0.1, 1.0 };
    constexpr std::array<int, 4> spans { 1, 2, 32, 4096 };
    constexpr std::array<int, 6> functions { 0, 1, 2, 3, 4, 5 };
    for (const double smoothingStep : smoothing)
        for (const int span : spans)
        {
            cs80::OriginalSubOscillator candidate, truthOsc;
            OriginalRateOracle truth;
            candidate.reset();
            truthOsc.reset();
            int sampleIndex = 0;
            for (const double anchor : targets)
            {
                // Exact repeat, one-ULP positive/negative target movements,
                // and target jumps intentionally stress invalidation.
                const std::array<double, 3> controls {
                    anchor, std::nextafter(anchor, std::numeric_limits<double>::infinity()),
                    std::nextafter(anchor, 0.0)
                };
                for (const double target : controls)
                    for (int n = 0; n < span; ++n, ++sampleIndex)
                    {
                        const double rate = candidate.circuitRate(target, smoothingStep);
                        const double old = truth.tick(target, smoothingStep);
                        check(sameBits(rate, old), "SUB log-node rate bit identity");
                        const int function = functions[(std::size_t) (sampleIndex % functions.size())];
                        const float output = candidate.tickCircuit(rate, 96000.0, function, -0.3f);
                        const float expected = truthOsc.tickCircuit(old, 96000.0, function, -0.3f);
                        check(sameBits(output, expected), "SUB waveform bit identity after cached rate");
                        check(sameBits(candidate.getPhase(), truthOsc.getPhase()),
                              "SUB double phase bit identity after cached rate");
                    }
            }
        }

    // Positive one-ULP witness for the mutation gate: retaining a stale target
    // log when the rate moves from 1.0 to its next double changes the result.
    {
        const double oneStep = std::nextafter(1.0, std::numeric_limits<double>::infinity());
        const double newTarget = std::log(oneStep);
        const double changed = std::exp(newTarget);
        check(! sameBits(std::log(1.0), newTarget),
              "one-ULP SUB input change is resolved by the log oracle");
        check(! sameBits(1.0, changed),
              "one-ULP SUB target mutation remains visible in exp output");
        cs80::OriginalSubOscillator candidate;
        candidate.reset();
        const double first = candidate.circuitRate(1.0, 1.0);
        const double second = candidate.circuitRate(oneStep, 1.0);
        check(sameBits(first, 1.0) && sameBits(second, changed),
              "cached SUB target is invalidated on one-ULP input change");
    }
}

cs80::ControlFrame makeFrame(bool sourcePerformance, float oversampling,
                             float pwmRate, float subRate)
{
    cs80::ControlFrame frame;
    // Native factory tab 0 is a compiled, deterministic two-rank source in both
    // policies. Avoid depending on the editor's selected memory-panel bank.
    frame.hardwareSources = { 0, 0 };
    auto& p = frame.effectiveValues;
    p[cs80::P_parityMode] = 1.0f;
    p[cs80::P_sourcePerformance] = sourcePerformance ? 1.0f : 0.0f;
    p[cs80::P_oversampling] = oversampling;
    p[cs80::P_analogNoise] = 0.0f;
    p[cs80::P_chorusOn] = 0.0f;
    p[cs80::P_ringAmount] = 0.0f;
    p[cs80::P_subOriginalMode] = 1.0f;
    p[cs80::P_subOriginalFunction] = 0.0f;
    p[cs80::P_subOriginalSpeed] = subRate;
    p[cs80::P_subVco] = 0.2f;
    p[cs80::P_lfo1Depth] = 0.0f;
    p[cs80::P_lfo2Depth] = 0.0f;
    p[cs80::P_lfo3Depth] = 0.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[cs80::chParam(rank, cs80::CP_pwmSpeed)] = pwmRate * (rank ? 0.73f : 1.0f);
        p[cs80::chParam(rank, cs80::CP_pwmDepth)] = rank ? 0.42f : 0.73f;
        p[cs80::chParam(rank, cs80::CP_saw)] = 1.0f;
        p[cs80::chParam(rank, cs80::CP_pulse)] = 1.0f;
    }
    return frame;
}

// Independent host block partitions are a source-policy *audio* oracle:
// exactly the same sample-accurate timeline must yield exactly the same host
// float bits. The compatibility script separately compares unoptimized and
// optimized circuit audio to the original pinned V2 bytes.
std::vector<float> hostTimeline(int hostRate, bool sourcePerformance,
                                float quality, int initialGap, int blockSize)
{
    cs80::Engine engine;
    engine.prepare((double) hostRate);
    auto controls = makeFrame(sourcePerformance, quality, 0.71f, 4.2f);
    if (! engine.setControlFrame(controls))
        throw std::runtime_error("failed to apply valid source-policy test controls");
    engine.reset();
    engine.setExpression(0.81f);
    std::vector<float> fullAudio;
    std::array<float, 512> block {};
    auto render = [&](int frames)
    {
        while (frames > 0)
        {
            const int n = std::min(frames, blockSize);
            engine.render(block.data(), n);
            fullAudio.insert(fullAudio.end(), block.begin(), block.begin() + 2 * n);
            frames -= n;
        }
    };
    render(initialGap);
    engine.noteOn(48, 0.78f, 1);
    render(137);
    engine.noteOff(48, 1);
    render(17); // short physical release/gate gap
    controls = makeFrame(sourcePerformance, quality, 3.7f, 5.3f);
    if (! engine.setControlFrame(controls))
        throw std::runtime_error("failed to change controls during idle/release");
    render(128);
    engine.noteOn(64, 0.85f, 1);
    render(283);
    engine.noteOff(64, 1);
    render(67);
    return fullAudio;
}

void sourceEngineBlockPartitionTruth()
{
    constexpr std::array<int, 3> rates { 44100, 48000, 96000 };
    constexpr std::array<float, 2> quality { 0.0f, 2.0f }; // 2x, then 8x/4x at 96k
    constexpr std::array<int, 6> gaps { 0, 1, 17, 128, 2048, 48000 };
    constexpr std::array<int, 3> partitions { 17, 128, 256 };
    bool nonZeroAudio = false;
    for (const int rate : rates)
        for (const bool performance : {false, true})
            for (const float q : quality)
                for (const int gap : gaps)
                {
                    const auto truth = hostTimeline(rate, performance, q, gap, 1);
                    for (const float a : truth)
                        nonZeroAudio |= std::abs(a) > 1.0e-6f;
                    for (const int chunk : partitions)
                    {
                        const auto actual = hostTimeline(rate, performance, q, gap, chunk);
                        bool exact = truth.size() == actual.size();
                        if (exact)
                            for (std::size_t n = 0; n < truth.size(); ++n)
                                if (! sameBits(truth[n], actual[n]))
                                {
                                    exact = false;
                                    break;
                                }
                        check(exact, "source-policy wake audio independent of host block partition at "
                                     + std::to_string(rate) + " Hz, gap " + std::to_string(gap)
                                     + ", block " + std::to_string(chunk));
                    }
                }
    check(nonZeroAudio, "source audio fixture produces audible note and wake samples");
}

} // namespace

int main()
{
    decisionTruthTable();
    pwmTrajectory();
    subRateMemoTruth();
    sourceEngineBlockPartitionTruth();
    std::cout << "Idle global clock truth: " << checks << " checks, "
              << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
