#pragma once
// DIAGNOSTIC ONLY. Compile the observer TU with -fno-access-control, never the core.
// Emit receives a stable
// field label and raw uint64 bits; it must do no floating arithmetic or core mutation.
#include "core/Engine.h"
#include <bit>
#include <cstdint>
#include <type_traits>
namespace wsc_staging_audit {
template<class T> std::uint64_t bits(T x) noexcept {
    if constexpr(std::is_same_v<T,float>) return std::bit_cast<std::uint32_t>(x);
    else if constexpr(std::is_same_v<T,double>) return std::bit_cast<std::uint64_t>(x);
    else return static_cast<std::uint64_t>(x);
}
template<class E,class T> void field(E& e,const char* name,T x) { e(name,bits(x)); }
template<class E,class C> void list(E& e,const char* name,const C& xs) { for(const auto& x:xs)field(e,name,x); }
template<class E> void runner(E& e,const cs80::modern::MsegRunner& r) {
    field(e,"mseg.position",r.position);field(e,"mseg.segment",r.segment);
    field(e,"mseg.running",r.running);field(e,"mseg.released",r.released);
    field(e,"mseg.overrideSegment",r.overrideSegment);field(e,"mseg.overrideLevel",r.overrideLevel);
    field(e,"mseg.value",r.value);
}
template<class E> void rankOffsets(E& e,const cs80::modern::RankOffsets& r) {
    for(float x:{r.pitch,r.pulseWidth,r.hpCutoff,r.lpCutoff,r.hpQ,r.lpQ,r.level,r.pan})field(e,"rankOffset",x);
}
template<class E> void modern(E& e,const cs80::modern::ModernLayer& m) {
    field(e,"modern.noteRng",m.noteRng.state);field(e,"modern.sampleRate",m.sampleRate);
    field(e,"modern.active",m.active);field(e,"modern.globalDestinations",m.globalDestinations);
    field(e,"modern.liveCount",m.liveCount);list(e,"modern.live",m.live);list(e,"modern.msegUsed",m.msegUsed);
    for(unsigned v=0;v<m.voices.size();++v){field(e,"modern.voice",v);const auto& x=m.voices[v];
        field(e,"modern.age",x.age);field(e,"modern.random",x.random);field(e,"modern.velocity",x.velocity);field(e,"modern.key",x.key);
        for(const auto& r:x.runners)runner(e,r);list(e,"modern.restartPending",x.restartPending);list(e,"modern.rate",x.rate);
        for(const auto& r:x.offsets)rankOffsets(e,r);
    }
    for(const auto& r:m.globalRunners)runner(e,r);
    list(e,"modern.globalRate",m.globalRate);list(e,"modern.transportRatePhase",m.transportRatePhase);
    field(e,"modern.transportSamples",m.transportSamples);list(e,"modern.globalValues",m.globalValues.v);
    list(e,"modern.newest",m.newest.v);list(e,"modern.newestCandidate",m.newestCandidate.v);
    field(e,"modern.newestThisSample",m.newestThisSample);field(e,"modern.monoPhraseStartAge",m.monoPhraseStartAge);
    field(e,"modern.anyGateLast",m.anyGateLast);field(e,"modern.anyGateNow",m.anyGateNow);
    field(e,"modern.monoTriggerPending",m.monoTriggerPending);field(e,"modern.lockPending",m.lockPending);
    list(e,"modern.freeStarted",m.freeStarted);list(e,"modern.monoRestartPending",m.monoRestartPending);
    field(e,"modern.subSpeedRatio",m.globalOffsets.subSpeedRatio);field(e,"modern.subDepthGain",m.globalOffsets.subDepthGain);
    list(e,"modern.fxMix",m.globalOffsets.fxMix);
    field(e,"transport.bpm",m.transport.bpm);field(e,"transport.ppq",m.transport.ppq);
    field(e,"transport.hasPosition",m.transport.hasPosition);field(e,"transport.playing",m.transport.playing);
}
template<class E> void pins(E& e,const cs80::WscOutputVolts& p) {
    field(e,"pins.saw",p.sawPin5);field(e,"pins.pulse",p.pulsePin7);field(e,"pins.sine",p.sinePin2);
}
template<class E> void oscillator(E& e,const cs80::ReconstructedOscillator& o) {
    const auto& s=o.getState();
    field(e,"wsc.charge",s.charge);field(e,"wsc.resetRemaining",s.resetRemainingSeconds);
    field(e,"wsc.inputBaseline",s.inputBaselineVolts);field(e,"wsc.triangleBaseline",s.triangleBaselineVolts);
    field(e,"wsc.heldFrequency",s.heldFrequencyHz);field(e,"wsc.heldThreshold",s.heldThresholdVolts);
    field(e,"wsc.elapsed",s.elapsedSeconds);field(e,"wsc.sawEnabled",s.sawEnabled);field(e,"wsc.pulseEnabled",s.pulseEnabled);
    for(const auto& r:s.residual)list(e,"wsc.residual",r);
    field(e,"wsc.residualCursor",s.residualCursor);field(e,"wsc.resetCount",s.resetCount);
    field(e,"wsc.endNodesValid",o.endNodesValid);const auto& n=o.endNodes;
    for(double x:{n.pin7,n.u,n.uSlope,n.triangle,n.triangleSlope,n.w,n.wSlope,n.triangleGain})field(e,"wsc.endNodes",x);
    pins(e,n.value);pins(e,n.slope);pins(e,n.constant);pins(e,n.ungatedValue);
    const auto& d=o.getDiagnostics();field(e,"wsc.invalidControl",d.invalidControl);
    field(e,"wsc.frequencyLimited",d.frequencyLimited);field(e,"wsc.capacityExceeded",d.eventCapacityExceeded);
    field(e,"wsc.eventCount",d.eventCount);
    for(double x:{d.rawPin7Volts,d.coupledInputVolts,d.trianglePin3Volts,d.shaperInputVolts})field(e,"wsc.diagnosticNode",x);
    pins(e,d.raw);
    for(std::size_t i=0;i<d.events.size();++i){const auto& x=d.events[i];
        field(e,"wsc.eventIndex",i);field(e,"wsc.event.seconds",x.secondsFromSampleStart);
        field(e,"wsc.event.absolute",x.absoluteSeconds);field(e,"wsc.event.charge",x.charge);
        field(e,"wsc.event.kind",x.kind);field(e,"wsc.event.resetActive",x.resetActive);pins(e,x.before);pins(e,x.after);
    }
}
template<class E> void filter(E& e,const cs80::Ig00156Stage& s) {
    for(double x:{s.net.sourceOhms,s.net.groundOhms,s.net.inputFarads,s.net.feedbackOhms,
        s.net.feedbackFarads,s.net.integratorFarads,s.net.feedforwardOhms,
        s.gs,s.gg,s.gf,s.gTotal,s.inputScale,s.feedforwardHz,
        s.kHalf,s.kHalfCoupling,s.gmPart,s.ffPart,s.qSiemensValue,
        s.kiGs,s.xFromFb,s.xFromIn,s.xFromU,s.xFromBp,s.inFromX,s.invA11,
        s.zIn,s.zFb,s.zBp,s.zLp,s.vIn,s.vFb,s.vBp,s.vLp,s.slope1,s.slope2})field(e,"filter",x);
    for(float x:{s.lastCutoff,s.lastQ,s.lastRate})field(e,"filter.key",x);
    field(e,"filter.lastUnwarped",s.lastUnwarped);field(e,"filter.coefficientValid",s.cutoffCoefficientsValid);
}
template<class E,class V> void envelope(E& e,const V& v) {
    const auto s=v.getState();field(e,"envelope.stage",s.stage);field(e,"envelope.value",s.value);
    field(e,"envelope.elapsed",s.elapsed);field(e,"envelope.preciseValue",v.preciseValue);field(e,"envelope.preciseTime",v.preciseTime);
}
template<class E> void engineOrderingState(E& e,const cs80::Engine& x) {
    field(e,"audioRng",x.audioRng.state);field(e,"modRng",x.modRng.state);
    for(const auto& r:x.idleNoise)field(e,"idleNoise",r.state);
    field(e,"voiceCounter",x.voiceCounter);field(e,"noteIngressOrder",x.noteIngressOrder);
    field(e,"nextVoice",x.nextVoice);field(e,"keyOrder",x.keyOrder);field(e,"heldCount",x.heldCount);
    field(e,"heldKeyCount",x.heldKeyCount);field(e,"unassignedCount",x.unassignedCount);
    modern(e,x.modern);
    for(unsigned vi=0;vi<x.voices.size();++vi){field(e,"voice",vi);const auto& v=x.voices[vi];
        field(e,"voice.active",v.active);field(e,"voice.held",v.held);field(e,"voice.sustained",v.sustained);field(e,"voice.pedalHeld",v.pedalHeld);
        field(e,"voice.note",v.note);field(e,"voice.channel",v.midiChannel);field(e,"voice.age",v.age);field(e,"voice.releaseGroup",v.releaseGroup);
        field(e,"voice.idleClock",v.idleClock);field(e,"voice.idleSteps",v.idleSteps);field(e,"voice.envelopeReset",v.envelopeReset);
        field(e,"voice.glidePitch",v.glidePitch);field(e,"voice.targetPitch",v.targetPitch);field(e,"voice.ribbonRatio",v.ribbonRatio);
        for(unsigned ch=0;ch<2;++ch){field(e,"rank",ch);const auto& r=v.ranks[ch];oscillator(e,r.wsc);
            filter(e,r.hpStage);filter(e,r.lpStage);envelope(e,r.fenv);envelope(e,r.aenv);
            field(e,"vca.node",r.vca.node);field(e,"vca.coupling",r.vca.coupling);field(e,"vca.cold",r.vcaCold);
            const auto& i=r.info;for(float z:{i.hpfHz,i.lpfHz,i.filterEnvelope,i.ampEnvelope,i.oscillatorHz,i.keyVolts,i.hpfQ,i.lpfQ})field(e,"rank.info",z);
            field(e,"rank.hpfEnabled",i.hpfEnabled);field(e,"rank.lpfEnabled",i.lpfEnabled);
            field(e,"rank.idleLead",r.idleLead);field(e,"rank.idleFirstSamples",r.idleFirstSamples);
        }
    }
}
// Pure-integer independent xorshift recurrence for ordered draw-count witnesses.
inline std::uint32_t nextAudioRng(std::uint32_t s) noexcept {s^=s<<13;s^=s>>17;s^=s<<5;return s;}
} // namespace wsc_staging_audit
