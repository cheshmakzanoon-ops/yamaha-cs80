#pragma once

#include "experimental/MemoryControlFrame.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace cs80::experimental
{
static_assert(MemoryPanelPortMap::railCount == 3 && source_factory::voiceCount == 8,
              "Review memory feedback topology before changing rail/receiver counts");
struct MemoryReceiverInputs
{
    source_factory::RankDrives voices {};
    // Actual per-key KC/KB boundary volts, NOT the separate Hz/V pitch key CV.
    std::array<double, source_factory::voiceCount> keyboardCutoffVolts {}, keyboardLevelVolts {};
    double masterResonance = 1.0; // inverse bridge: 1 -> 0V, same nominal source law
};

struct MemoryDirectControls
{
    source_factory::WscControls wsc;
    double dutyProxy = 0.5, pwmRateHz = 0.05, pwmDepth = 0.0, noiseGain = 0.0;
    double filterPathGain = 0.0, sinePathGain = 0.0;
    source_factory::FilterEnvelope filterEnvelope;
    source_factory::AmplitudeEnvelope amplitudeEnvelope;
    std::uint32_t clippedBusRows = 0;
};

struct MemoryReceiverReadout
{
    MemoryPanelPortMap::Voltages busVolts {};
    MemoryDirectControls direct;
    source_factory::CutoffReadout cutoff;
    std::array<std::array<double, 2>, source_factory::voiceCount> qVolts {}, q {};
    std::array<double, source_factory::voiceCount> levelPinVolts {}, levelGain {};
    std::array<std::string_view, 4> touchReceiverIds {};
    // residualAmps certifies the LOCAL LINEAR receiver closure only. It is not
    // the nonlinear source KCL residual or a bound on actual circuit error.
    double masterResVolts = 0.0, feedbackNorm = 0.0, residualAmps = 0.0, maxDeltaAmps = 0.0;
    bool masterResSaturated = false;
};

namespace memory_receiver_detail
{
inline constexpr double maximumFeedbackNorm = 0.5;
inline constexpr double maximumResidualAmps = 1e-12;
inline constexpr double gc = 1.0 / 39000.0, gl = 1.0 / 560000.0;

inline MemoryDirectControls decode(const MemoryPanelPortMap::Voltages& bus,
                                   memory_panels::Switches switches) noexcept
{
    namespace sf = source_factory;
    MemoryDirectControls out;
    for (std::size_t i = 0; i < bus.size(); ++i)
        if (!MemoryPanelPortMap::digital(i) && (bus[i] < 0.0 || bus[i] > 10.0)) out.clippedBusRows |= 1u << i;
    out.wsc.sawEnabled = switches.saw; out.wsc.pulseEnabled = switches.pulse;
    out.dutyProxy = 0.5 + 0.4 * sf::unit(bus[2]);
    out.wsc.pulseThresholdVolts = sf::nominalWscHalfSpanVolts * (1.0 - 2.0 * out.dutyProxy);
    out.pwmRateHz = 0.05 * std::pow(400.0, sf::unit(bus[0]));
    out.pwmDepth = sf::unit(bus[1]); out.noiseGain = sf::unit(bus[5]);
    out.filterEnvelope = {-5.0 * sf::unit(bus[10]), 5.0 * sf::unit(bus[11]), 0.0,
        sf::attackSeconds(bus[12]), sf::decayReleaseSeconds(bus[13]), sf::decayReleaseSeconds(bus[14])};
    out.filterPathGain = sf::unit(bus[15]); out.sinePathGain = sf::unit(bus[16]);
    out.amplitudeEnvelope = {0.0, 1.0, sf::unit(bus[19]),
        sf::attackSeconds(bus[17]), sf::decayReleaseSeconds(bus[18]), sf::decayReleaseSeconds(bus[20])};
    return out;
}
} // namespace memory_receiver_detail

// Simultaneous local-linear loaded receiver evaluation, not a nonlinear circuit
// solve or shipping Engine route. All receiver loads are ALREADY in frame.ports.
// Only injection currents are added. No allocation, locks, or open-ended loops.
// Unsupported/ill-conditioned/nonfinite results preserve the caller's output.
inline bool evaluateMemoryReceivers(const MemoryControlFrame& frame, const MemoryReceiverInputs& input,
                                   MemoryReceiverReadout& result) noexcept
{
    namespace sf = source_factory;
    namespace detail = memory_receiver_detail;
    const auto& map = frame.ports;
    if (!frame.ready || !map.ready || frame.frameVersion != memoryControlFrameVersion
        || map.rank < 0 || map.rank > 1 || map.selector < 11 || map.selector > 13
        || frame.adapterVersion != memory_control::version || frame.sourceModelVersion != sf::modelVersion
        || frame.sourceHash != sf::sourceFingerprint || !std::isfinite(input.masterResonance)) return false;
    MemoryReceiverReadout next;
    const double master = std::clamp(input.masterResonance, 0.0, 1.0);
    next.masterResSaturated = master != input.masterResonance;
    next.masterResVolts = 10.0 * (1.0 - master);
    next.touchReceiverIds = sf::touchReceiverIds[static_cast<std::size_t>(map.rank)];
    sf::RankDrives drives {};
    double velocitySum = 0.0, pressureSum = 0.0, envelopeSum = 0.0, kcSum = 0.0, kbSum = 0.0;
    for (std::size_t voice = 0; voice < drives.size(); ++voice)
    {
        bool saturated = false;
        if (!sf::sanitizeDrive(input.voices[voice], drives[voice], saturated)
            || !std::isfinite(input.keyboardCutoffVolts[voice]) || !std::isfinite(input.keyboardLevelVolts[voice])) return false;
        if (saturated) next.cutoff.saturatedVoices |= 1u << voice;
        velocitySum += drives[voice].velocity; pressureSum += drives[voice].pressure;
        envelopeSum += sf::envelopeVoltsFromState(drives[voice].filterEnvelopeZ);
        kcSum += input.keyboardCutoffVolts[voice]; kbSum += input.keyboardLevelVolts[voice];
    }
    const std::array<sf::CutoffSideMap, 2> sides {{
        sf::compileCutoffSide({0.0, 0.0}, sf::Grade::B, true),
        sf::compileCutoffSide({0.0, 0.0}, sf::Grade::B, false)}};
    MemoryPanelPortMap::Currents independent {};
    for (std::size_t side = 0; side < sides.size(); ++side)
    {
        const auto& m = sides[side];
        independent[6 + 2 * side] = detail::gc * (m.pinPerTouch * kcSum + m.pinPerEnvelopeVolt * envelopeSum);
        independent[7 + 2 * side] = (sf::voiceCount / 440000.0) * next.masterResVolts;
    }
    independent[21] = detail::gl * sf::levelPinPerTouch * kbSum;
    // A depth row has no receiver injection here. Its offset includes any
    // nonzero reference current at which the source map was prepared.
    const auto depthBase = [&](std::size_t row) {
        return map.operatingPoint[row] - map.localOhms[row] * map.referenceCurrents[row];
    };
    MemoryPanelPortMap::Currents constantDelta {};
    std::array<MemoryPanelPortMap::Currents, 3> injectionPerRail {};
    for (std::size_t row = 0; row < constantDelta.size(); ++row) constantDelta[row] = independent[row] - map.referenceCurrents[row];
    for (std::size_t side = 0; side < sides.size(); ++side)
    {
        const auto row = 6 + 2 * side;
        const double gain = detail::gc * sides[side].pinPerTouch;
        constantDelta[row] += gain * (velocitySum * depthBase(22) + pressureSum * depthBase(24));
        for (std::size_t rail = 0; rail < 3; ++rail)
            injectionPerRail[rail][row] = gain * (velocitySum * map.railToBus[rail][22] + pressureSum * map.railToBus[rail][24]);
    }
    const double levelGain = detail::gl * sf::levelPinPerTouch;
    constantDelta[21] += levelGain * (velocitySum * depthBase(23) + pressureSum * depthBase(25));
    for (std::size_t rail = 0; rail < 3; ++rail)
        injectionPerRail[rail][21] = levelGain * (velocitySum * map.railToBus[rail][23] + pressureSum * map.railToBus[rail][25]);

    // [I - currentToRail * injectionPerRail] rail = currentToRail * constantDelta.
    // Norm bound guarantees nonsingularity and an inverse norm < 2 in these
    // coordinates. It is a numerical guard, not a physical excursion bound.
    std::array<std::array<double, 4>, 3> augmented {};
    for (std::size_t rail = 0; rail < 3; ++rail)
    {
        double norm = 0.0;
        for (std::size_t col = 0; col < 3; ++col)
        {
            double feedback = 0.0;
            for (std::size_t row = 0; row < constantDelta.size(); ++row)
                feedback += map.currentToRail[rail][row] * injectionPerRail[col][row];
            norm += std::abs(feedback);
            augmented[rail][col] = (rail == col ? 1.0 : 0.0) - feedback;
        }
        if (!std::isfinite(norm) || norm >= detail::maximumFeedbackNorm) return false;
        next.feedbackNorm = std::max(next.feedbackNorm, norm);
        for (std::size_t row = 0; row < constantDelta.size(); ++row)
            augmented[rail][3] += map.currentToRail[rail][row] * constantDelta[row];
    }
    for (std::size_t col = 0; col < 3; ++col)
    {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < 3; ++row)
            if (std::abs(augmented[row][col]) > std::abs(augmented[pivot][col])) pivot = row;
        std::swap(augmented[pivot], augmented[col]);
        const double d = augmented[col][col];
        if (!std::isfinite(d) || std::abs(d) < 1e-12) return false;
        for (double& value : augmented[col]) value /= d;
        for (std::size_t row = 0; row < 3; ++row)
        {
            if (row == col) continue;
            const double factor = augmented[row][col];
            for (std::size_t k = 0; k < 4; ++k) augmented[row][k] -= factor * augmented[col][k];
        }
    }
    auto delta = constantDelta;
    for (std::size_t row = 0; row < delta.size(); ++row)
        for (std::size_t rail = 0; rail < 3; ++rail) delta[row] += injectionPerRail[rail][row] * augmented[rail][3];
    if (!map.evaluate(delta, next.busVolts)) return false;
    for (std::size_t row = 0; row < next.busVolts.size(); ++row)
        if (!std::isfinite(next.busVolts[row]) || (MemoryPanelPortMap::digital(row) && next.busVolts[row] != 0.0)) return false;

    auto actualInjection = independent;
    const double cutoffTouch = velocitySum * next.busVolts[22] + pressureSum * next.busVolts[24];
    for (std::size_t side = 0; side < sides.size(); ++side)
        actualInjection[6 + 2 * side] += detail::gc * sides[side].pinPerTouch * cutoffTouch;
    actualInjection[21] += levelGain * (velocitySum * next.busVolts[23] + pressureSum * next.busVolts[25]);
    for (std::size_t row = 0; row < delta.size(); ++row)
    {
        const double residual = std::abs(actualInjection[row] - map.referenceCurrents[row] - delta[row]);
        if (!std::isfinite(residual) || residual > detail::maximumResidualAmps) return false;
        next.residualAmps = std::max(next.residualAmps, residual);
        next.maxDeltaAmps = std::max(next.maxDeltaAmps, std::abs(delta[row]));
    }
    for (std::size_t side = 0; side < sides.size(); ++side)
        next.cutoff.loadedBusVolts[side] = next.busVolts[6 + 2 * side];
    for (std::size_t voice = 0; voice < drives.size(); ++voice)
    {
        const auto& d = drives[voice];
        const double touch = next.busVolts[22] * d.velocity + next.busVolts[24] * d.pressure + input.keyboardCutoffVolts[voice];
        const double levelTouch = next.busVolts[23] * d.velocity + next.busVolts[25] * d.pressure + input.keyboardLevelVolts[voice];
        for (std::size_t side = 0; side < sides.size(); ++side)
        {
            const auto& m = sides[side];
            const double pin = m.pinPerLoadedBus * next.cutoff.loadedBusVolts[side]
                + m.pinPerTouch * touch + m.pinPerEnvelopeVolt * sf::envelopeVoltsFromState(d.filterEnvelopeZ);
            const double cv = m.effectiveCvOffset + m.effectiveCvPerPinVolt * (pin - m.referencePinVolts);
            const double qv = 0.5 * (next.busVolts[7 + 2 * side] + next.masterResVolts);
            if (!std::isfinite(pin) || !std::isfinite(cv) || !std::isfinite(qv)) return false;
            next.cutoff.pinVolts[voice][side] = pin; next.cutoff.effectiveCv[voice][side] = cv;
            next.qVolts[voice][side] = qv; next.q[voice][side] = sf::qFromVolts(qv);
        }
        const double pin = sf::levelPinPerLoadedBus * next.busVolts[21] + sf::levelPinPerTouch * levelTouch;
        const double gain = pin / sf::levelUnitVolts;
        if (!std::isfinite(pin) || !std::isfinite(gain)) return false;
        next.levelPinVolts[voice] = pin; next.levelGain[voice] = gain;
    }
    next.direct = detail::decode(next.busVolts, frame.selectedSwitches);
    result = next;
    return true;
}
} // namespace cs80::experimental
