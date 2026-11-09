#include "reference/MemorySparseCompiler.h"
#include "experimental/MemorySparseNetwork.h"
#include "reference/MemoryPanelPortCompiler.h"
#include "experimental/LoadedMemoryReceivers.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>

namespace sp = cs80::experimental::memory_sparse;
namespace ref = cs80::reference::memory_panel;
namespace sf = cs80::source_factory;
namespace
{
std::atomic<bool> countAllocations {false};
std::atomic<int> allocations {0};
int checks = 0, failures = 0;
bool printedPerturbationFailure = false;
void check(bool good, const char* label)
{
    ++checks;
    if (!good)
    {
        const bool firstPerturbation = !printedPerturbationFailure && std::strcmp(label,"perturbed full-node state rejected") == 0;
        if (failures < 24 || firstPerturbation) std::printf("FAIL: %s\n",label);
        printedPerturbationFailure = printedPerturbationFailure || firstPerturbation;
        ++failures;
    }
}
double limit(const std::string& json, const char* key)
{
    const auto at = json.find(std::string("\"") + key + "\""), colon = json.find(':', at);
    if (at == std::string::npos || colon == std::string::npos) std::exit(2);
    const double value = std::strtod(json.c_str() + colon + 1, nullptr);
    if (!std::isfinite(value) || value <= 0.0) std::exit(2);
    return value;
}
double nominalControl(int row, double travel)
{
    namespace sf = cs80::source_factory;
    if (row == 1) return 0.05 * std::pow(400.0, travel);
    if (row == 3) return 0.5 + 0.4 * travel;
    if (row == 7 || row == 9)
        return 62.5 * std::exp2(10.0 * travel + (row == 7 ? sf::alignment.hpEffectiveCv - sf::alignment.hpBusVolts
                                                        : sf::alignment.lpEffectiveCv - sf::alignment.lpBusVolts));
    if (row == 8 || row == 10) return 1.0 - travel;
    if (row == 13 || row == 18) return std::pow(10.0, -3.0 * travel);
    if (row == 14 || row == 15 || row == 19 || row == 21) return 10.0 * std::pow(10.0, -3.0 * travel);
    return travel;
}
ref::ReceiverDrives physical(const cs80::experimental::MemoryReceiverInputs& input)
{
    ref::ReceiverDrives out; out.masterResVolts = 10.0 * (1.0 - input.masterResonance);
    for (std::size_t v = 0; v < 8; ++v)
        out.voices[v] = {(5.0 / 3.0) * input.voices[v].filterEnvelopeZ, input.voices[v].velocity, input.voices[v].pressure,
                        input.keyboardCutoffVolts[v], input.keyboardLevelVolts[v]};
    return out;
}
ref::Setup setup(int rank, int selector, double pot, int shape, int boundary)
{
    ref::Setup out; out.rank = rank; out.selector = selector; out.electrical.potOhms = pot;
    if (boundary == 1) out.electrical.boundary = sf::forwardActiveBoundaryV1;
    if (boundary == 2) out.electrical.boundary = sf::lowDropDiodeBoundaryV1;
    if (boundary == 3) out.electrical.boundary = sf::highDropDiodeBoundaryV1;
    for (std::size_t bank = 0; bank < 3; ++bank) for (std::size_t row = 0; row < 26; ++row)
        out.banks[bank][row] = shape < 5 ? shape / 4.0 : ((row * (shape == 5 ? 7 : 11) + bank * 3 + 2) % 9) / 8.0;
    return out;
}
ref::ReceiverDrives drives(int mode)
{
    ref::ReceiverDrives out; out.masterResVolts = mode * (10.0 / 3.0);
    for (int voice = 0; voice < 8; ++voice)
    {
        auto& v = out.voices[static_cast<std::size_t>(voice)];
        v.envelopeVolts = mode == 0 ? 0.0 : mode == 1 ? 5.0 : mode == 2 ? -5.0 : (voice % 2 ? 5.0 : -5.0);
        v.velocity = mode == 0 ? 0.0 : (voice + 1) / 8.0;
        v.pressure = mode / 3.0;
        v.keyboardCutoffVolts = mode * (voice - 3.5) * 0.1;
        v.keyboardLevelVolts = mode * voice * 0.1;
    }
    return out;
}
ref::NodeSnapshot dense(const ref::Setup& source, const ref::ReceiverDrives& input, ref::Result& result)
{
    auto config = source; const auto injection = ref::receiverInjection(input);
    for (std::size_t r = 0; r < 26; ++r) config.injectedAmps[r] += injection.constantAmps[r];
    ref::NodeSnapshot state; result = ref::solve(config, nullptr, &injection.feedback, &state);
    return state;
}
bool unchanged(const sp::State& a, const sp::State& b)
{
    return a.key == b.key && std::memcmp(a.volts.data(), b.volts.data(), sizeof(a.volts)) == 0;
}
}
#if defined(_MSC_VER)
#define CS80_TEST_NOINLINE __declspec(noinline)
#else
#define CS80_TEST_NOINLINE __attribute__((noinline))
#endif
CS80_TEST_NOINLINE void* operator new(std::size_t n)
{
    if (countAllocations.load()) allocations.fetch_add(1);
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
CS80_TEST_NOINLINE void operator delete(void* p) noexcept { std::free(p); }
CS80_TEST_NOINLINE void operator delete(void* p, std::size_t) noexcept { std::free(p); }
CS80_TEST_NOINLINE void* operator new[](std::size_t n) { return ::operator new(n); }
CS80_TEST_NOINLINE void operator delete[](void* p) noexcept { ::operator delete(p); }
CS80_TEST_NOINLINE void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

int main(int argc, char** argv)
{
    if (argc != 2 && !(argc == 3 && std::string(argv[2]) == "--negative-control")) return 2;
    const bool quick = argc == 3;
    std::ifstream file(argv[1]); std::stringstream stream; stream << file.rdbuf(); const auto json = stream.str();
    sp::Options options;
    options.maxIterations = static_cast<int>(limit(json, "maximum_newton_iterations"));
    options.nodeErrorVolts = limit(json, "requested_node_error_volts");
    const double tolerance = limit(json, "dense_comparison_absolute_volts"), perturbation = limit(json, "perturbation_volts");
    check(sp::verified::supportedArithmetic(), "certificate arithmetic assumptions supported");
    int cases = 0, maxCold = 0, maxWarm = 0;
    double worst = 0.0, worstBound = 0.0, worstRatio = 0.0;
    for (int rank : {0,1}) for (int selector : {11,12,13}) for (double pot : {10000.0,50000.0,100000.0})
        for (int shape = 0; shape < 7; ++shape) for (int boundary = 0; boundary < 4; ++boundary)
    {
        if (quick && (rank != 0 || selector != 11 || pot != 50000.0 || shape != 5 || boundary != 0)) continue;
        const auto source = setup(rank, selector, pot, shape, boundary);
        sp::Prepared prepared; check(ref::compileSparse(source, 31, prepared), "compile sparse source");
        sp::State previous; bool havePrevious = false;
        for (int mode = 0; mode < 4; ++mode)
        {
            const auto physical = drives(mode); sp::Inputs input;
            check(ref::sparseInputs(prepared, physical, input), "compile physical receiver drive");
            ref::Result result; const auto truth = dense(source, physical, result);
            check(result.converged && truth.valid, "dense full-node oracle converges");
            sp::State current;
            countAllocations = true;
            const auto cold = sp::update(prepared, input, nullptr, current, options);
            countAllocations = false;
            check(cold.accepted(), "cold sparse result is certified");
            if (!cold.accepted())
            {
                if (failures < 24) std::printf("  config %d/%d/%.0f/%d/%d/%d status %d iter %d bound %.12g\n", rank, selector, pot, shape, boundary, mode,
                    static_cast<int>(cold.status), cold.iterations, cold.certificate.nodeErrorUpperVolts);
                continue;
            }
            ++cases; maxCold = std::max(maxCold, cold.iterations); worstBound = std::max(worstBound, cold.certificate.nodeErrorUpperVolts);
            sp::State oracleState {prepared.key, truth.volts};
            const auto truthCertificate = sp::certify(prepared, input, oracleState, options.nodeErrorVolts);
            check(truthCertificate.accepted(), "independent dense nodes pass prepared-graph certificate");
            for (std::size_t node = 0; node < sp::nodeCount; ++node)
            {
                const double error = std::abs(current.volts[node] - truth.volts[node]);
                check(std::isfinite(error) && error <= tolerance, "every sparse node matches dense oracle");
                const double allowance = cold.certificate.nodeErrorUpperVolts + truthCertificate.nodeErrorUpperVolts;
                check(error <= allowance, "node discrepancy lies within two independent residual bounds");
                worst = std::max(worst, error);
                if (allowance > 0.0) worstRatio = std::max(worstRatio, error / allowance);
            }
            if (havePrevious)
            {
                sp::State warm = previous;
                countAllocations = true;
                const auto advanced = sp::update(prepared, input, &warm, warm, options);
                countAllocations = false;
                check(advanced.accepted(), "changed receiver drive accepts alias-safe warm update");
                if (advanced.accepted())
                {
                    maxWarm = std::max(maxWarm, advanced.iterations);
                    for (std::size_t node = 0; node < sp::nodeCount; ++node)
                        check(std::abs(warm.volts[node] - truth.volts[node]) <= tolerance, "warm full nodes match dense");
                }
            }
            previous = current; havePrevious = true;
            // Deliberate arbitrary-node challenges. Every rail, wiper, dummy
            // emitter and bus must participate in the independently callable gate.
            if (pot == 50000.0 && shape == 5 && boundary == 0 && mode == 3)
            {
                for (std::size_t node = 0; node < sp::nodeCount; ++node) for (double sign : {-1.0,1.0})
                {
                    auto changed = current; changed.volts[node] += sign * perturbation;
                    const auto certificate = sp::certify(prepared, input, changed, options.nodeErrorVolts);
                    check(!certificate.accepted(), "perturbed full-node state rejected");
                    if (certificate.status == sp::Status::residualTooLarge)
                        check(certificate.nodeErrorUpperVolts + cold.certificate.nodeErrorUpperVolts >= perturbation * 0.999999,
                              "perturbed-node numerical bound covers known displacement");
                    else check(certificate.status == sp::Status::diodeEnclosureFailed, "perturbed state only fails a documented enclosure gate");
                }
            }
        }
    }
    // Concrete counterexample made entirely from valid stored float controls.
    // It proves a tiny affine receiver-closure residual is NOT a nonlinear
    // source-error certificate. This is an expected rejection of the proposed
    // approximation budget, not a claim that inaccurate audio is acceptable.
    namespace ex = cs80::experimental;
    namespace sf = cs80::source_factory;
    ref::Setup reachable; reachable.rank = 0; reachable.selector = 11; reachable.electrical.potOhms = 10000.0;
    for (std::size_t bank = 0; bank < 3; ++bank)
    {
        ex::memory_control::ChannelValues values;
        for (std::size_t p = 0; p < values.size(); ++p) values[p] = cs80::kChannelDefs[p].def;
        const std::size_t pattern = bank == 0 ? 2 : bank == 2 ? 0 : 1;
        for (int row = 1; row <= 26; ++row)
        {
            const auto parameter = static_cast<std::size_t>(cs80::reference::programmer_identity::channelParamForOutput(row));
            const double travel = std::clamp(static_cast<double>(((row - 1) * 7 + pattern * 3) % 9) / 8.0, 0.001, 0.999);
            const auto& def = cs80::kChannelDefs[parameter];
            values[parameter] = static_cast<float>(std::clamp(nominalControl(row, travel), static_cast<double>(def.min), static_cast<double>(def.max)));
        }
        ex::memory_control::AdaptedBank adapted;
        check(ex::memory_control::adapt(values, adapted) && adapted.saturatedRows == 0,
              "counterexample bank is realizable through valid float host controls without clipping");
        reachable.banks[bank] = adapted.positions;
    }
    ex::MemoryReceiverInputs center; center.masterResonance = 0.5;
    for (std::size_t v = 0; v < 8; ++v)
    {
        center.voices[v] = {1.5, 0.5 * (v + 1) / 8.0, 0.5 * (8 - v) / 8.0};
        center.keyboardCutoffVolts[v] = 0.1 * (static_cast<double>(v) - 3.5);
        center.keyboardLevelVolts[v] = 0.05 * v;
    }
    const auto operating = ref::solveReceivers(reachable, physical(center));
    auto biased = reachable; biased.injectedAmps = operating.injectionAmps;
    ex::MemoryControlFrame frame;
    check(operating.converged && ref::compileLoadedPortMap(biased, frame.ports), "counterexample map prepares at its actual receiver operating point");
    frame.ready = true; frame.frameVersion = ex::memoryControlFrameVersion; frame.adapterVersion = ex::memory_control::version;
    frame.sourceModelVersion = sf::modelVersion; frame.sourceHash = sf::sourceFingerprint; frame.electrical = reachable.electrical;
    auto moved = center; moved.masterResonance *= 0.95;
    for (std::size_t v = 0; v < 8; ++v)
    {
        moved.voices[v].filterEnvelopeZ *= 0.95; moved.voices[v].velocity *= 0.95; moved.voices[v].pressure *= 0.95;
        moved.keyboardCutoffVolts[v] *= 0.95; moved.keyboardLevelVolts[v] *= 0.95;
    }
    const auto nonlinear = ref::solveReceivers(reachable, physical(moved));
    ex::MemoryReceiverReadout linear;
    check(nonlinear.converged && ex::evaluateMemoryReceivers(frame, moved, linear), "both counterexample models evaluate successfully");
    const double counterError = std::abs(nonlinear.network.busVolts[7] - linear.busVolts[7]);
    check(linear.residualAmps <= 1e-12 && counterError > 1e-3,
          "affine residual success cannot certify the nonlinear 1mV source budget");
    check(linear.maxDeltaAmps > 3e-6 && linear.maxDeltaAmps < 5e-6, "counterexample remains a small but unqualified current excursion");
    sp::Prepared counterGraph; sp::Inputs centerInput, movedInput;
    check(ref::compileSparse(reachable,91,counterGraph)
        && ref::sparseInputs(counterGraph,physical(center),centerInput)
        && ref::sparseInputs(counterGraph,physical(moved),movedInput),"prior affine counterexample compiles for sparse path");
    sp::State counterState;
    check(sp::update(counterGraph,centerInput,nullptr,counterState,options).accepted(),"counterexample center is certified");
    const auto counterUpdate = sp::update(counterGraph,movedInput,&counterState,counterState,options);
    check(counterUpdate.accepted(),"counterexample displaced source is independently certified");
    double correctedError = 0.0;
    for (std::size_t row = 0; row < sp::busCount; ++row)
        correctedError = std::max(correctedError,std::abs(counterState.volts[static_cast<std::size_t>(sp::busBase)+row]
            - nonlinear.network.busVolts[static_cast<std::size_t>(sp::outputs[row])]));
    check(correctedError <= tolerance,"sparse path resolves prior fixed-map counterexample");
    std::printf("Prior affine counterexample %.12g V; sparse bus error %.12g V; certified full-node bound %.12g V\n",
        counterError,correctedError,counterUpdate.certificate.nodeErrorUpperVolts);

    // Fail closed and preserve all previously published nodes/configuration.
    const auto source = setup(0,11,100000.0,5,0); sp::Prepared p;
    check(ref::compileSparse(source,77,p), "failure fixture compiles"); sp::Inputs input;
    check(ref::sparseInputs(p,drives(3),input), "failure fixture inputs compile");
    sp::State good; check(sp::update(p,input,nullptr,good,options).accepted(), "failure fixture solves");
    const auto reject = [&](const sp::Prepared& graph, const sp::Inputs& boundary, const sp::State* warm, sp::Options option, const char* label)
    {
        auto output = good; const auto report = sp::update(graph,boundary,warm,output,option);
        check(!report.accepted() && unchanged(output,good),label);
    };
    for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
    {
        auto badInput = input; badInput.constantAmps[6] = bad; reject(p,badInput,&good,options,"nonfinite current leaves publication untouched");
        badInput = input; badInput.feedbackG[0] = bad; reject(p,badInput,&good,options,"nonfinite feedback leaves publication untouched");
        auto badState = good; badState.volts[3] = bad; reject(p,input,&badState,options,"nonfinite warm node rejected");
        check(!sp::certify(p,input,badState,options.nodeErrorVolts).accepted(),"nonfinite full-node certificate rejected");
    }
    auto badInput = input; badInput.constantAmps[3] = 1e-6; reject(p,badInput,&good,options,"digital injection rejected");
    badInput = input; badInput.feedbackG[0] = -1e-6; reject(p,badInput,&good,options,"negative feedback rejected");
    badInput = input; badInput.feedbackG[0] = 1.0; reject(p,badInput,&good,options,"nonpositive dominance rejected");
    auto changedSource = source; changedSource.banks[0][0] = 0.234; sp::Prepared changed;
    check(ref::compileSparse(changedSource,77,changed),"changed same-generation source compiles");
    reject(changed,input,&good,options,"complete source identity catches same-generation stale wiper");
    check(ref::compileSparse(source,78,changed),"new generation compiles");
    reject(changed,input,&good,options,"generation mismatch rejected");
    auto option = options; option.maxIterations = 1; reject(p,input,nullptr,option,"exhausted iteration cap never publishes last iterate");
    option.maxIterations = 0; reject(p,input,nullptr,option,"zero step budget cannot publish a cold guess");
    option = options; option.maxIterations = 65; reject(p,input,&good,option,"over-hard-limit options rejected");
    option = options; option.nodeErrorVolts = 0.0; reject(p,input,&good,option,"invalid error budget rejected");
    option.nodeErrorVolts = 1e-300; reject(p,input,&good,option,"unachievable certificate precision fails closed");
    auto invalidGraph = p; invalidGraph.ready = false; reject(invalidGraph,input,&good,options,"unprepared graph rejected");
    auto overflow = good; overflow.volts[3] = std::numeric_limits<double>::max(); reject(p,input,&overflow,options,"overflowing candidate rejected");
    check(!sp::certify(p,input,overflow,options.nodeErrorVolts).accepted(),"overflowing residual cannot pass finite reduction");
    auto same = good; option = options; option.maxIterations = 0;
    const auto noWork = sp::update(p,input,&same,same,option);
    check(noWork.accepted() && unchanged(same,good),"already certified warm state requires no Newton step");
    const auto old = p; changedSource = source; changedSource.electrical.boundary.diode.seriesOhms = 0.0;
    check(!ref::compileSparse(changedSource,77,p) && p.key == old.key,"unsupported zero diode series rejected without replacing prepared graph");
    check(cases == (quick ? 4 : 2016),"all declared grid cases certified");
    check(allocations.load() == 0,"cold and warm update/certificate perform no heap allocation");
    std::printf("Sparse update: %d cases; %d checks, %d failures; max cold/warm steps %d/%d; max node error %.12g V; max bound %.12g V; max error/two-bound %.9g; allocations %d\n",
        cases,checks,failures,maxCold,maxWarm,worst,worstBound,worstRatio,allocations.load());
    return failures == 0 ? 0 : 1;
}
