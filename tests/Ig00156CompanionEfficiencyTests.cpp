// Built only against a temporary instrumented copy of Ig00156Filter.h.
// The production source and shipping binary contain no counter or mutation switch.
#include <cstdint>
#include <iostream>
namespace cs80_audit { inline std::uint64_t companionReconstructions = 0; }
#include "core/Ig00156Filter.h"

int main()
{
    unsigned failures = 0;
    int networkIndex = 0;
    std::cout << "[\n";
    for (const auto& network : {cs80::kIg00156HighPassNetwork, cs80::kIg00156LowPassNetwork})
    {
        cs80::Ig00156Stage stage(network);
        stage.set(1000.0f, 5.0f, 96000.0f);
        cs80_audit::companionReconstructions = 0;
        constexpr int steadySamples = 2048, movingSamples = 256;
        for (int i = 0; i < steadySamples; ++i)
        {
            stage.set(1000.0f, 5.0f, 96000.0f);
            stage.process(i % 2 ? -0.25f : 0.5f);
        }
        const auto steadyCount = cs80_audit::companionReconstructions;
        const auto exported = stage.getState();
        const auto exportCount = cs80_audit::companionReconstructions - steadyCount;
        if (steadyCount != 0)
        {
            ++failures;
            std::cerr << "FAIL: fixed audio segment reconstructs companion histories without an export\n";
        }
        if (exportCount != 1)
        {
            ++failures;
            std::cerr << "FAIL: one export after a fixed segment must reconstruct exactly once\n";
        }
        const auto beforeChange = cs80_audit::companionReconstructions;
        stage.set(2000.0f, 1.0f, 96000.0f);
        const auto changeCount = cs80_audit::companionReconstructions - beforeChange;
        const auto beforeMoving = cs80_audit::companionReconstructions;
        for (int i = 0; i < movingSamples; ++i)
        {
            stage.set(1500.0f + 0.25f * i, 0.5f + 0.001f * i, 96000.0f);
            stage.process(i % 2 ? -0.125f : 0.375f);
        }
        const auto movingCount = cs80_audit::companionReconstructions - beforeMoving;
        const auto final = stage.getState();
        // Moving-control counts are a qualification boundary, not a speed promise.
        std::cout << "  {\"network\": " << networkIndex
                  << ", \"steady_samples\": " << steadySamples
                  << ", \"steady_reconstructions\": " << steadyCount
                  << ", \"single_export_reconstructions\": " << exportCount
                  << ", \"coefficient_change_reconstructions\": " << changeCount
                  << ", \"moving_samples\": " << movingSamples
                  << ", \"moving_reconstructions\": " << movingCount
                  << ", \"state_checksum\": "
                  << exported.input + exported.feedback + exported.bp + exported.lp
                     + final.input + final.feedback + final.bp + final.lp << "}"
                  << (networkIndex == 0 ? ",\n" : "\n");
        ++networkIndex;
    }
    std::cout << "]\n";
    return failures == 0 ? 0 : 1;
}
