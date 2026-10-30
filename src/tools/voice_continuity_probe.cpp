// Standalone non-production kernel trace/timing. Not a full Engine/host test.
#include "experimental/ContinuousVoiceCircuit.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using Circuit = cs80::experimental::ContinuousVoiceCircuit;
namespace
{
using Clock = std::chrono::steady_clock;
struct Bank
{
    std::array<Circuit, 8> circuits;
    std::array<Circuit::Controls, 8> controls;
    explicit Bank(double hostRate, int audible)
    {
        for (int voice = 0; voice < 8; ++voice)
        {
            const float key = cs80::filterKeyVolts(static_cast<float>(48 + voice * 3));
            const float p0 = std::fmod((voice * 2 + 1) * .61803398875f, 1.0f);
            const float p1 = std::fmod((voice * 2 + 2) * .61803398875f, 1.0f);
            circuits[voice].prepare(hostRate * 2, key, {p0, p1});
            auto& c = controls[voice]; c.keyInputVolts = key;
            for (int rank = 0; rank < 2; ++rank)
            {
                auto& r = c.ranks[rank];
                // Match the current Engine's MIDI60=.5V / A4=440 interface,
                // rather than silently substituting the rounded service261.6Hz.
                r.hzPerVolt = cs80::midiToHz(60.0f) / .5f;
                r.vcoMultiplier = rank ? 1.002f : 1.0f;
                r.pulseEnabled = true; r.pulseWidth = .37f;
                r.hpfEffectiveCv = 3; r.lpfEffectiveCv = 6;
                r.hpfQ = 1.2f; r.lpfQ = 2.4f;
                r.filterLevel = .7f; r.sineLevel = .3f;
                r.outputGain = voice < audible ? .05f : 0;
            }
        }
    }
    double render(int hostFrames, bool continuous, bool moving, std::uint64_t& sample)
    {
        double sum = 0;
        for (int i = 0; i < hostFrames * 2; ++i, ++sample)
            for (int voice = 0; voice < 8; ++voice)
            {
                auto& c = controls[voice];
                if (moving)
                {
                    // Deterministic external control/noise schedule. There is
                    // no random draw that depends on the number of live voices.
                    const auto position = (sample + static_cast<std::uint64_t>(voice * 37)) % 1024;
                    const float triangle = static_cast<float>(position < 512 ? position : 1024 - position) / 512.0f;
                    c.trHigh = position >= 768;
                    c.susHigh = (sample / 2048) % 2 == 0;
                    for (int rank = 0; rank < 2; ++rank)
                    {
                        auto& r = c.ranks[rank];
                        r.pulseWidth = .2f + triangle * .6f;
                        r.lpfEffectiveCv = 4 + triangle * 3;
                        r.hpfEnabled = sample % 4096 < 3072;
                        r.lpfEnabled = sample % 3072 < 2048;
                        const auto noise = static_cast<std::uint32_t>(sample) * 747796405u
                            + static_cast<std::uint32_t>(voice * 2 + rank) * 2891336453u;
                        r.noiseInput = (static_cast<float>(noise >> 16) / 65535.0f - .5f) * .02f;
                    }
                }
                if (continuous || c.ranks[0].outputGain != 0)
                {
                    const auto result = circuits[voice].processSample(c);
                    if (!result.accepted) throw std::runtime_error("unexpected rejected kernel input");
                    sum += result.ranks[0].audio + result.ranks[1].audio;
                }
            }
        return sum;
    }
};
void trace(std::ostream& out)
{
    Circuit circuit;
    circuit.prepare(96000, .5f, {.13f, .71f});
    Circuit::Controls c;
    for (auto& r : c.ranks)
    {
        r.hzPerVolt = cs80::midiToHz(60.0f) / .5f;
        r.hpfEffectiveCv = 3; r.lpfEffectiveCv = 6;
        r.hpfQ = 1.2f; r.lpfQ = 2.4f; r.outputGain = .1f;
    }
    c.ranks[1].vcoMultiplier = 1.5f;
    out << "internal_sample,elapsed_seconds,tr_high,sus_high,ki_volts,held_k_volts,output_gain,hp_enabled,lp_enabled"
           ",phase0,hp0_ic1,hp0_ic2,lp0_ic1,lp0_ic2,audio0,requested_hz0"
           ",phase1,hp1_ic1,hp1_ic2,lp1_ic1,lp1_ic2,audio1,requested_hz1\n";
    out << std::setprecision(10);
    for (int sample = 0; sample < 8192; ++sample)
    {
        c.trHigh = sample >= 512 && sample < 6144;
        c.susHigh = sample < 4096;
        c.keyInputVolts = sample < 512 ? .5f : sample < 6144 ? 1.0f : .25f;
        for (auto& r : c.ranks)
        {
            r.outputGain = sample >= 512 && sample < 6144 ? 0 : .1f;
            r.hpfEnabled = sample < 2048 || sample >= 4096;
            r.lpfEnabled = sample < 3072 || sample >= 5120;
        }
        const auto result = circuit.processSample(c);
        const auto state = circuit.getState();
        if (!result.accepted) throw std::runtime_error("trace input rejected");
        if (sample % 16 != 0 && sample % 512 != 511) continue;
        out << sample << ',' << (sample + 1) / 96000.0 << ',' << c.trHigh << ',' << c.susHigh
            << ',' << c.keyInputVolts << ',' << result.heldKeyVolts << ',' << c.ranks[0].outputGain
            << ',' << c.ranks[0].hpfEnabled << ',' << c.ranks[0].lpfEnabled;
        for (int rank = 0; rank < 2; ++rank)
        {
            const auto& s = state.ranks[rank];
            out << ',' << s.phase << ',' << s.hpf.ic1 << ',' << s.hpf.ic2
                << ',' << s.lpf.ic1 << ',' << s.lpf.ic2 << ',' << result.ranks[rank].audio
                << ',' << result.ranks[rank].oscillatorHz;
        }
        out << '\n';
    }
}
void benchmark(std::ostream& out, int callbacks)
{
    out << "host_hz,frames,profile,audible_voices,policy,callbacks,median_ms,p99_ms,max_ms"
           ",median_rtf,p99_rtf,max_rtf,budget_exceedances,phase_process_cpu_seconds,checksum,state_checksum\n";
    out << std::setprecision(10);
    double skippedAudioChecksum = 0, skippedStateChecksum = 0;
    for (double rate : {44100., 48000., 96000.}) for (int block : {128, 256})
        for (bool moving : {false, true}) for (int audible : {0, 1, 8})
            for (bool continuous : {false, true})
            {
                Bank bank(rate, audible);
                std::uint64_t sample = 0;
                double checksum = bank.render(block * 32, continuous, moving, sample);
                std::vector<double> elapsed(static_cast<std::size_t>(callbacks));
                const auto cpuStart = std::clock();
                for (int callback = 0; callback < callbacks; ++callback)
                {
                    const auto start = Clock::now();
                    checksum += bank.render(block, continuous, moving, sample);
                    const auto end = Clock::now();
                    elapsed[static_cast<std::size_t>(callback)] =
                        std::chrono::duration<double, std::milli>(end - start).count();
                }
                const double cpu = static_cast<double>(std::clock() - cpuStart) / CLOCKS_PER_SEC;
                // Observe even muted states outside the timed callback, so the
                // optimizer cannot discard inaudible oscillator/filter work.
                double stateChecksum = 0;
                for (int voice = 0; voice < 8; ++voice)
                {
                    const auto state = bank.circuits[voice].getState();
                    const auto expected = continuous || voice < audible ? sample : 0;
                    if (state.samples != expected) throw std::runtime_error("kernel clock count mismatch");
                    stateChecksum += state.heldKeyVolts + static_cast<double>(state.samples);
                    for (const auto& rank : state.ranks)
                        stateChecksum += rank.phase + rank.hpf.ic1 + rank.hpf.ic2
                            + rank.lpf.ic1 + rank.lpf.ic2;
                }
                if (!std::isfinite(checksum)) throw std::runtime_error("nonfinite benchmark result");
                if (!std::isfinite(stateChecksum)) throw std::runtime_error("nonfinite kernel state");
                if (!continuous)
                {
                    skippedAudioChecksum = checksum;
                    skippedStateChecksum = stateChecksum;
                }
                else if (checksum != skippedAudioChecksum
                         || (audible == 8 && stateChecksum != skippedStateChecksum))
                    throw std::runtime_error("matched synthetic-policy checksum mismatch");
                std::sort(elapsed.begin(), elapsed.end());
                const double median = elapsed[elapsed.size() / 2];
                const double p99 = elapsed[static_cast<std::size_t>(std::ceil(elapsed.size() * .99)) - 1];
                const double maximum = elapsed.back(), budgetMs = block * 1000. / rate;
                const auto overruns = std::count_if(elapsed.begin(), elapsed.end(),
                    [budgetMs](double value) { return value > budgetMs; });
                out << rate << ',' << block << ',' << (moving ? "moving" : "static") << ',' << audible
                    << ',' << (continuous ? "continuous" : "skip_inaudible") << ',' << callbacks
                    << ',' << median << ',' << p99 << ',' << maximum << ',' << median / budgetMs
                    << ',' << p99 / budgetMs << ',' << maximum / budgetMs << ',' << overruns
                    << ',' << cpu << ',' << checksum << ',' << stateChecksum << '\n';
            }
}
}
int main(int argc, char** argv)
{
    try
    {
        if (argc < 3 || argc > 4 || (std::string(argv[1]) != "--trace" && std::string(argv[1]) != "--benchmark"))
            throw std::runtime_error("usage: voice_continuity_probe --trace FILE.csv | --benchmark FILE.csv [callbacks=256]");
        const bool timing = std::string(argv[1]) == "--benchmark";
        int callbacks = 256;
        if (argc == 4)
        {
            if (!timing) throw std::runtime_error("callbacks only applies to --benchmark");
            std::size_t consumed = 0;
            callbacks = std::stoi(argv[3], &consumed);
            if (consumed != std::string(argv[3]).size() || callbacks < 8 || callbacks > 4096)
                throw std::runtime_error("callbacks must be a complete integer in8..4096");
        }
        if (std::filesystem::exists(argv[2])) throw std::runtime_error("refusing to overwrite existing output");
        std::ofstream out(argv[2]);
        if (!out) throw std::runtime_error("cannot open output path");
        if (timing) benchmark(out, callbacks); else trace(out);
        out.flush();
        if (!out) throw std::runtime_error("output write failed");
        std::cout << (timing ? "72 timing rows" : "528 trace rows") << " written; kernel-only, not Engine/host qualification\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
