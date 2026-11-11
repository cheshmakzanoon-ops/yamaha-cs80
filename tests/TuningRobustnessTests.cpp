// Input-validation regressions for the existing Scala/KBM and MTS parsers.
// These are software robustness checks, not full MIDI conformance tests.
#include "core/Tuning.h"

#include <bit>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace tn = cs80::tuning;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        if (failures <= 40) std::printf("FAIL: %s\n", label.c_str());
    }
}

bool same(const tn::KeyboardMapping& a, const tn::KeyboardMapping& b)
{
    return a.size == b.size && a.first == b.first && a.last == b.last
        && a.middle == b.middle && a.reference == b.reference
        && a.referenceHz == b.referenceHz && a.octaveDegree == b.octaveDegree && a.map == b.map;
}
bool same(const tn::TuningTable& a, const tn::TuningTable& b)
{
    for (std::size_t n = 0; n < a.pitch.size(); ++n)
        if (std::bit_cast<std::uint32_t>(a.pitch[n]) != std::bit_cast<std::uint32_t>(b.pitch[n])
            || a.mapped[n] != b.mapped[n]) return false;
    return true;
}
tn::TuningTable sentinel()
{
    tn::TuningTable t;
    for (std::size_t n = 0; n < t.pitch.size(); ++n)
    {
        t.pitch[n] = static_cast<float>(n) - 64.375f;
        t.mapped[n] = n % 3 != 0;
    }
    return t;
}
using Header = std::array<std::string, 7>;
const Header mappingHeader { "2", "0", "127", "60", "69", "440.0", "12" };
std::string mappingText(const Header& header, const std::string& entries = "0\nx\n")
{
    std::string text;
    for (const auto& line : header) text += line + '\n';
    return text + entries;
}
void rejectMapping(const Header& header, const std::string& entries, const std::string& label)
{
    tn::KeyboardMapping before;
    before.size = 3; before.first = 4; before.last = 110; before.middle = 55;
    before.reference = 70; before.referenceHz = 432.0; before.octaveDegree = 19;
    before.map = { 1, -1, std::nullopt };
    auto actual = before;
    check(!tn::parseKeyboardMapping(mappingText(header, entries), actual).empty(), label + " rejected");
    check(same(actual, before), label + " leaves destination unchanged");
}
void testMapping()
{
    // Each external header integer used to be narrowed before being validated.
    for (std::size_t field = 0; field < 5; ++field)
        for (long long delta : { 4294967296LL, -4294967296LL })
        {
            auto header = mappingHeader;
            header[field] = std::to_string(std::stoll(header[field]) + delta);
            rejectMapping(header, "0\nx\n", "KBM wrapped header " + std::to_string(field));
        }
    for (std::size_t field = 0; field < 5; ++field)
        for (const auto* value : { "-1", "2147483648", "999999999999999999999999" })
        {
            auto header = mappingHeader;
            header[field] = value;
            rejectMapping(header, "0\nx\n", "KBM out-of-range header " + std::to_string(field));
        }
    for (std::size_t field = 0; field < 5; ++field)
    {
        auto header = mappingHeader;
        header[field] = field == 0 ? "1025" : "128";
        rejectMapping(header, "0\nx\n", "KBM upper bound " + std::to_string(field));
    }
    auto reversed = mappingHeader; reversed[1] = "61"; reversed[2] = "60";
    rejectMapping(reversed, "0\nx\n", "KBM reversed range");
    for (const auto& value : { std::to_string(static_cast<long long>((std::numeric_limits<int>::max)()) + 1),
                             std::string("4294967296"), std::string("4294967297"), std::string("-2147483649") })
        rejectMapping(mappingHeader, "0\n" + value + '\n', "KBM invalid later degree " + value);

    tn::KeyboardMapping m;
    check(tn::parseKeyboardMapping(mappingText(mappingHeader), m).empty()
              && m.size == 2 && m.map == std::vector<std::optional<int>>({ 0, std::nullopt }), "KBM ordinary mapping accepted");
    check(tn::parseKeyboardMapping(mappingText(mappingHeader, "0\n"), m).empty()
              && m.map == std::vector<std::optional<int>>({ 0, std::nullopt }), "KBM omitted entries remain unmapped");
    const auto intMax = (std::numeric_limits<int>::max)();
    check(tn::parseKeyboardMapping(mappingText(mappingHeader, std::to_string(intMax) + "\nX\n"), m).empty()
              && m.map == std::vector<std::optional<int>>({ intMax, std::nullopt }), "KBM largest stored degree remains representable");
    for(int degree : {-1,-2,(std::numeric_limits<int>::min)()})
        check(tn::parseKeyboardMapping(mappingText(mappingHeader,std::to_string(degree)+"\nx\n"),m).empty()
              && m.map[0].has_value() && *m.map[0]==degree && !m.map[1],
              "KBM negative signed degree remains distinct from unmapped x");
    const Header low { "0", "0", "0", "0", "0", "440", "0" };
    check(tn::parseKeyboardMapping(mappingText(low, ""), m).empty()
              && m.size == 0 && m.first == 0 && m.last == 0 && m.map.empty(), "KBM lower header bounds accepted");
    const Header high { "1024", "127", "127", "127", "127", "440", "1024" };
    check(tn::parseKeyboardMapping(mappingText(high, ""), m).empty()
              && m.size == 1024 && m.map.size() == 1024 && !m.map.back()
              && m.first == 127 && m.middle == 127 && m.reference == 127 && m.octaveDegree == 1024,
          "KBM upper header bounds accepted");
}

void testNumericTokens()
{
    // SCL permits text after a valid pitch value. Do not tighten this existing
    // decimal-prefix policy while fixing integer fields with no digits.
    for (const std::string token : { "700.0oops", "700.0/2", "700.0.5", "700.0e2", "700.0!comment" })
    {
        tn::Scale scale;
        check(tn::parseScale("test\n2\n" + token + "\n1200.0\n", scale).empty()
              && scale.cents == std::vector<double>({ 700.0, 1200.0 }),
              "SCL decimal-prefix behavior retained: " + token);
    }
    for (const std::string token : { "+ comment", "- comment" })
        for (std::size_t field : { 0u, 1u, 3u, 4u, 6u })
        {
            auto header = mappingHeader; header[field] = token;
            rejectMapping(header, "0\nx\n", "KBM sign without digits " + std::to_string(field));
        }
    for (const std::string token : { "700.0", "+700.0", "700.", ".5", "-3.5", "700.0 annotation", "700.0\tannotation" })
    {
        tn::Scale scale;
        check(tn::parseScale("test\n2\n" + token + "\n1200.0 period\n", scale).empty(),
              "SCL valid decimal and separated annotation retained: " + token);
    }
    auto header = mappingHeader; header[3] = "+60 middle"; header[5] = "440.0 reference";
    tn::KeyboardMapping mapping;
    check(tn::parseKeyboardMapping(mappingText(header), mapping).empty()
          && mapping.middle == 60 && mapping.referenceHz == 440.0,
          "KBM valid signed integer and separated annotations retained");
}

using Bytes = std::vector<std::uint8_t>;
Bytes counted(int sub, int count, int realtime = 0x7F)
{
    Bytes p { 0xF0, static_cast<std::uint8_t>(realtime), 0x7F, 0x08, static_cast<std::uint8_t>(sub), 0x00 };
    if (sub == 0x07) p.push_back(0x00);
    p.push_back(static_cast<std::uint8_t>(count));
    for (int i = 0; i < count; ++i)
        p.insert(p.end(), { static_cast<std::uint8_t>((7 * i) % 128), static_cast<std::uint8_t>((3 * i) % 128),
                            static_cast<std::uint8_t>((11 * i) % 128), static_cast<std::uint8_t>((29 * i) % 128) });
    p.push_back(0xF7);
    return p;
}
Bytes bulk()
{
    Bytes p { 0xF0, 0x7E, 0x7F, 0x08, 0x01, 0x7F };
    p.insert(p.end(), 16, ' ');
    for (int n = 0; n < 128; ++n)
        p.insert(p.end(), { static_cast<std::uint8_t>(n), 0x40, 0x00 });
    p.insert(p.end(), { 0x00, 0xF7 });
    return p;
}
Bytes octave(int sub, int realtime = 0x7F)
{
    Bytes p { 0xF0, static_cast<std::uint8_t>(realtime), 0x7F, 0x08, static_cast<std::uint8_t>(sub), 0x03, 0x7F, 0x7F };
    for (int pc = 0; pc < 12; ++pc)
    {
        p.push_back(static_cast<std::uint8_t>(pc == 0 ? 0 : pc == 1 ? 0x7F : 0x40));
        if (sub == 0x09) p.push_back(static_cast<std::uint8_t>(pc == 1 ? 0x7F : 0x00));
    }
    p.push_back(0xF7);
    return p;
}
void rejectPacket(const Bytes& p, const std::string& label, int device = 0x7F)
{
    const auto before = sentinel();
    auto actual = before;
    check(!tn::applyMtsSysEx(p.data(), static_cast<int>(p.size()), actual, device), label + " rejected");
    check(same(actual, before), label + " leaves every pitch and mapped flag unchanged");
}
void testValidMts()
{
    // Exercise every representable count, both bank forms and the existing
    // real-time/non-real-time acceptance. Bank/program routing is unchanged.
    for (int sub : { 0x02, 0x07 })
        for (int realtime : { 0x7E, 0x7F })
            for (int count = 0; count <= 127; ++count)
            {
                auto p = counted(sub, count, realtime);
                auto expected = sentinel(), actual = expected;
                for (int i = 0; i < count; ++i)
                {
                    const auto key = static_cast<std::size_t>((7 * i) % 128);
                    expected.pitch[key] = static_cast<float>((3 * i) % 128)
                        + static_cast<float>(((11 * i) % 128) * 128 + (29 * i) % 128) / 16384.0f;
                    expected.mapped[key] = true;
                }
                check(tn::applyMtsSysEx(p.data(), static_cast<int>(p.size()), actual) == (count != 0)
                          && same(actual, expected), "MTS counted valid payload " + std::to_string(sub) + "/" + std::to_string(count));
            }
    for (int sub : { 0x02, 0x07 })
    {
        auto p = counted(sub, 2);
        const auto at = static_cast<std::size_t>(sub == 0x02 ? 7 : 8);
        p[at + 1] = p[at + 2] = p[at + 3] = 0x7F;
        p[at + 4] = 127; p[at + 5] = 127; p[at + 6] = 127; p[at + 7] = 126;
        auto expected = sentinel(), actual = expected;
        expected.pitch[127] = 127.0f + 16382.0f / 16384.0f; expected.mapped[127] = true;
        check(tn::applyMtsSysEx(p.data(), static_cast<int>(p.size()), actual) && same(actual, expected),
              "MTS no-change marker and highest frequency preserved");
        p[at + 5] = p[at + 6] = p[at + 7] = 0x7F;
        rejectPacket(p, "MTS all-no-change is a no-op");
        p = counted(sub, 1); p[2] = 23;
        actual = sentinel();
        check(tn::applyMtsSysEx(p.data(), static_cast<int>(p.size()), actual, 23), "MTS matching device accepted");
        rejectPacket(p, "MTS other device ignored", 24);
    }
    for (int checksum = 0; checksum < 128; ++checksum)
    {
        auto p = bulk(); p[p.size() - 2] = static_cast<std::uint8_t>(checksum);
        p[22] = p[23] = p[24] = 0x7F;
        auto expected = sentinel(), actual = expected;
        for (std::size_t n = 1; n < 128; ++n) { expected.pitch[n] = static_cast<float>(n) + 0.5f; expected.mapped[n] = true; }
        check(tn::applyMtsSysEx(p.data(), static_cast<int>(p.size()), actual) && same(actual, expected),
              "MTS bulk01 retains checksum tolerance and no-change marker");
    }
    for (int sub : { 0x08, 0x09 })
        for (int realtime : { 0x7E, 0x7F })
            for (int channelHigh = 0; channelHigh <= 3; ++channelHigh)
            {
                auto p = octave(sub, realtime); p[5] = static_cast<std::uint8_t>(channelHigh);
                auto expected = sentinel(), actual = expected;
                for (std::size_t n = 0; n < 128; ++n)
                {
                    const float lo = sub == 0x08 ? -0.64f : -1.0f;
                    const float hi = sub == 0x08 ? 0.63f : 8191.0f / 8192.0f;
                    expected.pitch[n] = static_cast<float>(n) + (n % 12 == 0 ? lo : n % 12 == 1 ? hi : 0.0f);
                    expected.mapped[n] = true;
                }
                check(tn::applyMtsSysEx(p.data(), static_cast<int>(p.size()), actual) && same(actual, expected),
                      "MTS scale/octave valid offsets and channel bits accepted");
            }
}

void testInvalidMts()
{
    const std::array<Bytes, 5> valid { bulk(), counted(0x02, 2), counted(0x07, 2), octave(0x08), octave(0x09) };
    for (const auto& good : valid)
    {
        const auto label = "MTS sub " + std::to_string(good[4]);
        // Preserve the framing at every shortened length. In particular a
        // counted message with one complete entry must not apply that prefix.
        for (std::size_t size = 2; size < good.size(); ++size)
        {
            Bytes p(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(size));
            p.back() = 0xF7;
            rejectPacket(p, label + " truncated at " + std::to_string(size));
        }
        auto p = good; p.insert(p.end() - 1, 0x00);
        rejectPacket(p, label + " trailing payload");
        for (std::size_t at = 1; at + 1 < good.size(); ++at)
        {
            p = good; p[at] |= 0x80;
            rejectPacket(p, label + " high bit at " + std::to_string(at));
        }
        p = good; p.front() = 0;
        rejectPacket(p, label + " missing F0");
        p = good; p.back() = 0;
        rejectPacket(p, label + " missing F7");
        p = good; p[1] = 0x7D;
        rejectPacket(p, label + " wrong universal ID");
        p = good; p[3] = 0x09;
        rejectPacket(p, label + " wrong sub-ID");
        p = good; p[4] = 0x04;
        rejectPacket(p, label + " unsupported message");
    }
    for (int sub : { 0x02, 0x07 })
    {
        auto p = counted(sub, 2);
        const auto countAt = static_cast<std::size_t>(sub == 0x02 ? 6 : 7);
        p[countAt] = 1; rejectPacket(p, "MTS undeclared counted entry");
        p[countAt] = 3; rejectPacket(p, "MTS missing counted entry");
        p = counted(sub, 128); rejectPacket(p, "MTS non-7-bit count with complete payload");
    }
    for (int sub : { 0x08, 0x09 })
        for (int bit = 2; bit <= 6; ++bit)
        {
            auto p = octave(sub); p[5] |= static_cast<std::uint8_t>(1 << bit);
            rejectPacket(p, "MTS reserved channel option");
        }
    auto p = bulk(); p[1] = 0x7F;
    rejectPacket(p, "MTS bulk01 real-time envelope");
    auto t = sentinel(); const auto before = t;
    check(!tn::applyMtsSysEx(nullptr, 408, t) && same(t, before), "MTS null packet leaves table unchanged");
    for (int size : { -1, 0, 1, 2, 3, 4, 5 })
        check(!tn::applyMtsSysEx(p.data(), size, t) && same(t, before), "MTS short/negative size rejected");
}
} // namespace

int main()
{
    testMapping();
    testNumericTokens();
    testValidMts();
    testInvalidMts();
    std::printf("Tuning robustness: %d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
