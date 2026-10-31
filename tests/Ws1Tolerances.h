#pragma once
// Reads the pre-registered ws1-voice tolerances (tests/parity/tolerances-v2/
// ws1-voice.json) so a gate always uses the committed numbers. Minimal flat
// lookup: the first "key": number after the named stage object.
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ws1
{
class Tolerances
{
public:
    Tolerances(const std::string& path, const std::string& stage)
    {
        std::ifstream in(path);
        if (!in) throw std::runtime_error("cannot open tolerance file " + path);
        std::stringstream ss; ss << in.rdbuf(); text = ss.str();
        start = text.find("\"" + stage + "\"");
        if (start == std::string::npos) throw std::runtime_error("missing stage " + stage);
    }
    double operator[](const std::string& key) const
    {
        const auto at = text.find("\"" + key + "\"", start);
        if (at == std::string::npos) throw std::runtime_error("missing tolerance " + key);
        const auto colon = text.find(':', at);
        const char* begin = text.c_str() + colon + 1;
        char* end = nullptr;
        const double v = std::strtod(begin, &end);
        if (end == begin) throw std::runtime_error("non-numeric tolerance " + key);
        return v;
    }
    // Two-element numeric array "key": [lo, hi] (searched after the key).
    std::pair<double, double> range(const std::string& key) const
    {
        const auto at = text.find("\"" + key + "\"", start);
        if (at == std::string::npos) throw std::runtime_error("missing range " + key);
        const auto open = text.find('[', at);
        char* end = nullptr;
        const double lo = std::strtod(text.c_str() + open + 1, &end);
        const double hi = std::strtod(text.c_str() + text.find(',', open) + 1, nullptr);
        return { lo, hi };
    }

private:
    std::string text;
    std::size_t start = 0;
};
} // namespace ws1
