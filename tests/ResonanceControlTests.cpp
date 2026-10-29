// Conditional DC arithmetic only; synthetic fixtures are not measured CS-80 data.
#include "reference/ResonanceControl.h"
#include <iostream>
#include <random>
using namespace cs80::reference::resonance;
namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message) {
    ++checks;
    if (!ok) { if (failures < 20) std::cerr << message << '\n'; ++failures; }
}
void near(double a, double b, const char* message, double tolerance = 2e-9) {
    check(std::abs(a - b) <= tolerance * (1 + std::abs(a) + std::abs(b)), message);
}
Inputs fixture() {
    Inputs p;
    for (int b = 0; b < 5; ++b) p.sources[b] = {1.0 + b * .7, 1000.0 + b * 3000};
    for (int i = 0; i < 32; ++i) p.receivers[i] = {1e-6 * (1 + i % 7), (i % 5 - 2) * 1e-7};
    return p;
}
// Full 37-node oracle: stamp each physical branch separately. Long double and
// direct bus KCL avoid sharing the production source-elimination equations.
std::array<long double, 37> oracle(const Inputs& p) {
    constexpr int n = 37;
    std::array<std::array<long double, n>, n> a {};
    std::array<long double, n> rhs {}, x {};
    auto resistor = [&](int i, int j) {
        const long double g = 1.0L / 220000;
        a[i][i] += g; a[j][j] += g; a[i][j] -= g; a[j][i] -= g;
    };
    for (int rank = 0; rank < 2; ++rank)
        for (int side = 0; side < 2; ++side)
            for (int voice = 0; voice < 8; ++voice) {
                const int node = rank * 16 + side * 8 + voice;
                resistor(node, 32 + rank * 2 + side);
                resistor(node, 36);
                a[node][node] += p.receivers[node].siemens;
                rhs[node] -= p.receivers[node].amps;
            }
    for (int b = 0; b < 5; ++b) {
        const int node = 32 + b;
        if (p.sources[b].ohms == 0) {
            a[node].fill(0); a[node][node] = 1; rhs[node] = p.sources[b].volts;
        } else {
            a[node][node] += 1.0L / p.sources[b].ohms;
            rhs[node] += static_cast<long double>(p.sources[b].volts) / p.sources[b].ohms;
        }
    }
    for (int row = 0; row < n; ++row) {
        long double scale = 0;
        for (auto v : a[row]) scale = std::max(scale, std::abs(v));
        for (auto& v : a[row]) v /= scale;
        rhs[row] /= scale;
    }
    for (int k = 0; k < n; ++k) {
        int pivot = k;
        for (int i = k + 1; i < n; ++i)
            if (std::abs(a[i][k]) > std::abs(a[pivot][k])) pivot = i;
        if (std::abs(a[pivot][k]) < 1e-20L) throw std::runtime_error("singular oracle fixture");
        std::swap(a[k], a[pivot]); std::swap(rhs[k], rhs[pivot]);
        for (int i = k + 1; i < n; ++i) {
            const auto factor = a[i][k] / a[k][k];
            for (int j = k; j < n; ++j) a[i][j] -= factor * a[k][j];
            rhs[i] -= factor * rhs[k];
        }
    }
    for (int i = n - 1; i >= 0; --i) {
        long double v = rhs[i];
        for (int j = i + 1; j < n; ++j) v -= a[i][j] * x[j];
        x[i] = v / a[i][i];
    }
    return x;
}
void compare(const Inputs& p) {
    const auto result = solve(p);
    const auto reference = oracle(p);
    for (int i = 0; i < 32; ++i) {
        near(result.nodeVolts[i], static_cast<double>(reference[i]), "full37 node voltage");
        const double receiver = p.receivers[i].siemens * result.nodeVolts[i] + p.receivers[i].amps;
        near(receiver, result.receiverAmps[i], "receiver current", 1e-13);
        near((result.nodeVolts[i] - result.busVolts[i / 8]) / 220000
             + (result.nodeVolts[i] - result.busVolts[4]) / 220000 + receiver,
             0, "node KCL", 1e-13);
    }
    for (int b = 0; b < 5; ++b) {
        near(result.busVolts[b], static_cast<double>(reference[32 + b]), "full37 bus voltage");
        long double current = 0;
        for (int i = 0; i < 32; ++i) if (b == 4 || i / 8 == b)
            current += (reference[32 + b] - reference[i]) / 220000;
        near(result.sourceAmps[b], static_cast<double>(current), "independent source current", 1e-13);
        if (p.sources[b].ohms > 0)
            near((p.sources[b].volts - result.busVolts[b]) / p.sources[b].ohms,
                 result.sourceAmps[b], "source KCL", 1e-13);
    }
}
template<class F> void rejects(F run, const char* what) {
    bool threw = false;
    try { run(); } catch (const std::invalid_argument&) { threw = true; }
    catch (const std::overflow_error&) { threw = true; }
    check(threw, what);
}
}
int runTests() {
    auto p = fixture();
    for (auto& s : p.sources) s.ohms = 0;
    for (auto& r : p.receivers) r = {0, 0};
    auto result = solve(p);
    for (int i = 0; i < 32; ++i)
        near(result.nodeVolts[i], (p.sources[i / 8].volts + p.sources[4].volts) / 2,
             "ideal open-receiver divider");
    compare(p);
    p.receivers[0] = {1e-5, -2e-6};
    result = solve(p);
    near(result.nodeVolts[0], ((p.sources[0].volts + p.sources[4].volts) / 220000 + 2e-6)
         / (2.0 / 220000 + 1e-5), "loaded analytic divider");
    compare(p);
    std::mt19937 rng(29371);
    std::uniform_real_distribution<double> unit(0, 1);
    for (int trial = 0; trial < 250; ++trial) {
        p = fixture();
        for (int b = 0; b < 5; ++b)
            p.sources[b] = {30 * unit(rng) - 15, (trial + b) % 7 == 0 ? 0 : std::pow(10., 1 + 6 * unit(rng))};
        for (auto& r : p.receivers) r = {2e-5 * unit(rng), 1e-5 * (unit(rng) - .5)};
        compare(p);
    }
    p = fixture(); auto original = solve(p); p.receivers[0].siemens *= 20; auto changed = solve(p);
    check(std::abs(changed.nodeVolts[16] - original.nodeVolts[16]) > 1e-5,
          "shared finite RES couples opposite rank");
    p = fixture(); p.sources[4].ohms = 0; original = solve(p);
    p.receivers[0].siemens *= 20; changed = solve(p);
    for (int i = 8; i < 32; ++i)
        near(changed.nodeVolts[i], original.nodeVolts[i], "ideal RES removes other-group coupling", 1e-13);
    check(std::abs(changed.nodeVolts[1] - original.nodeVolts[1]) > 1e-5,
          "finite rank-local source still couples its eight voices");
    p = fixture(); original = solve(p);
    Inputs permuted = p;
    std::swap(permuted.sources[0], permuted.sources[2]); std::swap(permuted.sources[1], permuted.sources[3]);
    for (int i = 0; i < 16; ++i) std::swap(permuted.receivers[i], permuted.receivers[i + 16]);
    changed = solve(permuted);
    for (int i = 0; i < 32; ++i) near(changed.nodeVolts[i], original.nodeVolts[(i + 16) % 32], "explicit rank permutation");
    p = fixture(); for (auto& r : p.receivers) r = {2e-6, 1e-7};
    for (auto& s : p.sources) s = {3, 2000};
    result = solve(p);
    for (int i = 1; i < 32; ++i) near(result.nodeVolts[i], result.nodeVolts[0], "equal synthetic boundary symmetry");
    // Receiver offsets hold the network near one volt while very high source
    // resistance admits only ~1e-30 A. Check relative error at that scale;
    // the ordinary absolute KCL tolerance would hide a cancellation regression.
    p = fixture();
    for (auto& source : p.sources) source = {0, 1e30};
    for (auto& receiver : p.receivers) receiver = {1e-5, -1e-5};
    result = solve(p);
    for (int b = 0; b < 5; ++b) {
        const double expectedCurrent = -result.busVolts[b] / 1e30;
        check(std::abs(result.sourceAmps[b] / expectedCurrent - 1.0) < 1e-12,
              "high-impedance source current retains tiny scale");
    }
    rejects([] { solve(Inputs{}); }, "unknown defaults fail closed");
    for (int b = 0; b < 5; ++b) for (bool voltage : {false, true}) {
        auto q = fixture(); if (voltage) q.sources[b].volts = unknown; else q.sources[b].ohms = unknown;
        rejects([&] { solve(q); }, "each omitted source member rejected");
    }
    for (int i = 0; i < 32; ++i) for (bool current : {false, true}) {
        auto q = fixture(); if (current) q.receivers[i].amps = unknown; else q.receivers[i].siemens = unknown;
        rejects([&] { solve(q); }, "each omitted receiver member rejected");
    }
    for (double bad : {-1., std::numeric_limits<double>::infinity()}) {
        auto q = fixture(); q.sources[0].ohms = bad; rejects([&] { solve(q); }, "invalid source resistance");
        q = fixture(); q.receivers[0].siemens = bad; rejects([&] { solve(q); }, "invalid receiver conductance");
    }
    p = fixture(); p.sources[0].volts = std::numeric_limits<double>::infinity();
    rejects([&] { solve(p); }, "infinite source voltage");
    p = fixture(); p.receivers[0].amps = std::numeric_limits<double>::infinity();
    rejects([&] { solve(p); }, "infinite receiver current");
    p = fixture(); for (auto& s : p.sources) s.ohms = 1e300;
    for (auto& r : p.receivers) r = {0, 0};
    rejects([&] { solve(p); }, "numerically floating extreme network rejected");
    p = fixture();
    for (auto& source : p.sources) source = {1, 1e17};
    for (auto& receiver : p.receivers) receiver = {0, 0};
    rejects([&] { solve(p); }, "nearly floating common-mode accuracy rejected");
    for (auto& source : p.sources) source = {1, 1e9};
    result = solve(p);
    for (double v : result.nodeVolts)
        check(std::abs(v - 1.0) < 1e-10, "accepted common-mode network retains voltage accuracy");
    p = fixture(); p.receivers[0].amps = std::numeric_limits<double>::max();
    rejects([&] { solve(p); }, "unrepresentable voltage rejected");
    p = fixture(); for (auto& s : p.sources) s.volts = 0;
    for (auto& r : p.receivers) r.amps = 0;
    result = solve(p);
    for (double v : result.nodeVolts) check(v == 0, "zero excitation exact zero");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
int main() {
    try {
        return runTests();
    } catch (const std::exception& error) {
        std::cerr << "Unexpected failure in valid fixture: " << error.what() << '\n';
        return 1;
    }
}
