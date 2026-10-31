#pragma once
// Realtime headphone-amplifier listening model (IMPROVEMENT_PLAN Phase 3,
// output). Held to src/reference/HeadphoneAmp.h, which carries the source
// account, the inferred choices and their alternatives. Model units, not volts:
// the input is the engine's ideal LEFT/RIGHT preamp-source sample (HL/HR are
// taken before the output coupling capacitor and the HIGH/LOW switch).
#include <algorithm>
#include <cmath>

namespace cs80
{
class HeadphoneAmpCircuit
{
public:
    enum class Coupling { direct, capacitor };
    struct Profile
    {
        double inputOhms = 100.0e3, feedbackOhms = 150.0e3, outputOhms = 22.0; // SUB29
        double couplingFarads = 470.0e-6, biasOhms = 10.0e3;                   // PN5 alternative
        Coupling coupling = Coupling::direct;                                  // INFERRED revision
        double loadOhms = 32.0;                                                // INFERRED headphone
        double ceiling = 1.0, knee = 0.891250938;                              // INFERRED rail law
    };

    HeadphoneAmpCircuit() noexcept { prepare(96000.0, Profile {}); }

    void prepare(double sampleRate, const Profile& p) noexcept
    {
        profile = p;
        rate = sampleRate > 0.0 ? sampleRate : 96000.0;
        gain = -p.feedbackOhms / p.inputOhms;
        shunt = p.coupling == Coupling::capacitor ? p.loadOhms * p.biasOhms / (p.loadOhms + p.biasOhms) : p.loadOhms;
        // Trapezoidal series capacitor: tau = C (Ro + shunt).
        beta = 1.0 / (2.0 * p.couplingFarads * (p.outputOhms + shunt) * rate);
        reset();
    }
    void reset() noexcept { capacitorVolts = previousNode = 0.0; }

    // One internal sample: jack voltage across the headphone load.
    double process(double preampSource) noexcept
    {
        const double node = limit(gain * preampSource);
        if (profile.coupling == Coupling::direct)
            return node * shunt / (shunt + profile.outputOhms);
        // v_c' = (node - v_c) / (C R): trapezoid, then the current through R.
        capacitorVolts = (capacitorVolts * (1.0 - beta) + beta * (node + previousNode)) / (1.0 + beta);
        previousNode = node;
        return (node - capacitorVolts) * shunt / (shunt + profile.outputOhms);
    }

    const Profile& getProfile() const noexcept { return profile; }
    double getCapacitorVolts() const noexcept { return capacitorVolts; }

private:
    double limit(double x) const noexcept
    {
        const double m = std::abs(x), k = profile.knee * profile.ceiling;
        if (!(m > k)) return x;
        const double span = profile.ceiling - k;
        return std::copysign(k + span * std::tanh((m - k) / span), x);
    }
    Profile profile {};
    double rate = 96000.0, gain = -1.5, shunt = 32.0, beta = 0.0;
    double capacitorVolts = 0.0, previousNode = 0.0;
};
} // namespace cs80
