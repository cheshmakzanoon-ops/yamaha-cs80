#define cs80 cs80_before
#include "InstrumentBody-before.h"
#undef cs80
#include "/workspace/scratch/a9592b052b11/cs80-work/src/core/InstrumentBody.h"
#include <algorithm>
#include <cstdio>
#include <ctime>
#include <vector>

template<class Body> double timeBody(int model, int note, double& checksum)
{
    Body body;
    body.prepare(96000.0, 48000.0);
    body.noteOn(model, static_cast<float>(note), 0.8f, 0.6f, 1.0f, 317);
    const float frequency = static_cast<float>(440.0 * std::exp2((note - 69.0) / 12.0));
    const auto start = std::clock();
    for (int sample = 0; sample < 192000; ++sample)
        checksum += body.tick(frequency, 0.6f);
    return 1.0e9 * (std::clock() - start) / CLOCKS_PER_SEC / 192000.0;
}

int main()
{
    bool exact = true;
    std::uint64_t paritySamples = 0;
    for (int model : {1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 17, 18, 19})
        for (int note : {36, 60, 84})
            for (double rate : {44100.0, 48000.0, 96000.0})
            {
                cs80_before::body::InstrumentBody before;
                cs80::body::InstrumentBody current;
                before.prepare(rate * 2.0, rate); current.prepare(rate * 2.0, rate);
                before.noteOn(model, note, 0.8f, 0.6f, 1.0f, 317, 1.35f);
                current.noteOn(model, note, 0.8f, 0.6f, 1.0f, 317, 1.35f);
                const float frequency = static_cast<float>(440.0 * std::exp2((note - 69.0) / 12.0));
                for (int sample = 0; sample < static_cast<int>(0.7 * rate); ++sample)
                {
                    const float pressure = sample % 38400 > 19200 ? 0.8f : 0.0f;
                    exact = (before.tick(frequency, 0.6f, pressure)
                          == current.tick(frequency, 0.6f, pressure)) && exact;
                    ++paritySamples;
                }
            }
    std::printf("Unaffected-model parity: %llu samples, %s\n",
                static_cast<unsigned long long>(paritySamples), exact ? "bit-exact" : "DIFFERENT");
    double checksum = 0.0;
    for (int note : {36, 60, 84})
        for (int model : {1, 9, 10, 11, 12, 13, 14, 15})
        {
            std::vector<double> before, after;
            for (int trial = 0; trial < 5; ++trial)
            {
                if (trial % 2)
                {
                    after.push_back(timeBody<cs80::body::InstrumentBody>(model, note, checksum));
                    before.push_back(timeBody<cs80_before::body::InstrumentBody>(model, note, checksum));
                }
                else
                {
                    before.push_back(timeBody<cs80_before::body::InstrumentBody>(model, note, checksum));
                    after.push_back(timeBody<cs80::body::InstrumentBody>(model, note, checksum));
                }
            }
            std::sort(before.begin(), before.end()); std::sort(after.begin(), after.end());
            std::printf("note %d %-20s before %.3f ns/sample after %.3f ratio %.3f\n",
                        note, cs80::body::name(model), before[2], after[2], after[2] / before[2]);
        }
    std::printf("Checksum %.12g\n", checksum);
    return exact ? 0 : 1;
}
