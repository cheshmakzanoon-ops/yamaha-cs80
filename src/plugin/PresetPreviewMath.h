#pragma once

#include <algorithm>
#include <cmath>

namespace cs80::ui
{
// Preview-only release shape. The separately rounded shared-axis position can
// put progress just beyond one even when x < total; a negative base raised to
// a fractional power would then put NaN into the drawable envelope.
inline float presetPreviewReleaseShape(float progress) noexcept
{
    return std::pow(std::max(0.0f, 1.0f - progress), 1.7f);
}
} // namespace cs80::ui
