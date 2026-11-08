#include "PumpUntil.h"
#include <iostream>
#include <chrono>

#define CHECK(condition) do { if (! (condition)) { std::cerr << "Failed line " << __LINE__ << '\n'; return 1; } } while (false)

int main()
{
    using Clock = std::chrono::steady_clock;
    auto time = Clock::time_point{};
    int pumps = 0;
    const auto now = [&] { return time; };
    const auto pump = [&](int ms) { ++pumps; time += std::chrono::milliseconds(ms); };
    CHECK(cs80::tests::pumpUntil([] { return true; }, pump, now));
    CHECK(pumps == 0);
    // Presentation takes longer than the old fixed 30 ms allowance.
    CHECK(cs80::tests::pumpUntil([&] { return pumps == 8; }, pump, now));
    CHECK(pumps == 8);
    pumps = 0;
    CHECK(! cs80::tests::pumpUntil([] { return false; }, pump, now, 50));
    CHECK(pumps == 5);
    pumps = 0;
    CHECK(cs80::tests::pumpUntil([&] { return pumps == 5; }, pump, now, 50));
    CHECK(pumps == 5); // Completion at the deadline is accepted.
    pumps = 0;
    CHECK(! cs80::tests::pumpUntil([] { return false; }, pump, now, 0));
    CHECK(pumps == 0);
}
