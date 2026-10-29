#include "reference/LoadedControlTransient.h"
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
    check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
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
// Independent component-current sum excluding the capacitor. No model matrix
// is used in this oracle. Positive is current leaving the control pin.
std::array<double, 16> resistiveCurrent(const RankInputs& inputs, const RankResult& state)
{
    std::array<double, 16> current{};
    for (std::size_t index = 0; index < 8; ++index) {
        const auto& voice = inputs.voices[index];
        const double hp = state.pinVolts[2 * index], lp = state.pinVolts[2 * index + 1];
        double hpCurrent = hp / shuntOhms(voice.hpGrade, true)
            + voice.hpReceiver.siemens * hp + voice.hpReceiver.amps;
        double lpCurrent = lp / shuntOhms(voice.lpGrade, false)
            + voice.lpReceiver.siemens * lp + voice.lpReceiver.amps;
        for (std::size_t local = 0; local < 5; ++local) {
            const double terminal = state.localTerminalVolts[index][local];
            hpCurrent += (hp - terminal) / (local == 4 ? 100000 : 120000);
            lpCurrent += (lp - terminal) / (local == 4 ? 47000 : 39000);
        }
        hpCurrent += (hp - state.busTerminalVolts[0]) / 120000
            + (hp - state.busTerminalVolts[1]) / 680000
            + (hp - state.busTerminalVolts[2]) / 39000
            + (hp - voice.hpTrim.volts) / (150000 + voice.hpTrim.ohms);
        lpCurrent += (lp - state.busTerminalVolts[0]) / 39000
            + (lp - state.busTerminalVolts[1]) / 220000
            + (lp - state.busTerminalVolts[3]) / 39000
            + (lp - voice.lpTrim.volts) / (150000 + voice.lpTrim.ohms);
        current[2 * index] = hpCurrent;
        current[2 * index + 1] = lpCurrent;
    }
    return current;
}
constexpr double capacitance = 1e-8; // independent transcription: 0.01uF
constexpr double hpConductance = 1. / 2700 + 5. / 120000 + 1. / 680000
    + 1. / 100000 + 1. / 39000 + 1. / 150000;

template<class Action> void rejected(Action action, const char* message)
{
    bool caught = false;
    try { action(); } catch (const std::invalid_argument&) { caught = true; }
    check(caught, message);
}

void scalarConvergence()
{
    const auto inputs = fixture();
    std::array<double, 16> initial{};
    initial[0] = 1;
    const double duration = 1e-4;
    const double expected = std::exp(-hpConductance * duration / capacitance);
    double previousError = 0;
    for (int steps : {10, 20, 40, 80}) {
        RankTransient model(inputs, initial);
        for (int step = 0; step < steps; ++step)
            model.step(duration / steps, inputs);
        const double error = std::abs(model.state().pinVolts[0] - expected);
        check(error > 0 && error < 0.001, "scalar analytic accuracy");
        if (previousError > 0)
            check(previousError / error > 3.7 && previousError / error < 4.3,
                  "scalar second-order refinement");
        previousError = error;
        for (int pin = 1; pin < 16; ++pin)
            near(model.state().pinVolts[pin], 0, 0, "ideal-source transient isolation");
    }
    // Variable partition follows the exact trapezoidal scalar amplification
    // product, but is not asserted equal to a different finite partition.
    RankTransient partitioned(inputs, initial);
    double product = 1;
    for (double seconds : {1e-6, 2e-6, 4e-6, 3e-6}) {
        const double ratio = seconds * hpConductance / (2 * capacitance);
        product *= (1 - ratio) / (1 + ratio);
        partitioned.step(seconds, inputs);
    }
    near(partitioned.state().pinVolts[0], product, 2e-14, "variable timestep partition");

    // Trapezoidal is A-stable, not L-stable: coarse steps can alternate sign.
    RankTransient coarse(inputs, initial);
    coarse.step(10 * capacitance / hpConductance, inputs);
    near(coarse.state().pinVolts[0], -2. / 3, 1e-14, "coarse stable alternating mode");
}

void forcingTests()
{
    auto inputs = fixture();
    std::array<double, 16> zero{};
    const double duration = 1e-4, seconds = duration / 200;
    const double rate = hpConductance / capacitance;
    RankTransient stepModel(inputs, zero);
    inputs.buses[2].volts = 5;
    stepModel.setForcing(inputs);
    for (double voltage : stepModel.state().pinVolts)
        near(voltage, 0, 0, "step event preserves capacitor voltage");
    near(stepModel.state().busTerminalVolts[2], 5, 0, "step updates algebraic terminal");
    for (int step = 0; step < 200; ++step)
        stepModel.step(seconds, inputs);
    const double finalVoltage = 5 / (39000 * hpConductance);
    near(stepModel.state().pinVolts[0], finalVoltage * (1 - std::exp(-rate * duration)),
         1e-6, "explicit step analytic response");

    // Ramp requires both endpoint forcings, including the stored previous one.
    inputs = fixture();
    RankTransient ramp(inputs, zero);
    const double slope = 50000; // fixture volts/second, not a hardware speed
    for (int step = 1; step <= 200; ++step) {
        inputs.buses[2].volts = slope * step * seconds;
        ramp.step(seconds, inputs);
    }
    const double expected = slope / (39000 * hpConductance)
        * (duration - (1 - std::exp(-rate * duration)) / rate);
    near(ramp.state().pinVolts[0], expected, 3e-7, "ramp endpoint-forcing analytic response");

    // Explicit equilibrium initial state is preserved at different step sizes.
    inputs.voices[2].local[4].volts = -3;
    const auto equilibrium = solveRank(inputs);
    RankTransient stationary(inputs, equilibrium.pinVolts);
    for (double interval : {1e-8, 1e-6, 1e-4, .01}) {
        stationary.step(interval, inputs);
        for (int pin = 0; pin < 16; ++pin)
            near(stationary.state().pinVolts[pin], equilibrium.pinVolts[pin], 1e-14,
                 "DC equilibrium preserved");
    }
}

void symmetricModes()
{
    auto inputs = fixture();
    for (auto& source : inputs.buses)
        source.ohms = 47000;
    // Independent symmetric2x2 reduction. Common-source resistance becomes8R
    // when all8 voice pairs have identical voltages. Local sources stay ideal.
    double a = 1. / 2700 + 4. / 120000 + 1. / 100000 + 1. / 150000;
    double d = 1. / 3300 + 4. / 39000 + 1. / 47000 + 1. / 150000;
    double b = 0;
    for (const auto& resistances : {std::array<double, 2>{120000, 39000},
                                    std::array<double, 2>{680000, 220000}}) {
        const double gh = 1 / resistances[0], gl = 1 / resistances[1];
        const double r = 8 * 47000;
        const double denominator = 1 + r * (gh + gl);
        a += gh * (1 + r * gl) / denominator;
        d += gl * (1 + r * gh) / denominator;
        b -= r * gh * gl / denominator;
    }
    a += 1. / (39000 + 8 * 47000);
    d += 1. / (39000 + 8 * 47000);
    const double discriminant = std::sqrt((a - d) * (a - d) + 4 * b * b);
    for (double eigenvalue : {(a + d - discriminant) / 2, (a + d + discriminant) / 2}) {
        const double lpInitial = (eigenvalue - a) / b;
        std::array<double, 16> initial{};
        for (int voice = 0; voice < 8; ++voice) {
            initial[2 * voice] = 1;
            initial[2 * voice + 1] = lpInitial;
        }
        const double duration = 5e-5;
        double previousError = 0;
        for (int steps : {10, 20, 40}) {
            RankTransient model(inputs, initial);
            for (int step = 0; step < steps; ++step)
                model.step(duration / steps, inputs);
            const double exact = std::exp(-eigenvalue * duration / capacitance);
            const double error = std::abs(model.state().pinVolts[0] - exact);
            if (previousError > 0)
                check(previousError / error > 3.8 && previousError / error < 4.2,
                      "symmetric analytic mode convergence");
            previousError = error;
            for (int voice = 0; voice < 8; ++voice) {
                near(model.state().pinVolts[2 * voice], model.state().pinVolts[0],
                     2e-14, "transient rank symmetry");
                near(model.state().pinVolts[2 * voice + 1],
                     lpInitial * model.state().pinVolts[0], 2e-13, "symmetric eigenvector retained");
            }
        }
    }
}

void chargeBalance()
{
    auto inputs = fixture();
    std::array<double, 16> initial{};
    for (int voice = 0; voice < 8; ++voice) {
        auto& parameters = inputs.voices[voice];
        for (int local = 0; local < 5; ++local)
            parameters.local[local] = {std::sin(voice + local), 1000. * (voice + local + 1)};
        parameters.hpReceiver = {1e-6, 2e-6};
        parameters.lpReceiver = {2e-6, -3e-6};
        initial[2 * voice] = .1 * voice;
        initial[2 * voice + 1] = -.05 * voice;
    }
    for (int bus = 0; bus < 4; ++bus)
        inputs.buses[bus] = {std::cos(bus), 20000. * (bus + 1)};
    RankTransient model(inputs, initial);
    for (int step = 0; step < 100; ++step) {
        const auto before = model.state();
        const auto previousCurrent = resistiveCurrent(inputs, before);
        for (int bus = 0; bus < 4; ++bus)
            inputs.buses[bus].volts = 5 * std::sin(step * .1 + bus);
        for (int voice = 0; voice < 8; ++voice) {
            inputs.voices[voice].local[4].volts = 3 * std::sin(step * .03 + voice);
            inputs.voices[voice].hpReceiver.amps = 2e-6 * std::cos(step + voice);
        }
        const double seconds = (step % 5 + 1) * 1e-6;
        const auto after = model.step(seconds, inputs);
        const auto nextCurrent = resistiveCurrent(inputs, after);
        for (int pin = 0; pin < 16; ++pin) {
            const double capacitorCurrent = capacitance * (after.pinVolts[pin] - before.pinVolts[pin]) / seconds;
            near(capacitorCurrent + (previousCurrent[pin] + nextCurrent[pin]) / 2,
                 0, 2e-15, "trapezoidal charge KCL");
        }
    }
}

void invalidInputs()
{
    auto inputs = fixture();
    inputs.buses[2].volts = 3;
    std::array<double, 16> zero{};
    zero[0] = .75;
    RankTransient model(inputs, zero), untouched(inputs, zero);
    rejected([&] { RankTransient bad(RankInputs{}, zero); }, "unknown initial boundaries");
    auto invalidVolts = zero;
    invalidVolts[3] = unknown;
    rejected([&] { RankTransient bad(inputs, invalidVolts); }, "unknown initial capacitor voltage");
    for (double seconds : {0., -1., unknown, std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::denorm_min()})
        rejected([&] { model.step(seconds, inputs); }, "invalid timestep");
    auto changed = inputs;
    changed.buses[0].ohms = 1;
    changed.buses[0].volts = 9;
    rejected([&] { model.step(1e-6, changed); }, "changed bus impedance rejected");
    rejected([&] { model.setForcing(changed); }, "event cannot reconfigure");
    changed = inputs;
    changed.voices[3].hpReceiver.siemens = 1e-6;
    rejected([&] { model.step(1e-6, changed); }, "changed receiver rejected");
    changed = inputs;
    changed.voices[5].lpGrade = Grade::B;
    rejected([&] { model.step(1e-6, changed); }, "changed grade rejected");
    changed = inputs;
    changed.voices[1].local[3].ohms = 1;
    rejected([&] { model.step(1e-6, changed); }, "changed local impedance rejected");
    changed = inputs;
    changed.voices[4].hpTrim.ohms = 1;
    rejected([&] { model.step(1e-6, changed); }, "changed trim impedance rejected");
    changed = inputs;
    changed.voices[2].local[1] = {};
    rejected([&] { model.step(1e-6, changed); }, "unknown next input rejected");
    // Rejection must preserve both capacitor state AND previous forcing history.
    changed = inputs;
    changed.buses[2].volts = 4;
    const auto recovered = model.step(2e-6, changed);
    const auto control = untouched.step(2e-6, changed);
    for (int pin = 0; pin < 16; ++pin)
        near(recovered.pinVolts[pin], control.pinVolts[pin], 0, "failure preserves state and history");
}
} // namespace

int main()
{
    try {
        scalarConvergence();
        forcingTests();
        symmetricModes();
        chargeBalance();
        invalidInputs();
        std::cout << checks << " loaded-control transient checks passed (conditional RC only)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
