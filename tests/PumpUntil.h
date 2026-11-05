#pragma once

#include <chrono>

namespace cs80::tests
{
// Test-only bounded event pumping. Inject time as well as the pump so the
// scheduling contract can be exercised without a GUI or sleeping.
template <typename Ready, typename Pump, typename Now>
bool pumpUntil(Ready ready, Pump pump, Now now, int timeoutMs = 2000)
{
    const auto deadline = now() + std::chrono::milliseconds(timeoutMs);
    while (! ready())
    {
        if (now() >= deadline)
            return false;
        pump(10);
    }
    return true;
}
} // namespace cs80::tests
