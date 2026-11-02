#include "core/Tuning.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace tn = cs80::tuning;
namespace
{
int checks = 0, failures = 0;
void check(bool result, const std::string& label)
{
    ++checks;
    if (!result) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}
tn::TuningTable sentinel()
{
    tn::TuningTable result;
    for (int n = 0; n < 128; ++n)
    {
        result.pitch[(std::size_t) n] = (float) n + 0.125f;
        result.mapped[(std::size_t) n] = n % 3 != 0;
    }
    return result;
}
bool equal(const tn::TuningTable& a, const tn::TuningTable& b)
{
    return a.pitch == b.pitch && a.mapped == b.mapped;
}
std::vector<std::uint8_t> message(int sub)
{
    std::vector<std::uint8_t> data {0xF0, 0x7F, 0x7F, 0x08, (std::uint8_t) sub};
    if (sub == 0x07) data.push_back(0); // bank before program
    data.insert(data.end(), {0, 2, 60, 61, 0x40, 0, 69, 70, 0x20, 0, 0xF7});
    return data;
}
}
int main()
{
    for (const int sub : {0x02, 0x07})
    {
        const auto full = message(sub);
        const std::size_t header = sub == 0x02 ? 7 : 8;
        // Every prefix still has genuine F0/F7 framing. In particular, the first
        // complete entry must not leak into the table if the next is incomplete.
        for (std::size_t payloadBytes = 0; payloadBytes < 8; ++payloadBytes)
        {
            std::vector<std::uint8_t> shortMessage(full.begin(), full.begin() + header + payloadBytes);
            shortMessage.push_back(0xF7);
            auto table = sentinel();
            const auto name = std::to_string(sub) + ": truncated payload " + std::to_string(payloadBytes);
            check(!tn::applyMtsSysEx(shortMessage.data(), (int) shortMessage.size(), table), name + " rejected");
            check(equal(table, sentinel()), name + " leaves all 128 pitches/mapped flags unchanged");
        }
        auto table = sentinel();
        check(tn::applyMtsSysEx(full.data(), (int) full.size(), table), "complete two-entry message accepted");
        auto expected = sentinel();
        expected.pitch[60] = 61.5f; expected.mapped[60] = true;
        expected.pitch[69] = 70.25f; expected.mapped[69] = true;
        check(equal(table, expected), "complete two-entry message exact table and untouched other notes");

        auto noChange = full;
        noChange[header + 5] = noChange[header + 6] = noChange[header + 7] = 0x7F;
        table = sentinel();
        check(tn::applyMtsSysEx(noChange.data(), (int) noChange.size(), table), "complete no-change entry accepted");
        expected = sentinel(); expected.pitch[60] = 61.5f; expected.mapped[60] = true;
        check(equal(table, expected), "no-change sentinel preserves second note's pitch and mapping");

        auto largeCount = std::vector<std::uint8_t>(full.begin(), full.begin() + header + 4);
        largeCount[header - 1] = 127;
        largeCount.push_back(0xF7);
        table = sentinel();
        check(!tn::applyMtsSysEx(largeCount.data(), (int) largeCount.size(), table), "incomplete maximum legal count rejected");
        check(equal(table, sentinel()), "incomplete maximum count is atomic");

        std::vector<std::uint8_t> zero(full.begin(), full.begin() + header);
        zero[header - 1] = 0; zero.push_back(0xF7);
        table = sentinel();
        check(!tn::applyMtsSysEx(zero.data(), (int) zero.size(), table) && equal(table, sentinel()),
              "empty counted message leaves table unchanged");
    }
    std::cout << checks << " counted MTS message checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
