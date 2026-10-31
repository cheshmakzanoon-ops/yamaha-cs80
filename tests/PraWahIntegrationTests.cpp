// Engine integration of an inferred nominal PRA circuit, not hardware matching.
// The expected circuit is an independent physical-node companion/MNA solve;
// the source sine, stage order, downstream expression and loaded output
// coupling are reconstructed separately. Envelope/chorus/decimator primitives
// are shared intentionally: this verifies routing, not their physical accuracy.
#include "core/Engine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

using namespace cs80;
namespace
{
int checks = 0, failures = 0;
double maximumOutputError = 0.0, maximumStateError = 0.0, maximumOutputStateError = 0.0;
void check(bool ok, const char* what)
{
    ++checks;
    if (!ok)
    {
        if (failures < 25) std::printf("FAIL: %s\n", what);
        ++failures;
    }
}
void near(double a, double b, double tolerance, const char* what)
{
    check(std::isfinite(a) && std::isfinite(b) && std::abs(a-b) <= tolerance, what);
}
bool sameCaps(const Engine::PraWahState& a, const Engine::PraWahState& b)
{
    for (int k = 0; k < 4; ++k)
        if (a.capacitors[k].voltage != b.capacitors[k].voltage
            || a.capacitors[k].current != b.capacitors[k].current) return false;
    return true;
}
bool zeroCaps(const Engine::PraWahState& a)
{
    for (const auto& c : a.capacitors)
        if (c.voltage != 0.0 || c.current != 0.0) return false;
    return true;
}

// Explicit nominal profile values are pinned here, not read from production.
// This makes accidental profile edits fail instead of redefining the oracle.
// Ten unknowns include source-side input-capacitor plate E; all ideal amplifier
// constraints are stamped directly, without Engine/PraWahCircuit's reduction.
struct NodalCircuit
{
    std::array<double, 4> voltage{}, current{};
    std::array<double, 2> tick(double input, double rate, double opticalOhms)
    {
        enum { X, N, A, P, B, Q, C, D, Y, E, count };
        constexpr std::array<double, 4> capacitors {10e-6, 2.2e-9, 33e-9, 1e-6};
        std::array<double, 4> conductance{}, history{};
        for (int k = 0; k < 4; ++k)
        {
            conductance[k] = 2.0 * capacitors[k] * rate;
            history[k] = -conductance[k] * voltage[k] - current[k];
        }
        const double variableR = 1000.0 + 1.0 / (1.0 / opticalOhms + 1.0 / 470000.0);
        const auto& g = conductance;
        const auto& j = history;
        std::array<std::array<double, count>, count> m{};
        std::array<double, count> rhs{};
        m[X][X] = g[0] + 2e-4; m[X][E] = -g[0]; m[X][N] = -1e-4; rhs[X] = j[0];
        m[N][N] = 2e-4 + 1.0/470.0; m[N][X] = -1e-4; m[N][C] = -1.0/470.0;
        m[A][A] = 1.0; m[A][N] = 1.0;
        m[P][P] = 1.0/variableR + g[1]; m[P][A] = -1.0/variableR; rhs[P] = -j[1];
        m[B][B] = 1.0; m[B][P] = -2.0; m[B][A] = 1.0;
        m[Q][Q] = 1e-4 + g[2]; m[Q][B] = -1e-4; rhs[Q] = -j[2];
        m[C][C] = 1.0; m[C][Q] = -2.0; m[C][B] = 1.0;
        m[D][D] = g[3] + 2e-4; m[D][C] = -g[3]; rhs[D] = -j[3];
        m[Y][Y] = 1.0; m[Y][D] = 2.0; // nominal VR2/R = 20k/10k
        m[E][E] = 1.0 + 1000.0*g[0]; m[E][X] = -1000.0*g[0];
        rhs[E] = input - 1000.0*j[0];
        for (int row = 0; row < count; ++row)
        {
            double scale = 0.0;
            for (double x : m[row]) scale = std::max(scale, std::abs(x));
            for (double& x : m[row]) x /= scale;
            rhs[row] /= scale;
        }
        for (int pivot = 0; pivot < count; ++pivot)
        {
            int best = pivot;
            for (int row = pivot+1; row < count; ++row)
                if (std::abs(m[row][pivot]) > std::abs(m[best][pivot])) best = row;
            std::swap(m[best], m[pivot]); std::swap(rhs[best], rhs[pivot]);
            for (int row = pivot+1; row < count; ++row)
            {
                const double multiplier = m[row][pivot] / m[pivot][pivot];
                for (int col = pivot; col < count; ++col)
                    m[row][col] -= multiplier * m[pivot][col];
                rhs[row] -= multiplier * rhs[pivot];
            }
        }
        std::array<double, count> node{};
        for (int row = count-1; row >= 0; --row)
        {
            double value = rhs[row];
            for (int col = row+1; col < count; ++col) value -= m[row][col] * node[col];
            node[row] = value / m[row][row];
        }
        voltage = {node[E]-node[X], node[P], node[Q], node[D]-node[C]};
        for (int k = 0; k < 4; ++k) current[k] = g[k] * voltage[k] + j[k];
        return {node[X], node[Y]};
    }
};

// Independent output companion/MNA oracle. Nominal values are pinned rather
// than read from OutputTapCircuit defaults or its reduced coefficients. Nodes
// are the source-side capacitor plate, common junction, HIGH tap and LOW tap.
// At a selector event, an ideal voltage source holds the old capacitor voltage
// while the NEW resistor topology determines the rebased old-end current.
struct NodalOutputCircuit
{
    template <size_t N> using Matrix = std::array<std::array<long double, N>, N>;
    template <size_t N> using Vector = std::array<long double, N>;
    template <size_t N> static Vector<N> solve(Matrix<N> m, Vector<N> rhs)
    {
        for (size_t row = 0; row < N; ++row)
        {
            long double scale = 0.0L;
            for (auto x : m[row]) scale = std::max(scale, std::abs(x));
            for (auto& x : m[row]) x /= scale;
            rhs[row] /= scale;
        }
        for (size_t pivot = 0; pivot < N; ++pivot)
        {
            size_t best = pivot;
            for (size_t row = pivot + 1; row < N; ++row)
                if (std::abs(m[row][pivot]) > std::abs(m[best][pivot])) best = row;
            std::swap(m[best], m[pivot]); std::swap(rhs[best], rhs[pivot]);
            for (size_t row = pivot + 1; row < N; ++row)
            {
                const auto multiplier = m[row][pivot] / m[pivot][pivot];
                for (size_t col = pivot; col < N; ++col) m[row][col] -= multiplier * m[pivot][col];
                rhs[row] -= multiplier * rhs[pivot];
            }
        }
        Vector<N> node{};
        for (size_t row = N; row-- > 0;)
        {
            auto value = rhs[row];
            for (size_t col = row + 1; col < N; ++col) value -= m[row][col] * node[col];
            node[row] = value / m[row][row];
        }
        return node;
    }
    template <size_t N> static void branch(Matrix<N>& m, int a, int b, long double g)
    {
        m[a][a] += g; m[b][b] += g; m[a][b] -= g; m[b][a] -= g;
    }
    template <size_t N> static void resistors(Matrix<N>& m, Vector<N>& rhs,
                                             long double source, bool high)
    {
        m[0][0] += 1.0L / 100.0L; rhs[0] += source / 100.0L;
        branch(m, 1, 2, 1.0L / 560.0L);
        branch(m, 1, 3, 1.0L / 4700.0L);
        m[3][3] += 1.0L / 560.0L;
        m[high ? 2 : 3][high ? 2 : 3] += 1.0L / 100000.0L;
    }
    static Vector<5> heldCapacitor(long double source, long double voltage, bool high)
    {
        Matrix<5> m{}; Vector<5> rhs{};
        resistors(m, rhs, source, high);
        m[0][4] = 1.0L; m[1][4] = -1.0L;
        m[4][0] = 1.0L; m[4][1] = -1.0L; rhs[4] = voltage;
        return solve(m, rhs);
    }
    NodalOutputCircuit() { reset(true); }
    void reset(bool selectedHigh)
    {
        high = selectedHigh; capacitor = previousSource = 0.0L; samples = 0;
        const auto unit = heldCapacitor(1.0L, 0.0L, high);
        readoutGain = startGain = targetGain = unit[high ? 2 : 3];
    }
    float tick(float source, double rate, bool selectedHigh)
    {
        if (high != selectedHigh)
        {
            high = selectedHigh; startGain = readoutGain; samples = 0;
            const auto unit = heldCapacitor(1.0L, 0.0L, high);
            targetGain = unit[high ? 2 : 3];
        }
        const long double oldCurrent = heldCapacitor(previousSource, capacitor, high)[4];
        constexpr long double coupling = 10.0e-6L;
        const long double g = 2.0L * coupling * rate, j = -g * capacitor - oldCurrent;
        Matrix<4> m{}; Vector<4> rhs{};
        branch(m, 0, 1, g); rhs[0] -= j; rhs[1] += j;
        resistors(m, rhs, source, high);
        const auto node = solve(m, rhs);
        capacitor = node[0] - node[1]; previousSource = source;
        // Closed-form software dezipper since the last selector event, not
        // another copy of the production recurrence. The physical V is shared.
        readoutGain = targetGain + (startGain - targetGain) * std::exp(-++samples / (0.005L * rate));
        return static_cast<float>(readoutGain * (source - capacitor));
    }
    long double homogeneousStep(double rate) const
    {
        const auto conductance = heldCapacitor(1.0L, 0.0L, high)[4];
        const long double g = 2.0L * 10.0e-6L * rate;
        return (g - conductance) / (g + conductance);
    }
    long double capacitor = 0.0L, previousSource = 0.0L;
    long double readoutGain = 0.0L, startGain = 0.0L, targetGain = 0.0L;
    long long samples = 0;
    bool high = true;
};

ParamValues sinePatch(bool isolation = true, bool wet = false)
{
    ParamValues p;
    p[P_parityMode] = isolation ? 1.0f : 0.0f;
    // Oversampling quality (ws2-engine, plan 1.6): the circuit engine now defaults to
    // Standard (4x at 44.1/48 kHz). This fixture's independent oracle runs at the historical 2x
    // internal rate, so pin Eco; per-factor behavior is covered by OversamplingTests.
    p[P_oversampling] = 0.0f;
    p[P_footMode] = wet ? 1.0f : 0.0f;
    p[P_volume] = 0.25f; p[P_mix] = 0.0f; p[P_detune] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    p[P_ringDepth] = p[P_ringAmount] = 0.0f;
    p[P_ringSpeed] = 77.0f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_noise)] = p[chParam(rank, CP_vcfLevel)] = 0.0f;
        p[chParam(rank, CP_sine)] = rank == 0 ? 1.0f : 0.0f;
        p[chParam(rank, CP_vLevel)] = 1.0f;
        p[chParam(rank, CP_vA)] = 0.001f; p[chParam(rank, CP_vD)] = 0.04f;
        p[chParam(rank, CP_vS)] = 0.7f; p[chParam(rank, CP_vR)] = 0.1f;
        p[chParam(rank, CP_initLevel)] = p[chParam(rank, CP_afterLevel)] = 0.0f;
        p[chParam(rank, CP_pan)] = 0.0f;
    }
    return p;
}
void setup(Engine& e, const ParamValues& p, double rate = 48000.0)
{
    e.setParams(p); e.prepare(rate); e.noteOn(69, 0.8f);
}
std::vector<float> render(Engine& e, int frames, int partition = 127)
{
    std::vector<float> result(static_cast<std::size_t>(frames)*2);
    for (int n = 0; n < frames; n += partition)
        e.render(result.data()+n*2, std::min(partition, frames-n));
    return result;
}

struct PipelineOracle
{
    explicit PipelineOracle(const Engine& e, const ParamValues& p)
        : rate(e.getInternalSampleRate()), phase(e.getRankAudioState(0, 0).phase),
          isolated(p[P_parityMode] != 0.0f), wet(p[P_footMode] != 0.0f), mix(wet ? 1.0f : 0.0f),
          ringAmount(p[P_ringAmount]), chorusOn(p[P_chorusOn] != 0.0f),
          tremolo(p[P_chorusMode] != 0.0f), chorusSpeed(p[P_chorusSpeed]), chorusDepth(p[P_chorusDepth])
    {
        envelope.noteOn(0.001f, 0.04f, 0.7f, 0.1f);
        chorus.prepare(static_cast<float>(rate));
        bbd.prepare(static_cast<float>(rate));
        for (auto& d : decimator) d.prepare(static_cast<float>(rate));
        outputHigh = p[P_outputLevel] != 0.0f;
        for (auto& output : outputs) output.reset(outputHigh);
    }
    void setPedal(float value)
    {
        if (!std::isfinite(value)) return;
        expression = std::clamp(value, 0.0f, 1.0f);
        startLog = opticalLog;
        targetLog = std::log(1000000.0) + (std::log(2000.0)-std::log(1000000.0))*expression;
        opticalSamples = 0;
    }
    std::array<float, 2> frame()
    {
        const float sr = static_cast<float>(rate);
        const float coefficient = static_cast<float>(-std::expm1(-1.0 / (0.012*rate)));
        for (int os = 0; os < 2; ++os)
        {
            phase += 440.0f/sr;
            if (phase >= 1.0f) phase -= std::floor(phase);
            const float sine = std::sin(kTwoPi*phase) + 0.035f*std::sin(3.0f*kTwoPi*phase);
            const float amp = envelope.tick(sr, true, isolated);
            float sample = sine * 1.0f * amp * 1.0f * 0.17f * 0.5f;
            ringPhase += 77.0f * (1.0f/sr); ringPhase -= std::floor(ringPhase);
            if (ringAmount > 0.001f)
                sample = lerp(sample, sample*std::sin(kTwoPi*ringPhase), ringAmount);
            const float targetMix = wet ? 1.0f : 0.0f;
            if (mix != targetMix)
            {
                mix += (targetMix-mix) * coefficient;
                if (std::abs(mix-targetMix) < 1e-4f) mix = targetMix;
            }
            if (isolated)
            {
                // Closed-form response since the last target change, not the
                // production recurrence. Exactly 12ms is one log-R time constant.
                opticalLog = targetLog + (startLog-targetLog)*std::exp(-++opticalSamples/(0.012*rate));
                const auto nodes = circuit.tick(sample, rate, std::exp(opticalLog));
                sample = lerp(static_cast<float>(nodes[0]), static_cast<float>(nodes[1]), mix);
            }
            else if (mix > 0.0f)
            {
                const float targetHz = wahCentreHz(expression);
                legacyHz = legacyRunning ? legacyHz+(targetHz-legacyHz)*coefficient : targetHz;
                legacyRunning = true;
                legacyWah.set(legacyHz, kProvisionalWahQ, sr);
                sample = lerp(sample, legacyWah.process(sample).bp
                              * (kProvisionalWahMakeup/kProvisionalWahQ), mix);
            }
            else if (legacyRunning) { legacyWah.reset(); legacyRunning = false; }
            // Isolation selects the OE2 delay path; the legacy policy keeps the generic chorus.
            const auto ch = isolated ? bbd.process(sample, sample, chorusSpeed, chorusDepth, chorusOn, tremolo)
                                     : chorus.process(sample, sample, chorusSpeed, chorusDepth, chorusOn, tremolo);
            const std::array<float, 2> stereo {ch.l, ch.r};
            for (int channel = 0; channel < 2; ++channel)
            {
                const float dc = stereo[channel]-lastInput[channel]
                                 + std::exp(-kTwoPi*10.0f*(1.0f/sr))*lastDc[channel];
                lastInput[channel] = stereo[channel]; lastDc[channel] = dc;
                // PRA receives unscaled waveform; expression acts after chorus
                // and before the loaded output coupling network. Retain float
                // source scaling, with no additional absolute active-stage gain.
                const float source = stereo[channel] * expression;
                // Phase 1.5 gain staging precedes the loaded output network.
                const float staged = static_cast<float>(
                    circuitOutputDriver(static_cast<double>(source) * kCircuitOutputReference));
                const float out = isolated ? outputs[channel].tick(staged, rate, outputHigh)
                                           : softClip(dc*expression);
                decimator[channel].push(out);
            }
        }
        return {decimator[0].output(), decimator[1].output()};
    }
    double rate, opticalLog = std::log(2000.0), startLog = std::log(2000.0), targetLog = std::log(2000.0);
    int opticalSamples = 0;
    float phase, expression = 1.0f;
    bool isolated, wet, outputHigh = true;
    float mix, ringAmount, ringPhase = 0.0f;
    bool chorusOn, tremolo;
    float chorusSpeed, chorusDepth;
    Adsr envelope;
    NodalCircuit circuit;
    Chorus chorus;
    BbdChorus bbd;
    std::array<Decimator, 2> decimator;
    std::array<NodalOutputCircuit, 2> outputs;
    std::array<float, 2> lastInput{}, lastDc{};
    Svf legacyWah;
    float legacyHz = 0.0f;
    bool legacyRunning = false;
};

void compareState(const Engine& e, const PipelineOracle& oracle)
{
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto state = e.getPraWahState(channel);
        near(state.sampleRate, oracle.rate, 0.0, "circuit configured at 2x host rate");
        near(state.wetMix, oracle.mix, 0.0, "12ms foot switch fade follows established policy");
        if (!oracle.isolated)
        {
            check(zeroCaps(state), "legacy audio never clocks the reconstruction circuit");
            near(state.opticalOhms, 2000.0, 1e-9, "legacy leaves reconstruction optical state dormant");
            continue;
        }
        near(std::log(state.opticalOhms), oracle.opticalLog, 2e-11, "physical pedal reaches log-R optical state");
        near(std::log(state.targetOhms), oracle.targetLog, 2e-14, "pedal controls declared optical target");
        for (int k = 0; k < 4; ++k)
        {
            maximumStateError = std::max(maximumStateError, std::abs(state.capacitors[k].voltage-oracle.circuit.voltage[k]));
            near(state.capacitors[k].voltage, oracle.circuit.voltage[k], 2e-9, "independent MNA capacitor voltage matches Engine");
            near(state.capacitors[k].current, oracle.circuit.current[k], 2e-11, "independent MNA capacitor current matches Engine");
        }
        const auto output = e.getOutputTapState(channel);
        const auto& expectedOutput = oracle.outputs[channel];
        const double expectedVoltage = static_cast<double>(expectedOutput.capacitor);
        maximumOutputStateError = std::max(maximumOutputStateError,
                                           std::abs(output.state.capacitorVolts - expectedVoltage));
        near(output.sampleRate, oracle.rate, 0.0, "output companion runs at Engine internal sample rate");
        check((output.mode == OutputTapCircuit::Mode::high) == oracle.outputHigh,
              "output selector reaches the physical topology independently of wah selection");
        near(output.state.capacitorVolts, expectedVoltage, 2e-11,
             "independent downstream MNA capacitor voltage matches Engine after expression");
        near(output.state.previousSourceVolts, static_cast<double>(expectedOutput.previousSource), 2e-10,
             "output source uses unchanged float scaling after chorus and expression");
        near(output.state.readoutGain, static_cast<double>(expectedOutput.readoutGain), 2e-12,
             "output readout follows independent closed-form five-millisecond selector slew");
    }
}

void testIndependentPipeline()
{
    for (double rate : {8000.0, 44100.0, 96000.0, 768000.0})
        for (bool isolated : {false, true})
        {
            auto p = sinePatch(isolated);
            // At ordinary rate the oracle includes the documented surrounding
            // stages, exposing a circuit placed before ring or after chorus.
            p[P_ringAmount] = rate == 44100.0 ? 0.7f : 0.0f;
            p[P_chorusOn] = rate == 44100.0 ? 1.0f : 0.0f;
            Engine e; setup(e, p, rate);
            PipelineOracle expected(e, p);
            const int frames = rate == 44100.0 ? 12500 : 1700;
            double energy = 0.0;
            for (int n = 0; n < frames; ++n)
            {
                if (n == 73 || n == 611 || n == 1097)
                {
                    const float pedal = n == 73 ? 0.2f : n == 611 ? 0.0f : 0.8f;
                    const auto before = e.getPraWahState(0);
                    e.setExpression(pedal); expected.setPedal(pedal);
                    check(sameCaps(before, e.getPraWahState(0)), "pedal event preserves capacitor history before next sample");
                }
                if (n == 307 || n == 853 || n == 1201 || n == 7000)
                {
                    expected.wet = !expected.wet;
                    p[P_footMode] = expected.wet ? 1.0f : 0.0f;
                    const auto before = e.getPraWahState(0);
                    e.setParams(p);
                    check(sameCaps(before, e.getPraWahState(0)), "EXP/EXP-WAH event never resets circuit history");
                }
                if (isolated && (n == 211 || n == 499 || n == 1357 || n == 6000))
                {
                    expected.outputHigh = !expected.outputHigh;
                    p[P_outputLevel] = expected.outputHigh ? 1.0f : 0.0f;
                    e.setParams(p);
                }
                float actual[2]; e.render(actual, 1);
                const auto wanted = expected.frame();
                for (int channel = 0; channel < 2; ++channel)
                {
                    const double error = std::abs(static_cast<double>(actual[channel])-wanted[channel]);
                    maximumOutputError = std::max(maximumOutputError, error);
                    near(actual[channel], wanted[channel], 2e-7, "Engine full pipeline equals independent PRA, expression and loaded output oracles");
                    energy += static_cast<double>(actual[channel])*actual[channel];
                }
                if (n % 37 == 0 || n == frames-1) compareState(e, expected);
            }
            check(energy > 1e-7, "full-pipeline fixture is nontrivial and audible");
        }
    // Initial WAH selection must be fully wet at reset, not faded in from EXP.
    // Both output selector endpoints must initialize their own loaded readout.
    for (bool high : { true, false })
    {
        auto p = sinePatch(true, true); p[P_chorusOn] = p[P_chorusMode] = 1.0f;
        p[P_outputLevel] = high ? 1.0f : 0.0f;
        Engine e; setup(e, p);
        PipelineOracle expected(e, p);
        for (int n = 0; n < 2000; ++n)
        {
            float actual[2]; e.render(actual, 1); const auto wanted = expected.frame();
            for (int k = 0; k < 2; ++k)
                near(actual[k], wanted[k], 2e-7, "initial wet route, tremolo order and loaded HIGH/LOW endpoint");
        }
        compareState(e, expected);
    }
}

void testAlwaysClockedAndLifecycle()
{
    auto dry = sinePatch(), wet = sinePatch(true, true);
    Engine a, b; setup(a, dry); setup(b, wet);
    PipelineOracle dryHistory(a, dry), wetHistory(b, wet);
    auto predictedRender = [](Engine& e, PipelineOracle& expected, int frames)
    {
        auto actual = render(e, frames);
        for (int n = 0; n < frames; ++n)
        {
            const auto wanted = expected.frame();
            for (int channel = 0; channel < 2; ++channel)
            {
                const double error = std::abs(static_cast<double>(actual[2*n + channel]) - wanted[channel]);
                maximumOutputError = std::max(maximumOutputError, error);
                near(actual[2*n + channel], wanted[channel], 2e-7,
                     "each foot-mode history matches its own independent downstream capacitor history");
            }
        }
        compareState(e, expected);
        return actual;
    };
    for (float pedal : {0.4f, 0.0f, 0.9f})
    {
        a.setExpression(pedal); b.setExpression(pedal);
        dryHistory.setPedal(pedal); wetHistory.setPedal(pedal);
        predictedRender(a, dryHistory, 1900); predictedRender(b, wetHistory, 1900);
        for (int channel = 0; channel < 2; ++channel)
        {
            const auto x = a.getPraWahState(channel), y = b.getPraWahState(channel);
            check(sameCaps(x, y) && x.opticalOhms == y.opticalOhms,
                  "dry and wet selection have exactly identical continuously clocked physical histories");
            check(!zeroCaps(x), "EXP and expression zero still charge the active input network");
        }
    }
    a.setParams(wet); dryHistory.wet = true;
    predictedRender(a, dryHistory, 7000); predictedRender(b, wetHistory, 7000);
    check(a.getPraWahState(0).wetMix == 1.0f, "switch fade reaches wet endpoint");
    for (int channel = 0; channel < 2; ++channel)
        check(sameCaps(a.getPraWahState(channel), b.getPraWahState(channel)),
              "wet enable after EXP preserves exact PRA history without a stale filter tail");
    // The following output capacitors legitimately retain different prior
    // dry/wet input histories. Do not demand exact final-host equality or wait
    // an arbitrary time until rounding hides their difference. Predict each
    // stream independently, then verify the remaining charge difference follows
    // the independently stamped homogeneous pole once their inputs coincide.
    const auto chargeDifference = dryHistory.outputs[0].capacitor - wetHistory.outputs[0].capacitor;
    check(std::abs(chargeDifference) > 1e-12L, "different prior wah outputs leave distinct downstream charge");
    near(a.getOutputTapState(0).state.capacitorVolts - b.getOutputTapState(0).state.capacitorVolts,
         static_cast<double>(chargeDifference), 2e-14,
         "actual downstream charge retains the independently predicted preceding branch history");
    const auto tailA = predictedRender(a, dryHistory, 500);
    const auto tailB = predictedRender(b, wetHistory, 500);
    check(tailA != tailB, "valid downstream charge history remains in host samples after PRA paths coincide");
    const long double wantedDifference = chargeDifference
        * std::pow(dryHistory.outputs[0].homogeneousStep(dryHistory.rate), 1000);
    near(static_cast<double>(dryHistory.outputs[0].capacitor - wetHistory.outputs[0].capacitor),
         static_cast<double>(wantedDifference), 2e-14,
         "remaining downstream history difference decays at the independently stamped output pole");
    near(a.getOutputTapState(0).state.capacitorVolts - b.getOutputTapState(0).state.capacitorVolts,
         static_cast<double>(wantedDifference), 2e-14,
         "actual downstream charge difference follows the predicted pole rather than being erased");

    // Already silent voices do not turn the capacitor network off.
    a.allNotesOff(false); b.allNotesOff(false);
    const auto charged = a.getPraWahState(0);
    render(a, 2000); render(b, 2000);
    check(!sameCaps(charged, a.getPraWahState(0)), "idle voices do not freeze charged circuit decay");
    check(sameCaps(a.getPraWahState(0), b.getPraWahState(0)), "idle tail decays independently of former branch choice");

    a.setExpression(0.25f); render(a, 100);
    a.reset();
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto state = a.getPraWahState(channel);
        check(zeroCaps(state), "reset clears every capacitor voltage and current");
        near(state.opticalOhms, 2000.0, 1e-9, "reset optical state matches expression reset to toe");
        near(state.targetOhms, 2000.0, 1e-9, "reset optical target matches expression reset to toe");
        check(state.wetMix == 1.0f, "reset starts on current WAH selection");
    }
    auto silence = render(a, 2000);
    check(std::all_of(silence.begin(), silence.end(), [](float x) { return x == 0.0f; }), "reset clears audio and decimator tails");
    a.setExpression(0.1f); render(a, 91); a.prepare(96000.0);
    near(a.getPraWahState(0).sampleRate, 192000.0, 0.0, "reprepare updates internal circuit timestep");
    near(a.getPraWahState(0).opticalOhms, 2000.0, 1e-9, "reprepare resets expression and optical state consistently");
    check(zeroCaps(a.getPraWahState(0)), "reprepare clears old-rate capacitor state");
    a.noteOn(69, 0.8f); a.setExpression(0.3f); render(a, 1000);
    auto legacy = wet; legacy[P_parityMode] = 0.0f;
    a.setParams(legacy);
    check(a.activeVoices() == 0 && zeroCaps(a.getPraWahState(0)), "leaving isolation clears active circuit and voices");
    a.noteOn(69, 0.8f); a.setExpression(0.1f); render(a, 1000);
    check(zeroCaps(a.getPraWahState(0)), "legacy WAH uses only the historical SVF");
    a.setParams(dry);
    check(a.activeVoices() == 0 && zeroCaps(a.getPraWahState(0)), "entering isolation clears legacy history");
    near(a.getPraWahState(0).opticalOhms, 2000.0, 1e-9, "mode change optical state matches controller reset");
    check(a.getPraWahState(0).wetMix == 0.0f, "mode reset follows new EXP selection");
    check(a.getPraWahState(-1).sampleRate == 0.0 && a.getPraWahState(2).sampleRate == 0.0,
          "invalid diagnostic channel is safe and empty");
}

void testPedalTimeConstantAndPartition()
{
    for (double rate : {8000.0, 44100.0, 48000.0, 192000.0, 768000.0})
    {
        Engine e; auto p = sinePatch(); e.setParams(p); e.prepare(rate);
        e.setExpression(-1.0f); // finite physical travel clamps to heel
        const int frames = static_cast<int>(std::round(rate*0.012));
        render(e, frames);
        const double time = frames/rate;
        const double expectedLog = std::log(1e6)+(std::log(2000.0)-std::log(1e6))*std::exp(-time/0.012);
        near(std::log(e.getPraWahState(0).opticalOhms), expectedLog, 2e-11,
             "12ms optical time constant is physical-time invariant even while dry and silent");
        near(e.getPraWahState(0).targetOhms, 1e6, 1e-8, "negative expression clamps to heel optical target");
        check(zeroCaps(e.getPraWahState(0)), "zero audio remains exactly zero while optical state moves");
        e.setExpression(2.0f); render(e, 1);
        near(e.getPraWahState(0).targetOhms, 2000.0, 1e-9, "expression above one clamps to toe target");
        e.setExpression(std::numeric_limits<float>::quiet_NaN());
        e.setExpression(std::numeric_limits<float>::infinity());
        render(e, 1);
        near(e.getPraWahState(0).targetOhms, 2000.0, 1e-9, "nonfinite pedal values ignored");
    }
    auto p = sinePatch(); p[P_ringAmount] = 0.6f; p[P_chorusOn] = 1.0f;
    Engine a, b; setup(a, p); setup(b, p);
    for (int step = 0; step < 9; ++step)
    {
        const float pedal = static_cast<float>((step*7)%11)/10.0f;
        a.setExpression(pedal); b.setExpression(pedal);
        p[P_footMode] = static_cast<float>(step%2); a.setParams(p); b.setParams(p);
        check(render(a, 1031, 1) == render(b, 1031, 257), "pedal/foot trajectory is exactly render-partition invariant");
        for (int channel = 0; channel < 2; ++channel)
            check(sameCaps(a.getPraWahState(channel), b.getPraWahState(channel)), "partitioning preserves all capacitor histories exactly");
    }
}

void testFinitePolyphony()
{
    double peak = 0.0;
    for (double rate : {8000.0, 44100.0, 192000.0, 768000.0})
    {
        ParamValues p; p[P_parityMode] = 1.0f; p[P_ringAmount] = 0.8f; p[P_chorusOn] = 1.0f;
        p[P_oversampling] = 0.0f; // 2x oracle fixture; see sinePatch()
        Engine e; e.setParams(p); e.prepare(rate);
        for (int note : {24, 36, 48, 60, 72, 84, 96, 108}) e.noteOn(note, 1.0f);
        for (int step = 0; step < 80; ++step)
        {
            e.setExpression(static_cast<float>(step%7)/6.0f);
            p[P_footMode] = static_cast<float>(step%2); e.setParams(p);
            if (step == 40) for (int note : {24, 36, 48, 60, 72, 84, 96, 108}) e.noteOff(note);
            for (float value : render(e, 113))
            {
                check(std::isfinite(value) && std::abs(value) < 20.0f, "polyphonic control-reversal fixture finite and bounded");
                peak = std::max(peak, std::abs(static_cast<double>(value)));
            }
            for (int channel = 0; channel < 2; ++channel)
                for (const auto& c : e.getPraWahState(channel).capacitors)
                    check(std::isfinite(c.voltage) && std::isfinite(c.current), "polyphonic capacitor histories remain finite");
        }
    }
    std::printf("Polyphonic fixture peak %.9g (fixture bound, not universal analog headroom)\n", peak);
}
} // namespace

int main()
{
    testIndependentPipeline();
    testAlwaysClockedAndLifecycle();
    testPedalTimeConstantAndPartition();
    testFinitePolyphony();
    std::printf("Independent pipeline maximum output error %.9g; PRA capacitor voltage error %.9g; output capacitor voltage error %.9g\n",
                maximumOutputError, maximumStateError, maximumOutputStateError);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
