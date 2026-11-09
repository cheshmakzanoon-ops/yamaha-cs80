#include "plugin/VoiceActivity.h"

#include <atomic>
#include <cstdio>
#include <thread>

int main()
{
    int checks = 0, failures = 0;
    const auto check = [&](bool ok, const char* label) { ++checks; if (!ok) { ++failures; if (failures < 8) std::printf("FAIL: %s\n", label); } };
    cs80::VoiceActivity meter;
    check(meter.read() == cs80::VoiceActivityReading{}, "initial state is silent eight-voice capacity");
    for (unsigned mask = 0; mask <= 0xffffu; ++mask)
    {
        meter.publish(mask, mask >> 8, true);
        const auto value = meter.read();
        int count = 0;
        for (int i = 0; i < 16; ++i) count += (mask >> i) & 1u;
        check(value.mask == mask && value.capacity == 16 && value.count() == count,
              "every sixteen-slot mask and its count survive one publication");
        bool slots = true;
        for (int i = 0; i < 16; ++i) slots &= value.active(i) == (((mask >> i) & 1u) != 0);
        check(slots && !value.active(-1) && !value.active(16) && !value.active(100), "slot identity and bounds");
    }
    for (unsigned mask = 0; mask < 256; ++mask)
    {
        meter.publish(mask | 0xffffff00u, 0xffffffffu, false);
        const auto value = meter.read();
        check(value.capacity == 8 && value.mask == mask && !value.active(8),
              "inactive extension and out-of-range bits cannot leak into eight-voice display");
    }
    meter.publish(0, 0, true);
    check(meter.read().capacity == 16 && meter.read().count() == 0, "silent extension mode retains its denominator");
    meter.reset();
    check(meter.read() == cs80::VoiceActivityReading{}, "lifecycle reset clears capacity and stale activity");
    std::atomic<bool> done { false };
    std::thread writer([&] {
        for (int i = 0; i < 200000; ++i) { meter.publish(0xff, 0xff, true); meter.publish(0x55, 0xff, false); }
        done.store(true, std::memory_order_release);
    });
    bool coherent = true;
    do
    {
        const auto value = meter.read();
        coherent &= value == cs80::VoiceActivityReading{}
            || value == cs80::VoiceActivityReading{0xffffu, 16}
            || value == cs80::VoiceActivityReading{0x55u, 8};
    } while (!done.load(std::memory_order_acquire));
    writer.join();
    check(coherent, "capacity and mask cannot tear across concurrent publications");
    std::printf("Voice activity: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
