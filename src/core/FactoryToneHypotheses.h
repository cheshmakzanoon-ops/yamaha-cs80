#pragma once

#include "HardwareControl.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

// Explicit engineering hypotheses, separate from immutable classified evidence.
// No entry is a recovered resistor or an amendment of HardwareControl.h.
// docs/FACTORY_TONE_INTEGRATION_PROPOSAL.md section4 defines this policy.
namespace cs80::factory_hypotheses {
inline constexpr std::uint32_t version = 1;
enum class Policy : std::uint32_t { sameRankOutputMedian = 1 };
inline constexpr std::string_view policyId = "same-rank-same-output-median-excitation-v1";
inline constexpr std::string_view uncertaintyDomain =
    "nominal passive excitation x in [0,1]; not a statistical confidence interval";
struct CellAddress {
    int board = 0;
    char column = '?';
    int output = 0;
    constexpr bool operator==(const CellAddress&) const = default;
};
struct BasisCell {
    CellAddress address;
    hardware::Branch classifiedBranch;
    double excitation = 0.0;
};
struct VisibleAlternative {
    double excitation = 0.0;
    std::string_view label;
};
struct InferredCell {
    CellAddress address;
    Policy policy = Policy::sameRankOutputMedian;
    double nominalExcitation = 0.0;
    double nominalDriveVolts = 0.0, nominalSourceOhms = 0.0;
    double minimumExcitation = 0.0, maximumExcitation = 1.0;
    std::string_view sourceExclusionReason;
    std::array<BasisCell, 11> basis {};
    std::size_t basisCount = 0;
    // Reading/revision alternatives are NOT votes in the accepted-cell median.
    // Labels report visible markings, never claims about installed components.
    std::array<VisibleAlternative, 2> visibleAlternatives {};
    std::size_t visibleAlternativeCount = 0;
};
constexpr int rankOf(int board) noexcept {
    return board == 51 || board == 52 ? 0 : board == 53 || board == 54 ? 1 : -1;
}
constexpr std::optional<double> acceptedExcitation(hardware::Branch branch) noexcept {
    if (branch.kind == hardware::BranchKind::open) return 0.0;
    if (branch.kind == hardware::BranchKind::direct) return 1.0;
    if (branch.kind == hardware::BranchKind::resistor && branch.resistanceOhms() > 0.0)
        return 10000.0 / (10000.0 + branch.resistanceOhms());
    return std::nullopt; // unknown never silently becomes an open/zero vote
}
constexpr VisibleAlternative resistorMark(double ohms, std::string_view label) noexcept {
    return {10000.0 / (10000.0 + ohms), label};
}
constexpr VisibleAlternative diodeMark(std::string_view label) noexcept { return {1.0, label}; }
constexpr InferredCell medianCell(CellAddress address, std::string_view reason,
                                 VisibleAlternative first = {}, VisibleAlternative second = {}) noexcept {
    InferredCell result;
    result.address = address;
    result.sourceExclusionReason = reason;
    result.visibleAlternatives = {first,second};
    result.visibleAlternativeCount = !second.label.empty() ? 2 : !first.label.empty() ? 1 : 0;
    std::array<double, 11> sorted {};
    const int rank = rankOf(address.board);
    for (int selector = 0; selector < 11; ++selector) {
        const int board = 51 + 2 * rank + (selector >= 6 ? 1 : 0);
        const char column = static_cast<char>('A' + (selector >= 6 ? selector - 6 : selector));
        const auto branch = hardware::findBranch(board,column,address.output);
        const auto accepted = acceptedExcitation(branch);
        if (!accepted) continue;
        const double x = *accepted;
        result.basis[result.basisCount] = {{board,column,address.output},branch,x};
        std::size_t at = result.basisCount++;
        while (at > 0 && sorted[at-1] > x) { sorted[at] = sorted[at-1]; --at; }
        sorted[at] = x;
    }
    if (result.basisCount != 0) {
        const std::size_t middle = result.basisCount / 2;
        result.nominalExcitation = result.basisCount % 2 != 0 ? sorted[middle]
            : 0.5 * (sorted[middle-1] + sorted[middle]);
    }
    result.nominalDriveVolts = 10.0 * result.nominalExcitation;
    result.nominalSourceOhms = 10000.0 * (1.0 - result.nominalExcitation);
    return result;
}
// Frozen at compile time from classified same-rank/output bases only. Canonical
// address iteration makes historical source-table ordering irrelevant.
inline constexpr std::array<InferredCell, 25> cells {{
    medianCell({51,'A',20}, "1.2K revision applicability", resistorMark(1200,"visible 1.2K revision candidate")),
    medianCell({51,'A',25}, "overdrawn diode", diodeMark("overdrawn diode/direct candidate")),
    medianCell({51,'C',25}, "marked10K/bypass topology", resistorMark(10000,"marked 10K candidate"), diodeMark("marked bypass/direct candidate")),
    medianCell({51,'E',22}, "12K revision applicability", resistorMark(12000,"visible 12K revision candidate")),
    medianCell({51,'E',26}, "marked/overdrawn diode", diodeMark("marked diode/direct candidate")),
    medianCell({51,'F',22}, "overwritten3.6K/12K", resistorMark(3600,"overwritten 3.6K candidate"), resistorMark(12000,"overwritten 12K candidate")),
    medianCell({51,'F',25}, "overdrawn5.6K/diode", resistorMark(5600,"overdrawn 5.6K candidate"), diodeMark("overdrawn diode/direct candidate")),
    medianCell({51,'F',26}, "marked diode", diodeMark("marked diode/direct candidate")),
    medianCell({52,'A',26}, "marked/overdrawn diode", diodeMark("marked diode/direct candidate")),
    medianCell({52,'B',9}, "crossed12K/10K", resistorMark(12000,"crossed 12K candidate"), resistorMark(10000,"crossed 10K candidate")),
    medianCell({52,'C',22}, "overwritten former value/12K", resistorMark(12000,"visible 12K candidate; former value unresolved")),
    medianCell({52,'E',20}, "1.2K revision applicability", resistorMark(1200,"visible 1.2K revision candidate")),
    medianCell({53,'A',20}, "1.2K revision applicability", resistorMark(1200,"visible 1.2K revision candidate")),
    medianCell({53,'A',25}, "overdrawn diode", diodeMark("overdrawn diode/direct candidate")),
    medianCell({53,'B',14}, "uncertain25K-looking glyph; readability gate", resistorMark(25000,"25K-looking reading hypothesis; glyph NOT resolved")),
    medianCell({53,'B',20}, "1.2K revision applicability", resistorMark(1200,"visible 1.2K revision candidate")),
    medianCell({53,'C',20}, "1.2K revision applicability", resistorMark(1200,"visible 1.2K revision candidate")),
    medianCell({53,'E',23}, "crossed16K/22K", resistorMark(16000,"crossed 16K candidate"), resistorMark(22000,"crossed 22K candidate")),
    medianCell({53,'E',26}, "crossed diode", diodeMark("crossed diode/direct candidate")),
    medianCell({53,'F',22}, "12K revision applicability", resistorMark(12000,"visible 12K revision candidate")),
    medianCell({53,'F',26}, "crossed diode", diodeMark("crossed diode/direct candidate")),
    medianCell({54,'A',26}, "crossed diode", diodeMark("crossed diode/direct candidate")),
    medianCell({54,'C',22}, "crossed8.2K/12K", resistorMark(8200,"crossed 8.2K candidate"), resistorMark(12000,"crossed 12K candidate")),
    medianCell({54,'C',25}, "crossed56K/diode", resistorMark(56000,"crossed 56K candidate"), diodeMark("crossed diode/direct candidate")),
    medianCell({54,'C',26}, "crossed diode", diodeMark("crossed diode/direct candidate"))
}};
constexpr const InferredCell* find(CellAddress address) noexcept {
    for (const auto& cell : cells) if (cell.address == address) return &cell;
    return nullptr;
}
constexpr const InferredCell* find(int board, char column, int output) noexcept {
    return find({board,column,output});
}
constexpr bool manifestValid() noexcept {
    for (std::size_t i=0; i<cells.size(); ++i) {
        const auto& cell=cells[i];
        if (hardware::findBranch(cell.address.board,cell.address.column,cell.address.output).kind
            != hardware::BranchKind::unknown || cell.basisCount == 0 || cell.basisCount > 11
            || cell.nominalExcitation < 0 || cell.nominalExcitation > 1
            || cell.sourceExclusionReason.empty() || cell.address.output == 4 || cell.address.output == 5) return false;
        for (std::size_t j=i+1; j<cells.size(); ++j) if (cells[j].address == cell.address) return false;
        for (std::size_t j=0; j<cell.basisCount; ++j)
            if (rankOf(cell.basis[j].address.board) != rankOf(cell.address.board)
                || cell.basis[j].address.output != cell.address.output
                || cell.basis[j].classifiedBranch.kind == hardware::BranchKind::unknown) return false;
    }
    std::size_t unknownCount=0;
    for (const auto& column:hardware::verifiedColumns) for (int output=1;output<=26;++output)
        if (hardware::findBranch(column.board,column.column,output).kind == hardware::BranchKind::unknown) {
            bool present=false;
            for(const auto& cell:cells)
                if(cell.address == CellAddress{column.board,column.column,output}) present=true;
            if (!present) return false;
            ++unknownCount;
        }
    return unknownCount == cells.size();
}
static_assert(manifestValid(), "Inference overlay must cover exactly the25 unresolved source addresses");
constexpr std::uint64_t manifestFingerprint() noexcept {
    std::uint64_t hash=14695981039346656037ull;
    const auto add=[&](std::uint64_t n) constexpr {
        for (int byte=0;byte<8;++byte) { hash^=n&255u;hash*=1099511628211ull;n>>=8; }
    };
    const auto string=[&](std::string_view s) constexpr {
        add(s.size());for(char c:s)add(static_cast<unsigned char>(c));
    };
    add(version);string(policyId);string(uncertaintyDomain);
    for(const auto& cell:cells) {
        add(cell.address.board);add(cell.address.column);add(cell.address.output);add(static_cast<std::uint32_t>(cell.policy));
        add(static_cast<std::uint64_t>(cell.nominalExcitation*1e12+.5));
        add(static_cast<std::uint64_t>(cell.nominalDriveVolts*1e9+.5));
        add(static_cast<std::uint64_t>(cell.nominalSourceOhms*1e9+.5));
        add(static_cast<std::uint64_t>(cell.minimumExcitation*1e12+.5));
        add(static_cast<std::uint64_t>(cell.maximumExcitation*1e12+.5));
        string(cell.sourceExclusionReason);add(cell.basisCount);
        for(std::size_t j=0;j<cell.basisCount;++j) {
            const auto& b=cell.basis[j];add(b.address.board);add(b.address.column);add(b.address.output);
            add(static_cast<std::uint32_t>(b.classifiedBranch.kind));add(static_cast<std::uint32_t>(b.classifiedBranch.sourceUnit));
            add(static_cast<std::uint64_t>(b.classifiedBranch.sourceValue*1e6+.5));
            add(static_cast<std::uint64_t>(b.excitation*1e12+.5));
        }
        add(cell.visibleAlternativeCount);
        for(std::size_t j=0;j<cell.visibleAlternativeCount;++j) {
            add(static_cast<std::uint64_t>(cell.visibleAlternatives[j].excitation*1e12+.5));string(cell.visibleAlternatives[j].label);
        }
    }
    return hash;
}
inline constexpr std::uint64_t fingerprint=manifestFingerprint();
static_assert(fingerprint == 0xe5f877a025fc52c6ull,
              "Hypothesis manifest/basis changed: review and version the overlay");

// ---------------------------------------------------------------------------
// Re-examination of the 25 cells (ws6-library, 2026-10-31). This record does
// NOT amend the frozen v1 manifest above, HardwareControl.h, or any audio: every
// cell stays an unknown source cell with its labelled median hypothesis. New
// native crops could not be fetched in this session (service-manual hosts and
// the Actions log store refused the proxy tunnel; see
// docs/workstreams/ws6-library.md), so the outcome rests on the recorded native
// inspections: schematic pages S31/S33/S35/S37 (T51..T54 circuit diagrams,
// Actions run 40790769942) and board pages S32/S34/S36/S38 (run 40801679102).
// candidate is the reading used by the "visible candidates" sensitivity
// scenario: the first visible alternative listed for the cell (a convention,
// not a preference), or for T53/B/14 the board-artwork reading.
enum class ReviewOutcome : std::uint32_t {
    heldRevisionMark = 1,   // legible value beside a revision mark / crossed or overdrawn part
    heldReadability = 2,    // glyph not resolvable on the schematic
    boardCandidate = 3      // schematic unresolved, board artwork supplies a candidate
};
struct CellReview {
    CellAddress address;
    std::string_view schematicPage, boardPage;
    ReviewOutcome outcome = ReviewOutcome::heldRevisionMark;
    VisibleAlternative candidate;
    std::string_view note;
};
inline constexpr std::string_view reviewId = "ws6-cell-review-2026-10-31";
constexpr CellReview firstAlternative(CellAddress a, std::string_view schematic, std::string_view board,
                                      std::string_view note = "held: revision applicability unresolved") noexcept {
    const auto* cell = find(a);
    return {a, schematic, board, ReviewOutcome::heldRevisionMark,
            cell != nullptr && cell->visibleAlternativeCount > 0 ? cell->visibleAlternatives[0] : VisibleAlternative{}, note};
}
inline constexpr std::array<CellReview, 25> review {{
    firstAlternative({51,'A',20}, "S31", "S32"), firstAlternative({51,'A',25}, "S31", "S32"),
    firstAlternative({51,'C',25}, "S31", "S32"), firstAlternative({51,'E',22}, "S31", "S32"),
    firstAlternative({51,'E',26}, "S31", "S32"), firstAlternative({51,'F',22}, "S31", "S32"),
    firstAlternative({51,'F',25}, "S31", "S32"), firstAlternative({51,'F',26}, "S31", "S32"),
    firstAlternative({52,'A',26}, "S33", "S34"), firstAlternative({52,'B',9}, "S33", "S34"),
    firstAlternative({52,'C',22}, "S33", "S34"), firstAlternative({52,'E',20}, "S33", "S34"),
    firstAlternative({53,'A',20}, "S35", "S36"), firstAlternative({53,'A',25}, "S35", "S36"),
    // S36 native r0c0 prints 75K(39K) at the footprint provisionally traced to
    // B/14. Paired board artwork is shared by T51 and T53; outside values match
    // T53 and parenthesised values T51 at C/12 20K(13K), A/9 750(1K) and B/14's
    // own parenthesised 39K (= classified T51/B/14). An inference, not a legend:
    // the cell stays unknown until a second reader traces the footprint.
    {{53,'B',14}, "S35", "S36", ReviewOutcome::boardCandidate, resistorMark(75000, "board artwork 75K (paired with T51 39K)"),
     "schematic glyph unresolved; board-artwork candidate 75K pending footprint trace"},
    firstAlternative({53,'B',20}, "S35", "S36"), firstAlternative({53,'C',20}, "S35", "S36"),
    firstAlternative({53,'E',23}, "S35", "S36"), firstAlternative({53,'E',26}, "S35", "S36"),
    firstAlternative({53,'F',22}, "S35", "S36"), firstAlternative({53,'F',26}, "S35", "S36"),
    firstAlternative({54,'A',26}, "S37", "S38"), firstAlternative({54,'C',22}, "S37", "S38"),
    firstAlternative({54,'C',25}, "S37", "S38"), firstAlternative({54,'C',26}, "S37", "S38")
}};
constexpr const CellReview* findReview(CellAddress a) noexcept {
    for (const auto& r : review) if (r.address == a) return &r;
    return nullptr;
}
constexpr bool reviewValid() noexcept {
    for (const auto& cell : cells) {
        const auto* r = findReview(cell.address);
        if (r == nullptr || r->candidate.label.empty() || r->candidate.excitation < 0.0
            || r->candidate.excitation > 1.0 || r->schematicPage.empty() || r->boardPage.empty()) return false;
    }
    for (std::size_t i = 0; i < review.size(); ++i)
        for (std::size_t j = i + 1; j < review.size(); ++j)
            if (review[i].address == review[j].address) return false;
    return true;
}
static_assert(reviewValid(), "The re-examination record must cover exactly the 25 manifest cells");

} // namespace cs80::factory_hypotheses
