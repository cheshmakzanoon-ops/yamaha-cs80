#pragma once
#include "LoadedControlRank.h"

namespace cs80::reference::loaded_control {
// Conditional 16-node RC experiment, not an IC dynamic model. The only stored
// charge is in the documented 0.01uF grounded cutoff-node capacitors.
// See docs/LOADED_CONTROL_EVIDENCE.md for boundary assumptions and provenance.
class RankTransient {
public:
    // Initial voltages are explicit (not silently reset to zero or to DC).
    RankTransient(const RankInputs& initialInputs, const std::array<double, 16>& initialVolts)
        : configuration_(initialInputs)
    {
        for (double voltage : initialVolts)
            require(std::isfinite(voltage));
        const auto system = rank_detail::assemble(initialInputs);
        matrix_ = system.matrix;
        forcing_ = system.rhs;
        state_ = rank_detail::resultAt(initialInputs, initialVolts);
    }

    const RankResult& state() const { return state_; }

    // Endpoint trapezoidal forcing: previous and next source voltages/current
    // offsets contribute. This does not mean a discontinuous step occurs at
    // the start of the interval. Use setForcing() for that explicit event.
    RankResult step(double seconds, const RankInputs& nextInputs)
    {
        require(std::isfinite(seconds) && seconds > 0);
        const auto nextSystem = checkedSystem(nextInputs);
        const double chargeConductance = nodeFarads / seconds;
        require(std::isfinite(chargeConductance) && chargeConductance > 0);
        rank_detail::Matrix left{};
        rank_detail::Vector right{};
        for (std::size_t row = 0; row < 16; ++row) {
            right[row] = chargeConductance * state_.pinVolts[row]
                + (forcing_[row] + nextSystem.rhs[row]) / 2;
            for (std::size_t column = 0; column < 16; ++column) {
                left[row][column] = matrix_[row][column] / 2;
                right[row] -= matrix_[row][column] * state_.pinVolts[column] / 2;
            }
            left[row][row] += chargeConductance;
        }
        // Form the complete candidate before changing any observable state.
        const auto candidate = rank_detail::resultAt(nextInputs,
            rank_detail::solvePositive(left, right));
        forcing_ = nextSystem.rhs;
        state_ = candidate;
        return state_;
    }

    // Zero-duration source/current-offset event: capacitor voltages continuous,
    // algebraic source terminals and the next interval's starting forcing change.
    // Resistances, conductances and grades cannot change through this method.
    RankResult setForcing(const RankInputs& inputs)
    {
        const auto system = checkedSystem(inputs);
        const auto candidate = rank_detail::resultAt(inputs, state_.pinVolts);
        forcing_ = system.rhs;
        state_ = candidate;
        return state_;
    }

private:
    rank_detail::System checkedSystem(const RankInputs& inputs) const
    {
        // Validate every source/receiver first. Unknown inputs never become
        // topology comparisons against NaN or implicit ideal/open boundaries.
        const auto system = rank_detail::assemble(inputs);
        for (std::size_t bus = 0; bus < 4; ++bus)
            require(inputs.buses[bus].ohms == configuration_.buses[bus].ohms);
        for (std::size_t index = 0; index < 8; ++index) {
            const auto& next = inputs.voices[index];
            const auto& fixed = configuration_.voices[index];
            for (std::size_t local = 0; local < 5; ++local)
                require(next.local[local].ohms == fixed.local[local].ohms);
            require(next.hpTrim.ohms == fixed.hpTrim.ohms && next.lpTrim.ohms == fixed.lpTrim.ohms);
            require(next.hpReceiver.siemens == fixed.hpReceiver.siemens
                 && next.lpReceiver.siemens == fixed.lpReceiver.siemens);
            require(next.hpGrade == fixed.hpGrade && next.lpGrade == fixed.lpGrade);
        }
        return system;
    }

    // Reconfigure by constructing a new instance with explicitly chosen initial
    // voltages. No hidden capacitor-state/forcing reset is supplied.
    RankInputs configuration_;
    rank_detail::Matrix matrix_{};
    rank_detail::Vector forcing_{};
    RankResult state_{};
};
} // namespace cs80::reference::loaded_control
