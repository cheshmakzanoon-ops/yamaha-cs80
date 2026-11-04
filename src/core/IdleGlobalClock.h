#pragma once

// CPU-only decision at the global PWM readout, not an alternate oscillator.
// Every PWM phase must still take Engine's original float += and floor step on
// EVERY internal sample, including silence. Skipping this sine is safe only
// when no rank will consume it: source-policy idle ranks read it on their
// staggered ~3 kHz idle steps, while an active rank reads every sample.
// Legacy always takes the historical sine path, unchanged.
namespace cs80::idle_global_clock
{

[[nodiscard]] constexpr bool shouldReadPwm(bool sourcePolicy, bool anyActive,
                                            bool idleStepDue) noexcept
{
    return ! sourcePolicy || anyActive || idleStepDue;
}

} // namespace cs80::idle_global_clock
