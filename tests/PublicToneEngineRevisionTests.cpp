#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace cs80;
namespace sf=cs80::source_factory;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* message) {
    ++checks;
    if(!ok) {++failures;if(failures<=80)std::fprintf(stderr,"FAIL: %s\n",message);}
}
// Per-sample bit identity, not floating-point numeric equality or struct padding.
bool samePcm(const std::vector<float>& a,const std::vector<float>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(std::bit_cast<std::uint32_t>(a[i])!=std::bit_cast<std::uint32_t>(b[i]))return false;
    return true;
}
ControlFrame fixture(bool isolation=true) {
    ControlFrame frame;
    auto& p=frame.effectiveValues;
    p[P_parityMode]=isolation?1.f:0.f;p[P_sourcePerformance]=isolation?0.f:1.f;
    p[P_oversampling]=0;p[P_drift]=p[P_voiceDispersion]=p[P_analogNoise]=0;
    p[P_detune]=p[P_chorusOn]=p[P_ringAmount]=p[P_subVco]=p[P_subVcf]=p[P_subVca]=0;
    return frame;
}
std::vector<float> render(Engine& engine,int frames) {
    std::vector<float> result(static_cast<std::size_t>(frames)*2);
    for(int offset=0;offset<frames;offset+=64)
        engine.render(result.data()+offset*2,std::min(64,frames-offset));
    return result;
}
bool same(Ig00156Stage::State a,Ig00156Stage::State b) {
    return a.input==b.input&&a.feedback==b.feedback&&a.bp==b.bp&&a.lp==b.lp;
}
bool same(Engine::RankAudioState a,Engine::RankAudioState b) {
    return a.phase==b.phase&&a.hpf.ic1==b.hpf.ic1&&a.hpf.ic2==b.hpf.ic2
        &&a.lpf.ic1==b.lpf.ic1&&a.lpf.ic2==b.lpf.ic2&&same(a.hpCircuit,b.hpCircuit)&&same(a.lpCircuit,b.lpCircuit);
}
template<class State> bool sameEnvelope(State a,State b) {
    return a.stage==b.stage&&a.value==b.value&&a.elapsed==b.elapsed;
}
bool same(Engine::RankEnvelopeState a,Engine::RankEnvelopeState b) {
    return sameEnvelope(a.amplitude,b.amplitude)&&sameEnvelope(a.filter,b.filter);
}
bool same(const ReconstructedOscillator::State& a,const ReconstructedOscillator::State& b) {
    return a.charge==b.charge&&a.resetRemainingSeconds==b.resetRemainingSeconds
        &&a.inputBaselineVolts==b.inputBaselineVolts&&a.triangleBaselineVolts==b.triangleBaselineVolts
        &&a.heldFrequencyHz==b.heldFrequencyHz&&a.heldThresholdVolts==b.heldThresholdVolts
        &&a.elapsedSeconds==b.elapsedSeconds&&a.sawEnabled==b.sawEnabled&&a.pulseEnabled==b.pulseEnabled
        &&a.residual==b.residual&&a.residualCursor==b.residualCursor&&a.resetCount==b.resetCount;
}
bool same(Ig00151Vca::State a,Ig00151Vca::State b) { return a.level==b.level&&a.coupling==b.coupling; }
struct DynamicSnapshot {
    std::array<Engine::RankAudioState,16> audio;
    std::array<Engine::RankEnvelopeState,16> envelope;
    std::array<ReconstructedOscillator::State,16> oscillator;
    std::array<Ig00151Vca::State,16> vca;
    explicit DynamicSnapshot(const Engine& engine) {
        for(int voice=0;voice<8;++voice)for(int rank=0;rank<2;++rank) {
            const int i=voice*2+rank;audio[i]=engine.getRankAudioState(voice,rank);
            envelope[i]=engine.getRankEnvelopeState(voice,rank);oscillator[i]=engine.getRankOscillatorState(voice,rank);
            vca[i]=engine.getRankVcaState(voice,rank);
        }
    }
    bool unchanged(const Engine& engine) const {
        for(int voice=0;voice<8;++voice)for(int rank=0;rank<2;++rank) {
            const int i=voice*2+rank;
            if(!same(audio[i],engine.getRankAudioState(voice,rank))
                ||!same(envelope[i],engine.getRankEnvelopeState(voice,rank))
                ||!same(oscillator[i],engine.getRankOscillatorState(voice,rank))
                ||!same(vca[i],engine.getRankVcaState(voice,rank)))return false;
        }
        return true;
    }
};
void controls(Engine& engine,int rank,int selector,FactoryToneRevision revision) {
    sf::ToneControls expected;
    check(revision==FactoryToneRevision::frozenV1?sf::compileTone(rank,selector,expected)
        :sf::compileTonePublicEstimate(rank,selector,expected),"independently qualified expected policy compiles");
    const auto actual=engine.getSourceToneInfo(rank);
    check(actual.selector==selector&&actual.route==sf::route(rank,selector,true),"source address and route retained");
    check(actual.provenance.hypothesisVersion==expected.provenance.hypothesisVersion
        &&actual.provenance.hypothesisHash==expected.provenance.hypothesisHash
        &&actual.provenance.hypothesisOverrideMask==expected.provenance.hypothesisOverrideMask,
        "actual Engine uses explicitly selected factory revision");
    check(actual.provenance.knownCells==expected.provenance.knownCells
        &&actual.provenance.unknownCells==expected.provenance.unknownCells
        &&actual.provenance.inferredCells==expected.provenance.inferredCells
        &&!actual.provenance.hardwareMeasured,"Engine retains all evidence exclusions");
    const auto buses=engine.getSourceBusReadout(rank);
    for(std::size_t i=0;i<buses.size();++i)
        check(buses[i]==expected.busVolts[i],"actual Engine buses match selected revision exactly");
}
void allFactoryPaths() {
    check(ControlFrame{}.factoryToneRevision==FactoryToneRevision::frozenV1,"existing ControlFrame default stays V1");
    check(ControlFrame::programmer(ParamValues{}).factoryToneRevision==FactoryToneRevision::frozenV1,
          "existing programmer builder stays V1");
    int complete=0;
    for(double rate:{44100.,48000.,96000.})for(bool isolation:{false,true})
        for(int rank=0;rank<2;++rank)for(int selector=0;selector<11;++selector) {
            auto frame=fixture(isolation);frame.hardwareSources[rank]=selector;
            Engine ordinary,v1,v2;ordinary.prepare(rate);v1.prepare(rate);v2.prepare(rate);
            check(ordinary.setControlFrame(frame),"ordinary existing frame accepted");
            frame.factoryToneRevision=FactoryToneRevision::frozenV1;
            check(v1.setControlFrame(frame),"explicit frozen frame accepted");
            frame.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
            check(v2.setControlFrame(frame),"explicit public candidate frame accepted");
            controls(ordinary,rank,selector,FactoryToneRevision::frozenV1);
            controls(v1,rank,selector,FactoryToneRevision::frozenV1);
            controls(v2,rank,selector,FactoryToneRevision::publicAmendedV2);
            const auto count=v2.getSourceCompileCount();
            check(v2.setControlFrame(frame)&&v2.getSourceCompileCount()==count,"identical V2 frame does not recompile");
            ordinary.noteOn(60,.83f);v1.noteOn(60,.83f);v2.noteOn(60,.83f);
            const auto a=render(ordinary,256),b=render(v1,256),c=render(v2,256);
            check(samePcm(a,b),"default and explicit V1 PCM bit-identical");
            if(sf::unknownMask(*sf::address(rank,selector))==0) {
                ++complete;check(samePcm(a,c),"seven complete tones retain exact PCM in V2");
            }
        }
    check(complete==7*3*2,"all seven complete tones covered at three rates and both policies");
}
void cacheTransitionsAndAtomicity() {
    Engine engine;engine.prepare(48000);
    auto frame=fixture();frame.hardwareSources={0,1};
    check(engine.setControlFrame(frame),"initial two inferred sources accepted");
    const auto initial=engine.getSourceCompileCount();
    engine.noteOn(60,.8f);render(engine,128);
    for(auto revision:{FactoryToneRevision::publicAmendedV2,FactoryToneRevision::frozenV1}) {
        const auto count=engine.getSourceCompileCount();
        const DynamicSnapshot state(engine);
        frame.factoryToneRevision=revision;
        check(engine.setControlFrame(frame),"same-address revision transition accepted");
        check(engine.getSourceCompileCount()==count+2,"both actual source cache keys include revision");
        check(state.unchanged(engine),"revision preserves all16 oscillator/filter/envelope/VCA semantic states");
        controls(engine,0,0,revision);controls(engine,1,1,revision);
    }
    check(engine.getSourceCompileCount()==initial+4,"only changed revisions compiled");
    frame.factoryToneRevision=FactoryToneRevision::publicAmendedV2;check(engine.setControlFrame(frame),"V2 selected before Legacy trip");
    auto legacy=frame;legacy.effectiveValues[P_parityMode]=legacy.effectiveValues[P_sourcePerformance]=0;
    legacy.factoryToneRevision=FactoryToneRevision::frozenV1;
    const auto beforeLegacy=engine.getSourceCompileCount();
    check(engine.setControlFrame(legacy)&&engine.getSourceCompileCount()==beforeLegacy,"Legacy never compiles a factory policy");
    frame.factoryToneRevision=FactoryToneRevision::frozenV1;
    check(engine.setControlFrame(frame)&&engine.getSourceCompileCount()==beforeLegacy+2,"Legacy round-trip cannot hide stale V2 cache");
    controls(engine,0,0,FactoryToneRevision::frozenV1);
    auto panel=ControlFrame::programmer(frame.effectiveValues,{13,13});
    panel.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
    check(engine.setControlFrame(panel),"PANEL programmer remains accepted under explicit V2");
    frame.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
    check(engine.setControlFrame(frame),"PANEL to V2 factory transition accepted");
    controls(engine,0,0,FactoryToneRevision::publicAmendedV2);
    engine.reset();engine.prepare(48000);controls(engine,0,0,FactoryToneRevision::publicAmendedV2);
    const auto count=engine.getSourceCompileCount();
    engine.setParams(frame.effectiveValues);
    frame.factoryToneRevision=FactoryToneRevision::frozenV1;
    check(engine.setControlFrame(frame)&&engine.getSourceCompileCount()==count+2,"setParams then default frame restores V1 rather than stale V2");
    for(int invalid:{0,3,99})for(bool inactive:{false,true}) {
        const auto params=engine.getParams().v;const auto bus=engine.getSourceBusReadout(0);
        const auto oldCount=engine.getSourceCompileCount();const DynamicSnapshot state(engine);auto bad=frame;
        bad.factoryToneRevision=static_cast<FactoryToneRevision>(invalid);bad.effectiveValues[P_volume]=.11f;
        if(inactive)bad.effectiveValues[P_parityMode]=bad.effectiveValues[P_sourcePerformance]=0;
        check(!engine.setControlFrame(bad),"unknown factory revision rejected even in Legacy");
        check(engine.getControlFrameStatus()!=Engine::ControlFrameStatus::accepted,"invalid revision has visible diagnostic");
        check(engine.getParams().v==params&&engine.getSourceBusReadout(0)==bus&&engine.getSourceCompileCount()==oldCount
            &&state.unchanged(engine),
            "invalid revision rejection preserves checked exposed transaction state");
    }
}
void perRankCacheAndProgrammer() {
    Engine engine;engine.prepare(48000);auto frame=fixture();frame.hardwareSources={0,13};
    frame.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
    check(engine.setControlFrame(frame),"mixed rank V2 source accepted");
    controls(engine,0,0,FactoryToneRevision::publicAmendedV2);
    auto count=engine.getSourceCompileCount();
    frame.hardwareSources={13,1};frame.factoryToneRevision=FactoryToneRevision::frozenV1;
    check(engine.setControlFrame(frame)&&engine.getSourceCompileCount()==count+1,"only newly active rank compiled in V1");
    count=engine.getSourceCompileCount();frame.hardwareSources={0,1};
    check(engine.setControlFrame(frame)&&engine.getSourceCompileCount()==count+1,
          "per-rank key detects stale inactive V2 despite other rank already using V1");
    controls(engine,0,0,FactoryToneRevision::frozenV1);controls(engine,1,1,FactoryToneRevision::frozenV1);
    for(int selector:{11,12,13}) {
        auto panel=ControlFrame::programmer(frame.effectiveValues,{selector,selector});
        check(engine.setControlFrame(panel),"memory or PANEL V1 accepted");
        const auto buses0=engine.getSourceBusReadout(0),buses1=engine.getSourceBusReadout(1);
        count=engine.getSourceCompileCount();panel.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
        check(engine.setControlFrame(panel)&&engine.getSourceCompileCount()==count,
              "unchanged programmer positions never recompile due to factory-only revision");
        check(engine.getSourceBusReadout(0)==buses0&&engine.getSourceBusReadout(1)==buses1,
              "memory and PANEL controls are exactly unchanged by factory revision");
        count=engine.getSourceCompileCount();frame.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
        check(engine.setControlFrame(frame)&&engine.getSourceCompileCount()==count+2,
              "each MEMORY/PANEL selector returns to both actual factory V2 controls");
        controls(engine,0,0,FactoryToneRevision::publicAmendedV2);
        controls(engine,1,1,FactoryToneRevision::publicAmendedV2);
    }
}
void actualAudioChangeAndLegacy() {
    auto frame=fixture();frame.hardwareSources={0,13};
    Engine old,candidate;old.prepare(48000);candidate.prepare(48000);
    check(old.setControlFrame(frame),"Brass V1 fixture accepted");
    frame.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
    check(candidate.setControlFrame(frame),"Brass V2 fixture accepted");
    old.noteOn(60,.9f);candidate.noteOn(60,.9f);
    const auto a=render(old,24000),b=render(candidate,24000);
    double changed=0,energy=0;for(std::size_t i=0;i<a.size();++i){changed+=double(a[i]-b[i])*(a[i]-b[i]);energy+=double(b[i])*b[i];}
    check(changed>1e-12&&energy>1e-12,"opt-in V2 drives actual nonzero changed Engine PCM, not unused metadata");
    check(factoryPresets().size()==222,"original factory inventory count unchanged");
    for(double rate:{44100.,48000.,96000.})for(const auto& preset:factoryPresets()) {
        Engine v1,v2;v1.prepare(rate);v2.prepare(rate);auto p=preset.values;
        p[P_parityMode]=p[P_sourcePerformance]=0;ControlFrame legacy{p,{0,1}};
        check(v1.setControlFrame(legacy),"Legacy V1 frame accepted");
        legacy.factoryToneRevision=FactoryToneRevision::publicAmendedV2;
        check(v2.setControlFrame(legacy),"Legacy V2 request accepted but inert");
        v1.noteOn(48,.7f);v2.noteOn(48,.7f);
        check(samePcm(render(v1,64),render(v2,64)),"all222 Legacy programs retain exact PCM under revision requests");
    }
}
}
int main() {
    check(!samePcm({0.f},{-0.f}),"bit-exact PCM oracle distinguishes signed zero");
    check(samePcm({-.25f,0.f},{-.25f,0.f}),"bit-exact PCM oracle accepts identical bits");
    allFactoryPaths();cacheTransitionsAndAtomicity();perRankCacheAndProgrammer();actualAudioChangeAndLegacy();
    std::printf("Public tone Engine revision: %d checks, %d failures; candidate is not qualified default\n",checks,failures);
    return failures?1:0;
}
