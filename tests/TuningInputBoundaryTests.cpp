#include "core/Tuning.h"

#include <array>
#include <iostream>
#include <limits>
#include <string>

namespace tn = cs80::tuning;
namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
bool equal(const tn::KeyboardMapping& a, const tn::KeyboardMapping& b)
{
    return a.size == b.size && a.first == b.first && a.last == b.last
        && a.middle == b.middle && a.reference == b.reference
        && a.referenceHz == b.referenceHz && a.octaveDegree == b.octaveDegree
        && a.map == b.map;
}
std::string text(const std::array<long long, 5>& header, const std::string& entries = {})
{
    std::string result;
    for (const auto value : header) result += std::to_string(value) + "\n";
    return result + "440.0\n12\n" + entries;
}
tn::KeyboardMapping sentinel()
{
    tn::KeyboardMapping result;
    result.size = 3; result.first = 13; result.last = 113;
    result.middle = 59; result.reference = 71; result.referenceHz = 432.0;
    result.octaveDegree = 7; result.map = {0, 2, 4};
    return result;
}
void reject(const std::string& input, const std::string& name)
{
    auto output = sentinel();
    check(!tn::parseKeyboardMapping(input, output).empty(), name + " must be rejected, not narrowed");
    check(equal(output, sentinel()), name + " must leave caller state unchanged");
}
}

int main()
{
    static_assert(sizeof(int) == 4, "this fixture targets the project's supported 32-bit int ABIs");
    const std::array<long long, 5> normal {0, 0, 127, 60, 69};
    tn::KeyboardMapping output;
    check(tn::parseKeyboardMapping(text(normal), output).empty(), "ordinary no-map header accepted");
    check(output.size == 0 && output.first == 0 && output.last == 127
        && output.middle == 60 && output.reference == 69, "ordinary header values preserved exactly");

    // The independent expected result uses the external value in its original
    // wide representation, never the implementation's narrowed int.
    for (std::size_t field = 0; field < normal.size(); ++field)
        for (const long long wrap : {-4294967296LL, 4294967296LL})
        {
            auto header = normal;
            header[field] += wrap;
            reject(text(header), "header " + std::to_string(field) + " value " + std::to_string(header[field]));
        }
    for (const long long degree : {2147483648LL, 4294967295LL, 4294967296LL, 8589934592LL, 999999999999LL})
    {
        auto header = normal; header[0] = 2;
        reject(text(header, "3\n" + std::to_string(degree) + "\n"), "mapping degree " + std::to_string(degree));
    }
    for (const int degree : {0, 12, 1024, (std::numeric_limits<int>::max)()})
    {
        auto header = normal; header[0] = 1;
        check(tn::parseKeyboardMapping(text(header, std::to_string(degree) + "\n"), output).empty(),
              "representable mapping degree accepted");
        check(output.map.size() == 1 && output.map[0] == degree, "representable degree preserved exactly");
    }
    // Existing bounded-header policy is unchanged, including boundary values.
    for (std::size_t field = 0; field < normal.size(); ++field)
    {
        auto header = normal; header[field] = -1;
        reject(text(header), "negative supported-range header");
        header = normal; header[field] = field == 0 ? 1025 : 128;
        reject(text(header), "above supported-range header");
    }
    auto mapped = normal; mapped[0] = 2;
    check(tn::parseKeyboardMapping(text(mapped, "0\nx\n"), output).empty()
        && output.map == std::vector<int>({0, -1}), "existing explicit unmapped entry retained");
    std::cout << checks << " tuning input boundary checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
