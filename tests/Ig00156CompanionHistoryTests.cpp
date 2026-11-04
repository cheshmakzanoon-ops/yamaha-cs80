// Exact software continuation contract, registered in cpu/lazy-filter-histories.json.
// The eager oracle is frozen at 44e321c and committed by 29f4820; it does not call
// the candidate's companion reconstruction helper. The independently qualified
// phase-correction kernel and existing nodal truth gates remain unchanged.
#include "core/Ig00156Filter.h"
#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
 #include <xmmintrin.h>
 #define CS80_COMPANION_HAS_MXCSR 1
#else
 #define CS80_COMPANION_HAS_MXCSR 0
#endif

namespace cs80 {
#include "reference/Ig00156StageCompanionPrior.inc"
}

namespace {
using Live = cs80::Ig00156Stage;
using Prior = cs80::PriorIg00156Stage;
struct StopAfterFailures {};

struct Checks
{
    std::uint64_t total = 0, failures = 0;
    std::string context;
    int sample = -1;

    void require(bool value, const char* label, int field = -1)
    {
        ++total;
        if (value) return;
        ++failures;
        std::cerr << "FAIL: " << context << " sample=" << sample
                  << " " << label << " field=" << field << '\n';
        if (failures == 32) throw StopAfterFailures {};
    }

    template <class A, class B>
    void bits(A actual, B expected, const char* label, int field = -1)
    {
        static_assert(std::is_same_v<A, B>);
        static_assert(std::is_same_v<A, double> || std::is_same_v<A, float>);
        if constexpr (std::is_same_v<A, double>)
            require(std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected), label, field);
        else
            require(std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected), label, field);
    }
} check;

template <class A, class B>
void state(const A& actual, const B& expected, const char* label)
{
    const std::array a { actual.input, actual.feedback, actual.bp, actual.lp };
    const std::array b { expected.input, expected.feedback, expected.bp, expected.lp };
    for (std::size_t i = 0; i < a.size(); ++i) check.bits(a[i], b[i], label, static_cast<int>(i));
}

template <class A, class B>
void memory(const A& actual, const B& expected)
{
    state(actual.z, expected.z, "memory histories");
    state(actual.v, expected.v, "memory nodes");
    check.bits(actual.slope1, expected.slope1, "memory slope1");
    check.bits(actual.slope2, expected.slope2, "memory slope2");
    check.bits(actual.halfIntegrators, expected.halfIntegrators, "memory integrator half step");
    check.bits(actual.halfCouplings, expected.halfCouplings, "memory coupling half step");
    for (std::size_t i = 0; i < actual.inputs.size(); ++i)
        check.bits(actual.inputs[i], expected.inputs[i], "memory input history", static_cast<int>(i));
    check.bits(actual.inputRate, expected.inputRate, "memory input rate");
}

void observe(const Live& live, const Prior& prior)
{
    state(live.getState(), prior.getState(), "public histories");
    state(live.getNodes(), prior.getNodes(), "public nodes");
    memory(live.getMemory(), prior.getMemory());
    for (std::size_t i = 0; i < live.getInputHistory().size(); ++i)
        check.bits(live.getInputHistory()[i], prior.getInputHistory()[i], "public input history", static_cast<int>(i));
    check.bits(live.halfSteps().integrators, prior.halfSteps().integrators, "integrator half step");
    check.bits(live.halfSteps().couplings, prior.halfSteps().couplings, "coupling half step");
    check.bits(live.getSolverResidual(), prior.getSolverResidual(), "solver residual");
    check.require(live.getLastNewtonSteps() == prior.getLastNewtonSteps(), "Newton count");
    check.require(live.getCoefficientRevision() == prior.getCoefficientRevision(), "coefficient revision");
    check.bits(live.passbandGain(), prior.passbandGain(), "passband gain");
    check.bits(live.qSiemens(), prior.qSiemens(), "Q conductance");
    check.bits(live.feedforwardCutoffHz(), prior.feedforwardCutoffHz(), "feedforward cutoff");
    const auto& a = live.network();
    const auto& b = prior.network();
    const std::array aa {a.sourceOhms, a.groundOhms, a.inputFarads, a.feedbackOhms,
                         a.feedbackFarads, a.integratorFarads, a.feedforwardOhms};
    const std::array bb {b.sourceOhms, b.groundOhms, b.inputFarads, b.feedbackOhms,
                         b.feedbackFarads, b.integratorFarads, b.feedforwardOhms};
    for (std::size_t i = 0; i < aa.size(); ++i)
        check.bits(aa[i], bb[i], "network field", static_cast<int>(i));
    // Repeated exports must not change any public state or continuation.
    state(live.getState(), prior.getState(), "repeated histories");
    memory(live.getMemory(), prior.getMemory());
}

struct Pair
{
    Live live;
    Prior prior;
    explicit Pair(const cs80::Ig00156Network& network) : live(network), prior(network) {}
    void set(float cutoff, float q, float rate, bool idle = false)
    {
        live.set(cutoff, q, rate, idle);
        prior.set(cutoff, q, rate, idle);
    }
    void step(float input)
    {
        const auto a = live.process(input);
        const auto b = prior.process(input);
        check.bits(a.hp, b.hp, "HP output");
        check.bits(a.lp, b.lp, "LP output");
        check.bits(a.bp, b.bp, "BP output");
        check.require(live.getLastNewtonSteps() == prior.getLastNewtonSteps(), "per-sample Newton count");
        check.bits(live.getSolverResidual(), prior.getSolverResidual(), "per-sample solver residual");
    }
    void observe() const { ::observe(live, prior); }
    void reset() { live.reset(); prior.reset(); }
    void clock(float cutoff, float q, float rate, bool idle, bool transfer)
    {
        const auto a = live.halfSteps();
        const auto b = prior.halfSteps();
        set(cutoff, q, rate, idle);
        observe(); // Last sample's histories, even though coefficients have changed.
        if (transfer) { live.reReference(a); prior.reReference(b); }
        observe();
    }
};

float signal(int n, float amplitude, std::uint32_t& random)
{
    random ^= random << 13; random ^= random >> 17; random ^= random << 5;
    if (n % 67 == 0) return std::copysign(std::numeric_limits<float>::denorm_min(), n % 2 ? -1.0f : 1.0f);
    if (n % 31 == 0) return n % 2 ? -0.0f : 0.0f;
    const float noise = static_cast<float>(static_cast<int>(random & 0x1fffffu) - 0x100000) / 1048576.0f;
    const float tone = static_cast<float>(0.7 * std::sin(0.071 * n) + 0.3 * std::sin(0.831 * n));
    return amplitude * (n % 2 ? noise : tone);
}

void trajectories(const cs80::Ig00156Network& network, int networkIndex, float rate, int fpMode)
{
    for (float cutoff : {0.0f, 1000.0f, 12000.0f, 0.45f * rate})
        for (float q : {0.35f, 1.0f, 9.0f})
            for (float amplitude : {0.0f, 1.0e-4f, 0.5f, 8.0f})
                for (bool moving : {false, true})
                    for (int cadence : {0, 1, 17})
                    {
                        check.context = "trajectory network=" + std::to_string(networkIndex)
                            + " rate=" + std::to_string(rate) + " cutoff=" + std::to_string(cutoff)
                            + " q=" + std::to_string(q) + " amplitude=" + std::to_string(amplitude)
                            + " moving=" + std::to_string(moving) + " cadence=" + std::to_string(cadence)
                            + " fp=" + std::to_string(fpMode);
                        Pair p(network);
                        check.sample = -1;
                        p.observe();
                        std::uint32_t random = 0x80f156u;
                        for (int n = 0; n < 257; ++n)
                        {
                            check.sample = n;
                            const float fc = moving ? cutoff * (1.0f + static_cast<float>(n % 11 - 5) * 0.01f) : cutoff;
                            const float resonance = moving ? q + static_cast<float>(n % 7 - 3) * 0.03f : q;
                            p.set(fc, resonance, rate);
                            if (moving && n % 41 == 0) p.observe();
                            p.step(signal(n, amplitude, random));
                            if (cadence > 0 && n % cadence == 0) p.observe();
                        }
                        p.observe();
                    }
}

template <class DestinationState, class SourceState>
DestinationState convertState(const SourceState& source)
{ return {source.input, source.feedback, source.bp, source.lp}; }

Live::Memory convertMemory(const Prior::Memory& source)
{
    return {convertState<Live::State>(source.z), convertState<Live::Nodes>(source.v),
        source.slope1, source.slope2, source.halfIntegrators, source.halfCouplings,
        source.inputs, source.inputRate};
}

void continuePair(Pair& pair, int first, int count)
{
    std::uint32_t random = 0x15680u;
    for (int n = first; n < first + count; ++n)
    {
        check.sample = n;
        pair.step(signal(n, 0.75f, random));
        if (n % 13 == 0) pair.observe();
    }
    pair.observe();
}

void boundaries(const cs80::Ig00156Network& network, int networkIndex, float rate, int fpMode)
{
    check.context = "boundaries network=" + std::to_string(networkIndex)
        + " rate=" + std::to_string(rate) + " fp=" + std::to_string(fpMode);
    Pair p(network);
    p.set(1536.0f, 5.0f, rate);
    continuePair(p, 0, 129);
    // End in pending audio state without first exporting it.
    p.step(0.37f);
    Pair copied = p;
    copied.observe();
    p.observe();
    continuePair(copied, 130, 17);
    Pair assigned(networkIndex == 0 ? cs80::kIg00156LowPassNetwork : cs80::kIg00156HighPassNetwork);
    assigned = copied;
    assigned.observe();
    continuePair(assigned, 147, 13);

    p.step(-0.29f);
    p.set(8192.0f, 0.5f, rate);
    p.observe();
    p.set(4096.0f, 9.0f, rate);
    p.set(2048.0f, 1.0f, rate);
    p.observe();
    p.set(2048.0f, 1.0f, rate); // Unchanged set must keep the same revision/history.
    p.observe();
    continuePair(p, 160, 29);

    p.step(0.61f);
    p.clock(900.0f, 5.0f, 3000.0f, true, true);
    continuePair(p, 189, 67);
    const auto idleMemory = p.prior.getMemory();
    p.clock(12000.0f, 0.5f, rate, false, true);
    continuePair(p, 256, 37);
    // A same-rate audio/idle mode change still replaces the numerical method.
    p.step(0.21f);
    p.clock(1000.0f, 5.0f, rate, true, false);
    continuePair(p, 293, 7);
    p.clock(1000.0f, 5.0f, rate, false, false);
    continuePair(p, 300, 7);
    // The public stage supports direct clock changes without the Engine adapter.
    p.step(-0.4f);
    p.clock(500.0f, 1.0f, 3000.0f, true, false);
    continuePair(p, 307, 11);
    p.clock(500.0f, 1.0f, rate, false, false);
    p.step(0.4f);
    p.live.reReference({0.0, 0.0});
    p.prior.reReference({0.0, 0.0});
    p.observe();
    p.step(-0.2f);
    p.live.reReference({2.0 * p.live.halfSteps().integrators, 0.5 * p.live.halfSteps().couplings});
    p.prior.reReference({2.0 * p.prior.halfSteps().integrators, 0.5 * p.prior.halfSteps().couplings});
    p.observe();
    continuePair(p, 318, 7);

    p.step(0.7f);
    p.reset(); // Discard pending histories, rather than reviving the old input.
    p.observe();
    continuePair(p, 325, 17);
    p.step(-0.3f);
    const Prior::State suppliedHistories {0.125, -0.25, 0.5, -0.75};
    const Prior::Nodes suppliedNodes {-0.0, 0.01, 0.02, -0.03};
    const std::array<double, 4> suppliedInputs {0.2, -0.1, 0.05, -0.025};
    p.live.setState(convertState<Live::State>(suppliedHistories), convertState<Live::Nodes>(suppliedNodes), suppliedInputs);
    p.prior.setState(suppliedHistories, suppliedNodes, suppliedInputs);
    p.observe();
    continuePair(p, 342, 13);
    p.step(0.4f);
    p.live.setState({-0.0, 0.0, -0.0, 0.0}, {0.0, -0.0, 0.0, -0.0});
    p.prior.setState({-0.0, 0.0, -0.0, 0.0}, {0.0, -0.0, 0.0, -0.0});
    p.observe();
    continuePair(p, 355, 11);

    // Feed the same independent eager snapshot to both restore implementations.
    p.step(-0.1f);
    p.live.restoreMemory(convertMemory(idleMemory));
    p.prior.restoreMemory(idleMemory);
    p.observe();
    p.clock(1600.0f, 1.0f, rate, false, true);
    continuePair(p, 366, 37);
    p.step(0.2f);
    const auto audioMemory = p.prior.getMemory();
    memory(p.live.getMemory(), audioMemory);
    p.live.restoreMemory(convertMemory(audioMemory));
    p.prior.restoreMemory(audioMemory);
    p.observe();
    p.set(1600.0f, 1.0f, rate);
    p.observe();
    continuePair(p, 403, 17);
}

void checkRegistration(const char* path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("Cannot open registered lazy-history contract");
    const std::string text((std::istreambuf_iterator<char>(input)), {});
    for (const char* key : {"maximum_output_difference", "maximum_exported_node_history_slope_difference"})
    {
        const std::string needle = std::string("\"") + key + "\"";
        const auto found = text.find(needle);
        const auto colon = found == std::string::npos ? found : text.find(':', found + needle.size());
        if (colon == std::string::npos) throw std::runtime_error("Missing exact zero-difference contract key");
        const char* first = text.c_str() + colon + 1;
        char* end = nullptr;
        const double value = std::strtod(first, &end);
        if (end == first || value != 0.0) throw std::runtime_error("Registered exact limit must remain zero");
    }
}

struct FixedFloatingPointEnvironment
{
    int originalRounding = std::fegetround();
#if CS80_COMPANION_HAS_MXCSR
    unsigned originalMxcsr = _mm_getcsr();
    static constexpr int modes = 2;
    void select(int mode)
    {
        if (std::fesetround(FE_TONEAREST) != 0) throw std::runtime_error("Cannot select fixed nearest rounding");
        const unsigned current = _mm_getcsr();
        _mm_setcsr((current & ~0x8040u) | (mode == 0 ? 0u : 0x8040u));
    }
#else
    static constexpr int modes = 1;
    void select(int)
    {
        if (std::fesetround(FE_TONEAREST) != 0) throw std::runtime_error("Cannot select fixed nearest rounding");
    }
#endif
    ~FixedFloatingPointEnvironment()
    {
        std::fesetround(originalRounding);
#if CS80_COMPANION_HAS_MXCSR
        _mm_setcsr(originalMxcsr);
#endif
    }
};
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("usage: cs80_ig00156_companion_history_tests CONTRACT");
        checkRegistration(argv[1]);
        FixedFloatingPointEnvironment fp;
        for (int mode = 0; mode < fp.modes; ++mode)
        {
            fp.select(mode);
            int networkIndex = 0;
            for (const auto& network : {cs80::kIg00156HighPassNetwork, cs80::kIg00156LowPassNetwork})
            {
                for (float rate : {88200.0f, 96000.0f, 176400.0f, 192000.0f, 352800.0f, 384000.0f})
                {
                    trajectories(network, networkIndex, rate, mode);
                    boundaries(network, networkIndex, rate, mode);
                }
                ++networkIndex;
            }
        }
        std::cout << "Fixed FP modes: " << fp.modes
                  << " (nearest rounding; FTZ/DAZ off and on where MXCSR is available)\n";
    }
    catch (const StopAfterFailures&) {}
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    std::cout << "Companion histories: " << check.total << " checks, " << check.failures << " failures\n";
    return check.failures == 0 ? 0 : 1;
}
