#include "plugin/OutputMeterDisplay.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
int failures = 0;
void check(bool ok, const char* message)
{
    if (!ok) { ++failures; std::printf("FAIL: %s\n", message); }
}
bool near(float actual, float expected, float tolerance = 0.0001f)
{ return std::abs(actual - expected) <= tolerance; }
}

int main()
{
    using cs80::ui::OutputMeterDisplay;
    OutputMeterDisplay meter;
    meter.reset(10.0);
    check(meter.atFloor() && meter.samplePeak() == 0.0f && meter.peakDbfs() == -120.0f
              && !meter.overRange() && !meter.nonFinite(),
          "silence has an explicit finite display floor and no event flags");
    meter.update({std::numeric_limits<float>::denorm_min(), false, false}, 10.0);
    check(meter.atFloor() && meter.peakDbfs() == -120.0f && !meter.nonFinite(),
          "finite subnormal samples remain below display floor, not nonfinite");
    meter.update({1.0f, false, false}, 10.0);
    check(near(meter.peakDbfs(), 0.0f) && !meter.overRange(),
          "exact full scale is 0 dBFS, not over-range");
    meter.update({}, 10.5);
    check(near(meter.peakDbfs(), -6.0f), "numeric sample peak releases at 12 dB per elapsed second");
    meter.update({}, 11.0);
    check(near(meter.peakDbfs(), -12.0f), "peak decay depends on elapsed time rather than callback count");

    meter.update({2.0f, false, false}, 12.0);
    check(near(meter.peakDbfs(), 6.0206f) && meter.overRange(),
          "peak above one infers OVER even when its separate event flag arrives in another poll");
    meter.update({}, 12.99);
    check(meter.overRange(), "OVER remains visible for its one-second hold");
    meter.update({}, 13.0);
    check(!meter.overRange(), "OVER expires at one elapsed second without another event");
    meter.update({0.0f, true, false}, 13.0);
    check(meter.overRange(), "a separate over-range event is visible even with a zero numeric peak");
    meter.update({0.0f, false, true}, 13.0);
    check(meter.nonFinite() && std::isfinite(meter.peakDbfs()),
          "NON-FINITE event holds independently without poisoning numeric peak");
    meter.update({}, 14.0);
    check(!meter.overRange() && meter.nonFinite(), "OVER and NON-FINITE use independent hold durations");
    meter.update({}, 15.0);
    check(!meter.nonFinite(), "NON-FINITE expires at two elapsed seconds");

    meter.reset(20.0);
    meter.update({std::numeric_limits<float>::max(), false, false}, 20.0);
    check(std::isfinite(meter.peakDbfs()) && meter.peakDbfs() > 770.0f && meter.overRange(),
          "largest finite float formats as a finite positive dBFS peak");
    meter.update({}, 120.0);
    check(meter.atFloor() && !meter.overRange(), "long forward clock jump clears stale display peak and holds");
    for (float value : {std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),
                        std::numeric_limits<float>::quiet_NaN()})
    {
        meter.reset(200.0);
        meter.update({value, false, false}, 200.0);
        check(meter.nonFinite() && meter.atFloor() && std::isfinite(meter.peakDbfs()),
              "defensive nonfinite numeric input becomes an event, never a numeric peak");
    }

    meter.reset(300.0);
    meter.update({2.0f, true, true}, 300.0);
    meter.update({}, 299.0);
    meter.update({}, std::numeric_limits<double>::quiet_NaN());
    meter.update({}, std::numeric_limits<double>::infinity());
    check(meter.samplePeak() == 2.0f && meter.overRange() && meter.nonFinite(),
          "backward and nonfinite timestamps cannot create negative elapsed time or decay growth");
    meter.update({}, 301.0);
    check(near(meter.peakDbfs(), 6.0206f - 12.0f) && !meter.overRange() && meter.nonFinite(),
          "valid clock recovery resumes from last valid monotonic timestamp without extending holds");
    meter.update({}, 302.0);
    check(!meter.nonFinite(), "clock anomaly does not extend the two-second event hold");
    meter.reset(0.0);
    check(meter.atFloor() && !meter.overRange() && !meter.nonFinite(),
          "new-editor presentation reset clears historical peak and event holds");
    meter.update({-1.0f, false, false}, 0.0);
    check(meter.atFloor() && !meter.overRange(), "unexpected negative magnitude cannot create a false overload");
    meter.reset(-std::numeric_limits<double>::max());
    meter.update({1.0f, false, false}, -std::numeric_limits<double>::max());
    meter.update({}, std::numeric_limits<double>::max());
    check(meter.atFloor() && std::isfinite(meter.peakDbfs()),
          "overflow-sized positive elapsed interval safely decays to floor");

    std::printf("Output meter display: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures == 0 ? 0 : 1;
}
