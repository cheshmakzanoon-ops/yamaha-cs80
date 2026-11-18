#pragma once
#include <cmath>
#include <cstdint>
namespace cs80::benchmark {
// Diagnostic only: a failed clock read invalidates the entire aggregate.
struct CallbackCpuMeasurement {
    bool requested = false, failed = false;
    double seconds = 0.0;
    std::uint64_t calls = 0;
    void observe(double before, double after) noexcept {
        if (!requested) return;
        if (!std::isfinite(before) || !std::isfinite(after) || before < 0.0 || after < before) {
            failed = true;
            return;
        }
        seconds += after - before;
        ++calls;
    }
    const char* status() const noexcept {
        return !requested ? "disabled" : failed || calls == 0 ? "unavailable" : "available";
    }
    bool available() const noexcept { return requested && !failed && calls != 0; }
};
}
