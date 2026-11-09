#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cs80::experimental::memory_sparse
{
// Offline research only. The layout deliberately mirrors the independently
// implemented dense reference; factory selectors 0..10 are not supported.
inline constexpr int railCount = 3, busCount = 24, wipersPerBank = 22;
inline constexpr int emitterNode = 69, busBase = 70, nodeCount = 94;
inline constexpr std::uint64_t modelVersion = 1;
inline constexpr std::array<int, busCount> outputs {{0,1,2,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25}};
inline constexpr std::array<int, wipersPerBank> wiperBuses {{0,1,2,3,5,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23}};
inline constexpr std::array<int, 6> feedbackTargets {{4,4,6,6,19,19}};
inline constexpr std::array<int, 6> feedbackSources {{20,22,20,22,21,23}};
constexpr int wiperNode(int bank, int wiper) noexcept { return 3 + bank * wipersPerBank + wiper; }
using Nodes = std::array<double, nodeCount>;
using ConfigurationKey = std::array<std::uint64_t, 120>;
// Immutable after compileSparse succeeds. Do not edit coefficients or reuse a
// State key with a manually altered graph; this is not a serialized host format.
struct Prepared
{
    ConfigurationKey key {};
    std::array<std::array<double, busCount>, railCount> topG {}, bottomG {}, followerRatio {}, dividerG {};
    std::array<std::array<double, busCount>, 11> factoryR {};
    std::array<std::array<bool, busCount>, 11> factoryPresent {};
    std::array<double, busCount> load {};
    std::array<double, 26> backgroundAmps {};
    double driverG = 0.0, selectedVolts = 0.0;
    double diodeIs = 0.0, diodeNvt = 0.0, diodeR = 0.0, followerR = 0.0;
    int activeBank = -1;
    bool ready = false;
};
struct Inputs
{
    std::array<double, 26> constantAmps {};
    // Only the six named physical receiver edges are admitted. Their target
    // and source sets are disjoint, so L^2=0 in the bus-only inverse.
    std::array<double, 6> feedbackG {};
};
struct State { ConfigurationKey key {}; Nodes volts {}; };
enum class Status
{
    success, invalidConfiguration, invalidInput, staleState, invalidState,
    invalidOptions, arithmeticUnsupported, diodeEnclosureFailed,
    linearSolveFailed, iterationLimit, residualTooLarge
};
struct Certificate
{
    Status status = Status::invalidState;
    double residualUpperAmps = 0.0, dominanceLowerSiemens = 0.0, nodeErrorUpperVolts = 0.0;
    std::array<double, nodeCount> residualNodeUpperAmps {};
    bool accepted() const noexcept { return status == Status::success; }
};
struct Options { int maxIterations = 64; double nodeErrorVolts = 1e-8; };
struct Report
{
    Status status = Status::invalidState;
    int iterations = 0;
    Certificate certificate;
    bool accepted() const noexcept { return status == Status::success; }
};
} // namespace cs80::experimental::memory_sparse
