// Phase 2.3: keyboard contour driven by the KBC network (circuit engine).
// Gate: every one of the 61 physical keys and every LOW/HIGH lever pair of the
// pre-registered grid, read back from the engine's filter cutoffs and level,
// against src/reference/KeyboardContour.h with the declared key map. Plus the
// CS-80 adjustment anchors and the reported C2 code conflict. Numerical checks
// of a declared model, not a hardware comparison.
#include "core/Engine.h"
#include "core/KbcContour.h"
#include "reference/KeyboardContour.h"
#include "Ws3Tolerances.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace cs80;
namespace kc = cs80::reference::keyboard_contour;
namespace tol = cs80::ws3_tolerances;

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const std::string& what)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", what.c_str()); }
}

double referenceVolts(int key, double low, double high, bool q1, bool q2, bool q3, int oc, int tap)
{
    kc::Inputs in;
    in.q1 = q1; in.q2 = q2; in.q3 = q3; in.selectedOc = oc; in.noteTap = tap;
    in.lowTravel = low; in.highTravel = high; in.vr1Ohms = kbc::kVr1Ohms; in.vr2Ohms = kbc::kVr2Ohms;
    (void) key;
    return kc::evaluate(in).outputVolts;
}

// Independent restatement of the declared map, written from the table in the
// header comment rather than copied from the implementation.
void declaredCode(int key, bool& q1, bool& q2, bool& q3, int& oc, int& tap)
{
    if (key == 0) { q1 = true; q2 = false; q3 = false; oc = 0; tap = 11; return; }
    oc = 1 + (key - 1) / 12;
    tap = (key - 1) % 12;
    const char* codes[] = {"100", "110", "010", "110", "001", "101"};
    q1 = codes[oc][0] == '1'; q2 = codes[oc][1] == '1'; q3 = codes[oc][2] == '1';
}

ParamValues panel()
{
    ParamValues p;
    p[P_parityMode] = 1.0f;
    p[P_voiceDispersion] = p[P_drift] = p[P_analogNoise] = 0.0f;
    for (int r = 0; r < 2; ++r)
    {
        p[chParam(r, CP_fIl)] = p[chParam(r, CP_fAl)] = 0.0f; // no filter EG movement
        p[chParam(r, CP_initBrill)] = p[chParam(r, CP_initLevel)] = 0.0f;
        p[chParam(r, CP_vS)] = 1.0f;
    }
    return p;
}

struct Readout { float hp, lp; };
Readout settle(Engine& e)
{
    std::vector<float> out(64 * 2);
    e.render(out.data(), 64);
    return { e.getRankInfo(0, 0).hpfHz, e.getRankInfo(0, 0).lpfHz };
}

void testGrid()
{
    const double grid[] = {-1.0, -0.5, 0.0, 0.5, 1.0};
    double worst = 0.0;
    int tested = 0;
    for (int key = 0; key < kbc::kKeyCount; ++key)
    {
        bool q1, q2, q3; int oc, tap;
        declaredCode(key, q1, q2, q3, oc, tap);
        const auto code = kbc::keyCode(key);
        check(code.q1 == q1 && code.q2 == q2 && code.q3 == q3 && code.octaveTap == oc && code.noteTap == tap,
              "declared code for key " + std::to_string(key));
        const int note = kbc::kLowestMidiNote + key;
        Engine neutral;
        neutral.prepare(48000.0);
        neutral.setParams(panel());
        neutral.noteOn(note, 0.8f);
        const auto base = settle(neutral);
        for (double low : grid)
            for (double high : grid)
            {
                const double expected = referenceVolts(key, low, high, q1, q2, q3, oc, tap);
                // Brilliance board: read back the KC volts from both cutoffs.
                auto p = panel();
                p[P_kbdBrillianceLow] = static_cast<float>(low);
                p[P_kbdBrillianceHigh] = static_cast<float>(high);
                Engine e;
                e.prepare(48000.0);
                e.setParams(p);
                e.noteOn(note, 0.8f);
                const auto r = settle(e);
                // Only unclamped cutoffs carry the full CV (Svf clamps to 18 Hz..0.45 fs).
                const double lpVolts = std::log2(double(r.lp) / base.lp) / kbc::kKcLpCvPerVolt;
                const double hpVolts = std::log2(double(r.hp) / base.hp) / kbc::kKcHpCvPerVolt;
                worst = std::max({worst, std::abs(lpVolts - expected), std::abs(hpVolts - expected)});
                ++tested;
            }
    }
    check(worst <= tol::kContourVoltsAbsErrorMax,
          "61 keys x 25 lever settings: engine contour matches the KBC reference within "
          + std::to_string(tol::kContourVoltsAbsErrorMax) + " V (worst " + std::to_string(worst) + ")");
    std::printf("contour gate: %d key/lever cases, worst %.3g V\n", tested, worst);
}

void testAnchorsAndConflict()
{
    // CS-80 Adjustment Procedures PDF29/30 (printed 27/28): O1 targets.
    const double c2 = kbc::outputVolts(12, -1.0, 0.0);   // C2, LOW minimum
    const double c5s = kbc::outputVolts(49, 0.0, 1.0);   // C5#, HIGH maximum
    check(std::abs(c2 + tol::kCalibrationAnchorVolts) <= tol::kCalibrationAnchorToleranceVolts,
          "C2 LOW-minimum anchor -2.76 V (" + std::to_string(c2) + ")");
    check(std::abs(c5s - tol::kCalibrationAnchorVolts) <= tol::kCalibrationAnchorToleranceVolts,
          "C5# HIGH-maximum anchor +2.76 V (" + std::to_string(c5s) + ")");
    // The conflict, reported: the literal KAS6 label C2 -> Q100 puts C2 flat at
    // OC1 = L, where VR1 cannot act and O1 would read -4.02 V, outside the
    // printed -2.76 +/- 0.14 V.
    const double literal = referenceVolts(12, -1.0, 0.0, true, false, false, 1, 11);
    std::printf("C2 anchor: declared map %.4f V, literal KAS6 code 100 %.4f V (target -2.76 +/- 0.14)\n", c2, literal);
    check(std::abs(literal + tol::kCalibrationAnchorVolts) > tol::kCalibrationAnchorToleranceVolts,
          "the literal KAS6 C2 code misses the C2 trim anchor (the documented conflict)");
    // OC0/OC1 non-identifiability: C1 at OC0 or OC1 gives identical volts.
    check(referenceVolts(0, -0.7, 0.3, true, false, false, 0, 11) == referenceVolts(0, -0.7, 0.3, true, false, false, 1, 11),
          "OC0 and OC1 are indistinguishable at every lever setting");
    // Print the full curve at the lever extremes for the report.
    std::printf("contour O1 volts, LOW min | HIGH max, keys C1..C6:\n");
    for (int key = 0; key < kbc::kKeyCount; ++key)
        std::printf("%s%+.3f|%+.3f", key % 12 == 0 ? "\n  " : " ", kbc::outputVolts(key, -1.0, 0.0), kbc::outputVolts(key, 0.0, 1.0));
    std::printf("\n");
}

void testEngineContract()
{
    // Neutral levers contribute exactly nothing.
    for (int key : {0, 12, 30, 49, 60})
        check(kbc::outputVolts(key, 0.0, 0.0) == 0.0, "neutral levers give exactly 0 V");
    // Destination independence and level path.
    auto p = panel();
    p[P_kbdLevelLow] = -1.0f;
    Engine level; level.prepare(48000.0); level.setParams(p); level.noteOn(24, 0.8f);
    Engine neutral; neutral.prepare(48000.0); neutral.setParams(panel()); neutral.noteOn(24, 0.8f);
    const auto a = settle(level), b = settle(neutral);
    check(a.lp == b.lp && a.hp == b.hp, "level contour leaves both filters alone");
    std::vector<float> x(4800 * 2), y(4800 * 2);
    level.render(x.data(), 4800); neutral.render(y.data(), 4800);
    double ex = 0, ey = 0;
    for (std::size_t i = 0; i < x.size(); ++i) { ex += double(x[i]) * x[i]; ey += double(y[i]) * y[i]; }
    check(ex < ey * 0.5, "LOW LEVEL minimum attenuates the lowest key");
    // A lever move reaches a held note (the KBC multiplexer refreshes the hold).
    p = panel();
    Engine live; live.prepare(48000.0); live.setParams(p); live.noteOn(84, 0.8f);
    const auto before = settle(live);
    p[P_kbdBrillianceHigh] = 1.0f; live.setParams(p);
    const auto after = settle(live);
    check(after.lp > before.lp * 3.0f, "HIGH BRILLIANCE reaches a held top key");
    // Source-selected ranks include the shared FH/FL bus loading of all channels.
    ControlFrame frame;
    frame.effectiveValues = panel();
    frame.effectiveValues[P_kbdBrillianceLow] = -1.0f;
    frame.hardwareSources = {9, 9};
    Engine source; source.prepare(48000.0); check(source.setControlFrame(frame), "source frame");
    source.noteOn(24, 0.8f);
    const auto s = settle(source);
    check(std::isfinite(s.lp) && s.lp > 0.0f, "source-selected contour finite");
    // Legacy keeps its frozen weights.
    auto lp = panel(); lp[P_parityMode] = 0.0f; lp[P_kbdBrillianceLow] = 1.0f;
    Engine legacy; legacy.prepare(48000.0); legacy.setParams(lp); legacy.noteOn(28, 0.8f);
    auto ln = panel(); ln[P_parityMode] = 0.0f;
    Engine legacyNeutral; legacyNeutral.prepare(48000.0); legacyNeutral.setParams(ln); legacyNeutral.noteOn(28, 0.8f);
    const double legacyOct = std::log2(double(settle(legacy).lp) / settle(legacyNeutral).lp);
    check(std::abs(legacyOct - kLegacyKeyboardBrillianceOctaves * keyboardContourWeights(28).low) < 1e-4,
          "Legacy contour keeps its historical per-key weights");
}
} // namespace

int main()
{
    testGrid();
    testAnchorsAndConflict();
    testEngineContract();
    std::printf("Keyboard contour key map: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
