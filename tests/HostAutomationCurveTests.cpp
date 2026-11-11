#include "plugin/HostAutomationBridge.h"
#include "Ws1Tolerances.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

namespace
{
using Point = std::pair<int, double>;
using Points = std::vector<Point>;
bool read(void* context, int index, int& position, double& value) noexcept
{
    const auto& points = *static_cast<const Points*>(context);
    if (index < 0 || static_cast<size_t>(index) >= points.size()) return false;
    position = points[static_cast<size_t>(index)].first;
    value = points[static_cast<size_t>(index)].second;
    return true;
}

// Independent scalar oracle: each sample looks up its bracketing absolute
// knots. It shares neither the production cursor nor block-relative state.
double truth(const Points& points, double initial, int sample)
{
    const Point left = [&] {
        for (auto i = points.rbegin(); i != points.rend(); ++i)
            if (i->first <= sample) return *i;
        return Point{-1, initial};
    }();
    for (const auto& right : points)
        if (right.first > sample)
        {
            const double weightRight = double(sample - left.first) / double(right.first - left.first);
            return left.second * (1.0 - weightRight) + right.second * weightRight;
        }
    return left.second;
}
}

int main(int argc, char** argv)
{
    int failures = 0;
    double maxError = 0.0;
    const auto check = [&](bool pass, const char* message) {
        if (!pass) { ++failures; if (failures <= 16) std::cerr << "FAIL: " << message << '\n'; }
    };
    try
    {
        const ws1::Tolerances limits(argc > 1 ? argv[1] : "tests/parity/tolerances-v2/host-automation.json",
                                     "host_automation");
        // Keep the frozen eighteen-lane contract intact; the separately
        // versioned pan capability appends lanes rather than rewriting it.
        constexpr std::array original {
            int(cs80::P_volume), int(cs80::P_mix), int(cs80::P_brilliance), int(cs80::P_resonance),
            int(cs80::P_masterTune), int(cs80::P_detune),
            cs80::chParam(0, cs80::CP_hpfCutoff), cs80::chParam(0, cs80::CP_hpfRes),
            cs80::chParam(0, cs80::CP_lpfCutoff), cs80::chParam(0, cs80::CP_lpfRes),
            cs80::chParam(0, cs80::CP_vLevel), cs80::chParam(0, cs80::CP_pw),
            cs80::chParam(1, cs80::CP_hpfCutoff), cs80::chParam(1, cs80::CP_hpfRes),
            cs80::chParam(1, cs80::CP_lpfCutoff), cs80::chParam(1, cs80::CP_lpfRes),
            cs80::chParam(1, cs80::CP_vLevel), cs80::chParam(1, cs80::CP_pw) };
        const ws1::Tolerances panLimits(argc > 2 ? argv[2]
            : "tests/parity/tolerances-v2/host-automation-pan-v1.json", "host_automation_pan");
        check(original.size() == limits["key_parameter_count"], "frozen registered key roster size");
        check(cs80::kSampleAccurateParameters.size() == panLimits["key_parameter_count"], "expanded pan roster size");
        check(cs80::kSampleAccurateParameters.size() >= original.size()
            && std::equal(original.begin(), original.end(), cs80::kSampleAccurateParameters.begin()),
            "original eighteen lanes retain their order");
        check(cs80::kSampleAccurateParameters.size() == original.size() + 2
            && cs80::kSampleAccurateParameters[original.size()] == cs80::chParam(0, cs80::CP_pan)
            && cs80::kSampleAccurateParameters[original.size() + 1] == cs80::chParam(1, cs80::CP_pan),
            "only the two rank pan lanes are appended");
        const std::vector<Points> cases {
            {{2047, 0.2}},
            {{2047, 0.9}},
            {{0, 0.4}, {13, 0.8}, {14, 0.2}, {127, 0.2}, {128, 0.9}, {1023, 0.3}, {2047, 0.7}},
            {{0, 0.8}, {2047, 0.8}},
            {{63, 0.2}, {64, 0.9}, {255, 0.9}, {256, 0.1}, {1023, 0.1}, {1024, 0.75}, {2047, 0.4}}
        };
        for (const auto& absolute : cases)
            for (int size : {1, 7, 64, 256, 1024})
                for (int start = 0; start < 2048; start += size)
                {
                    const int count = std::min(size, 2048 - start);
                    Points relative;
                    for (const auto& point : absolute)
                        if (point.first >= start && point.first < start + count)
                            relative.emplace_back(point.first - start, point.second);
                    if (relative.empty() || relative.back().first != count - 1)
                        relative.emplace_back(count - 1, truth(absolute, 0.2, start + count - 1));
                    cs80::HostAutomationCurve curve;
                    curve.begin({&relative, static_cast<int>(relative.size()), read, nullptr, nullptr},
                                truth(absolute, 0.2, start - 1));
                    for (int sample = 0; sample < count; ++sample)
                    {
                        const auto error = std::abs(curve.valueAt(sample) - truth(absolute, 0.2, start + sample));
                        maxError = std::max(maxError, error);
                        check(error <= limits["normalized_curve_error"], "every host sample matches the absolute curve");
                        const int span = curve.samplesUntilChange(sample, count - sample);
                        for (int held = 1; held < span; ++held)
                            check(std::abs(truth(absolute, 0.2, start + sample + held) - truth(absolute, 0.2, start + sample))
                                      <= limits["normalized_curve_error"], "constant-span batching never skips a change");
                    }
                    check(std::abs(curve.finalValue() - truth(absolute, 0.2, start + count - 1))
                              <= limits["normalized_curve_error"], "last sample reaches final parameter state");
                }
        Points dense;
        for (int i = 0; i < 10000; ++i) dense.emplace_back(i, (i & 1) != 0 ? 0.75 : 0.25);
        cs80::HostAutomationCurve curve;
        curve.begin({&dense, static_cast<int>(dense.size()), read, nullptr, nullptr}, 0.25);
        for (int i = 0; i < 10000; ++i)
            check(curve.valueAt(i) == dense[static_cast<size_t>(i)].second, "borrowed queue has no fixed point-count truncation");

        // A malformed host readback can precede otherwise valid points. The
        // prior sample (-1) must be finite so interpolation cannot inject NaN
        // into the processor before the first scheduled event. The first knot
        // and the final host publication retain their original sample/value.
        Points initialGuardPoints {{7, 0.75}, {11, 0.25}};
        for (double malformed : {
                 std::numeric_limits<double>::quiet_NaN(),
                 std::numeric_limits<double>::infinity(),
                 -std::numeric_limits<double>::infinity()})
        {
            cs80::HostAutomationCurve guarded;
            guarded.begin({&initialGuardPoints,
                           static_cast<int>(initialGuardPoints.size()), read, nullptr, nullptr}, malformed);
            bool finite = true;
            double beforeKnot = 0.0, atKnot = 0.0, lastKnot = 0.0;
            for (int sample = 0; sample <= 11; ++sample)
            {
                const double value = guarded.valueAt(sample);
                finite = finite && std::isfinite(value) && value >= 0.0 && value <= 1.0;
                if (sample == 6) beforeKnot = value;
                if (sample == 7) atKnot = value;
                if (sample == 11) lastKnot = value;
            }
            check(finite, "nonfinite host initial is rejected before sample interpolation");
            check(beforeKnot == 0.75 * 7.0 / 8.0 && atKnot == 0.75 && lastKnot == 0.25,
                  "malformed host initial uses a finite zero fallback without moving event knots");
            check(guarded.finalValue() == 0.25,
                  "valid host endpoint survives malformed prior readback");
        }

        // A hostile host can report INT_MAX as a future knot. Subtraction
        // from the implicit previous offset -1 must widen *before* arithmetic:
        // INT_MAX - (-1) is not representable as signed int. The independent
        // denominator below is evaluated in double and the source must never
        // reverse the slope of this rising ramp or invoke signed-overflow UB.
        Points extremeFuture {{std::numeric_limits<int>::max(), 0.75}};
        cs80::HostAutomationCurve farFuture;
        farFuture.begin({&extremeFuture, 1, read, nullptr, nullptr}, 0.25);
        const double actualStart = farFuture.valueAt(0);
        const double expectedStart = 0.25 + 0.5
            / (static_cast<double>(std::numeric_limits<int>::max()) + 1.0);
        check(std::isfinite(actualStart) && actualStart > 0.25 && actualStart < 0.75,
              "extreme positive host offset retains forward interpolation");
        check(std::abs(actualStart - expectedStart) <= limits["extreme_offset_denominator_error"],
              "extreme host offset matches widened independent denominator");

        // Rendering skips regressing/invalid knots. Endpoint publication must
        // follow that same accepted path, including early and suspended exits.
        struct EndpointCase { Points points; double expected; int failedRead = -1; };
        const double nan = std::numeric_limits<double>::quiet_NaN();
        const std::vector<EndpointCase> endpointCases {
            {{{0, 0.25}, {8, 0.75}, {3, 0.1}, {6, 0.2}}, 0.75},
            {{{8, 0.75}, {3, 0.1}, {8, 0.4}, {6, 0.2}}, 0.4},
            {{{std::numeric_limits<int>::max(), 0.75}, {0, 0.1}}, 0.75},
            {{{7, 2.0}, {6, nan}, {5, 0.1}, {9, -2.0}, {8, 0.75}}, 0.0},
            {{{-1, 0.9}, {-10, 0.2}}, 0.125},
            {{{3, nan}, {4, std::numeric_limits<double>::infinity()}}, 0.125},
            {{{0, 0.25}, {8, 0.75}, {9, 0.9}, {3, 0.1}}, 0.75, 2},
            {{{0, 0.25}, {4, 0.5}, {8, 0.75}}, 0.75},
            {{}, 0.125}
        };
        struct EndpointQueue { const EndpointCase* fixture; };
        const auto endpointRead = +[](void* context, int index, int& offset, double& value) noexcept {
            const auto& fixture = *static_cast<EndpointQueue*>(context)->fixture;
            if (index == fixture.failedRead || index < 0
                || index >= static_cast<int>(fixture.points.size())) return false;
            offset = fixture.points[static_cast<size_t>(index)].first;
            value = fixture.points[static_cast<size_t>(index)].second;
            return true;
        };
        const auto publish = +[](void* context, float value) noexcept { *static_cast<float*>(context) = value; };
        int endpointChecks = 0, endpointFailures = 0;
        const auto endpointCheck = [&](bool pass, const char* message) {
            ++endpointChecks;
            if (!pass) ++endpointFailures;
            check(pass, message);
        };
        for (const auto& fixture : endpointCases)
            for (int through : {-1, 0, 3, 8, 12})
            {
                EndpointQueue data {&fixture};
                float published = -1.0f;
                cs80::HostAutomationCurve tested, untouched;
                const cs80::HostAutomationQueue queue {
                    &data, static_cast<int>(fixture.points.size()), endpointRead, &published, publish};
                tested.begin(queue, 0.125);
                untouched.begin(queue, 0.125);
                for (int sample = 0; sample <= through; ++sample)
                    endpointCheck(tested.valueAt(sample) == untouched.valueAt(sample),
                                  "endpoint fixture renders identically before publication");
                endpointCheck(tested.finalValue() == fixture.expected,
                              "endpoint ignores knots rejected by the render cursor");
                tested.commit();
                endpointCheck(published == static_cast<float>(fixture.expected),
                              "commit publishes the accepted endpoint at early and ordinary exits");
                endpointCheck(tested.finalValue() == fixture.expected,
                              "repeated endpoint queries are stable");
                for (int sample = through + 1; sample <= 16; ++sample)
                    endpointCheck(tested.valueAt(sample) == untouched.valueAt(sample),
                                  "endpoint query and commit do not advance the live render cursor");
                endpointCheck(tested.valueAt(std::numeric_limits<int>::max()) == fixture.expected,
                              "published endpoint agrees with complete accepted-queue traversal");
            }
        // Match the processor's publication order: keep full precision first,
        // then notify the host using that value without a second queue walk.
        struct PublicationProbe
        {
            Points points {{0, 0.25}, {8, 0.75}, {3, 0.1}, {6, 0.2}};
            int reads = 0, notifications = 0;
            double stored = -1.0, observedStored = -1.0;
            float published = -1.0f;
        } publication;
        const auto countedRead = +[](void* context, int index, int& position, double& value) noexcept {
            auto& probe = *static_cast<PublicationProbe*>(context);
            ++probe.reads;
            return read(&probe.points, index, position, value);
        };
        const auto notify = +[](void* context, float value) noexcept {
            auto& probe = *static_cast<PublicationProbe*>(context);
            ++probe.notifications;
            probe.observedStored = probe.stored;
            probe.published = value;
        };
        cs80::HostAutomationCurve precomputed;
        precomputed.begin({&publication, 4, countedRead, &publication, notify}, 0.125);
        publication.stored = precomputed.finalValue();
        const int readsBeforeCommit = publication.reads;
        precomputed.commitFinalValue(publication.stored);
        endpointCheck(publication.reads == readsBeforeCommit,
                      "publishing a computed endpoint never rereads queue points");
        endpointCheck(publication.notifications == 1 && publication.published == 0.75f
                          && publication.observedStored == 0.75,
                      "endpoint storage precedes its one host callback");
        endpointCheck(precomputed.valueAt(0) == 0.25,
                      "computed endpoint publication leaves the live cursor unchanged");
        precomputed.commit();
        endpointCheck(publication.notifications == 2 && publication.published == 0.75f,
                      "ordinary commit still computes and publishes the accepted endpoint");
        PublicationProbe noDestination;
        cs80::HostAutomationCurve noNotification;
        noNotification.begin({&noDestination, 4, countedRead, nullptr, nullptr}, 0.125);
        const int readsWithoutNotification = noDestination.reads;
        noNotification.commit();
        noNotification.commitFinalValue(0.75);
        endpointCheck(noDestination.reads == readsWithoutNotification,
                      "commit without a publication callback remains a no-op");
        std::cout << "endpoint publication checks=" << endpointChecks
                  << ", failures=" << endpointFailures << '\n';

        std::cout << "maximum normalized curve error=" << maxError << '\n';
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
