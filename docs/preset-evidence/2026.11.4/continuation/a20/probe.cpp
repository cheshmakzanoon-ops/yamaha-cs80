#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

double cpuSeconds()
{
    timespec value{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &value) != 0) throw std::runtime_error("thread CPU clock failed");
    return value.tv_sec + 1e-9 * value.tv_nsec;
}

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const int caseIndex = std::stoi(argv[1]);
    const bool moving = std::stoi(argv[2]) != 0;
    const std::array<const char*, 4> names{"Solo Violin", "Solo Cello", "Ensemble Strings", "Ensemble Strings"};
    if (caseIndex < 0 || caseIndex >= static_cast<int>(names.size())) return 2;
    const auto& library = cs80::programLibrary();
    const auto found = std::find_if(library.begin(), library.end(), [&](const auto& p) { return p.name == names[caseIndex]; });
    if (found == library.end()) return 3;
    auto values = found->values;
    values[cs80::P_oversampling] = 1.0f;
    values[cs80::P_pitchBendRange] = 2.0f;
    if (caseIndex == 3)
        for (int rank=0; rank<2; ++rank) {
            values[cs80::bodyParam(rank,0)] = 0.0f;
            values[cs80::bodyParam(rank,1)] = 0.0f;
        }
    auto engine = std::make_unique<cs80::Engine>();
    engine->prepare(48000.0);
    engine->setParams(values);
    for (int note : {48, 52, 55, 59, 60, 64, 67, 71}) engine->noteOn(note, 0.85f, 1);
    constexpr int block = 64;
    constexpr int warmFrames = 12032; // 0.251s, full block boundary
    constexpr int measuredFrames = 24000; // 0.5s
    std::array<float, block*2> warm{};
    std::vector<float> audio(measuredFrames*2);
    constexpr double twoPi = 6.283185307179586476925;
    auto setPitch = [&](int sample) {
        const float bend = moving ? static_cast<float>(0.2 * std::sin(twoPi * 5.0 * sample / 48000.0)) : 0.0f;
        engine->pitchBend(1, bend); // +/-0.2 of a two-semitone range = +/-40 cents
    };
    for (int frame=0; frame<warmFrames; frame+=block) {
        setPitch(frame);
        engine->render(warm.data(), block);
    }
    if (engine->activeVoices() != 8) return 4;
    const auto start = cpuSeconds();
    for (int frame=0; frame<measuredFrames; frame+=block) {
        setPitch(warmFrames+frame);
        engine->render(audio.data()+2*frame, std::min(block, measuredFrames-frame));
    }
    const auto seconds = cpuSeconds()-start;
    bool finite = true;
    double peak = 0.0, energy=0.0;
    std::uint64_t hash=14695981039346656037ull;
    for (float value : audio) {
        finite = finite && std::isfinite(value);
        peak = std::max(peak, std::abs(static_cast<double>(value)));
        energy += static_cast<double>(value)*value;
        const auto bits=std::bit_cast<std::uint32_t>(value);
        for (int byte=0; byte<4; ++byte) { hash ^= (bits>>(byte*8))&255u; hash *=1099511628211ull; }
    }
    std::cout << std::setprecision(17) << caseIndex << ',' << names[caseIndex] << ',' << moving << ','
              << seconds << ',' << peak << ',' << std::sqrt(energy/audio.size()) << ','
              << hash << ',' << engine->activeVoices() << ',' << sizeof(cs80::Engine) << '\n';
    return finite && peak > 1e-5 && engine->activeVoices()==8 ? 0 : 5;
}
