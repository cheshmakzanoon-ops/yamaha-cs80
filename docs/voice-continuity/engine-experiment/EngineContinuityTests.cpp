// Candidate-only executable contract. Not a hardware-accuracy assertion.
#include "core/Engine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using namespace cs80;
namespace {
int checks = 0, failures = 0;
void check(bool yes, const char* label)
{ ++checks; if (!yes) { ++failures; std::printf("FAIL %s\n", label); } }
using Profile = experimental::ExperimentalVoiceContinuityProfile;
using Circuit = experimental::ContinuousVoiceCircuit;
ControlFrame frame(int selector = 13)
{
    ControlFrame f; f.hardwareSources = {selector, selector};
    auto& p = f.effectiveValues;
    p[P_sourcePerformance] = 1;
    p[P_analogNoise] = p[P_voiceDispersion] = p[P_drift] = p[P_detune] = 0;
    p[P_keyRelease] = p[P_masterSustain] = 0;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[chParam(rank, CP_noise)] = 0;
        p[chParam(rank, CP_vA)] = p[chParam(rank, CP_vD)] = .001f;
        p[chParam(rank, CP_vR)] = .001f;
        p[chParam(rank, CP_fR)] = 2;
    }
    return f;
}
void init(Engine& e, const ControlFrame& f, double rate = 48000)
{
    e.prepare(rate);
    check(e.setControlFrame(f), "accepted frame");
    Profile p; p.enabled = true;
    check(e.setExperimentalVoiceContinuity(p), "accepted experimental profile");
}
void render(Engine& e, int n = 128, const float* external = nullptr)
{
    std::vector<float> audio((size_t) n * 2);
    e.render(audio.data(), n, external);
    check(std::all_of(audio.begin(), audio.end(), [](float x){ return std::isfinite(x); }), "finite output");
    check(e.getExperimentalRejectedSamples() == 0, "all derived circuit controls accepted");
}
bool different(const Engine::RankAudioState& a, const Engine::RankAudioState& b)
{ return a.phase != b.phase || a.hpf.ic1 != b.hpf.ic1 || a.hpf.ic2 != b.hpf.ic2
    || a.lpf.ic1 != b.lpf.ic1 || a.lpf.ic2 != b.lpf.ic2; }

void domain()
{
    Decimator fir; fir.prepare(96000);
    double phase[2] {}; int i = 0;
    for (float x : fir.getCoefficients()) phase[i++ % 2] += std::abs(x);
    const double externalBound = 2 * std::max(phase[0], phase[1]);
    // Master tune/glide cancel between the current KI reference and VCO ratio.
    // Initial pitch is nonpositive. Positive gliss rounding is at most .5 st.
    const double semitones = 48 + 96 + .5 + 6 + 5 * externalBound + .07 + .9 + .5;
    const double multiplier = 8 * std::exp2(semitones / 12);
    check(externalBound < 2.7, "unchanged FIR polyphase L1 bounds external SUB");
    check(multiplier > 4096 && multiplier < 262144, "v2 domain covers declared legal controller bound");
    std::printf("domain external_bound=%.12g positive_multiplier_bound=%.12g\n", externalBound, multiplier);
    Circuit c; check(c.prepare(96000, 1024, {.2f,.7f}), "wide-domain prepare");
    Circuit::Controls controls; controls.keyInputVolts = 1024;
    for (auto& rank : controls.ranks) { rank.hzPerVolt = 4096; rank.vcoMultiplier = 262144; }
    check(c.processSample(controls).accepted, "v2 exact maximum multiplier accepted without overflow");
    const auto before = c.getState();
    controls.ranks[1].vcoMultiplier = std::nextafter(262144.f, INFINITY);
    check(!c.processSample(controls).accepted, "v2 first above-domain multiplier rejected");
    const auto after = c.getState();
    check(before.samples == after.samples && before.heldKeyVolts == after.heldKeyVolts
        && before.ranks[0].phase == after.ranks[0].phase
        && before.ranks[1].phase == after.ranks[1].phase, "v2 rejection does not partially clock rank zero");
}
void lifecycle()
{
    for (double rate : {8000., 44100., 48000., 96000., 768000.})
    {
        Engine e; auto f = frame(); init(e, f, rate);
        for (int v = 0; v < 8; ++v)
        {
            const auto info = e.getVoiceInfo(v); const auto c = e.getExperimentalVoiceCircuitState(v);
            check(c.prepared && c.samples == 0 && c.heldKeyVolts == .5f, "explicit cold circuit state");
            check(!info.active && !info.held && !info.pedalHeld && info.note == 60
                && info.channel == 1 && info.velocity == 0 && info.pressure == 0
                && info.bend == 0 && info.ribbonRatio == 1, "all exposed cold voice fields initialized");
        }
        const auto old = e.getRankAudioState(0,0);
        render(e, 137);
        check(e.getExperimentalVoiceCircuitState(0).samples == 274, "idle clock uses exact internal sample count");
        check(different(old,e.getRankAudioState(0,0)), "idle circuit continues");
        const auto before = e.getRankAudioState(0,0);
        e.noteOn(67,.8f,2);
        const auto assigned = e.getRankAudioState(0,0);
        check(!different(before,assigned), "assignment has no instantaneous phase or filter reset");
        render(e, 73);
        check(e.getExperimentalVoiceCircuitState(0).samples == 420, "assignment retains circuit clock epoch");
        check(e.getLatencySamples() == 63, "latency unchanged");
        e.prepare(rate);
        check(e.getExperimentalVoiceCircuitState(0).samples == 0 && e.activeVoices() == 0, "prepare resets experimental lifecycle");
    }
}
void keyAndRelease()
{
    auto f = frame(); f.effectiveValues[P_mpeOn] = 1;
    Engine e; init(e,f);
    e.noteOn(48,.7f,2); render(e,256);
    const float held = e.getExperimentalVoiceCircuitState(0).heldKeyVolts;
    e.channelPressure(2,.9f); e.pitchBend(2,.7f); e.noteOff(48,2);
    for (int i = 0; i < 300 && e.activeVoices(); ++i) render(e,64);
    check(e.activeVoices() == 0, "short VCA naturally retires");
    render(e,64);
    const auto before = e.getRankAudioState(0,0);
    const auto eg = e.getRankEnvelopeState(0,0).filter;
    render(e,1024);
    check(different(before,e.getRankAudioState(0,0)), "retired phase and filter history advances");
    check(e.getRankEnvelopeState(0,0).filter.value != eg.value, "retired PANEL filter envelope advances");
    check(e.getVoiceInfo(0).velocity == 0 && e.getVoiceInfo(0).pressure == 0
        && e.getVoiceInfo(0).bend == 0, "retired expression cleared by explicit profile");
    f.effectiveValues[P_masterTune] = 2;
    check(e.setControlFrame(f), "live master tune"); render(e,64);
    check(e.getExperimentalVoiceCircuitState(0).heldKeyVolts == held, "Sustain I holds release key K across upstream tuning");
    f.effectiveValues[P_sustainMode] = 1;
    check(e.setControlFrame(f), "Sustain II live switch"); render(e,64);
    check(e.getExperimentalVoiceCircuitState(0).heldKeyVolts == filterKeyVolts(50), "Sustain II tracks retained per-circuit KI, not global last note");
    e.noteOn(96,.5f,3); render(e,64);
    check(e.getExperimentalVoiceCircuitState(0).heldKeyVolts == filterKeyVolts(50), "another voice does not overwrite retained KI");
    e.allNotesOff(false); render(e,64);
    check(e.activeVoices() == 0 && e.getRankInfo(0,0).ampEnvelope == 0, "Panic allocation stays mute while circuit clocks");
}
void selectorsAndBypass()
{
    for (double rate : {44100.,48000.,96000.}) for (int selector = 0; selector < 14; ++selector)
    {
        auto f = frame(selector); Engine e; init(e,f,rate);
        for (int v = 0; v < 8; ++v) e.noteOn(36+v*7,.1f*v+.2f,1+v);
        render(e,127); e.allNotesOff(false);
        for (int bypass = 0; bypass < 4; ++bypass)
        {
            for (int r = 0; r < 2; ++r)
            {
                f.effectiveValues[chParam(r,CP_hpfOn)] = bypass & 1;
                f.effectiveValues[chParam(r,CP_lpfOn)] = (bypass >> 1) & 1;
                f.effectiveValues[chParam(r,CP_noise)] = .13f;
            }
            check(e.setControlFrame(f), "live bypass/selector update");
            const auto a = e.getRankAudioState(0,0); render(e,127);
            const auto b = e.getRankAudioState(0,0);
            check(a.hpf.ic1 != b.hpf.ic1 || a.hpf.ic2 != b.hpf.ic2, "HP hidden state clocks under every bypass");
            check(a.lpf.ic1 != b.lpf.ic1 || a.lpf.ic2 != b.lpf.ic2, "LP hidden state clocks under every bypass");
            check(e.activeVoices() == 0, "bypass never revives retired allocation");
        }
    }
}
void extremeControllers()
{
    std::array<float,256> external;
    for (size_t i = 0; i < external.size(); ++i) external[i] = i % 3 ? 1.f : -1.f;
    for (int selector : {0,1,3,6,9,11,12,13}) for (int sign : {-1,1})
    {
        auto f = frame(selector); auto& p = f.effectiveValues;
        p[P_mpeOn] = 1; p[P_pitchBendRange] = 48; p[P_mpeBendRange] = 96;
        p[P_masterTune] = 2; p[P_detune] = 50; p[P_voiceDispersion] = p[P_drift] = 1;
        p[P_subOriginalFunction] = 5; p[P_subExtLevel] = 1;
        p[P_subVco] = p[P_subAtVco] = p[P_modWheelSub] = 1;
        p[P_glideMode] = 2; p[P_initBend] = 1;
        p[P_lfo1Depth] = p[P_lfo2Depth] = p[P_lfo3Depth] = 1;
        p[P_lfo1Dest] = p[P_lfo2Dest] = p[P_lfo3Dest] = 1;
        for (int rank = 0; rank < 2; ++rank) p[chParam(rank,CP_feet)] = 5;
        Engine e; init(e,f); e.noteOn(sign > 0 ? 127 : 0,1,2);
        e.channelPressure(2,1); e.pitchBend(1,(float)sign); e.pitchBend(2,(float)sign);
        e.controlChange(1,1); e.setRibbon(1);
        for (int b = 0; b < 8; ++b) render(e,256,external.data());
        check(e.getRankInfo(0,0).oscillatorHz >= 0, "extreme requested VCO rate finite and nonnegative");
    }
}
void partition()
{
    auto f = frame(3); f.effectiveValues[P_subVco] = .3f;
    f.effectiveValues[P_subOriginalFunction] = 5; f.effectiveValues[P_subExtLevel] = 1;
    Engine a,b; init(a,f); init(b,f);
    a.noteOn(51,.71f,2); b.noteOn(51,.71f,2);
    std::array<float,4096> ext; std::array<float,8192> x {},y {};
    for (size_t i=0;i<ext.size();++i) ext[i]=std::sin((float)i*.017f);
    a.render(x.data(),4096,ext.data());
    int at=0; for(int size : {1,127,256,73,2048,1591}) { b.render(y.data()+2*at,size,ext.data()+at); at+=size; }
    check(at==4096 && x==y, "enabled output independent of render partition including external input");
    for(int v=0;v<8;++v) for(int r=0;r<2;++r)
        check(!different(a.getRankAudioState(v,r),b.getRankAudioState(v,r)), "enabled hidden state partition exact");
}
}
int main()
{
    domain(); lifecycle(); keyAndRelease(); selectorsAndBypass(); extremeControllers(); partition();
    std::printf("Engine continuity candidate: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
