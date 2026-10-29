// Same-toolchain raw-float regression fixture, not hardware evidence.
// Compile against each revision's own core headers and library.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <cstdio>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    FILE* file = std::fopen(argv[1], "wb");
    if (!file) return 3;
    for (double rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto& preset : cs80::factoryPresets())
        {
            cs80::Engine engine;
            engine.prepare(rate);
            engine.setParams(preset.values);
            std::vector<float> block(514);
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
