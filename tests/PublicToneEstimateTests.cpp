// Independent literal contract, committed before the estimated policy.
// These are public-source engineering choices, never installed-part facts.
#include "core/SourceFactoryTones.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <vector>

namespace sf = cs80::source_factory;
namespace fh = cs80::factory_hypotheses;
namespace hw = cs80::hardware;
namespace {
int checks = 0, failures = 0;
void check(bool value, const char* why) {
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", why); }
}
void near(double actual, double expected, const char* why) {
    check(std::isfinite(actual) && std::abs(actual - expected) <= 1e-12 * std::max(1.0, std::abs(expected)), why);
}
struct Expected { int board; char column; int output; int ohms; };
// Zero means the selected DIRECT hypothesis, not a measured zero-ohm part.
constexpr std::array<Expected, 25> expected {{
    {51,'A',20,1200}, {51,'A',25,0}, {51,'C',25,0}, {51,'E',22,12000},
    {51,'E',26,0}, {51,'F',22,12000}, {51,'F',25,0}, {51,'F',26,0},
    {52,'A',26,0}, {52,'B',9,10000}, {52,'C',22,12000}, {52,'E',20,1200},
    {53,'A',20,1200}, {53,'A',25,0}, {53,'B',14,75000}, {53,'B',20,1200},
    {53,'C',20,1200}, {53,'E',23,22000}, {53,'E',26,0}, {53,'F',22,12000},
    {53,'F',26,0}, {54,'A',26,0}, {54,'C',22,12000}, {54,'C',25,0}, {54,'C',26,0}
}};
const Expected* find(int board, char column, int output) {
    for (const auto& row : expected)
        if (row.board == board && row.column == column && row.output == output) return &row;
    return nullptr;
}
double excitation(const Expected& row) { return 10000.0 / (10000.0 + row.ohms); }
// Full semantic numeric controls, not object padding or pointer representation.
std::vector<double> controls(const sf::ToneControls& t) {
    std::vector<double> out;
    const auto source = [&out](sf::SourceEquivalent s) { out.push_back(s.volts); out.push_back(s.ohms); };
    for (auto s : t.sources) source(s);
    for (auto x : t.busVolts) out.push_back(x);
    for (auto x : t.hypothesisExcitations) out.push_back(x);
    for (auto x : {double(t.wsc.sawEnabled),double(t.wsc.pulseEnabled),double(t.wsc.pulseThresholdVolts),
                  t.dutyProxy,t.pwmRateHz,t.pwmDepth,t.noiseGain,t.filterPathGain,t.sinePathGain}) out.push_back(x);
    const auto f = t.filterEnvelope;
    for (auto x : {f.initialVolts,f.attackVolts,f.sustainVolts,f.attackSeconds,f.decaySeconds,f.releaseSeconds}) out.push_back(x);
    const auto a = t.amplitudeEnvelope;
    for (auto x : {a.initialRatio,a.attackRatio,a.sustainRatio,a.attackSeconds,a.decaySeconds,a.releaseSeconds}) out.push_back(x);
    out.push_back(t.initialBrillianceVolts); out.push_back(t.afterBrillianceVolts);
    source(t.boundary.qHp); source(t.boundary.qLp); source(t.boundary.level);
    out.push_back(t.boundary.initialLevelVolts); out.push_back(t.boundary.afterLevelVolts);
    for (const auto& m : t.cutoff)
        for (auto x : {m.pinPerLoadedBus,m.pinPerTouch,m.pinPerEnvelopeVolt,m.busBaseVolts,
                      m.busPerTouchSum,m.busPerEnvelopeVoltSum,m.effectiveCvPerPinVolt,m.referencePinVolts,m.effectiveCvOffset}) out.push_back(x);
    return out;
}
void exact(double a, double b, const char* why) {
    check(std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b), why);
}
sf::InferenceScenario independentScenario(int rank, int selector) {
    sf::InferenceScenario scenario;
    const auto* a = sf::address(rank, selector);
    if (!a) return scenario;
    for (const auto& row : expected)
        if (row.board == a->board && row.column == a->column)
            scenario.overrides[scenario.count++] = {row.output, excitation(row)};
    return scenario;
}
bool compileCandidate(int rank, int selector, sf::ToneControls& out,
                      sf::Grade hp = sf::Grade::B, sf::Grade lp = sf::Grade::B) {
#ifdef CS80_TEST_PUBLIC_ESTIMATE_BASELINE
    return sf::compileTone(rank, selector, out, hp, lp);
#else
    return sf::compileTonePublicEstimate(rank, selector, out, hp, lp);
#endif
}
void compiledContracts() {
    check(fh::fingerprint == 0xe5f877a025fc52c6ull, "frozen V1 hypothesis fingerprint");
    check(sf::sourceFingerprint == 0xa95212770767733cull, "classified source fingerprint");
    int addressed = 0, complete = 0;
    for (int rank = 0; rank < 2; ++rank) for (int selector = 0; selector < 11; ++selector) {
        const auto* a = sf::address(rank, selector);
        const auto unknown = sf::unknownMask(*a);
        if (!unknown) ++complete;
        for (int output = 1; output <= 26; ++output) {
            const bool isExpected = find(a->board, a->column, output) != nullptr;
            check(isExpected == ((unknown & (1u << (output-1))) != 0), "literal policy covers exactly excluded addresses");
            addressed += isExpected;
        }
        for (auto hp : {sf::Grade::A, sf::Grade::B, sf::Grade::C})
          for (auto lp : {sf::Grade::A, sf::Grade::B, sf::Grade::C}) {
            sf::ToneControls old, candidate, literal;
            check(sf::compileTone(rank, selector, old, hp, lp), "old median arm compiles");
            check(compileCandidate(rank, selector, candidate, hp, lp), "public estimate compiles");
            check(sf::compileToneForSensitivity(rank, selector, independentScenario(rank, selector), literal, hp, lp), "independent literal arm compiles");
            check(!candidate.provenance.hardwareMeasured, "estimate never claims hardware measurement");
            check(candidate.provenance.knownCells == old.provenance.knownCells
                && candidate.provenance.unknownCells == old.provenance.unknownCells
                && candidate.provenance.inferredCells == old.provenance.inferredCells, "evidence classification unchanged");
            check(candidate.provenance.hypothesisVersion == (unknown ? 2u : 0u), "estimated policy version explicit");
            check(candidate.provenance.hypothesisHash == (unknown ? 0x832eee6f44f5cfccull : 0ull), "independently pinned selected-policy fingerprint");
            check(candidate.provenance.hypothesisOverrideMask == unknown, "all and only excluded cells estimated");
            if (!unknown) {
                const auto before = controls(old), after = controls(candidate);
                check(before.size() == after.size(), "complete-tone semantic field count");
                for (std::size_t i=0;i<before.size();++i) exact(before[i],after[i],"complete-tone full control bits unchanged");
                check(candidate.source.rank == old.source.rank && candidate.source.selector == old.source.selector
                    && candidate.source.board == old.source.board && candidate.source.column == old.source.column
                    && candidate.source.name == old.source.name && candidate.source.sourceName == old.source.sourceName
                    && candidate.analogShuntMask == old.analogShuntMask && candidate.hpGrade == old.hpGrade
                    && candidate.lpGrade == old.lpGrade && candidate.compiled == old.compiled
                    && candidate.hypotheses == old.hypotheses, "complete-tone identity and semantic flags exact");
            }
            for (int output = 1; output <= 26; ++output) {
                const auto i = static_cast<std::size_t>(output-1);
                if (const auto* row = find(a->board, a->column, output)) {
                    check(hw::findBranch(a->board, a->column, output).kind == hw::BranchKind::unknown, "estimated cell not relabeled source fact");
                    near(candidate.hypothesisExcitations[i], excitation(*row), "literal amended-mark excitation");
                    near(candidate.sources[i].volts, 10.0 * excitation(*row), "literal source equivalent volts");
                    near(candidate.sources[i].ohms, 10000.0 * (1.0-excitation(*row)), "literal source equivalent ohms");
                } else {
                    check(candidate.sources[i].volts == old.sources[i].volts
                        && candidate.sources[i].ohms == old.sources[i].ohms, "classified equivalents stay exact");
                }
                near(candidate.busVolts[i], literal.busVolts[i], "all26 buses match independent literal selection");
            }
            near(candidate.filterEnvelope.decaySeconds, literal.filterEnvelope.decaySeconds, "filter decay selected coherently");
            sf::RankDrives drives {};
            for (int voice = 0; voice < 8; ++voice) drives[voice] = {-3.0+6.0*voice/7.0, voice/7.0, (7-voice)/7.0};
            sf::CutoffReadout actual, reference;
            check(sf::evaluateCutoff(candidate, drives, actual), "estimated provenance accepted by readout");
            check(sf::evaluateCutoff(literal, drives, reference), "literal reference readout valid");
            for (int voice = 0; voice < 8; ++voice) for (int side = 0; side < 2; ++side) {
                near(actual.pinVolts[voice][side], reference.pinVolts[voice][side], "eight-voice pin readout");
                near(actual.effectiveCv[voice][side], reference.effectiveCv[voice][side], "eight-voice effective CV");
                if (!unknown) {
                    exact(actual.pinVolts[voice][side],reference.pinVolts[voice][side],"complete-tone dynamic pin bits exact");
                    exact(actual.effectiveCv[voice][side],reference.effectiveCv[voice][side],"complete-tone dynamic CV bits exact");
                }
            }
            if (!unknown) for (int side=0;side<2;++side)
                exact(actual.loadedBusVolts[side],reference.loadedBusVolts[side],"complete-tone loaded bus bits exact");
            if (unknown) {
                const auto saved = candidate.provenance;
                for (int bit = 0; bit < 26; ++bit) {
                    candidate.provenance.hypothesisOverrideMask = unknown ^ (1u << bit);
                    sf::CutoffReadout untouched;
                    untouched.loadedBusVolts = {123.0, -456.0};
                    for (auto& pair : untouched.pinVolts) pair = {17.0, -19.0};
                    for (auto& pair : untouched.effectiveCv) pair = {23.0, -29.0};
                    untouched.saturatedVoices = 0x12345678u;
                    actual = untouched;
                    check(!sf::evaluateCutoff(candidate, drives, actual), "policy2 requires exact selected-cell override mask");
                    check(actual.loadedBusVolts == untouched.loadedBusVolts
                        && actual.pinVolts == untouched.pinVolts
                        && actual.effectiveCv == untouched.effectiveCv
                        && actual.saturatedVoices == untouched.saturatedVoices,
                        "malformed selected-policy mask preserves complete readout");
                }
                candidate.provenance = saved;
                auto partial = independentScenario(rank, selector);
                partial.count = 1; // V1 sensitivity intentionally allows a partial override.
                sf::ToneControls v1Partial;
                check(sf::compileToneForSensitivity(rank,selector,partial,v1Partial,hp,lp)
                    && v1Partial.provenance.hypothesisVersion == 1
                    && sf::evaluateCutoff(v1Partial,drives,actual),
                    "V1 partial sensitivity remains accepted");
                candidate.provenance.hypothesisHash ^= 1;
                check(!sf::evaluateCutoff(candidate, drives, actual), "forged estimated fingerprint rejected");
                candidate.provenance.hypothesisHash ^= 1;
                candidate.provenance.hypothesisVersion = 99;
                check(!sf::evaluateCutoff(candidate, drives, actual), "unknown policy version rejected");
            }
        }
    }
    check(addressed == 25 && complete == 7, "25 estimated cells and seven source-complete tones");
    for (const auto& id : std::array<std::array<int,2>,4>{{{-1,0},{2,0},{0,-1},{0,11}}}) {
        sf::ToneControls out; out.source.selector = 1234;
        check(!compileCandidate(id[0],id[1],out) && out.source.selector == 1234, "invalid selection preserves output");
    }
    sf::ToneControls out; out.source.selector = 1234;
    check(!compileCandidate(0,0,out,static_cast<sf::Grade>(99)) && out.source.selector == 1234, "invalid grade preserves output");
}
void allSensitivities() {
    std::puts("board,column,output,x,source_volts,source_ohms,filter_decay_s,amp_sustain,initial_brilliance_v,after_brilliance_v,level_source_v,after_level_v");
    for (const auto& row : expected) {
        const int rank = row.board >= 53 ? 1 : 0;
        const int selector = (row.board % 2 == 0 ? 6 : 0) + row.column - 'A';
        const auto* old = fh::find(row.board,row.column,row.output);
        std::array<double,7> alternatives {0.0,1.0,old->nominalExcitation,excitation(row)};
        std::size_t count = 4;
        for (std::size_t i=0;i<old->visibleAlternativeCount;++i) alternatives[count++] = old->visibleAlternatives[i].excitation;
        alternatives[count++] = fh::findReview({row.board,row.column,row.output})->candidate.excitation;
        for (std::size_t n=0;n<count;++n) {
            auto scenario = independentScenario(rank,selector);
            for (std::size_t i=0;i<scenario.count;++i)
                if (scenario.overrides[i].output == row.output) scenario.overrides[i].excitation = alternatives[n];
            sf::ToneControls tone;
            check(sf::compileToneForSensitivity(rank,selector,scenario,tone), "unselected sensitivity compiles");
            const auto& source = tone.sources[row.output-1];
            near(source.volts,10*alternatives[n],"sensitivity source volts independently computed");
            near(source.ohms,10000*(1-alternatives[n]),"sensitivity source ohms independently computed");
            std::printf("%d,%c,%d,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g\n",
                row.board,row.column,row.output,alternatives[n],source.volts,source.ohms,
                tone.filterEnvelope.decaySeconds,tone.amplitudeEnvelope.sustainRatio,tone.initialBrillianceVolts,
                tone.afterBrillianceVolts,tone.boundary.level.volts,tone.boundary.afterLevelVolts);
        }
    }
    // Correlated topology ambiguity: all low-confidence direct choices in a
    // tone open together, versus all selected together and the frozen median.
    // No worst-looking tone or arm is suppressed.
    std::puts("JOINT controls order: source(V,R)x26,busVx26,hypothesisXx26,WSC3,duty/PWM/noise/path6,filterEG6,ampEG6,IB/AB2,boundary8,cutoffMap9x2");
    for (int rank=0;rank<2;++rank) for (int selector=0;selector<11;++selector) {
        for (int arm=0;arm<3;++arm) {
            auto scenario = arm == 0 ? sf::InferenceScenario{} : independentScenario(rank,selector);
            if (arm == 2) for (std::size_t i=0;i<scenario.count;++i) {
                const auto* a=sf::address(rank,selector);
                if (find(a->board,a->column,scenario.overrides[i].output)->ohms == 0)
                    scenario.overrides[i].excitation = 0.0;
            }
            sf::ToneControls tone;
            check(sf::compileToneForSensitivity(rank,selector,scenario,tone),"all correlated sensitivity arms compile");
            std::printf("JOINT,%d,%d,%s",rank,selector,arm==0?"v1_median":arm==1?"full_selected":"all_disputed_direct_open");
            for (double x : controls(tone)) {
                check(std::isfinite(x),"correlated controls finite without normalization");
                std::printf(",%.17g",x);
            }
            std::putchar('\n');
        }
    }
}
}
int main() {
    compiledContracts();
    allSensitivities();
    std::printf("Public tone estimates: %d checks, %d failures; no hardware claim\n",checks,failures);
    return failures ? 1 : 0;
}
