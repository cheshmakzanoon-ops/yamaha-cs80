// Pure preview arithmetic: no JUCE, processor, GUI or message loop.
#include "plugin/PresetPreviewMath.h"

#include <cmath>
#include <cstdio>

int main()
{
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const char* message)
    {
        ++checks;
        if (!ok)
        {
            if (failures < 10) std::fprintf(stderr, "FAIL: %s\n", message);
            ++failures;
        }
    };
    using cs80::ui::presetPreviewReleaseShape;
    const float pastEnd = std::nextafter(1.0f, 2.0f);
    check(presetPreviewReleaseShape(pastEnd) == 0.0f,
          "one-ULP endpoint overshoot terminates at zero instead of NaN");
    check(presetPreviewReleaseShape(2.0f) == 0.0f,
          "later positions stay at the ended envelope");
    check(presetPreviewReleaseShape(0.0f) == 1.0f,
          "release begins at full scale");
    check(presetPreviewReleaseShape(1.0f) == 0.0f,
          "exact release endpoint is zero");

    // Every valid progress value retains the former expression unchanged.
    for (int i = 0; i <= 65536; ++i)
    {
        const float t = float(i) / 65536.0f;
        const float original = std::pow(1.0f - t, 1.7f);
        check(presetPreviewReleaseShape(t) == original,
              "valid release shape is numerically unchanged");
    }
    const float beforeEnd = std::nextafter(1.0f, 0.0f);
    check(presetPreviewReleaseShape(beforeEnd) > 0.0f
          && std::isfinite(presetPreviewReleaseShape(beforeEnd)),
          "last in-range float remains positive and finite");

    // Finite preset found by the production signature-math sweep. On GCC 14
    // the final shared-axis x is below total, but the release division rounds
    // above one. Other compilers may round that boundary differently.
    const auto span = [](float t)
    {
        return 0.14f + 0.86f * std::log10(1.0f + t * 40.0f)
                            / std::log10(1.0f + 20.0f * 40.0f);
    };
    const float attack = span(0x1.11815cp+4f);
    const float decay = span(0x1.2dddbcp+4f);
    const float release = span(0x1.abbf1p+2f);
    constexpr float hold = 0.55f;
    const float total = attack + decay + hold + release;
    const float x = total * 55.0f / 55.0f;
    const float t = (x - attack - decay - hold) / release;
    check(std::isfinite(presetPreviewReleaseShape(t)),
          "reproduced finite-preset endpoint stays finite");
    std::printf("Preset preview math: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
