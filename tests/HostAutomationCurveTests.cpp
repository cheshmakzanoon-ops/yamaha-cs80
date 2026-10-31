#include "plugin/HostAutomationBridge.h"
#include "Ws1Tolerances.h"

#include <cmath>
#include <iostream>
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
        check(cs80::kSampleAccurateParameters.size() == limits["key_parameter_count"], "registered key roster");
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
        std::cout << "maximum normalized curve error=" << maxError << '\n';
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
    return failures == 0 ? 0 : 1;
}
