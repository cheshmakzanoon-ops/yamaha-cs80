#pragma once

#include "FactoryToneHypotheses.h"
#include <array>
#include <cstdint>
#include <string_view>

// Engineering estimate only. HardwareControl and the frozen V1 median manifest
// remain unchanged. No row becomes a known installed component or serial range.
// Selection was preregistered before evaluation; see PUBLIC_TONE_ESTIMATES_PREREG_20261102.md.
namespace cs80::public_tone_estimates {
inline constexpr std::uint32_t version = 2;
inline constexpr std::string_view policyId = "public-amended-marks-v2";
enum class Basis : std::uint32_t { amendedResistor = 1, drawnDirect = 2, pairedBoard = 3 };
struct Estimate {
    factory_hypotheses::CellAddress address;
    std::uint32_t ohms; // zero denotes a DIRECT topology estimate, not a recovered 0-ohm part
    Basis basis;
    constexpr double excitation() const noexcept { return 10000.0 / (10000.0 + ohms); }
    constexpr std::string_view confidence() const noexcept {
        return basis == Basis::drawnDirect ? "low" : "moderate";
    }
};
inline constexpr std::array<Estimate, 25> cells {{
    {{51,'A',20},1200,Basis::amendedResistor}, {{51,'A',25},0,Basis::drawnDirect},
    {{51,'C',25},0,Basis::drawnDirect}, {{51,'E',22},12000,Basis::amendedResistor},
    {{51,'E',26},0,Basis::drawnDirect}, {{51,'F',22},12000,Basis::amendedResistor},
    {{51,'F',25},0,Basis::drawnDirect}, {{51,'F',26},0,Basis::drawnDirect},
    {{52,'A',26},0,Basis::drawnDirect}, {{52,'B',9},10000,Basis::amendedResistor},
    {{52,'C',22},12000,Basis::amendedResistor}, {{52,'E',20},1200,Basis::amendedResistor},
    {{53,'A',20},1200,Basis::amendedResistor}, {{53,'A',25},0,Basis::drawnDirect},
    {{53,'B',14},75000,Basis::pairedBoard}, {{53,'B',20},1200,Basis::amendedResistor},
    {{53,'C',20},1200,Basis::amendedResistor}, {{53,'E',23},22000,Basis::amendedResistor},
    {{53,'E',26},0,Basis::drawnDirect}, {{53,'F',22},12000,Basis::amendedResistor},
    {{53,'F',26},0,Basis::drawnDirect}, {{54,'A',26},0,Basis::drawnDirect},
    {{54,'C',22},12000,Basis::amendedResistor}, {{54,'C',25},0,Basis::drawnDirect},
    {{54,'C',26},0,Basis::drawnDirect}
}};
constexpr bool manifestValid() noexcept {
    if (cells.size() != factory_hypotheses::cells.size()) return false;
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto& c = cells[i];
        if (factory_hypotheses::findIndex(c.address) == factory_hypotheses::cells.size()
            || hardware::findBranch(c.address.board,c.address.column,c.address.output).kind != hardware::BranchKind::unknown
            || (c.basis == Basis::drawnDirect) != (c.ohms == 0)) return false;
        for (std::size_t j = i + 1; j < cells.size(); ++j)
            if (c.address == cells[j].address) return false;
    }
    return true;
}
constexpr std::uint64_t manifestFingerprint() noexcept {
    std::uint64_t h = 14695981039346656037ull;
    const auto add = [&h](std::uint64_t n) constexpr {
        for (int i = 0; i < 8; ++i) { h ^= n & 255u; h *= 1099511628211ull; n >>= 8; }
    };
    add(version);
    for (char c : policyId) add(static_cast<unsigned char>(c));
    for (const auto& c : cells) {
        add(c.address.board); add(c.address.column); add(c.address.output);
        add(c.ohms); add(static_cast<std::uint32_t>(c.basis));
    }
    return h;
}
inline constexpr std::uint64_t fingerprint = manifestFingerprint();
static_assert(manifestValid(), "Public estimates must cover exactly the 25 excluded addresses");
static_assert(fingerprint == 0x832eee6f44f5cfccull,
              "Public estimate choices changed: review and version the policy");
} // namespace cs80::public_tone_estimates
