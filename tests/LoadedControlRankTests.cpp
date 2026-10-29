#include "reference/LoadedControlRank.h"
#include <iostream>
#include <stdexcept>

using namespace cs80::reference::loaded_control;
namespace {
int checks = 0;
void check(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
void near(double actual, double expected, double tolerance, const char* message)
{
    check(std::abs(actual - expected) <= tolerance, message);
}
RankInputs fixture()
{
    RankInputs inputs;
    for (auto& voice : inputs.voices) {
        for (auto& source : voice.local)
            source = {0, 0};
        voice.hpTrim = voice.lpTrim = {0, 0};
        voice.hpReceiver = voice.lpReceiver = {0, 0};
        voice.hpGrade = voice.lpGrade = Grade::A;
    }
    for (auto& source : inputs.buses)
        source = {0, 0};
    return inputs;
}
Inputs pair(const RankInputs& inputs, std::size_t index)
{
    Inputs result;
    const auto& voice = inputs.voices[index];
    for (int local = 0; local < 4; ++local)
        result.shared[local] = voice.local[local];
    result.shared[4] = inputs.buses[0];
    result.shared[5] = inputs.buses[1];
    result.shared[6] = voice.local[4];
    result.fh = inputs.buses[2];
    result.fl = inputs.buses[3];
    result.hpTrim = voice.hpTrim;
    result.lpTrim = voice.lpTrim;
    result.hpReceiver = voice.hpReceiver;
    result.lpReceiver = voice.lpReceiver;
    result.hpGrade = voice.hpGrade;
    result.lpGrade = voice.lpGrade;
    return result;
}
void kcl(const RankInputs& inputs, const RankResult& result)
{
    double busCurrent[4]{};
    for (std::size_t index = 0; index < 8; ++index) {
        const auto& voice = inputs.voices[index];
        const double hp = result.pinVolts[2 * index], lp = result.pinVolts[2 * index + 1];
        double hpCurrent = hp / shuntOhms(voice.hpGrade, true)
            + voice.hpReceiver.siemens * hp + voice.hpReceiver.amps;
        double lpCurrent = lp / shuntOhms(voice.lpGrade, false)
            + voice.lpReceiver.siemens * lp + voice.lpReceiver.amps;
        // Deliberately literal resistor transcription, independent of model stamps.
        for (std::size_t local = 0; local < 5; ++local) {
            const double terminal = result.localTerminalVolts[index][local];
            const double hpBranch = (hp - terminal) / (local == 4 ? 100000 : 120000);
            const double lpBranch = (lp - terminal) / (local == 4 ? 47000 : 39000);
            hpCurrent += hpBranch;
            lpCurrent += lpBranch;
            const auto& source = voice.local[local];
            if (source.ohms == 0)
                near(terminal, source.volts, 1e-14, "ideal local");
            else
                near((terminal - source.volts) / source.ohms - hpBranch - lpBranch,
                     0, 1e-15, "local KCL");
        }
        const double hpResistance[4]{120000, 680000, 39000, 0};
        const double lpResistance[4]{39000, 220000, 0, 39000};
        for (int bus = 0; bus < 4; ++bus) {
            const double terminal = result.busTerminalVolts[bus];
            const double hpBranch = hpResistance[bus] ? (hp - terminal) / hpResistance[bus] : 0;
            const double lpBranch = lpResistance[bus] ? (lp - terminal) / lpResistance[bus] : 0;
            hpCurrent += hpBranch;
            lpCurrent += lpBranch;
            busCurrent[bus] += hpBranch + lpBranch;
        }
        hpCurrent += (hp - voice.hpTrim.volts) / (150000 + voice.hpTrim.ohms);
        lpCurrent += (lp - voice.lpTrim.volts) / (150000 + voice.lpTrim.ohms);
        near(hpCurrent, 0, 2e-15, "HP KCL");
        near(lpCurrent, 0, 2e-15, "LP KCL");
    }
    for (int bus = 0; bus < 4; ++bus) {
        const auto& source = inputs.buses[bus];
        const double terminal = result.busTerminalVolts[bus];
        if (source.ohms == 0)
            near(terminal, source.volts, 1e-14, "ideal bus");
        else
            near((terminal - source.volts) / source.ohms - busCurrent[bus],
                 0, 2e-15, "full fanout bus KCL");
    }
}
} // namespace

int main()
{
    try {
        auto inputs = fixture();
        auto result = solveRank(inputs);
        for (double voltage : result.pinVolts)
            near(voltage, 0, 0, "zero");

        // Equal voices under common sources stay equal, with all8 source loads present.
        for (auto& source : inputs.buses)
            source = {3, 47000};
        result = solveRank(inputs);
        for (int voice = 1; voice < 8; ++voice) {
            near(result.pinVolts[2 * voice], result.pinVolts[0], 1e-14, "symmetric HP");
            near(result.pinVolts[2 * voice + 1], result.pinVolts[1], 1e-14, "symmetric LP");
        }
        kcl(inputs, result);
        // Symmetric8-voice loading equals a single identical pair with8x bus
        // source resistance. This is a symmetry reduction, not a hardware value.
        auto reduced = pair(inputs, 0);
        reduced.shared[4].ohms *= 8;
        reduced.shared[5].ohms *= 8;
        reduced.fh.ohms *= 8;
        reduced.fl.ohms *= 8;
        const auto symmetric = solve(reduced);
        near(result.pinVolts[0], symmetric.hpVolts, 1e-13, "eightfold HP bus loading");
        near(result.pinVolts[1], symmetric.lpVolts, 1e-13, "eightfold LP bus loading");
        check(std::abs(result.pinVolts[0] - solve(pair(inputs, 0)).hpVolts) > 1e-4,
              "not eight independent pairs");

        // Independent ideal common sources isolate voices. One perturbed local source
        // changes other voices only once a genuinely common source has finite Z.
        inputs = fixture();
        inputs.voices[0].local[0] = {10, 22000};
        result = solveRank(inputs);
        for (int pin = 2; pin < 16; ++pin)
            near(result.pinVolts[pin], 0, 0, "ideal bus voice isolation");
        for (int bus = 0; bus < 4; ++bus) {
            auto finite = inputs;
            finite.buses[bus].ohms = 47000;
            const auto coupled = solveRank(finite);
            check(coupled.pinVolts[bus == 3 ? 3 : 2] > 0, "crossvoice common bus coupling");
            kcl(finite, coupled);
        }

        // With ideal common buses rank solve equals8 independently solved pairs,
        // including finite local source impedances and distinct per-chip grades.
        const Grade grades[]{Grade::A, Grade::B, Grade::C};
        for (int trial = 0; trial < 100; ++trial) {
            inputs = fixture();
            for (int index = 0; index < 8; ++index) {
                auto& voice = inputs.voices[index];
                voice.hpGrade = grades[(trial + index) % 3];
                voice.lpGrade = grades[(trial + 2 * index) % 3];
                for (int local = 0; local < 5; ++local)
                    voice.local[local] = {12 * std::sin(trial + index + local),
                                          1000. * (trial + index + local)};
                voice.hpTrim = trimWiper(-15, 15, (trial % 10) / 9.);
                voice.lpTrim = trimWiper(-15, 15, (index + 1) / 8.);
                voice.hpReceiver = {1e-6, 2e-6};
                voice.lpReceiver = {2e-6, -3e-6};
            }
            for (int bus = 0; bus < 4; ++bus)
                inputs.buses[bus] = {std::sin(trial + bus), 0};
            result = solveRank(inputs);
            for (int voice = 0; voice < 8; ++voice) {
                const auto independent = solve(pair(inputs, voice));
                near(result.pinVolts[2 * voice], independent.hpVolts, 1e-13, "pair HP limit");
                near(result.pinVolts[2 * voice + 1], independent.lpVolts, 1e-13, "pair LP limit");
            }
            kcl(inputs, result);
            for (int bus = 0; bus < 4; ++bus)
                inputs.buses[bus].ohms = 1000. * (trial + bus + 1);
            kcl(inputs, solveRank(inputs));
        }

        const auto rejected = [](const RankInputs& parameters) {
            try {
                (void)solveRank(parameters);
                return false;
            } catch (const std::invalid_argument&) {
                return true;
            }
        };
        check(rejected(RankInputs{}), "unknowns rejected");
        for (int bus = 0; bus < 4; ++bus) {
            inputs = fixture();
            inputs.buses[bus] = {};
            check(rejected(inputs), "each unknown bus rejected");
        }
        for (int voice = 0; voice < 8; ++voice) {
            inputs = fixture();
            inputs.voices[voice].hpReceiver = {};
            check(rejected(inputs), "each unknown receiver rejected");
            inputs = fixture();
            inputs.voices[voice].lpGrade = Grade::unknown;
            check(rejected(inputs), "each unknown grade rejected");
            for (int local = 0; local < 5; ++local) {
                inputs = fixture();
                inputs.voices[voice].local[local] = {};
                check(rejected(inputs), "each unknown local rejected");
            }
        }
        std::cout << checks << " loaded-rank checks passed (synthetic boundaries only)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
