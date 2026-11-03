// Offline diagnostic. This is not a host parameter, physical SPEED law, or
// hardware-parity test. See docs/OE1_SPEED_ENDPOINT_DESIGN.md.
#include "core/BbdChorus.h"
#include "core/Params.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <numbers>
#include <limits>

namespace {
// Yamaha Adjustment Procedure, printed p21, step 1. Tolerances apply to
// periods (not symmetrically to reciprocal frequencies).
constexpr double do1Period = .056;
constexpr double do2Period = .55;
constexpr double periodTolerance = .05;
// Software equation check only; not a hardware waveform tolerance. Fixed
// before evaluation, matching the existing chorus-tabs trajectory tolerance.
constexpr double trajectoryLimit = 2e-6;

struct Result {
    double ownRateError = 0;
    double serviceRateError = 0;
    double peak = 0;
    bool finite = true;
};

Result probe(float speed, double sampleRate) {
    cs80::BbdChorus unmodulated, modulated;
    unmodulated.prepare(static_cast<float>(sampleRate));
    modulated.prepare(static_cast<float>(sampleRate));
    Result result;
    // Same deterministic BBD history/noise in the two instances. Only DEPTH
    // differs. This isolates the existing software VCA trajectory without a
    // private-state accessor or changing the production oscillator.
    const int count = static_cast<int>(2.0 * sampleRate);
    for (int n = 0; n < count; ++n) {
        const float x = static_cast<float>(.3 * std::sin(2 * std::numbers::pi * 997 * n / sampleRate));
        const auto base = unmodulated.process(x, x, speed, 0, true, true);
        const auto actual = modulated.process(x, x, speed, 1, true, true);
        const double t = (n + 1) / sampleRate;
        const auto residual = [&](double f1, double f2) {
            const double s1 = std::sin(2 * std::numbers::pi * f1 * t);
            const double s2 = std::sin(2 * std::numbers::pi * f2 * t);
            // Existing declared software branch assignment, not recovered
            // physical LO1..LO4 routing. Literal coefficients are from p21.
            const double gainL = (.65 * (1 + s1) + .58 * (1 + s2)) / 1.23;
            const double gainR = (.58 * (1 - s2) + .65 * (1 - s1)) / 1.23;
            const double wantL = x + (static_cast<double>(base.l) - x) * gainL;
            const double wantR = x + (static_cast<double>(base.r) - x) * gainR;
            const double errorL = std::abs(actual.l - wantL);
            const double errorR = std::abs(actual.r - wantR);
            const bool finite = std::isfinite(wantL) && std::isfinite(wantR)
                && std::isfinite(errorL) && std::isfinite(errorR);
            result.finite = result.finite && finite;
            return finite ? std::max(errorL, errorR) : std::numeric_limits<double>::infinity();
        };
        result.ownRateError = std::max(result.ownRateError,
            residual(speed, speed * (do1Period / do2Period)));
        result.serviceRateError = std::max(result.serviceRateError,
            residual(1 / do1Period, 1 / do2Period));
        result.peak = std::max({result.peak, std::abs(static_cast<double>(actual.l)),
                               std::abs(static_cast<double>(actual.r))});
        result.finite = result.finite && std::isfinite(base.l) && std::isfinite(base.r)
            && std::isfinite(actual.l) && std::isfinite(actual.r);
    }
    return result;
}
}

int main(int argc, char** argv) {
    const bool fixture = argc == 2 && std::strcmp(argv[1], "--service-fixture") == 0;
    const bool host = argc == 2 && std::strcmp(argv[1], "--host-endpoint") == 0;
    if (!fixture && !host) {
        std::fprintf(stderr, "Use --host-endpoint or --service-fixture. The fixture bypasses host limits.\n");
        return 2;
    }
    const auto& p = cs80::paramDef(cs80::P_chorusSpeed);
    const float speed = fixture ? static_cast<float>(1 / do1Period) : p.max;
    const double period1 = 1 / static_cast<double>(speed);
    const double period2 = 1 / (static_cast<double>(speed) * cs80::BbdChorus::kDo2PerDo1);
    const bool periodPass = std::abs(period1 / do1Period - 1) <= periodTolerance
        && std::abs(period2 / do2Period - 1) <= periodTolerance;
    std::printf("mode=%s host_id=%s host_index=%d host_min=%.9g host_max=%.9g host_default=%.9g host_skew=%.9g\n",
        fixture ? "service-fixture-NOT-host-accessible" : "host-endpoint",
        p.id, static_cast<int>(cs80::P_chorusSpeed), p.min, p.max, p.def, p.skew);
    std::printf("rate_hz=%.12g do1_period_s=%.12g do2_period_s=%.12g period_anchor=%s\n",
        speed, period1, period2, periodPass ? "PASS" : "FAIL");
    bool implementationPass = true;
    bool fixtureTrajectoryPass = true;
    for (double sampleRate : {88200., 96000., 192000.}) {
        const auto r = probe(speed, sampleRate);
        implementationPass = implementationPass && r.finite && r.ownRateError <= trajectoryLimit;
        fixtureTrajectoryPass = fixtureTrajectoryPass && r.serviceRateError <= trajectoryLimit;
        std::printf("sample_rate=%.0f own_rate_error=%.12g service_rate_error=%.12g peak=%.12g finite=%s\n",
            sampleRate, r.ownRateError, r.serviceRateError, r.peak, r.finite ? "yes" : "no");
    }
    // The production endpoint must not become a passing hardware claim just
    // because its limitation is already documented. Host mode exits nonzero
    // while the printed period anchor is unreachable. Fixture mode's pass
    // means only that this existing software oscillator can run at that rate.
    const bool pass = implementationPass && periodPass && (!fixture || fixtureTrajectoryPass);
    std::printf("result=%s scope=%s trajectory_limit=%.9g\n", pass ? "PASS" : "FAIL",
        fixture ? "offline-software-capability-only" : "host-period-anchor", trajectoryLimit);
    return pass ? 0 : 1;
}
