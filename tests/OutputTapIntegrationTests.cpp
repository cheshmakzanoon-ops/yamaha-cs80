// Isolation-only output integration of a declared inferred nominal profile.
// Not measured hardware calibration. The output oracle independently stamps
// physical resistor/capacitor nodes; no production reduction/coefficient helper
// is reused. Reviewed upstream DSP primitives are used only to supply stimuli.
#include "core/Engine.h"
#include "core/RingModulatorCircuit.h"

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
double maximumAudioError = 0.0, maximumVoltageError = 0.0;
void check(bool ok, const char* what)
{
    ++checks;
    if (!ok) { if (failures < 30) std::printf("FAIL: %s\n", what); ++failures; }
}
void near(double actual, double expected, double tolerance, const char* what)
{
    check(std::isfinite(actual) && std::isfinite(expected)
          && std::abs(actual-expected) <= tolerance, what);
}

template<std::size_t N>
std::array<double, N> solve(std::array<std::array<double, N>, N> matrix, std::array<double, N> rhs)
{
    for (std::size_t row = 0; row < N; ++row)
    {
        double scale = 0.0;
        for (double value : matrix[row]) scale = std::max(scale, std::abs(value));
        for (double& value : matrix[row]) value /= scale;
        rhs[row] /= scale;
    }
    for (std::size_t column = 0; column < N; ++column)
    {
        std::size_t pivot = column;
        for (std::size_t row = column+1; row < N; ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        std::swap(matrix[pivot], matrix[column]); std::swap(rhs[pivot], rhs[column]);
        for (std::size_t row = column+1; row < N; ++row)
        {
            const double factor = matrix[row][column]/matrix[column][column];
            for (std::size_t col = column; col < N; ++col) matrix[row][col] -= factor*matrix[column][col];
            rhs[row] -= factor*rhs[column];
        }
    }
    std::array<double, N> solution{};
    for (std::size_t reverse = N; reverse > 0; --reverse)
    {
        const auto row = reverse-1;
        double value = rhs[row];
        for (std::size_t col = row+1; col < N; ++col) value -= matrix[row][col]*solution[col];
        solution[row] = value/matrix[row][row];
    }
    return solution;
}

// Pinned nominal profile: Rs=100, Rh=560, Ra=4700, Rb=560, RL=100k,
// C=10uF. E is source-side capacitor plate; X common; H/L physical taps.
// A separate old-knot solve enforces E-X=storedV and rebases capacitor current
// onto the newly selected topology, including switches while already charged.
struct OutputOracle
{
    enum { E, X, H, L, I, count };
    OutputOracle(double sampleRate, bool selectedHigh) : rate(sampleRate), high(selectedHigh)
    {
        endpoint[0] = fixedVoltage(1.0, 0.0, true)[H];
        endpoint[1] = fixedVoltage(1.0, 0.0, false)[L];
        readout = startReadout = selectedGain();
    }
    template<std::size_t N>
    static void resistors(std::array<std::array<double, N>, N>& m, bool selectedHigh)
    {
        const auto branch = [&](int a, int b, double resistance)
        {
            const double conductance = 1.0/resistance;
            m[a][a] += conductance; m[b][b] += conductance;
            m[a][b] -= conductance; m[b][a] -= conductance;
        };
        m[E][E] += 1.0/100.0;
        branch(X, H, 560.0); branch(X, L, 4700.0);
        m[L][L] += 1.0/560.0;
        m[selectedHigh ? H : L][selectedHigh ? H : L] += 1.0/100000.0;
    }
    static std::array<double, count> fixedVoltage(double source, double chargeVoltage, bool selectedHigh)
    {
        std::array<std::array<double, count>, count> m{};
        std::array<double, count> rhs{};
        resistors(m, selectedHigh);
        m[E][I] = 1.0; m[X][I] = -1.0;
        m[I][E] = 1.0; m[I][X] = -1.0;
        rhs[E] = source/100.0; rhs[I] = chargeVoltage;
        return solve(m, rhs);
    }
    double selectedGain() const { return endpoint[high ? 0 : 1]; }
    void select(bool selectedHigh)
    {
        if (selectedHigh != high)
        {
            startReadout = readout; samplesSinceSwitch = 0; high = selectedHigh;
        }
    }
    OutputTapCircuit::Nodes tick(double source)
    {
        const auto old = fixedVoltage(previousSource, voltage, high);
        const double g = 2.0*10e-6*rate;
        const double history = -g*voltage-old[I];
        std::array<std::array<double, 4>, 4> m{};
        std::array<double, 4> rhs{};
        resistors(m, high);
        m[E][E] += g; m[X][X] += g; m[E][X] -= g; m[X][E] -= g;
        rhs[E] = source/100.0-history; rhs[X] = history;
        const auto node = solve(m, rhs);
        voltage = node[E]-node[X]; previousSource = source;
        // Independent closed-form dezipper between selector events.
        readout = selectedGain()+(startReadout-selectedGain())
                   * std::exp(-++samplesSinceSwitch/(0.005*rate));
        OutputTapCircuit::Nodes out;
        out.capacitorInputVolts = node[E]; out.capacitorVolts = voltage;
        out.commonVolts = node[X]; out.highTapVolts = node[H]; out.lowTapVolts = node[L];
        out.jackVolts = node[high ? H : L];
        out.sourceAmps = (source-node[E])/100.0;
        out.highSeriesAmps = (node[X]-node[H])/560.0;
        out.lowSeriesAmps = (node[X]-node[L])/4700.0;
        out.shuntAmps = node[L]/560.0; out.loadAmps = out.jackVolts/100000.0;
        out.audibleJackVolts = readout*(source-voltage);
        nodes = out;
        return out;
    }
    double rate, voltage = 0.0, previousSource = 0.0, readout = 0.0, startReadout = 0.0;
    std::array<double, 2> endpoint{};
    bool high;
    int samplesSinceSwitch = 0;
    OutputTapCircuit::Nodes nodes;
};

// Independent ideal summing-amplifier constraints given normalized L/R driver
// voltages. Recover the two 47k-input nodes from each 220k feedback equation,
// then apply GENERAL's two 100k inputs and its separate 220k feedback equation.
// No already-loaded jack voltage, arithmetic average or 0.47 constant is used.
double generalSource(double leftPreamp, double rightPreamp)
{
    const double a = -47000.0*leftPreamp/220000.0;
    const double b = -47000.0*rightPreamp/220000.0;
    return -220000.0*(a/100000.0+b/100000.0);
}

ParamValues fixture(bool high = true)
{
    ParamValues p; p[P_parityMode] = 1.0f; p[P_outputLevel] = high ? 1.0f : 0.0f;
    // Oversampling quality (ws2-engine, plan 1.6): the circuit engine now defaults to
    // Standard (4x at 44.1/48 kHz). This fixture's independent oracle runs at the historical 2x
    // internal rate, so pin Eco; per-factor behavior is covered by OversamplingTests.
    p[P_oversampling] = 0.0f;
    p[P_mix] = p[P_detune] = 0.0f; p[P_volume] = 0.61f;
    p[P_ringSpeed] = 77.0f; p[P_ringDepth] = 0.0f; p[P_ringAmount] = 0.6f;
    p[P_chorusOn] = 1.0f; p[P_chorusDepth] = 0.8f;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_saw)] = p[chParam(rank, CP_pulse)] = 0.0f;
        p[chParam(rank, CP_noise)] = p[chParam(rank, CP_vcfLevel)] = 0.0f;
        p[chParam(rank, CP_sine)] = rank == 0 ? 1.0f : 0.0f;
        p[chParam(rank, CP_vLevel)] = 1.0f;
        p[chParam(rank, CP_vA)] = 0.001f; p[chParam(rank, CP_vD)] = 0.04f;
        p[chParam(rank, CP_vS)] = 0.7f; p[chParam(rank, CP_vR)] = 0.1f;
        p[chParam(rank, CP_initLevel)] = p[chParam(rank, CP_afterLevel)] = 0.0f;
    }
    return p;
}
void setup(Engine& e, const ParamValues& p, double rate)
{
    e.setParams(p); e.prepare(rate); e.noteOn(69, 0.8f);
}
std::vector<float> render(Engine& e, int frames, int block = 127)
{
    std::vector<float> samples(static_cast<std::size_t>(frames)*2);
    for (int n = 0; n < frames; n += block)
        e.render(samples.data()+2*n, std::min(block, frames-n));
    return samples;
}

struct Pipeline
{
    Pipeline(const Engine& e, const ParamValues& p)
        : rate(e.getInternalSampleRate()), phase(e.getRankAudioState(0, 0).phase),
          volume(p[P_volume]), volumeTarget(p[P_volume]), wet(p[P_footMode] != 0.0f), wetMix(wet ? 1.0f : 0.0f),
          ringAmount(p[P_ringAmount]), chorusSpeed(p[P_chorusSpeed]), chorusDepth(p[P_chorusDepth]),
          taps{OutputOracle(rate, p[P_outputLevel] != 0.0f), OutputOracle(rate, p[P_outputLevel] != 0.0f),
               OutputOracle(rate, p[P_outputLevel] != 0.0f)}
    {
        envelope.noteOn(0.001f, 0.04f, 0.7f, 0.1f);
        wah.prepare(rate, PraWahCircuit::Profile{}, 1.0);
        chorus.prepare(static_cast<float>(rate));
        for (auto& dec : decimator) dec.prepare(static_cast<float>(rate));
    }
    void pedal(float x) { expression = x; wah.setPedal(x); }
    void select(bool high) { for (auto& tap : taps) tap.select(high); }
    std::array<float, 2> frame()
    {
        const float sr = static_cast<float>(rate);
        for (int os = 0; os < 2; ++os)
        {
            phase += 440.0f/sr; if (phase >= 1.0f) phase -= std::floor(phase);
            const float sine = std::sin(kTwoPi*phase)+0.035f*std::sin(3.0f*kTwoPi*phase);
            const float amp = envelope.tick(sr, true, true);
            float source = sine*amp*0.17f*0.5f;
            ringPhase += 77.0f*(1.0f/sr); ringPhase -= std::floor(ringPhase);
            // ws4 Step 2, intentional circuit-engine change: offset RMO multiplier.
            source = RingModulatorCircuit::blend(source, std::sin(kTwoPi*ringPhase), ringAmount);
            const auto nodes = wah.process(source);
            const float targetMix = wet ? 1.0f : 0.0f;
            if (wetMix != targetMix)
            {
                wetMix += (targetMix-wetMix)*static_cast<float>(-std::expm1(-1.0/(0.012*rate)));
                if (std::abs(wetMix-targetMix) < 1e-4f) wetMix = targetMix;
            }
            source = lerp(static_cast<float>(nodes.x), static_cast<float>(nodes.y), wetMix);
            const auto stereo = chorus.process(source, source, chorusSpeed, chorusDepth, true, false);
            // ws2 parameter smoothing (intentional circuit-engine change): VOLUME
            // and the expression gain follow the declared 5 ms one-pole (the
            // former 0.003 per-sample volume constant was rate dependent),
            // starting on their targets at the first sample; the PRA optical cell
            // reads the pedal target.
            const double k = smoothingCoefficient(kControlSmoothingSeconds, rate);
            if (!gainSnapped) { gainExpression.snap(expression); volumeSmoother.snap(volume); gainSnapped = true; }
            volume = static_cast<float>(volumeSmoother.advance(volumeTarget, k));
            const float smoothed = static_cast<float>(gainExpression.advance(expression, k));
            const float gain = volume*4.0f*smoothed;
            // Phase 1.5 gain staging: fixed reference, then inferred driver headroom.
            const auto stage = [](float x) {
                return static_cast<float>(circuitOutputDriver(static_cast<double>(x) * kCircuitOutputReference));
            };
            const float left = stage(stereo.l*gain), right = stage(stereo.r*gain);
            const auto l = taps[0].tick(left), r = taps[1].tick(right);
            taps[2].tick(generalSource(left, right));
            decimator[0].push(static_cast<float>(l.audibleJackVolts));
            decimator[1].push(static_cast<float>(r.audibleJackVolts));
        }
        return {decimator[0].output(), decimator[1].output()};
    }
    double rate;
    float phase, volume, volumeTarget, expression = 1.0f;
    ControlSmoother gainExpression, volumeSmoother;
    bool gainSnapped = false;
    bool wet;
    float wetMix, ringAmount, ringPhase = 0.0f, chorusSpeed, chorusDepth;
    Adsr envelope;
    PraWahCircuit wah;
    BbdChorus chorus; // isolation policy: OE2 delay path
    std::array<Decimator, 2> decimator;
    std::array<OutputOracle, 3> taps;
};

void compareState(const Engine& e, const Pipeline& oracle)
{
    for (int tap = 0; tap < 3; ++tap)
    {
        const auto actual = e.getOutputTapState(tap);
        const auto& expected = oracle.taps[tap];
        maximumVoltageError = std::max(maximumVoltageError, std::abs(actual.state.capacitorVolts-expected.voltage));
        near(actual.sampleRate, oracle.rate, 0.0, "all output networks prepared at 2x host rate");
        check(actual.mode == (expected.high ? OutputTapCircuit::Mode::high : OutputTapCircuit::Mode::low), "all linked selectors follow current endpoint");
        near(actual.state.capacitorVolts, expected.voltage, 2e-10, "independent nodal capacitor voltage matches Engine");
        near(actual.state.previousSourceVolts, expected.previousSource, 2e-8, "output source is post-volume/expression, GENERAL is pre-coupling sum");
        near(actual.state.readoutGain, expected.readout, 2e-12, "5ms readout matches independent closed-form trajectory");
        const std::array<double, 12> got {
            actual.nodes.capacitorInputVolts, actual.nodes.capacitorVolts, actual.nodes.commonVolts,
            actual.nodes.highTapVolts, actual.nodes.lowTapVolts, actual.nodes.jackVolts,
            actual.nodes.sourceAmps, actual.nodes.highSeriesAmps, actual.nodes.lowSeriesAmps,
            actual.nodes.shuntAmps, actual.nodes.loadAmps, actual.nodes.audibleJackVolts
        };
        const auto& n = expected.nodes;
        const std::array<double, 12> wanted {
            n.capacitorInputVolts, n.capacitorVolts, n.commonVolts, n.highTapVolts, n.lowTapVolts,
            n.jackVolts, n.sourceAmps, n.highSeriesAmps, n.lowSeriesAmps, n.shuntAmps, n.loadAmps, n.audibleJackVolts
        };
        for (std::size_t k = 0; k < got.size(); ++k)
            near(got[k], wanted[k], k >= 6 && k <= 10 ? 2e-11 : 2e-8, "all physical output ports and separate audible readout match independent nodal oracle");
    }
}

void independentFullStage()
{
    for (double rate : {8000.0, 44100.0, 96000.0, 768000.0})
        for (bool initialHigh : {false, true})
        {
            auto p = fixture(initialHigh); Engine e; setup(e, p, rate); Pipeline oracle(e, p);
            double energy = 0.0;
            const int frames = rate == 44100.0 ? 10500 : 1800;
            for (int frame = 0; frame < frames; ++frame)
            {
                if (frame == 73 || frame == 353 || frame == 709 || frame == 1153)
                {
                    const auto before = e.getOutputTapState(0).state;
                    p[P_outputLevel] = 1.0f-p[P_outputLevel]; e.setParams(p); oracle.select(p[P_outputLevel] != 0.0f);
                    const auto after = e.getOutputTapState(0).state;
                    check(before.capacitorVolts == after.capacitorVolts && before.previousSourceVolts == after.previousSourceVolts
                          && before.readoutGain == after.readoutGain, "HIGH/LOW event preserves physical charge and dezipper history before next interval");
                    check(e.getOutputTapState(0).mode == (p[P_outputLevel] != 0.0f ? OutputTapCircuit::Mode::high : OutputTapCircuit::Mode::low),
                          "selector changes next-interval topology immediately");
                }
                if (frame == 179 || frame == 619 || frame == 997)
                {
                    const float pedal = frame == 619 ? 0.0f : frame == 179 ? 0.3f : 0.8f;
                    e.setExpression(pedal); oracle.pedal(pedal);
                }
                if (frame == 401 || frame == 1301)
                {
                    p[P_volume] = frame == 401 ? 0.17f : 0.83f;
                    p[P_footMode] = 1.0f-p[P_footMode]; e.setParams(p);
                    oracle.volumeTarget = p[P_volume]; oracle.wet = p[P_footMode] != 0.0f;
                }
                float actual[2]; e.render(actual, 1); const auto expected = oracle.frame();
                for (int channel = 0; channel < 2; ++channel)
                {
                    const double difference = std::abs(static_cast<double>(actual[channel])-expected[channel]);
                    maximumAudioError = std::max(maximumAudioError, difference);
                    near(actual[channel], expected[channel], 2e-7, "complete post-expression output circuit and decimator routing matches independent oracle");
                    energy += static_cast<double>(actual[channel])*actual[channel];
                }
                if (frame%29 == 0 || frame == frames-1) compareState(e, oracle);
            }
            check(energy > 1e-8, "output integration fixture is nontrivial");
        }
}

bool sameState(const Engine::OutputTapState& a, const Engine::OutputTapState& b)
{
    return a.state.capacitorVolts == b.state.capacitorVolts
        && a.state.previousSourceVolts == b.state.previousSourceVolts
        && a.state.readoutGain == b.state.readoutGain && a.mode == b.mode;
}
void checkReset(const Engine& e, bool high, double rate)
{
    const OutputOracle reference(rate, high);
    for (int tap = 0; tap < 3; ++tap)
    {
        const auto state = e.getOutputTapState(tap);
        check(state.state.capacitorVolts == 0.0 && state.state.previousSourceVolts == 0.0, "reset clears all three independent coupling histories");
        near(state.state.readoutGain, reference.selectedGain(), 2e-15, "reset initializes readout at current physical endpoint, not unity or legacy0.1");
        check(state.nodes.jackVolts == 0.0 && state.nodes.audibleJackVolts == 0.0, "reset clears last-node diagnostics");
        near(state.sampleRate, rate, 0.0, "reset/prepare retains configured internal rate");
    }
}

void lifecycleAndAlwaysRunning()
{
    auto p = fixture(false); Engine e; e.setParams(p); e.prepare(48000.0); checkReset(e, false, 96000.0);
    e.noteOn(69, 0.8f); render(e, 900);
    for (int tap = 0; tap < 3; ++tap)
        check(std::abs(e.getOutputTapState(tap).state.capacitorVolts) > 1e-9, "LOW still clocks all physical output capacitors including unheard GENERAL");
    // The expression multiplier precedes coupling: muting the source cannot
    // silently erase or freeze the charged output-capacitor decay.
    e.setExpression(0.0f);
    // ws2 parameter smoothing (intentional circuit-engine change): the
    // expression gain lands exactly on zero through its 5 ms one-pole; the
    // coupling capacitor (tau about 54 ms) still holds charge to drain.
    render(e, 9600); // 0.2 s at 48 kHz: 40 tau
    auto before = e.getOutputTapState(2);
    render(e, 1);
    auto after = e.getOutputTapState(2);
    check(after.state.previousSourceVolts == 0.0 && after.state.capacitorVolts != before.state.capacitorVolts,
          "zero expression drains rather than freezes the unheard GENERAL capacitor");
    check(after.nodes.audibleJackVolts != 0.0, "zero source still exposes physical stored-charge decay");
    e.allNotesOff(false); before = e.getOutputTapState(2); render(e, 1200); after = e.getOutputTapState(2);
    check(std::abs(after.state.capacitorVolts) < std::abs(before.state.capacitorVolts), "voice-idle periods continue output decay");
    e.reset(); checkReset(e, false, 96000.0);
    // ws4 Step 2, intentional circuit-engine change: with MODULATION up the RMO carrier
    // feedthrough sounds even with no notes, so "cleared" means identical to a fresh engine.
    const auto silent = render(e, 2000);
    Engine fresh; fresh.setParams(p); fresh.prepare(48000.0);
    check(silent == render(fresh, 2000), "reset clears output and decimator tails exactly");
    p[P_outputLevel] = 1.0f; e.setParams(p); e.reset(); checkReset(e, true, 96000.0);
    e.noteOn(69, 0.8f); render(e, 500); e.prepare(192000.0); checkReset(e, true, 384000.0);
    e.noteOn(69, 0.8f); render(e, 500);
    p[P_parityMode] = 0.0f; e.setParams(p); checkReset(e, true, 384000.0);
    e.noteOn(69, 0.8f); render(e, 1100);
    for (int tap = 0; tap < 3; ++tap)
        check(e.getOutputTapState(tap).state.capacitorVolts == 0.0 && e.getOutputTapState(tap).state.previousSourceVolts == 0.0,
              "legacy branch never processes reconstructed output circuitry");
    p[P_parityMode] = 1.0f; p[P_outputLevel] = 0.0f; e.setParams(p); checkReset(e, false, 384000.0);
    check(e.activeVoices() == 0, "mode entry keeps established voice reset contract");
    check(e.getOutputTapState(-1).sampleRate == 0.0 && e.getOutputTapState(3).sampleRate == 0.0, "invalid output diagnostics safely empty");

    // Channel-scoped panic must not wipe the shared output charge.
    e.noteOn(60, 0.8f, 2); e.noteOn(64, 0.8f, 3); render(e, 500);
    before = e.getOutputTapState(2); e.controlChange(120, 0.0f, 2);
    check(sameState(before, e.getOutputTapState(2)), "channel panic preserves global output capacitor state");
    check(e.activeVoices() == 1, "output integration preserves channel-exact panic semantics");
}

void independentGeneralAndInertMono()
{
    auto p = fixture(); p[P_chorusMode] = 1.0f;
    Engine stereo, storedMono; setup(stereo, p, 48000.0);
    p[P_outputMode] = 0.0f; setup(storedMono, p, 48000.0);
    for (int pass = 0; pass < 9; ++pass)
    {
        p[P_outputLevel] = static_cast<float>(pass%2);
        p[P_outputMode] = 1.0f; stereo.setParams(p);
        p[P_outputMode] = 0.0f; storedMono.setParams(p);
        check(render(stereo, 733) == render(storedMono, 733, 61), "P_outputMode remains inert with forced LEFT/RIGHT listening in isolation");
        for (int tap = 0; tap < 3; ++tap)
            check(sameState(stereo.getOutputTapState(tap), storedMono.getOutputTapState(tap)), "inert stored mono cannot change any load or electrical history");
        const auto left = stereo.getOutputTapState(0), right = stereo.getOutputTapState(1), general = stereo.getOutputTapState(2);
        near(general.state.previousSourceVolts, generalSource(left.state.previousSourceVolts, right.state.previousSourceVolts), 1e-15,
             "GENERAL source obeys independently derived ideal active summing equations before coupling");
        near(general.state.capacitorVolts, 0.47*(left.state.capacitorVolts+right.state.capacitorVolts), 2e-12,
             "GENERAL maintains its own continuously clocked nominal charge trajectory");
        near(general.nodes.audibleJackVolts, 0.47*(left.nodes.audibleJackVolts+right.nodes.audibleJackVolts), 2e-12,
             "equal-load GENERAL is0.47sum, not average of jacks");
        check(std::abs(general.nodes.audibleJackVolts-0.5*(left.nodes.audibleJackVolts+right.nodes.audibleJackVolts)) > 1e-9,
              "GENERAL diagnostic is distinguishable from a fabricated mono average");
    }
    const auto lr = render(stereo, 2000);
    bool differs = false;
    for (std::size_t i = 0; i < lr.size(); i += 2) differs = differs || lr[i] != lr[i+1];
    check(differs, "forced-stereo fixture exercises unequal LEFT and RIGHT source signals");
}

void readoutTimeConstantAndPartition()
{
    for (double rate : {8000.0, 44100.0, 48000.0, 192000.0, 768000.0})
    {
        // Silent fixture: ws4 Step 2 carrier feedthrough sounds whenever MODULATION is up,
        // and ws4 Step 3 BBD noise whenever the chorus is on.
        auto p = fixture(); p[P_ringAmount] = 0.0f; p[P_chorusOn] = 0.0f; Engine e; e.setParams(p); e.prepare(rate);
        const OutputOracle oracle(rate*2.0, true);
        p[P_outputLevel] = 0.0f; e.setParams(p);
        const int frames = static_cast<int>(std::round(rate*0.005)); render(e, frames);
        const double time = frames/rate;
        const double expected = oracle.endpoint[1]+(oracle.endpoint[0]-oracle.endpoint[1])*std::exp(-time/0.005);
        for (int tap = 0; tap < 3; ++tap)
        {
            near(e.getOutputTapState(tap).state.readoutGain, expected, 2e-12, "5ms is a sample-rate-independent output readout time constant while silent");
            check(e.getOutputTapState(tap).state.capacitorVolts == 0.0, "silent readout movement never fabricates capacitor charge");
        }
    }
    auto p = fixture(); Engine a, b; setup(a, p, 48000.0); setup(b, p, 48000.0);
    for (int segment = 0; segment < 13; ++segment)
    {
        p[P_outputLevel] = static_cast<float>(segment%2); p[P_footMode] = static_cast<float>((segment/2)%2);
        a.setParams(p); b.setParams(p);
        const float pedal = static_cast<float>((segment*3)%11)/10.0f; a.setExpression(pedal); b.setExpression(pedal);
        check(render(a, 997, 1) == render(b, 997, 251), "output automation is bit-exact render-partition invariant");
        for (int tap = 0; tap < 3; ++tap)
            check(sameState(a.getOutputTapState(tap), b.getOutputTapState(tap)), "all output histories are exactly partition invariant");
    }
}

void boundedPolyphonicSweeps()
{
    double peak = 0.0;
    for (double rate : {8000.0, 44100.0, 192000.0, 768000.0})
    {
        ParamValues p; p[P_parityMode] = 1.0f; p[P_volume] = 1.0f; p[P_chorusOn] = 1.0f;
        p[P_oversampling] = 0.0f; // 2x oracle fixture; see fixture()
        Engine e; e.setParams(p); e.prepare(rate);
        for (int note : {24, 36, 48, 60, 72, 84, 96, 108}) e.noteOn(note, 1.0f);
        for (int step = 0; step < 70; ++step)
        {
            p[P_outputLevel] = static_cast<float>(step%2); p[P_footMode] = static_cast<float>((step/3)%2); e.setParams(p);
            e.setExpression(static_cast<float>(step%5)/4.0f);
            if (step == 40) e.allNotesOff(true);
            for (float sample : render(e, 113))
            {
                check(std::isfinite(sample) && std::abs(sample) < 20.0f, "polyphonic output switching finite within fixture bound");
                peak = std::max(peak, std::abs(static_cast<double>(sample)));
            }
            for (int tap = 0; tap < 3; ++tap)
            {
                const auto state = e.getOutputTapState(tap);
                check(std::isfinite(state.state.capacitorVolts) && std::isfinite(state.nodes.jackVolts)
                      && state.state.readoutGain >= 0.0 && state.state.readoutGain <= 1.0,
                      "every physical output and software readout remains finite and passive");
            }
        }
    }
    std::printf("Polyphonic output fixture peak %.9g (not a universal synth headroom bound)\n", peak);
}
} // namespace

int main()
{
    independentFullStage(); lifecycleAndAlwaysRunning(); independentGeneralAndInertMono();
    readoutTimeConstantAndPartition(); boundedPolyphonicSweeps();
    std::printf("Output pipeline max sample error %.9g; capacitor voltage error %.9g\n", maximumAudioError, maximumVoltageError);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
