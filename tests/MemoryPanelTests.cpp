// Memory/PANEL programmer panels: switch states and electrical loading
// (src/core/MemoryPanels.h, compiled) held to the offline truth model
// (src/reference/MemoryPanelNetwork.h) with the pre-registered tolerances in
// tests/parity/tolerances-v2/ws6-library.json (stage memory_panel_v1).
// Model-consistency checks, not hardware evidence.
#include "core/MemoryPanels.h"
#include "reference/MemoryPanelNetwork.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

namespace mp = cs80::memory_panels;
namespace sf = cs80::source_factory;
namespace truth = cs80::reference::memory_panel;

namespace
{
int failures = 0;
void check(bool ok, const std::string& message)
{
    if (!ok)
    {
        ++failures;
        if (failures < 40) std::printf("FAIL: %s\n", message.c_str());
    }
}

double tolerance(const std::string& json, const std::string& stage, const std::string& key)
{
    const auto at = json.find("\"" + stage + "\"");
    const auto k = at == std::string::npos ? at : json.find("\"" + key + "\"", at);
    if (k == std::string::npos) { std::printf("missing tolerance %s.%s\n", stage.c_str(), key.c_str()); std::exit(2); }
    return std::strtod(json.c_str() + json.find(':', k) + 1, nullptr);
}

struct Lcg
{
    std::uint32_t state;
    double next() { state = state * 1664525u + 1013904223u; return (double) (state >> 8) / 16777216.0; }
};

mp::Positions randomBank(Lcg& r)
{
    mp::Positions p {};
    for (int output = 1; output <= 26; ++output)
        p[(size_t) (output - 1)] = mp::isSwitchRow(output) ? (r.next() < 0.5 ? 0.0 : 1.0) : r.next();
    return p;
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) { std::printf("usage: %s tolerances.json\n", argv[0]); return 2; }
    std::ifstream in(argv[1]);
    std::stringstream text;
    text << in.rdbuf();
    const auto json = text.str();
    const double tolBus = tolerance(json, "memory_panel_v1", "bus_volts");
    const double tolInactive = tolerance(json, "memory_panel_v1", "inactive_source_current_amps");
    const double tolResidual = tolerance(json, "memory_panel_v1", "newton_residual_amps");
    const bool verbose = std::getenv("CS80_MEMORY_PANEL_VERBOSE") != nullptr;
    double worstBus = 0.0, worstInactive = 0.0, worstResidual = 0.0;

    // --- Compiled vs truth in every source state (four memories, PANEL, tabs).
    Lcg rng { 0x5eed1234u };
    for (int rank = 0; rank < 2; ++rank)
        for (int selector : { 0, 3, 7, 10, 11, 12, 13 })
            for (int trial = 0; trial < 4; ++trial)
            {
                truth::Setup setup;
                setup.rank = rank;
                setup.selector = selector;
                for (auto& bank : setup.banks) bank = randomBank(rng);
                const auto t = truth::solve(setup);
                const std::string label = "rank " + std::to_string(rank + 1) + " selector " + std::to_string(selector);
                check(t.converged, "truth network converges: " + label);
                worstResidual = std::max(worstResidual, t.residualAmps);
                worstInactive = std::max(worstInactive, t.worstInactiveAmps);
                check(t.residualAmps <= tolResidual, "truth Newton residual: " + label);
                check(t.worstInactiveAmps <= tolInactive, "unselected sources carry only diode leakage: " + label);
                const int bankIndex = mp::sourceFor(selector) == mp::Source::memoryA ? 0
                                    : mp::sourceFor(selector) == mp::Source::memoryB ? 1 : 2;
                const auto c = mp::compileBuses(rank, selector, setup.banks[(size_t) bankIndex]);
                check(c.ok, "compiled buses: " + label);
                for (int output = 1; output <= 26; ++output)
                {
                    if (mp::isSwitchRow(output)) continue;
                    const double error = std::abs(c.busVolts[(size_t) (output - 1)] - t.busVolts[(size_t) (output - 1)]);
                    worstBus = std::max(worstBus, error);
                    check(error <= tolBus, "compiled bus within tolerance: " + label + " output " + std::to_string(output)
                                               + " error " + std::to_string(error));
                }
                if (mp::sourceFor(selector) != mp::Source::factoryTab)
                    check(t.railVolts[(size_t) bankIndex] < sf::nominalBoundaryV1.driver.openCircuitVolts
                              && std::abs(t.railVolts[(size_t) bankIndex] - c.railVolts) <= tolBus,
                          "selected rail sags under its pot bank and branch currents: " + label);
                for (int s = 0; s < 3; ++s)
                    if (s != bankIndex || mp::sourceFor(selector) == mp::Source::factoryTab)
                        check(std::abs(t.railVolts[(size_t) s]) < 1e-6, "unselected rails stay at the reference: " + label);
            }

    // --- Switch states: only the selected source decides; isolation is complete.
    for (int rank = 0; rank < 2; ++rank)
    {
        truth::Setup a, b;
        a.rank = b.rank = rank;
        a.selector = b.selector = 3;
        Lcg r1 { 11u }, r2 { 97u };
        for (int s = 0; s < 3; ++s) { a.banks[(size_t) s] = randomBank(r1); b.banks[(size_t) s] = randomBank(r2); }
        const auto ta = truth::solve(a), tb = truth::solve(b);
        double moved = 0.0;
        for (int output = 1; output <= 26; ++output)
            moved = std::max(moved, std::abs(ta.busVolts[(size_t) (output - 1)] - tb.busVolts[(size_t) (output - 1)]));
        check(moved < 1e-6, "memory and PANEL pots cannot move a factory tab's buses (diode isolation)");
        for (int selector = 0; selector < 11; ++selector)
        {
            const auto* address = sf::address(rank, selector);
            bool pulse = false, saw = false;
            sf::switchEnabled(cs80::hardware::findBranch(address->board, address->column, 4), pulse);
            sf::switchEnabled(cs80::hardware::findBranch(address->board, address->column, 5), saw);
            const auto s = mp::switchStates(rank, selector, a.banks[0]);
            check(s.pulse == pulse && s.saw == saw, "factory tab switch rows come from its matrix column");
        }
        mp::Positions on {}, off {};
        on[3] = on[4] = 1.0;
        check(mp::switchStates(rank, 11, on).pulse && mp::switchStates(rank, 11, on).saw
                  && !mp::switchStates(rank, 12, off).pulse && !mp::switchStates(rank, 13, off).saw,
              "memory/PANEL waveform switches follow the selected bank");
        check(mp::memoryBank(rank, mp::Source::memoryA) == rank && mp::memoryBank(rank, mp::Source::memoryB) == rank + 2,
              "rank I uses memories 1/3, rank II memories 2/4 (plugin banks 0/2 and 1/3)");
    }

    // --- Loading law: an unbuffered memory pot works into the 10 k bus shunt.
    {
        mp::Positions bank {};
        double previousAl = -1.0, previousLp = -1.0;
        for (int step = 0; step <= 20; ++step)
        {
            const double x = step / 20.0;
            for (int output = 1; output <= 26; ++output)
                if (!mp::isSwitchRow(output)) bank[(size_t) (output - 1)] = x;
            const auto c = mp::compileBuses(0, 11, bank);
            check(c.busVolts[11] >= previousAl && c.busVolts[8] >= previousLp, "bus voltage rises with the memory slider");
            previousAl = c.busVolts[11];
            previousLp = c.busVolts[8];
            if (step == 10)
            {
                check(c.busVolts[11] < 0.75 * c.busVolts[8],
                      "mid-travel: the unbuffered AL pot sags against the buffered LP pot (wiper impedance into the shunt)");
                if (verbose)
                    for (double pot : { 10000.0, 50000.0, 100000.0 })
                    {
                        mp::Electrical e = mp::nominalElectrical;
                        e.potOhms = pot;
                        const auto s = mp::compileBuses(0, 11, bank, e);
                        std::printf("pot %6.0f ohm, all sliders at 0.5: rail %.3f V, AL bus %.3f V, LP bus %.3f V, "
                                    "VCA sustain bus %.3f V\n", pot, s.railVolts, s.busVolts[11], s.busVolts[8], s.busVolts[19]);
                    }
            }
        }
    }

    std::printf("worst compiled-vs-truth bus error %.3g V, inactive source current %.3g A, Newton residual %.3g A\n",
                worstBus, worstInactive, worstResidual);
    if (failures != 0)
    {
        std::printf("%d memory panel failures\n", failures);
        return 1;
    }
    std::printf("memory/PANEL switch and loading model passed\n");
    return 0;
}
