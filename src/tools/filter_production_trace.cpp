// Bounded offline observation of the production FilterEnv, not an envelope model.
// The source-bound identity deliberately covers dirty local edits at configure.
#include "core/Dsp.h"
#include "tools/TruthInputSha256.h"

#include <array>
#include <charconv>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
constexpr std::size_t maxInputBytes = 1024 * 1024;
constexpr std::size_t maxEvents = 4096;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool isDigit(char c) noexcept { return c >= '0' && c <= '9'; }

int integer(std::string_view text, int low, int high)
{
    require(!text.empty() && text.size() <= 10, "Expected bounded canonical unsigned integer");
    require(text.size() == 1 || text.front() != '0', "Integer has leading zeros");
    for (const char c : text)
        require(isDigit(c), "Expected canonical unsigned integer");
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
                && value >= low && value <= high, "Integer outside bounds");
    return value;
}

// Check the mathematical decimal domain before binary32 conversion. Comparing
// only rounded floats would admit e.g. 1.000000001 to a domain ending at 1.
// Bounds are powers of ten or signed level magnitude 8; no float parser or
// intermediate binary64 rounding is needed to perform these comparisons.
struct Decimal
{
    std::string digits;
    int exponent = 0;
    bool negative = false;

    int comparePowerOfTen(int power, char leading = '1') const
    {
        if (digits.empty())
            return -1;
        const int order = static_cast<int>(digits.size()) + exponent - 1;
        if (order != power)
            return order < power ? -1 : 1;
        if (digits.front() != leading)
            return digits.front() < leading ? -1 : 1;
        for (std::size_t i = 1; i < digits.size(); ++i)
            if (digits[i] != '0')
                return 1;
        return 0;
    }
};

Decimal parseDecimal(std::string_view text)
{
    require(!text.empty() && text.size() <= 64, "Expected bounded ASCII decimal");
    Decimal result;
    std::size_t position = 0;
    if (text[position] == '+' || text[position] == '-')
        result.negative = text[position++] == '-';
    while (position < text.size() && isDigit(text[position]))
        result.digits += text[position++];
    if (position < text.size() && text[position] == '.')
    {
        ++position;
        while (position < text.size() && isDigit(text[position]))
        {
            result.digits += text[position++];
            --result.exponent;
        }
    }
    require(!result.digits.empty(), "Decimal requires mantissa digits");
    if (position < text.size() && (text[position] == 'e' || text[position] == 'E'))
    {
        ++position;
        bool negativeExponent = false;
        if (position < text.size() && (text[position] == '+' || text[position] == '-'))
            negativeExponent = text[position++] == '-';
        const auto start = position;
        int exponent = 0;
        while (position < text.size() && isDigit(text[position]))
        {
            require(position - start < 3, "Decimal exponent exceeds three digits");
            exponent = 10 * exponent + (text[position++] - '0');
        }
        require(position > start && exponent <= 300, "Decimal exponent outside [-300,300]");
        result.exponent += negativeExponent ? -exponent : exponent;
    }
    require(position == text.size(), "Invalid ASCII decimal");
    const auto firstNonzero = result.digits.find_first_not_of('0');
    if (firstNonzero == std::string::npos)
        result.digits.clear();
    else
        result.digits.erase(0, firstNonzero);
    return result;
}

float parameter(std::string_view text, bool zeroAllowed, int minimumPower, int maximumPower, bool signedLevel = false)
{
    const auto decimal = parseDecimal(text);
    require(signedLevel || !decimal.negative || decimal.digits.empty(), "Parameter outside bounds");
    require((zeroAllowed || decimal.comparePowerOfTen(minimumPower) >= 0)
                && decimal.comparePowerOfTen(maximumPower, signedLevel ? '8' : '1') <= 0, "Parameter outside bounds");
    // from_chars does not accept a leading +, although the reference grammar
    // does. Strip only that sign and parse directly into the actual API type.
    if (text.front() == '+')
        text.remove_prefix(1);
    float value = 0.0f;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value,
                                        std::chars_format::general);
    require(parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size()
                && std::isfinite(value) && (value != 0.0f || decimal.digits.empty()),
            "Unrepresentable float parameter (overflow or underflow)");
    return value;
}

std::string readInput()
{
    std::string bytes;
    std::array<char, 8192> buffer{};
    while (std::cin)
    {
        // Read at most limit+1 bytes, including when the source is oversized.
        const auto requested = std::min(buffer.size(), maxInputBytes + 1 - bytes.size());
        std::cin.read(buffer.data(), static_cast<std::streamsize>(requested));
        const auto count = static_cast<std::size_t>(std::cin.gcount());
        bytes.append(buffer.data(), count);
        require(bytes.size() <= maxInputBytes, "Event input exceeds 1 MiB");
    }
    require(std::cin.eof() && !std::cin.bad(), "Event input read failed");
    return bytes;
}

enum class Action { FreshOn, NoteOff, RetainedRetrigger, Reset };
struct Event { int sample; Action action; };

std::vector<Event> parseEvents(const std::string& bytes, int samples)
{
    std::size_t position = 0;
    const auto line = [&]()
    {
        const auto end = bytes.find('\n', position);
        require(end != std::string::npos && end - position <= 128,
                "Expected LF-terminated event line of at most 128 bytes");
        const std::string_view value(bytes.data() + position, end - position);
        position = end + 1;
        return value;
    };
    require(line() == "event,sample,action", "Expected normalized header event,sample,action");
    std::vector<Event> events;
    int previousSample = 0;
    while (position < bytes.size())
    {
        require(events.size() < maxEvents, "Event count exceeds 4096");
        auto row = line();
        const auto firstComma = row.find(',');
        require(firstComma != std::string_view::npos, "Missing event columns");
        require(row.substr(0, firstComma) == std::to_string(events.size()),
                "Expected canonical contiguous event index");
        row.remove_prefix(firstComma + 1);
        const auto secondComma = row.find(',');
        require(secondComma != std::string_view::npos, "Missing event action");
        const int sample = integer(row.substr(0, secondComma), 0, samples - 1);
        require(sample >= previousSample, "Event samples must be nondecreasing");
        row.remove_prefix(secondComma + 1);
        Action action;
        if (row == "fresh_on") action = Action::FreshOn;
        else if (row == "note_off") action = Action::NoteOff;
        else if (row == "retained_retrigger") action = Action::RetainedRetrigger;
        else if (row == "reset") action = Action::Reset;
        else throw std::runtime_error("Unknown event action or extra CSV columns");
        events.push_back({sample, action});
        previousSample = sample;
    }
    return events;
}

const char* stageName(cs80::FilterEnv::Stage stage)
{
    switch (stage)
    {
        case cs80::FilterEnv::Stage::Idle: return "idle";
        case cs80::FilterEnv::Stage::Attack: return "attack";
        case cs80::FilterEnv::Stage::Decay: return "decay";
        case cs80::FilterEnv::Stage::Sustain: return "sustain";
        case cs80::FilterEnv::Stage::Release: return "release";
    }
    throw std::runtime_error("Unknown production envelope stage");
}

void checkState(const cs80::FilterEnv::State& state)
{
    require(std::isfinite(state.value) && state.value >= -8.0f && state.value <= 8.0f,
            "Production envelope outside finite [-8,8]");
    require(std::isfinite(state.elapsed) && state.elapsed >= 0.0f && state.elapsed <= 10.0f,
            "Production elapsed outside finite [0,10]");
    (void) stageName(state.stage);
}

void jsonString(std::ostream& output, std::string_view text)
{
    constexpr char hex[] = "0123456789abcdef";
    output << '"';
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\')
            output << '\\' << static_cast<char>(c);
        else if (c < 0x20 || c >= 0x7f)
            output << "\\u00" << hex[c >> 4] << hex[c & 15];
        else
            output << static_cast<char>(c);
    }
    output << '"';
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        require(argc == 8, "Usage: cs80_filter_production_trace RATE SAMPLES IL AL A D R");
        const int rate = integer(argv[1], 8000, 384000);
        const int samples = integer(argv[2], 2, 100001);
        require(samples - 1 <= 10 * rate, "Endpoint span exceeds ten seconds");
        const float il = parameter(argv[3], true, 0, 0, true);
        const float al = parameter(argv[4], true, 0, 0, true);
        const float attack = parameter(argv[5], false, -3, 0);
        const float decay = parameter(argv[6], false, -2, 1);
        const float release = parameter(argv[7], false, -2, 1);
        const float sampleRate = static_cast<float>(rate);
        const auto input = readInput();
        const auto events = parseEvents(input, samples);

        // No stdout is written until all input and every production state have
        // passed validation. Nothing is written to an output file or directory.
        std::ostringstream output;
        output.imbue(std::locale::classic());
        output << std::setprecision(std::numeric_limits<double>::max_digits10);
        output << "{\"schema\":\"cs80-filter-production-trace-v1\",\"parameters\":{"
               << "\"il\":" << static_cast<double>(il)
               << ",\"al\":" << static_cast<double>(al)
               << ",\"attack\":" << static_cast<double>(attack)
               << ",\"decay\":" << static_cast<double>(decay)
               << ",\"release\":" << static_cast<double>(release)
               << ",\"rate_hz\":" << static_cast<double>(sampleRate)
               << "},\"samples\":" << samples << ",\"events\":" << events.size()
               << ",\"input_bytes\":" << input.size()
               << ",\"input_sha256\":\"" << cs80::tools::inputSha256(input)
               << "\",\"source_sha256\":{"
               << "\"src/tools/filter_production_trace.cpp\":\"" << FILTER_PRODUCTION_TRACE_SHA
               << "\",\"src/core/Dsp.h\":\"" << FILTER_PRODUCTION_DSP_SHA
               << "\",\"src/tools/TruthInputSha256.h\":\"" << FILTER_PRODUCTION_HASH_SHA
               << "\",\"CMakeLists.txt\":\"" << FILTER_PRODUCTION_CMAKE_SHA
               << "\"},\"compiler\":";
        jsonString(output, FILTER_PRODUCTION_COMPILER);
        output << ",\"build_config\":";
        jsonString(output, FILTER_PRODUCTION_BUILD_CONFIG);
        output << ",\"cplusplus\":" << __cplusplus << "}\n"
               << "sample,time_s,envelope,stage,elapsed,gate,last_event\n";

        cs80::FilterEnv envelope;
        envelope.reset();
        bool gate = false;
        std::size_t cursor = 0;
        for (int sample = 0; sample < samples; ++sample)
        {
            // Endpoints, not blocks: zero has no tick; later endpoints first
            // finish exactly one production interval, then apply their events.
            if (sample > 0)
            {
                const float value = envelope.tick(sampleRate, true, true);
                require(std::isfinite(value) && value >= -8.0f && value <= 8.0f,
                        "Production tick outside finite [-8,8]");
                checkState(envelope.getState());
            }
            while (cursor < events.size() && events[cursor].sample == sample)
            {
                switch (events[cursor].action)
                {
                    case Action::FreshOn:
                    case Action::RetainedRetrigger:
                        // Retained mode preserves FilterEnv's private double state.
                        envelope.noteOn(il, al, attack, decay, release, 0.0f,
                                        events[cursor].action == Action::RetainedRetrigger);
                        gate = true;
                        break;
                    case Action::NoteOff:
                        envelope.noteOff();
                        gate = false;
                        break;
                    case Action::Reset:
                        envelope.reset();
                        gate = false;
                        break;
                }
                checkState(envelope.getState());
                ++cursor;
            }
            const auto state = envelope.getState();
            checkState(state);
            output << sample << ',' << static_cast<double>(sample) / rate << ','
                   << static_cast<double>(state.value) << ',' << stageName(state.stage) << ','
                   << static_cast<double>(state.elapsed) << ',' << (gate ? 1 : 0) << ','
                   << static_cast<int>(cursor) - 1 << '\n';
        }
        require(cursor == events.size(), "Not all events were consumed");
        require(bool(output), "Cannot buffer production trace");
        const auto result = output.str();
        std::cout.write(result.data(), static_cast<std::streamsize>(result.size()));
        std::cout.flush();
        require(bool(std::cout), "Production trace output write failed");
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "filter_production_trace: " << error.what() << '\n';
        return 1;
    }
}
