// Same-toolchain raw-float regression fixture, not hardware evidence.
// Compile against each revision's own core headers and library.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <cstdio>
#include <cstring>
#include <array>
#include <vector>

int main(int argc, char** argv)
{
    const bool tails = argc == 3 && std::strcmp(argv[2], "--tails") == 0;
    if (argc != 2 && !tails) return 2;
    FILE* file = std::fopen(argv[1], "wb");
    if (!file) return 3;
    // Pin the shipped bank; adding new programs must not change this corpus.
    const auto& bank = cs80::factoryPresets();
    if (bank.size() < 222) { std::fclose(file); return 5; }
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (std::size_t index = 0; index < 222; ++index)
        {
            const auto& preset = bank[index];
            cs80::Engine engine;
            engine.prepare(rate);
            auto values = preset.values;
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
