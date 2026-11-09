#include "reference/MemoryPanelPortCompiler.h"
#include "reference/MemoryPanelPorts.h"

#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <new>
#include <sstream>
#include <string>

namespace ref = cs80::reference::memory_panel;
using Map = cs80::experimental::MemoryPanelPortMap;
namespace
{
std::atomic<bool> countAllocations { false };
std::atomic<unsigned> allocations { 0 };
int checks = 0, failures = 0;
void check(bool good, const char* label)
{
    ++checks;
    if (!good) { if (failures < 20) std::printf("FAIL: %s\n", label); ++failures; }
}
double limit(const std::string& json, const char* key)
{
    const auto at = json.find(std::string("\"") + key + "\"");
    const auto colon = json.find(':', at);
    if (at == std::string::npos || colon == std::string::npos)
    { std::fprintf(stderr, "missing limit %s\n", key); std::exit(2); }
    char* end = nullptr;
    const char* begin = json.c_str() + colon + 1;
    const double value = std::strtod(begin, &end);
    if (end == begin || !std::isfinite(value) || value <= 0.0)
    { std::fprintf(stderr, "invalid limit %s\n", key); std::exit(2); }
    return value;
}
void* allocate(std::size_t n)
{
    if (countAllocations.load()) ++allocations;
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
bool same(const Map& a, const Map& b)
{
    return a.ready == b.ready && a.rank == b.rank && a.selector == b.selector
        && a.operatingPoint == b.operatingPoint && a.referenceCurrents == b.referenceCurrents
        && a.localOhms == b.localOhms && a.railToBus == b.railToBus && a.currentToRail == b.currentToRail;
}
}
void* operator new(std::size_t n) { return allocate(n); }
void* operator new[](std::size_t n) { return allocate(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

int main(int argc, char** argv)
{
    if (argc != 2) { std::fprintf(stderr, "usage: %s tolerances.json\n", argv[0]); return 2; }
    std::ifstream input(argv[1]); std::stringstream text; text << input.rdbuf();
    const auto json = text.str();
    const double matrixAbs = limit(json, "matrix_absolute_ohms"), matrixRel = limit(json, "matrix_relative");
    const double predictionAbs = limit(json, "prediction_absolute_volts"), predictionRel = limit(json, "prediction_relative");
    double worstMatrix = 0.0, worstPrediction = 0.0, largestCoupling = 0.0, largestAsymmetry = 0.0;
    int points = 0, derivatives = 0;
    Map last;
    ref::Setup lastSetup;
    for (int rank : { 0, 1 })
        for (int selector : { 11, 12, 13 })
            for (double pot : { 10000.0, 50000.0, 100000.0 })
                for (int scenario = 0; scenario < 5; ++scenario)
    {
        ref::Setup setup;
        setup.rank = rank; setup.selector = selector; setup.electrical.potOhms = pot;
        for (std::size_t bank = 0; bank < setup.banks.size(); ++bank)
            for (std::size_t row = 0; row < Map::outputCount; ++row)
                setup.banks[bank][row] = scenario < 3 ? scenario * 0.5
                    : static_cast<double>((row * 7 + bank * 3) % 9) / 8.0;
        if (scenario == 4) setup.injectedAmps[7] = 100e-6;
        const auto oracle = ref::linearise(setup);
        Map map;
        const bool compiled = ref::compileLoadedPortMap(setup, map);
        check(oracle.valid && compiled && map.ready, "dense reference and fixed-size compiler accept operating point");
        if (!oracle.valid || !compiled) continue;
        ++points;
        check(map.rank == rank && map.selector == selector && map.referenceCurrents == setup.injectedAmps,
              "prepared map retains source and current operating-point identity");
        Map::Voltages zero;
        check(map.evaluate({}, zero), "zero current delta is accepted");
        for (std::size_t row = 0; row < Map::outputCount; ++row)
            check(std::bit_cast<std::uint64_t>(zero[row]) == std::bit_cast<std::uint64_t>(oracle.operatingPoint.busVolts[row]),
                  "zero excitation preserves exact nonlinear operating-point bus bits");
        for (std::size_t i = 0; i < Map::analogCount; ++i)
            for (std::size_t j = 0; j < Map::analogCount; ++j)
            {
                const double expected = oracle.loadedOhms[i * Map::analogCount + j];
                const double actual = map.derivative(oracle.rows[i], oracle.rows[j]);
                const double error = std::abs(actual - expected);
                check(std::isfinite(error) && error <= matrixAbs + matrixRel * std::abs(expected),
                      "factorized derivative matches every full-reference Jacobian port");
                worstMatrix = std::max(worstMatrix, error); ++derivatives;
                if (i != j) largestCoupling = std::max(largestCoupling, std::abs(expected));
                largestAsymmetry = std::max(largestAsymmetry,
                    std::abs(expected - oracle.loadedOhms[j * Map::analogCount + i]));
            }
        // Algebraic comparison only: large current vectors do NOT qualify
        // linearization accuracy against the nonlinear circuit away from here.
        for (double magnitude : { 1e-9, 1e-7, 1e-6, 100e-6, 200e-6 })
        {
            Map::Currents delta {};
            for (std::size_t j = 0; j < Map::analogCount; ++j)
                delta[static_cast<std::size_t>(oracle.rows[j] - 1)] = magnitude * (static_cast<int>(j % 5) - 2) * 0.5;
            Map::Voltages actual;
            check(map.evaluate(delta, actual), "finite current-vector evaluation succeeds");
            for (std::size_t i = 0; i < Map::analogCount; ++i)
            {
                const auto row = static_cast<std::size_t>(oracle.rows[i] - 1);
                double expected = oracle.operatingPoint.busVolts[row];
                for (std::size_t j = 0; j < Map::analogCount; ++j)
                    expected += oracle.loadedOhms[i * Map::analogCount + j] * delta[static_cast<std::size_t>(oracle.rows[j] - 1)];
                const double error = std::abs(actual[row] - expected);
                check(std::isfinite(error) && error <= predictionAbs + predictionRel * std::abs(expected),
                      "bounded evaluator equals full dense linear-map multiplication");
                worstPrediction = std::max(worstPrediction, error);
            }
            check(actual[3] == 0.0 && actual[4] == 0.0, "digital rows never become analog buses");
        }
        last = map; lastSetup = setup;
    }
    check(points == 90 && derivatives == 90 * 576, "complete operating-point and derivative grid ran");
    check(largestCoupling > 0.1, "grid would detect deleting shared-rail coupling");
    check(largestAsymmetry > 0.1, "grid would detect imposing reciprocal follower ports");

    // Refuse unsupported/malformed input without replacing a previously valid map.
    for (int field = 0; field < 11; ++field)
    {
        auto invalid = lastSetup; auto kept = last;
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        if (field == 0) invalid.rank = -1;
        if (field == 1) invalid.selector = 10;
        if (field == 2) invalid.selector = 14;
        if (field == 3) invalid.electrical.potOhms = std::numeric_limits<double>::infinity();
        if (field == 4) invalid.electrical.endOhms = 0.0;
        if (field == 5) invalid.electrical.followerOhms = -1.0;
        if (field == 6) invalid.electrical.boundary.diode.emission = nan;
        if (field == 7) invalid.banks[2][25] = nan;
        if (field == 8) invalid.banks[0][0] = 1.01;
        if (field == 9) invalid.injectedAmps[0] = nan;
        if (field == 10) invalid.injectedAmps[3] = 1e-7;
        check(!ref::compileLoadedPortMap(invalid, kept) && same(kept, last),
              "failed preparation preserves previous complete map");
    }
    Map::Voltages output; output.fill(7.0);
    const auto unchanged = output;
    check(!Map{}.evaluate({}, output) && output == unchanged, "unprepared map cannot publish voltages");
    for (double invalid : { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() })
    {
        Map::Currents delta {}; delta[0] = invalid;
        check(!last.evaluate(delta, output) && output == unchanged, "nonfinite input preserves output");
    }
    for (std::size_t i : { 3U, 4U })
    {
        Map::Currents delta {}; delta[i] = 1e-7;
        check(!last.evaluate(delta, output) && output == unchanged, "digital-row current injection rejected");
    }
    Map::Currents huge {}; huge[7] = std::numeric_limits<double>::max();
    check(!last.evaluate(huge, output) && output == unchanged, "finite arithmetic overflow fails without partial publication");

    Map::Currents delta {}; volatile double checksum = 0.0;
    allocations = 0; countAllocations = true;
    const auto start = std::chrono::steady_clock::now();
    bool evaluated = true;
    for (int i = 0; i < 20000; ++i)
    {
        delta[7] = (i & 1) ? 1e-7 : -1e-7;
        evaluated &= last.evaluate(delta, output);
        checksum = checksum + output[0];
    }
    const auto stop = std::chrono::steady_clock::now();
    countAllocations = false;
    check(evaluated && std::isfinite(checksum) && allocations == 0,
          "twenty thousand prepared-map evaluations allocate no ordinary C++ memory");
    const double ns = std::chrono::duration<double, std::nano>(stop - start).count() / 20000.0;
    std::printf("Memory port map: %d points, %d derivatives, %d checks, %d failures\n"
                "worst matrix %.9g ohms; linear prediction %.9g V; coupling %.9g ohms; asymmetry %.9g ohms\n"
                "prepared bytes=%zu; local evaluation %.3f ns (shared machine, not whole-instrument qualification)\n",
                points, derivatives, checks, failures, worstMatrix, worstPrediction, largestCoupling,
                largestAsymmetry, sizeof(Map), ns);
    return failures != 0;
}
