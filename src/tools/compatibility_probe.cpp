// Same-toolchain raw-float regression fixture, not hardware evidence.
// Compile against each revision's own core headers and library.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <array>
#include <fstream>
#include <iomanip>
#include <locale>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
// Immutable d56911a7c5f6953cd244457226b126cc567a538b bank dimensions.
// The candidate's intentionally revoiced factory bank is never a DSP test input.
constexpr std::size_t programCount = 222;
constexpr int parameterCount = 137;
using Corpus = std::array<cs80::ParamValues, programCount>;

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void exportCorpus(const char* path)
{
    const auto& bank = cs80::factoryPresets();
    require(bank.size() >= programCount && cs80::P_numParams == parameterCount,
            "export requires the pinned baseline's complete parameter schema");
    std::ofstream output(path);
    require(output.is_open(), "cannot create corpus");
    output.imbue(std::locale::classic());
    output << "CS80-COMPAT-CORPUS 1\nprograms " << programCount
           << "\nparameters " << parameterCount << '\n';
    for (int parameter = 0; parameter < parameterCount; ++parameter)
        output << cs80::paramId(parameter) << '\n';
    for (std::size_t index = 0; index < programCount; ++index)
    {
        output << "preset " << std::dec << index << '\n';
        for (int parameter = 0; parameter < parameterCount; ++parameter)
            output << std::hex << std::setw(8) << std::setfill('0')
                   << std::bit_cast<std::uint32_t>(bank[index].values[parameter]) << '\n';
    }
    output.close();
    require(!output.fail(), "cannot write complete corpus");
}

Corpus loadCorpus(const char* path)
{
    std::ifstream input(path);
    require(input.is_open(), "cannot open corpus");
    input.imbue(std::locale::classic());
    auto token = [&]()
    {
        std::string value;
        require(static_cast<bool>(input >> value), "truncated corpus");
        return value;
    };
    require(token() == "CS80-COMPAT-CORPUS" && token() == "1", "invalid corpus format");
    require(token() == "programs" && token() == std::to_string(programCount), "invalid program count");
    require(token() == "parameters" && token() == std::to_string(parameterCount), "invalid parameter count");

    std::unordered_map<std::string, int> parameters;
    for (int index = 0; index < cs80::P_numParams; ++index)
        require(parameters.emplace(cs80::paramId(index), index).second, "duplicate engine parameter ID");
    std::array<int, parameterCount> mapping{};
    std::unordered_set<std::string> seen;
    for (int& index : mapping)
    {
        const auto id = token();
        const auto found = parameters.find(id);
        require(found != parameters.end(), "unknown corpus parameter ID");
        require(seen.insert(id).second, "duplicate corpus parameter ID");
        index = found->second;
    }

    // Parameters introduced after the baseline retain this engine's core defaults.
    // In particular, the archival diagnostic leaves source/parity selection off.
    Corpus corpus{};
    for (std::size_t index = 0; index < programCount; ++index)
    {
        require(token() == "preset" && token() == std::to_string(index), "invalid preset index");
        for (int parameter : mapping)
        {
            const auto bits = token();
            std::uint32_t value = 0;
            const auto parsed = std::from_chars(bits.data(), bits.data() + bits.size(), value, 16);
            require(bits.size() == 8 && parsed.ec == std::errc{} && parsed.ptr == bits.data() + bits.size(),
                    "invalid parameter float bits");
            const float decoded = std::bit_cast<float>(value);
            require(std::isfinite(decoded), "non-finite corpus parameter");
            corpus[index][parameter] = decoded;
        }
    }
    std::string extra;
    require(!(input >> extra) && input.eof(), "extra corpus entries or read failure");
    return corpus;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc == 3 && std::strcmp(argv[1], "--export-corpus") == 0)
    {
        try { exportCorpus(argv[2]); return 0; }
        catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 6; }
    }
    const bool tails = argc == 5 && std::strcmp(argv[4], "--tails") == 0;
    if ((argc != 4 && !tails) || std::strcmp(argv[2], "--corpus") != 0) return 2;
    Corpus corpus;
    try { corpus = loadCorpus(argv[3]); }
    catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 6; }
    FILE* file = std::fopen(argv[1], "wb");
    if (!file) return 3;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (std::size_t index = 0; index < programCount; ++index)
        {
            cs80::Engine engine;
            engine.prepare(rate);
            auto values = corpus[index];
            if (tails)
            {
                // Deliberately expose shared RNG sequencing in addition to phase.
                values[cs80::P_analogNoise] = 0.2f;
                for (int rank = 0; rank < 2; ++rank)
                    values[cs80::chParam(rank, cs80::CP_noise)] = 0.3f;
            }
            engine.setParams(values);
            std::vector<float> block(514);
            if (tails)
            {
                std::array<int, 8> notes{};
                for (int voice = 0; voice < 8; ++voice)
                {
                    notes[voice] = 48 + voice * 2;
                    engine.noteOn(notes[voice], 0.7f, 1 + voice % 2);
                }
                auto renderBlock = [&]()
                {
                    engine.render(block.data(), 257);
                    return std::fwrite(block.data(), sizeof(float), block.size(), file) == block.size();
                };
                for (int warmup = 0; warmup < 8; ++warmup)
                    if (!renderBlock()) { std::fclose(file); return 4; }
                for (int event = 0; event < 32; ++event)
                {
                    const int voice = event % 8;
                    engine.noteOff(notes[voice], 1 + voice % 2);
                    if (!renderBlock()) { std::fclose(file); return 4; }
                    notes[voice] = 48 + (event * 5) % 36;
                    engine.noteOn(notes[voice], 0.65f, 1 + voice % 2);
                    if (!renderBlock()) { std::fclose(file); return 4; }
                }
                continue;
            }
            engine.noteOn(48, 0.3f);
            engine.noteOn(67, 0.9f, 2);
            for (int k = 0; k < 24; ++k)
            {
                if (k == 3) engine.controlChange(1, 0.7f);
                if (k == 5) engine.channelPressure(2, 0.6f);
                if (k == 7) engine.setRibbon(0.2f);
                if (k == 9) engine.controlChange(64, 1.0f);
                if (k == 11) engine.noteOff(48);
                if (k == 15) engine.controlChange(64, 0.0f);
                if (k == 18) engine.noteOff(67, 2);
                engine.render(block.data(), 257);
                if (std::fwrite(block.data(), sizeof(float), block.size(), file) != block.size())
                {
                    std::fclose(file);
                    return 4;
                }
            }
        }
    return std::fclose(file) == 0 ? 0 : 5;
}
