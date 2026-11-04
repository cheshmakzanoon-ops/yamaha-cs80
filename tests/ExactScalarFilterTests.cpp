// Zero-ULP continuation comparator registered before implementation in
// c56726fd8ad12b78edf745b319a9ddc5a6696f2f. This driver covers the IG00156
// branch of that contract; oscillator-only changes require its WSC fixtures.
// The generated frozen headers contain complete independent math dependencies.
#include "core/Ig00156Filter.h"
#include "exact_scalar_frozen/Ig00156Filter.h"

#include <array>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#define CS80_EXACT_TEST_X86_FP 1
#else
#define CS80_EXACT_TEST_X86_FP 0
#endif

#pragma STDC FENV_ACCESS ON

namespace frozen = cs80::scalar_kernel_frozen;
namespace {
using Network = cs80::Ig00156Network;
using Stage = cs80::Ig00156Stage;
using ReferenceStage = frozen::Ig00156Stage;
using Phase = cs80::detail::Ig00156PhaseCorrection<Network>;
using ReferencePhase = frozen::detail::Ig00156PhaseCorrection<frozen::Ig00156Network>;
constexpr std::array<float, 8> audioRates {88200, 96000, 176400, 192000, 352800, 384000, 705600, 768000};
constexpr std::array<std::array<float, 2>, 8> controls {{
    {0, .35f}, {40, 9}, {250, .5f}, {1000, 5},
    {6000, .35f}, {12000, 9}, {20000, 1}, {48000, 5}
}};
constexpr std::array<int, 6> snapshotLengths {0, 1, 7, 31, 127, 4095};
constexpr std::uint32_t seed = 0x8dac7955u;
constexpr double pi = 3.14159265358979323846;

struct Checks {
    std::uint64_t count = 0, failures = 0;
    std::string context;
    std::uint64_t sample = 0;

    template<class A, class B>
    void equal(const A& actual, const B& expected, const char* name, int coordinate = -1)
    {
        static_assert(sizeof(A) == sizeof(B));
        ++count;
        bool same;
        if constexpr (std::is_floating_point_v<A>) {
            static_assert(std::is_same_v<A, B>);
            if constexpr (sizeof(A) == sizeof(std::uint32_t))
                same = std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected);
            else
                same = std::bit_cast<std::uint64_t>(actual) == std::bit_cast<std::uint64_t>(expected);
        } else same = actual == expected;
        if (!same) {
            ++failures;
            if (failures <= 24) {
                std::cerr << "FAIL " << context << " sample=" << sample << " " << name;
                if (coordinate >= 0) std::cerr << '[' << coordinate << ']';
                std::cerr << " actual=" << std::setprecision(17) << actual << " expected=" << expected;
                if constexpr (std::is_floating_point_v<A>) {
                    if constexpr (sizeof(A) == 4)
                        std::cerr << " bits=0x" << std::hex << std::bit_cast<std::uint32_t>(actual)
                                  << "/0x" << std::bit_cast<std::uint32_t>(expected) << std::dec;
                    else
                        std::cerr << " bits=0x" << std::hex << std::bit_cast<std::uint64_t>(actual)
                                  << "/0x" << std::bit_cast<std::uint64_t>(expected) << std::dec;
                }
                std::cerr << '\n';
            }
        }
    }
    void require(bool condition, const char* name) { equal(condition, true, name); }
};
Checks check;

std::uint64_t fpControl() noexcept
{
    const auto round = static_cast<std::uint32_t>(std::fegetround());
#if CS80_EXACT_TEST_X86_FP
    // MXCSR bits 0..5 are sticky exception status, not control state.
    return (std::uint64_t(_mm_getcsr() & ~0x3fu) << 32) | round;
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t value = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(value));
    return (value << 32) | round;
#else
    return round;
#endif
}

struct FpEnvironment {
    std::fenv_t environment {};
#if CS80_EXACT_TEST_X86_FP
    unsigned mxcsr = _mm_getcsr();
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t fpcr = 0;
#endif
    FpEnvironment() noexcept
    {
        std::fegetenv(&environment);
#if defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
#endif
    }
    ~FpEnvironment()
    {
        std::fesetenv(&environment);
#if CS80_EXACT_TEST_X86_FP
        _mm_setcsr(mxcsr);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr));
#endif
    }
};

std::vector<unsigned> denormalModes()
{
#if CS80_EXACT_TEST_X86_FP
    alignas(16) std::array<unsigned char, 512> save {};
    _fxsave(save.data());
    unsigned writable = 0;
    std::memcpy(&writable, save.data() + 28, sizeof(writable));
    if (writable == 0) writable = 0xffbfu; // Architectural fallback MXCSR_MASK.
    std::vector<unsigned> modes {0};
    if ((writable & (1u << 15)) != 0) modes.push_back(1u << 15);
    if ((writable & (1u << 6)) != 0) modes.push_back(1u << 6);
    if ((writable & ((1u << 15) | (1u << 6))) == ((1u << 15) | (1u << 6)))
        modes.push_back((1u << 15) | (1u << 6));
    std::cout << "x86 MXCSR supported denormal control combinations=" << modes.size() << '\n';
    return modes;
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    // FPCR.FZ controls binary32/binary64 underflow on AArch64. FZ16 is not
    // relevant to the float/double kernels, and no unavailable DAZ is invented.
    std::cout << "AArch64 FPCR.FZ combinations=2; independent x86 DAZ unavailable\n";
    return {0, 1u << 24};
#else
    std::cout << "Denormal control API unavailable on this target; caller mode retained\n";
    return {0};
#endif
}

void selectDenormalMode(unsigned bits) noexcept
{
#if CS80_EXACT_TEST_X86_FP
    _mm_setcsr((_mm_getcsr() & ~((1u << 15) | (1u << 6))) | bits);
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
    std::uint64_t value = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(value));
    value = (value & ~(std::uint64_t(1) << 24)) | bits;
    __asm__ volatile("msr fpcr, %0" : : "r"(value));
#else
    (void)bits;
#endif
}

frozen::Ig00156Network referenceNetwork(const Network& n)
{
    return {n.sourceOhms, n.groundOhms, n.inputFarads, n.feedbackOhms,
            n.feedbackFarads, n.integratorFarads, n.feedforwardOhms};
}

template<class A, class B>
void compareState(const A& a, const B& b, const char* prefix)
{
    check.equal(a.input, b.input, prefix, 0);
    check.equal(a.feedback, b.feedback, prefix, 1);
    check.equal(a.bp, b.bp, prefix, 2);
    check.equal(a.lp, b.lp, prefix, 3);
}

template<class A, class B>
void compareMemory(const A& a, const B& b)
{
    compareState(a.z, b.z, "memory.companion");
    compareState(a.v, b.v, "memory.node");
    check.equal(a.slope1, b.slope1, "memory.firstSlope");
    check.equal(a.slope2, b.slope2, "memory.secondSlope");
    check.equal(a.halfIntegrators, b.halfIntegrators, "memory.integratorHalfStep");
    check.equal(a.halfCouplings, b.halfCouplings, "memory.couplingHalfStep");
    for (int i = 0; i < 4; ++i) check.equal(a.inputs[i], b.inputs[i], "memory.inputHistory", i);
    check.equal(a.inputRate, b.inputRate, "memory.inputRate");
}

template<class A, class B>
void compareOutput(const A& a, const B& b, const char* prefix)
{
    check.equal(a.lp, b.lp, prefix, 0);
    check.equal(a.hp, b.hp, prefix, 1);
    check.equal(a.bp, b.bp, prefix, 2);
}

template<class A, class B>
void compareStage(const A& a, const B& b)
{
    compareState(a.getNodes(), b.getNodes(), "stage.node");
    compareState(a.getState(), b.getState(), "stage.companion");
    const auto am = a.getMemory();
    const auto bm = b.getMemory();
    compareMemory(am, bm);
    // Repeated observers must preserve the exported value and continuation.
    compareMemory(a.getMemory(), am);
    compareMemory(b.getMemory(), bm);
    for (int i = 0; i < 4; ++i)
        check.equal(a.getInputHistory()[i], b.getInputHistory()[i], "stage.inputHistory", i);
    check.equal(a.halfSteps().integrators, b.halfSteps().integrators, "stage.halfIntegrator");
    check.equal(a.halfSteps().couplings, b.halfSteps().couplings, "stage.halfCoupling");
    check.equal(a.getSolverResidual(), b.getSolverResidual(), "stage.acceptedResidual");
    check.equal(a.getLastNewtonSteps(), b.getLastNewtonSteps(), "stage.NewtonCorrections");
    check.equal(a.getCoefficientRevision(), b.getCoefficientRevision(), "stage.coefficientRevision");
    check.equal(a.qSiemens(), b.qSiemens(), "stage.qConductance");
    check.equal(a.passbandGain(), b.passbandGain(), "stage.passbandGain");
    check.equal(a.feedforwardCutoffHz(), b.feedforwardCutoffHz(), "stage.feedforwardCutoff");
}

void comparePhase(const Phase& a, const ReferencePhase& b)
{
    for (int i = 0; i < 4; ++i) {
        check.equal(a.nodes()[i], b.nodes()[i], "phase.node", i);
        check.equal(a.inputHistory()[i], b.inputHistory()[i], "phase.inputHistory", i);
    }
    check.equal(a.firstSlope(), b.firstSlope(), "phase.firstSlope");
    check.equal(a.secondSlope(), b.secondSlope(), "phase.secondSlope");
    check.equal(a.getHpTanh(), b.getHpTanh(), "phase.acceptedHpOta");
    check.equal(a.getBpTanh(), b.getBpTanh(), "phase.acceptedBpOta");
    check.equal(a.inputRate(), b.inputRate(), "phase.inputRate");
    check.equal(a.solverResidual(), b.solverResidual(), "phase.acceptedResidual");
    check.equal(a.newtonSteps(), b.newtonSteps(), "phase.NewtonCorrections");
}

struct Pair {
    Stage candidate;
    ReferenceStage reference;
    Phase phase;
    ReferencePhase referencePhase;
    explicit Pair(const Network& n) : candidate(n), reference(referenceNetwork(n)),
                                      phase(n), referencePhase(referenceNetwork(n)) {}

    void observe() const
    {
        const auto control = fpControl();
        compareStage(candidate, reference);
        comparePhase(phase, referencePhase);
        check.equal(fpControl(), control, "observer.fpControlPreserved");
    }
    void set(float fc, float q, float rate, bool idle = false)
    {
        const auto control = fpControl();
        candidate.set(fc, q, rate, idle);
        check.equal(fpControl(), control, "candidate.set.fpControlPreserved");
        reference.set(fc, q, rate, idle);
        check.equal(fpControl(), control, "reference.set.fpControlPreserved");
        if (!idle) {
            phase.set(fc, q, rate);
            referencePhase.set(fc, q, rate);
        }
        check.equal(fpControl(), control, "phase.set.fpControlPreserved");
    }
    void process(float input, bool directPhase = true)
    {
        const auto control = fpControl();
        const auto a = candidate.process(input);
        check.equal(fpControl(), control, "candidate.process.fpControlPreserved");
        const auto b = reference.process(input);
        check.equal(fpControl(), control, "reference.process.fpControlPreserved");
        compareOutput(a, b, "stage.output");
        if (directPhase) {
            const auto pa = phase.process(input);
            check.equal(fpControl(), control, "candidate.phase.fpControlPreserved");
            const auto pb = referencePhase.process(input);
            check.equal(fpControl(), control, "reference.phase.fpControlPreserved");
            compareOutput(pa, pb, "phase.output");
        }
        observe();
    }
    void reset()
    {
        candidate.reset(); reference.reset(); phase.reset(); referencePhase.reset();
        observe();
    }
};

std::uint32_t xorshift(std::uint32_t& state) noexcept
{
    state ^= state << 13; state ^= state >> 17; state ^= state << 5;
    return state;
}

float inputAt(int family, std::uint64_t n, float rate, std::uint32_t& state)
{
    switch (family) {
        case 0: return 0.0f;
        case 1: return n == 0 ? 1.0f : 0.0f;
        case 2: return static_cast<float>(.6 * std::sin(2 * pi * 233 * double(n) / rate)
                                        + .2 * std::sin(2 * pi * 3137 * double(n) / rate));
        default: return static_cast<float>(4 * (2 * (double(xorshift(state)) + .5) / 4294967296.0 - 1));
    }
}

std::array<float, 2> movingControls(std::uint64_t n)
{
    auto control = controls[(n / 257) % controls.size()];
    if ((n / 1024) % 2 != 0) {
        control[0] = static_cast<float>(12000 + 11999 * std::sin(double(n) * .0031));
        control[1] = static_cast<float>(4.675 + 4.325 * std::sin(double(n) * .0017));
    }
    if (n % 31 == 0) {
        control[0] = std::nextafter(control[0], std::numeric_limits<float>::infinity());
        control[1] = std::nextafter(control[1], std::numeric_limits<float>::infinity());
    }
    return control;
}

struct NetworkCase { Network network; std::string name; };
std::vector<NetworkCase> nominalNetworks()
{
    return {{cs80::kIg00156HighPassNetwork, "HP"}, {cs80::kIg00156LowPassNetwork, "LP"}};
}

std::vector<NetworkCase> componentNetworks()
{
    std::vector<NetworkCase> cases;
    const std::array<std::pair<double Network::*, const char*>, 7> members {{
        {&Network::sourceOhms, "sourceOhms"}, {&Network::groundOhms, "groundOhms"},
        {&Network::inputFarads, "inputFarads"}, {&Network::feedbackOhms, "feedbackOhms"},
        {&Network::feedbackFarads, "feedbackFarads"}, {&Network::integratorFarads, "integratorFarads"},
        {&Network::feedforwardOhms, "feedforwardOhms"}
    }};
    for (const auto& nominal : nominalNetworks()) for (const auto& [member, name] : members)
        for (double ratio : {.5, 2.}) {
            auto changed = nominal.network;
            changed.*member *= ratio;
            cases.push_back({changed, nominal.name + "/" + name + "/" + std::to_string(ratio)});
        }
    return cases;
}

void fixedCase(const NetworkCase& net, float rate, std::array<float, 2> control, int family)
{
    check.context = "fixed/" + net.name + "/fs=" + std::to_string(rate)
                  + "/fc=" + std::to_string(control[0]) + "/q=" + std::to_string(control[1])
                  + "/input=" + std::to_string(family);
    Pair pair(net.network);
    pair.set(control[0], control[1], rate);
    pair.observe();
    std::uint32_t state = seed;
    for (std::uint64_t n = 0; n < 4096; ++n) {
        check.sample = n;
        // Repeated keys exercise the no-rebuild branch without changing time.
        pair.set(control[0], control[1], rate);
        pair.process(inputAt(family, n, rate, state));
    }
}

void fixedGrid()
{
    for (const auto& net : nominalNetworks()) for (float rate : audioRates)
        for (const auto control : controls) for (int family = 0; family < 4; ++family)
            fixedCase(net, rate, control, family);
    // Additional ground/feedforward stress cases strengthen observation; they
    // do not redefine any component tolerance or acceptance bound.
    for (const auto& net : componentNetworks()) for (float rate : {96000.f, 384000.f})
        for (const auto control : controls) for (int family : {2, 3})
            fixedCase(net, rate, control, family);
}

void movingGrid()
{
    for (const auto& net : nominalNetworks()) for (float rate : audioRates) {
        check.context = "moving/" + net.name + "/fs=" + std::to_string(rate);
        Pair pair(net.network);
        std::uint32_t state = seed;
        for (std::uint64_t n = 0; n < 16384; ++n) {
            check.sample = n;
            const auto control = movingControls(n);
            pair.set(control[0], control[1], rate);
            pair.set(control[0], control[1], rate);
            pair.observe();
            pair.process(inputAt(3, n, rate, state));
        }
    }
}

void copyAndRestore()
{
    for (const auto& net : nominalNetworks()) for (int length : snapshotLengths) {
        check.context = "copy-reset-restore/" + net.name + "/length=" + std::to_string(length);
        Pair original(net.network);
        original.set(12000, 5, 96000);
        std::uint32_t state = seed;
        for (int n = 0; n < length; ++n) {
            check.sample = static_cast<std::uint64_t>(n);
            original.process(inputAt(3, n, 96000, state));
        }
        original.observe();
        const auto cm = original.candidate.getMemory();
        const auto rm = original.reference.getMemory();
        const auto pn = original.phase.nodes(), ph = original.phase.inputHistory();
        const auto rn = original.referencePhase.nodes(), rh = original.referencePhase.inputHistory();
        const auto ps1 = original.phase.firstSlope(), ps2 = original.phase.secondSlope();
        const auto rs1 = original.referencePhase.firstSlope(), rs2 = original.referencePhase.secondSlope();
        const auto pr = original.phase.inputRate(), rr = original.referencePhase.inputRate();
        Pair copied(original);
        Pair assigned(cs80::kIg00156LowPassNetwork);
        assigned = original;
        auto* self = &original;
        original = *self;
        copied.observe(); assigned.observe(); original.observe();
        Pair restored(net.network);
        restored.set(100, 1, 384000);
        restored.candidate.restoreMemory(cm); restored.reference.restoreMemory(rm);
        restored.phase.restore(pn, ph, ps1, ps2, pr);
        restored.referencePhase.restore(rn, rh, rs1, rs2, rr);
        restored.observe();
        restored.set(12000, 5, 96000);
        restored.observe();
        for (int n = 0; n < 257; ++n) {
            check.sample = static_cast<std::uint64_t>(length + n);
            const auto input = inputAt(3, length + n, 96000, state);
            original.process(input); copied.process(input); assigned.process(input); restored.process(input);
            compareMemory(copied.candidate.getMemory(), original.candidate.getMemory());
            compareMemory(assigned.candidate.getMemory(), original.candidate.getMemory());
            // Restore refreshes the original OTA currents and coefficients.
            // Its continuation is compared with the equally restored frozen
            // object, not assumed equal to an object that was never restored.
        }
        copied.reset();
        copied.set(250, .5f, 96000);
        for (int n = 0; n < 127; ++n) {
            check.sample = n;
            copied.process(inputAt(2, n, 96000, state));
        }
        original.observe(); assigned.observe(); restored.observe();
    }
}

void clockTransfers()
{
    const std::array<float, 5> idleRates {1500, 3000, 48000.f / 31, 48000.f / 32, 96000.f / 63};
    for (const auto& net : nominalNetworks()) for (float audioRate : audioRates)
        for (float idleRate : idleRates) {
            check.context = "clock-transfer/" + net.name + "/audio=" + std::to_string(audioRate)
                          + "/idle=" + std::to_string(idleRate);
            Pair pair(net.network);
            pair.set(1000, 5, audioRate);
            std::uint32_t state = seed;
            std::uint64_t index = 0;
            float previousRate = audioRate;
            for (int segment = 0; segment < 24; ++segment) {
                const bool idle = segment % 2 != 0;
                const float rate = idle ? idleRate : audioRate;
                const auto ac = pair.candidate.halfSteps();
                const auto bc = pair.reference.halfSteps();
                pair.set(segment % 3 == 0 ? 12000.f : 1000.f, segment % 5 == 0 ? .35f : 5.f, rate, idle);
                if (rate != previousRate) {
                    pair.candidate.reReference(ac); pair.reference.reReference(bc);
                }
                previousRate = rate;
                pair.observe();
                for (int n = 0; n < (idle ? 7 : 127); ++n) {
                    check.sample = index++;
                    pair.process(inputAt(3, index, rate, state), !idle);
                }
                // Export/restore both clocks, with pending audio histories.
                if (segment == 5 || segment == 12) {
                    auto cm = pair.candidate.getMemory(); auto rm = pair.reference.getMemory();
                    pair.candidate.restoreMemory(cm); pair.reference.restoreMemory(rm);
                    pair.observe();
                    pair.set(segment % 3 == 0 ? 12000.f : 1000.f, segment % 5 == 0 ? .35f : 5.f, rate, idle);
                    pair.observe();
                }
            }
        }
}

void longContinuation()
{
    for (const auto& net : nominalNetworks()) {
        check.context = "long-continuation/" + net.name;
        Pair pair(net.network);
        pair.set(12000, 5, 96000);
        std::uint32_t state = seed;
        for (std::uint64_t n = 0; n < 262144; ++n) {
            check.sample = n;
            pair.process(inputAt(3, n, 96000, state));
            if (n == 65535 || n == 131071 || n == 196607) {
                Pair copy = pair;
                copy.observe();
                const auto cm = pair.candidate.getMemory();
                const auto rm = pair.reference.getMemory();
                pair.candidate.restoreMemory(cm); pair.reference.restoreMemory(rm);
                pair.set(12000, 5, 96000);
                pair.observe();
            }
        }
    }
}

std::vector<float> edgeInputs()
{
    return {0.f, -0.f, std::bit_cast<float>(std::uint32_t(1)), std::bit_cast<float>(std::uint32_t(0x80000001u)),
            std::bit_cast<float>(std::uint32_t(0x007fffffu)), std::bit_cast<float>(std::uint32_t(0x807fffffu)),
            std::numeric_limits<float>::min(), -std::numeric_limits<float>::min(), 1.f, -1.f, 64.f, -64.f,
            std::bit_cast<float>(std::uint32_t(0x7fc12345u)), std::bit_cast<float>(std::uint32_t(0xffc12345u)),
            std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()};
}

void exceptionalInputsAndControls()
{
    for (const auto& net : nominalNetworks()) for (float rate : audioRates) {
        check.context = "exceptional/" + net.name + "/fs=" + std::to_string(rate);
        const double ff = 1 / (2 * pi * net.network.feedforwardOhms * net.network.integratorFarads);
        const float top = static_cast<float>(std::max(0., .45 * double(rate) - ff));
        std::vector<float> cutoffs {0, -0.f, std::nextafter(0.f, -1.f), std::nextafter(0.f, 1.f),
                                   std::nextafter(top, 0.f), top, std::nextafter(top, std::numeric_limits<float>::infinity()),
                                   -1, std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                                   std::bit_cast<float>(std::uint32_t(0x7fc12345u))};
        std::vector<float> qs {std::nextafter(.35f, 0.f), .35f, std::nextafter(.35f, 1.f),
                              std::nextafter(9.f, 0.f), 9, std::nextafter(9.f, 10.f),
                              std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
                              std::bit_cast<float>(std::uint32_t(0x7fc12345u))};
        const auto inputs = edgeInputs();
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            Pair pair(net.network); pair.set(12000, 5, rate);
            check.sample = i;
            pair.process(inputs[i]);
            for (int n = 0; n < 17; ++n) pair.process(n % 2 ? -.125f : .125f);
        }
        for (float cutoff : cutoffs) for (float q : qs) {
            Pair pair(net.network); pair.set(cutoff, q, rate); pair.observe();
            for (int n = 0; n < 31; ++n) {
                check.sample = n;
                pair.process(n % 2 ? -.75f : .75f);
            }
            // Future continuation belongs to the contract even when the
            // original exceptional operation leaves a nonfinite physical state.
            pair.set(1000, 5, rate);
            for (int n = 0; n < 17; ++n) pair.process(.25f);
        }
    }
}

void rowCoordinateWitnesses()
{
    const std::array<double, 12> values {0., -0., std::bit_cast<double>(std::uint64_t(1)),
        std::bit_cast<double>(std::uint64_t(0x8000000000000001ull)),
        std::bit_cast<double>(std::uint64_t(0x000fffffffffffffull)),
        std::bit_cast<double>(std::uint64_t(0x800fffffffffffffull)),
        std::numeric_limits<double>::min(), -std::numeric_limits<double>::min(), 1., -1., 64., -64.};
    for (const auto& net : nominalNetworks()) for (float rate : audioRates)
        for (auto control : controls) for (int coordinate = 0; coordinate < 8; ++coordinate)
            for (double value : values) {
                check.context = "row-coordinate/" + net.name + "/fs=" + std::to_string(rate)
                              + "/coordinate=" + std::to_string(coordinate);
                Pair pair(net.network); pair.set(control[0], control[1], rate);
                std::array<double, 4> nodes {}, history {};
                if (coordinate < 4) nodes[coordinate] = value;
                else history[coordinate - 4] = value;
                pair.phase.restore(nodes, history, 1, 1, rate);
                pair.referencePhase.restore(nodes, history, 1, 1, rate);
                auto cm = pair.candidate.getMemory(); auto rm = pair.reference.getMemory();
                cm.v = {nodes[0], nodes[1], nodes[2], nodes[3]};
                rm.v = {nodes[0], nodes[1], nodes[2], nodes[3]};
                cm.z = cm.v; rm.z = rm.v; cm.inputs = history; rm.inputs = history;
                pair.candidate.restoreMemory(cm); pair.reference.restoreMemory(rm);
                pair.set(control[0], control[1], rate);
                pair.observe();
                for (int n = 0; n < 17; ++n) {
                    check.sample = n;
                    pair.process(n % 2 ? -.375f : .625f);
                }
            }
}

void floatingPointModes()
{
    FpEnvironment saved;
    const auto modes = denormalModes();
    const std::array<int, 4> rounding {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};
    const auto inputs = edgeInputs();
    for (int round : rounding) {
        if (std::fesetround(round) != 0) {
            std::cout << "Rounding mode unavailable=" << round << '\n';
            continue;
        }
        check.equal(std::fegetround(), round, "requestedRoundingModeSelected");
        for (unsigned mode : modes) {
            selectDenormalMode(mode);
            for (const auto& net : nominalNetworks()) for (float rate : audioRates)
                for (auto control : controls) {
                    check.context = "fp/" + net.name + "/round=" + std::to_string(round)
                                  + "/denormal=" + std::to_string(mode) + "/fs=" + std::to_string(rate);
                    Pair pair(net.network); pair.set(control[0], control[1], rate);
                    std::uint32_t state = seed;
                    for (int n = 0; n < 257; ++n) {
                        check.sample = n;
                        pair.set(control[0], control[1], rate);
                        pair.process(inputAt(3, n, rate, state));
                    }
                    for (float input : inputs) {
                        Pair edge(net.network); edge.set(control[0], control[1], rate);
                        edge.process(input); edge.process(.25f);
                    }
                }
        }
    }
    // Prepare gradual-underflow histories, then change the caller's effective
    // mode without resetting state or changing coefficient keys. Repeat the
    // entire sequence with each mode as the destination of the transition.
    for (int round : rounding) for (unsigned mode : modes) {
        std::fesetround(FE_TONEAREST); selectDenormalMode(0);
        check.context = "fp-transition/round=" + std::to_string(round) + "/denormal=" + std::to_string(mode);
        Pair pair(cs80::kIg00156HighPassNetwork); pair.set(1000, 5, 96000);
        auto cm = pair.candidate.getMemory(); auto rm = pair.reference.getMemory();
        const double tiny = std::bit_cast<double>(std::uint64_t(0x000123456789abcdull));
        cm.v = {tiny, -tiny, tiny, -tiny}; rm.v = {tiny, -tiny, tiny, -tiny};
        cm.z = cm.v; rm.z = rm.v;
        cm.inputs = {tiny, -tiny, tiny, -tiny}; rm.inputs = cm.inputs;
        pair.candidate.restoreMemory(cm); pair.reference.restoreMemory(rm);
        pair.phase.restore(cm.inputs, cm.inputs, 1, 1, 96000);
        pair.referencePhase.restore(rm.inputs, rm.inputs, 1, 1, 96000);
        pair.set(1000, 5, 96000); pair.observe();
        if (std::fesetround(round) != 0) continue;
        selectDenormalMode(mode);
        pair.observe();
        for (int n = 0; n < 127; ++n) {
            check.sample = n;
            pair.set(1000, 5, 96000);
            pair.process(n == 0 ? std::bit_cast<float>(std::uint32_t(1)) : (n % 2 ? -0.f : 0.f));
        }
        std::fesetround(FE_TONEAREST); selectDenormalMode(0);
        for (int n = 0; n < 127; ++n) pair.process(n % 2 ? -.25f : .25f);
    }
}
} // namespace

int main(int argc, char** argv)
{
    std::string_view suite = "all";
    if (argc == 3 && std::string_view(argv[1]) == "--suite") suite = argv[2];
    else if (argc != 1) {
        std::cerr << "usage: cs80_exact_scalar_filter_tests [--suite fixed|moving|state|row|fp|all]\n";
        return 2;
    }
    if (suite != "all" && suite != "fixed" && suite != "moving" && suite != "state" && suite != "row" && suite != "fp") {
        std::cerr << "unknown exact scalar filter suite\n"; return 2;
    }
    FpEnvironment restoreCaller;
    if (std::fesetround(FE_TONEAREST) != 0) {
        std::cerr << "FE_TONEAREST unavailable; registered baseline cannot be qualified\n";
        return 1;
    }
    selectDenormalMode(0);
    if (suite == "fixed" || suite == "all") fixedGrid();
    if (suite == "moving" || suite == "all") movingGrid();
    if (suite == "state" || suite == "all") {
        copyAndRestore(); clockTransfers(); longContinuation(); exceptionalInputsAndControls();
    }
    if (suite == "row" || suite == "all") rowCoordinateWitnesses();
    if (suite == "fp" || suite == "all") floatingPointModes();
    std::cout << "Exact scalar filter suite=" << suite << ": " << check.count << " checks, "
              << check.failures << " failures\n";
    return check.failures == 0 ? 0 : 1;
}
