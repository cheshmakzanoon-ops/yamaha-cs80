// Frozen-source M41 CP3/sine-to-IG00151 source-path counterfactual.
// Preregistered in b7b4599 BEFORE this trial; no src/** changes or score edits.
#define main cs80_original_engine_audit_main
#include "../../src/tools/engine_audit.cpp"
#undef main

#include <bit>
#include <limits>
#include <memory>

namespace cs80 {
// The existing test-only friend declared in Engine.h. It changes only the two
// scalar audio-path gains AFTER the regular accepted 64-frame ControlFrame; it
// never bypasses oscillator, filter, EG, VCA or output-circuit clocking.
struct EngineEventContinuityProbe {
    enum class Path { full, cp3Muted, sineMuted, bothMuted, cp3EnabledAtEvent };

    static std::array<float, 4> gains(const Engine& e) noexcept {
        return {e.rankSettings[0].vcfLevel, e.rankSettings[1].vcfLevel,
                e.rankSettings[0].sine, e.rankSettings[1].sine};
    }
    static void route(Engine& e, Path path, bool atOrAfterEvent) noexcept {
        const bool muteFilter = path == Path::cp3Muted || path == Path::bothMuted
            || (path == Path::cp3EnabledAtEvent && !atOrAfterEvent);
        const bool muteSine = path == Path::sineMuted || path == Path::bothMuted;
        for (int rank = 0; rank < Engine::kNumRanks; ++rank) {
            auto& settings = e.rankSettings[(std::size_t)rank];
            if (muteFilter) settings.vcfLevel = 0.0f;
            if (muteSine) settings.sine = 0.0f;
        }
    }
};
} // namespace cs80

namespace {
using Path = cs80::EngineEventContinuityProbe::Path;

struct Fixture {
    const char* id;
    Setup setup;
    int rate;
    double time;
    bool noteOn;
    double frozenScore;
};

struct Trial {
    Rendered pcm;
    int zeroTimeVcaMismatches = 0;
    std::array<float,4> unmodifiedGains {};
};

struct Observation {
    const char* variant;
    Rendered pcm;
    double jump = 0.0;
    double ratio = 0.0;
    double deltaJump = 0.0;
    long peakOffset = 0;
    long prefixMismatch = 0;
    std::uint64_t nonfinite = 0;
};

long eventFrame(double time, int rate) {
    return (long)std::ceil(time * rate / kBlock) * kBlock;
}

bool matchesTarget(const Event& ev, const Fixture& f) {
    return ev.t == f.time && ev.on == f.noteOn && ev.note == 60;
}

std::vector<Event> withoutTarget(const std::vector<Event>& all,const Fixture& f) {
    auto copy=all;
    const auto end=std::remove_if(copy.begin(),copy.end(),[&](const Event& ev){return matchesTarget(ev,f);});
    require(std::distance(end,copy.end()) == 1,"target MIDI event not unique");
    copy.erase(end,copy.end());
    return copy;
}

long pcmBitDiff(const Rendered& a,const Rendered& b,long begin,long end) {
    require(a.l.size()==b.l.size() && a.r.size()==b.r.size(),"PCM shape mismatch");
    require(begin>=0 && begin<=end && end<=(long)a.l.size(),"invalid PCM interval");
    long differences=0;
    for(long i=begin;i<end;++i) {
        const auto j=(std::size_t)i;
        differences += std::bit_cast<std::uint32_t>(a.l[j]) != std::bit_cast<std::uint32_t>(b.l[j]);
        differences += std::bit_cast<std::uint32_t>(a.r[j]) != std::bit_cast<std::uint32_t>(b.r[j]);
    }
    return differences;
}

// Require a bit-identical modeled capacitor at the zero-time Funky gate edge.
std::array<cs80::Ig00151Vca::State,16> vcaSnapshot(const cs80::Engine& e) {
    std::array<cs80::Ig00151Vca::State,16> state {};
    for(int voice=0;voice<8;++voice)
        for(int rank=0;rank<2;++rank)
            state[(std::size_t)(voice*2+rank)]=e.getRankVcaState(voice,rank);
    return state;
}
int chargeBitsChanged(const std::array<cs80::Ig00151Vca::State,16>& a,
                      const std::array<cs80::Ig00151Vca::State,16>& b) {
    int changed=0;
    for(std::size_t i=0;i<a.size();++i) {
        changed += std::bit_cast<std::uint32_t>(a[i].level)!=std::bit_cast<std::uint32_t>(b[i].level);
        changed += std::bit_cast<std::uint64_t>(a[i].coupling)!=std::bit_cast<std::uint64_t>(b[i].coupling);
    }
    return changed;
}

Trial run(const Fixture& f, const std::vector<Event>& events,
          Path path, bool hasTarget) {
    auto engine=std::make_unique<cs80::Engine>();
    engine->prepare(f.rate);
    cs80::ControlFrame frame;
    frame.effectiveValues=f.setup.values;
    frame.hardwareSources=f.setup.sources;
    if(f.setup.useProgrammer)
        frame=cs80::ControlFrame::programmer(frame.effectiveValues,frame.hardwareSources);
    require(engine->setControlFrame(frame),"initial source frame rejected");
    engine->reset();

    const long event=eventFrame(f.time,f.rate);
    const long samples=(long)std::llround((f.time+.020)*f.rate);
    Trial out;
    out.pcm.rate=f.rate;
    out.pcm.l.resize((std::size_t)samples);
    out.pcm.r.resize((std::size_t)samples);
    std::array<float,kBlock*2> buffer {};
    std::size_t next=0;
    for(long first=0;first<samples;first+=kBlock) {
        const int n=(int)std::min<long>(kBlock,samples-first);
        const bool atTarget=first==event;
        std::array<cs80::Ig00151Vca::State,16> before {};
        if (atTarget && !f.noteOn && hasTarget) before=vcaSnapshot(*engine);
        while(next<events.size() && eventFrame(events[next].t,f.rate)<=first) {
            const auto& midi=events[next++];
            if(midi.on)engine->noteOn(midi.note,midi.vel,1);
            else engine->noteOff(midi.note,1);
        }
        require(engine->setControlFrame(frame),"midphrase source frame rejected");
        if (atTarget && !f.noteOn && hasTarget) {
            out.zeroTimeVcaMismatches=chargeBitsChanged(before,vcaSnapshot(*engine));
            require(out.zeroTimeVcaMismatches==0,"Funky noteOff resets modeled IG00151 charge");
        }
        const auto source=cs80::EngineEventContinuityProbe::gains(*engine);
        if(first==0)out.unmodifiedGains=source;
        cs80::EngineEventContinuityProbe::route(*engine,path,first>=event);
        const auto modified=cs80::EngineEventContinuityProbe::gains(*engine);
        for(int rank=0;rank<2;++rank) {
            const auto j=(std::size_t)rank;
            const bool mutedFilter=path==Path::cp3Muted||path==Path::bothMuted
                ||(path==Path::cp3EnabledAtEvent && first<event);
            const bool mutedSine=path==Path::sineMuted||path==Path::bothMuted;
            require(mutedFilter ? modified[j]==0.0f : modified[j]==source[j],
                    "CP3 test-only audio path intervention ineffective");
            require(mutedSine ? modified[j+2]==0.0f : modified[j+2]==source[j+2],
                    "sine test-only audio path intervention ineffective");
        }
        engine->render(buffer.data(),n);
        for(int i=0;i<n;++i) {
            const float left=buffer[(std::size_t)i*2],right=buffer[(std::size_t)i*2+1];
            out.pcm.nonfinite+=(std::uint64_t)(!std::isfinite(left)||!std::isfinite(right));
            out.pcm.l[(std::size_t)(first+i)]=std::isfinite(left)?left:0.0f;
            out.pcm.r[(std::size_t)(first+i)]=std::isfinite(right)?right:0.0f;
        }
    }
    require(out.pcm.nonfinite==0,"nonfinite source PCM");
    return out;
}

double differentialJump(const Rendered& a,const Rendered& b,long begin,long end) {
    double peak=0.0;
    for(long i=std::max(1L,begin);i<end;++i) {
        const auto j=(std::size_t)i,prev=(std::size_t)(i-1);
        peak=std::max({peak,
            std::fabs(((double)a.l[j]-a.l[prev])-((double)b.l[j]-b.l[prev])),
            std::fabs(((double)a.r[j]-a.r[prev])-((double)b.r[j]-b.r[prev]))});
    }
    return peak;
}

long derivativePeakOffset(const Rendered& r,long begin,long end) {
    double maximum=-1.0;long offset=0;
    for(long i=std::max(1L,begin);i<end;++i)
        for(const auto* ch:{&r.l,&r.r}) {
            const double value=std::fabs((double)(*ch)[(std::size_t)i]-(*ch)[(std::size_t)(i-1)]);
            if(value>maximum){maximum=value;offset=i-begin;}
        }
    return offset;
}

void c1SmoothInjection(Rendered& r,long first) {
    const long attack=(long)std::ceil(.004*r.rate);
    for(long sample=0;first+sample<(long)r.l.size();++sample) {
        const double t=std::min(sample,attack)/(double)attack;
        const double hann=.5-.5*std::cos(std::numbers::pi*t);
        const double wave=std::sin(2.0*std::numbers::pi*220.0*sample/r.rate);
        r.l[(std::size_t)(first+sample)]+=(float)(.10*hann*wave);
    }
}

void mutationOracles() {
    Rendered reference;
    reference.l={0.f,.01f,.02f,.03f,.04f,.05f};
    reference.r={0.f,0.f,0.f,0.f,0.f,0.f};
    auto modified=reference;
    require(pcmBitDiff(reference,modified,0,3)==0,"identical prefix rejected");
    modified.l[1]=std::nextafter(modified.l[1],std::numeric_limits<float>::infinity());
    require(pcmBitDiff(reference,modified,0,3)==1,"one-bit pre-event mutation escaped");
    modified=reference;
    modified.l[4]+=.2f;
    require(pcmBitDiff(reference,modified,3,6)==1,"one-sample downstream glitch escaped");
    modified.r[5]+=.2f;
    require(pcmBitDiff(reference,modified,3,6)==2,"two-sample downstream glitch misclassified");
    auto invalidEvents=std::vector<Event>{{1.4,false,60,0.0f}};
    Fixture f{"oracle",{},48000,1.4,true,0.0};
    require(std::none_of(invalidEvents.begin(),invalidEvents.end(),
                         [&](const Event& event){return matchesTarget(event,f);}),
            "wrong event polarity escaped identity control");
}

void verifyPrereg(const std::string& path,const std::string& amendment,
                  const std::vector<Fixture>& fixtures) {
    std::ifstream input(path);
    require((bool)input,"registered causal contract unavailable");
    const std::string document((std::istreambuf_iterator<char>(input)),{});
    require(document.find("\"status\": \"REGISTERED\"")!=std::string::npos,
            "causal experiment not preregistered");
    require(document.find("7dd159d491012fd47b46c0aa651e86ea959ca289")!=std::string::npos,
            "frozen core source identity missing from preregistration");
    require(document.find("\"num_engine_renders\": 63")!=std::string::npos,
            "registered trial size has changed");
    for(const auto& fixture:fixtures)
        require(document.find(std::string("\"id\":\"")+fixture.id+"\"")!=std::string::npos,
                "fixture omitted from preregistration");
    std::ifstream amended(amendment);
    require((bool)amended,"registered positive-control amendment unavailable");
    const std::string correction((std::istreambuf_iterator<char>(amended)),{});
    require(correction.find("\"status\": \"REGISTERED_BEFORE_SECOND_TRIAL\"")!=std::string::npos &&
            correction.find("b7b4599")!=std::string::npos &&
            correction.find("7dd159d491012fd47b46c0aa651e86ea959ca289")!=std::string::npos,
            "positive-control amendment source or chronology not registered");
}

void writeObservation(std::ostream& csv,const Fixture& f,const char* name,
                      const Rendered& r,const Rendered* omitted,double reference,
                      double fullJump,long frame,long end,int zeroTimeBits) {
    const double jump=maxJump(r,frame,end);
    const double ratio=jump/fullJump;
    const double delta=omitted?differentialJump(r,*omitted,frame,end):0.0;
    const long prefix=omitted?pcmBitDiff(r,*omitted,0,frame):0;
    csv<<f.id<<','<<f.rate<<','<<name<<','<<frame<<','<<reference<<','<<jump<<','<<delta<<','
       <<derivativePeakOffset(r,frame,end)<<','<<ratio<<','<<prefix<<','<<r.nonfinite<<','
       <<zeroTimeBits<<'\n';
    require(std::isfinite(jump)&&std::isfinite(ratio)&&std::isfinite(delta),"nonfinite metric");
    if(omitted)require(prefix==0,"paired path event/omission pre-event PCM not bit-identical");
    require(!r.nonfinite,"bad PCM in observed variant");
}

void trial(const Fixture& f,std::ostream& csv,int& primarySuccess,int& renders) {
    const auto fullEvents=phrase(),omitEvents=withoutTarget(fullEvents,f);
    const long frame=eventFrame(f.time,f.rate);
    const long end=frame+(long)std::llround(.008*f.rate);
    const auto full=run(f,fullEvents,Path::full,true);++renders;
    const auto omit=run(f,omitEvents,Path::full,false);++renders;
    const auto cp3=run(f,fullEvents,Path::cp3Muted,true);++renders;
    const auto cp3Omit=run(f,omitEvents,Path::cp3Muted,false);++renders;
    const auto sine=run(f,fullEvents,Path::sineMuted,true);++renders;
    const auto sineOmit=run(f,omitEvents,Path::sineMuted,false);++renders;
    const auto zero=run(f,fullEvents,Path::bothMuted,true);++renders;
    const auto zeroOmit=run(f,omitEvents,Path::bothMuted,false);++renders;
    // Separately preregistered amendment ff05da3: without a first Bass note,
    // there is no sounding carrier to respond to an abrupt CP3 gain switch.
    // For BOTH Bass onsets, retain the matching noteOn to keep a live carrier;
    // for Funky noteOff, omit the release so a held note supplies the carrier.
    const auto step=run(f,f.noteOn?fullEvents:omitEvents,
                        Path::cp3EnabledAtEvent,f.noteOn);++renders;
    require(end<=(long)full.pcm.l.size(),"event window exceeds frozen render");
    require(full.unmodifiedGains==omit.unmodifiedGains &&
            full.unmodifiedGains==cp3.unmodifiedGains &&
            full.unmodifiedGains==sine.unmodifiedGains &&
            full.unmodifiedGains==zero.unmodifiedGains,"path inputs changed original source controls");
    require(full.unmodifiedGains[0]+full.unmodifiedGains[1]>0.0f,
            "non-vacuity: source CP3 gains were already zero");

    const double originalReference=f.noteOn
        ? std::max(maxJump(full.pcm,idx(full.pcm,T::chordOn),idx(full.pcm,T::chordOff)),1.e-7)
        : std::max(maxJump(full.pcm,frame-(long)(.06*f.rate),frame),1.e-7);
    const double fullJump=maxJump(full.pcm,frame,end);
    const double originalScore=fullJump/originalReference;
    require(std::fabs(originalScore-f.frozenScore)<1.e-5,
            "source original click gate score unexpectedly changed");
    require(fullJump>0.0&&std::isfinite(fullJump),"vacuous full-path derivative");
    require(pcmBitDiff(full.pcm,omit.pcm,0,frame)==0,"native event/omission PCM prefix differs");
    require(pcmBitDiff(cp3.pcm,cp3Omit.pcm,0,frame)==0,"muted CP3 event/omit prefix differs");
    require(pcmBitDiff(sine.pcm,sineOmit.pcm,0,frame)==0,"muted sine event/omit prefix differs");
    require(pcmBitDiff(zero.pcm,zeroOmit.pcm,0,frame)==0,"muted carriers event/omit prefix differs");
    const auto& stepComparison=f.noteOn?cp3.pcm:cp3Omit.pcm;
    require(pcmBitDiff(stepComparison,step.pcm,0,frame)==0,"CP3 step changed pre-event prefix");
    require(pcmBitDiff(stepComparison,step.pcm,frame,(long)step.pcm.l.size())>0,
            "in-engine positive CP3 branch step had no PCM effect");
    const double ratio=maxJump(zero.pcm,frame,end)/fullJump;
    primarySuccess+=(ratio<=0.10);
    const auto zBits=full.zeroTimeVcaMismatches;

    writeObservation(csv,f,"full_source_event",full.pcm,&omit.pcm,originalReference,fullJump,frame,end,zBits);
    writeObservation(csv,f,"full_omitted_event",omit.pcm,nullptr,originalReference,fullJump,frame,end,0);
    writeObservation(csv,f,"filter_cp3_muted_event",cp3.pcm,&cp3Omit.pcm,originalReference,fullJump,frame,end,cp3.zeroTimeVcaMismatches);
    writeObservation(csv,f,"filter_cp3_muted_omitted_event",cp3Omit.pcm,nullptr,originalReference,fullJump,frame,end,0);
    writeObservation(csv,f,"sine_bypass_muted_event",sine.pcm,&sineOmit.pcm,originalReference,fullJump,frame,end,sine.zeroTimeVcaMismatches);
    writeObservation(csv,f,"sine_bypass_muted_omitted_event",sineOmit.pcm,nullptr,originalReference,fullJump,frame,end,0);
    writeObservation(csv,f,"all_carriers_muted_event",zero.pcm,&zeroOmit.pcm,originalReference,fullJump,frame,end,zero.zeroTimeVcaMismatches);
    writeObservation(csv,f,"all_carriers_muted_omitted_event",zeroOmit.pcm,nullptr,originalReference,fullJump,frame,end,0);
    writeObservation(csv,f,f.noteOn?"filter_cp3_enabled_at_event_with_bass_note_on"
                                     :"filter_cp3_enabled_at_event_with_funky_note_off_omitted",
                     step.pcm,nullptr,originalReference,fullJump,frame,end,0);

    auto glitch=zeroOmit.pcm;
    glitch.l[(std::size_t)(frame+1)]+=.20f;
    require(pcmBitDiff(glitch,zeroOmit.pcm,0,frame)==0,
            "digital positive changes pre-event PCM");
    require(pcmBitDiff(glitch,zeroOmit.pcm,frame,(long)glitch.l.size())==1,
            "digital positive was not exactly one sample");
    require(differentialJump(glitch,zeroOmit.pcm,frame,end)>.19,
            "digital positive not detected as one-sample derivative");
    writeObservation(csv,f,"known_positive_one_sample_020",glitch,nullptr,originalReference,fullJump,frame,end,0);
    auto smooth=zeroOmit.pcm;
    c1SmoothInjection(smooth,frame);
    require(pcmBitDiff(smooth,zeroOmit.pcm,0,frame)==0,
            "smooth negative changes pre-event PCM");
    require(pcmBitDiff(smooth,zeroOmit.pcm,frame,(long)smooth.l.size())>0,
            "smooth negative not injected");
    require(std::bit_cast<std::uint32_t>(smooth.l[(std::size_t)frame]) ==
            std::bit_cast<std::uint32_t>(zeroOmit.pcm.l[(std::size_t)frame]),
            "negative control starts with a digital step");
    writeObservation(csv,f,"known_c1_smooth_220hz",smooth,nullptr,originalReference,fullJump,frame,end,0);

    std::cerr<<std::setprecision(9)<<"fixture "<<f.id
             <<" original_score="<<originalScore<<" original_jump="<<fullJump
             <<" cp3_muted="<<maxJump(cp3.pcm,frame,end)/fullJump
             <<" sine_muted="<<maxJump(sine.pcm,frame,end)/fullJump
             <<" all_carriers_muted="<<ratio
             <<" source_gains="<<full.unmodifiedGains[0]<<'/'<<full.unmodifiedGains[1]
             <<" sine_gains="<<full.unmodifiedGains[2]<<'/'<<full.unmodifiedGains[3]
             <<" positive_step_delta="<<differentialJump(step.pcm,stepComparison,frame,end)
             <<" zero_time_vca_bit_changes="<<zBits<<'\n';
}
} // namespace

int main(int argc,char** argv) {
    try {
        require(argc==7 && std::string(argv[1])=="--prereg" &&
                std::string(argv[3])=="--amendment" && std::string(argv[5])=="--out",
                "usage: probe --prereg CONTRACT --amendment AMENDMENT --out CSV");
        const auto bank=programSetups(true),tabs=tabSetups(true);
        const auto bass=std::find_if(bank.begin(),bank.end(),[](const Setup& s){return s.name=="Bass Stack";});
        const auto funky=std::find_if(tabs.begin(),tabs.end(),[](const Setup& s){return s.name=="Funky 1 + Funky 2";});
        require(bass!=bank.end() && funky!=tabs.end(),"frozen Bass or Funky fixture missing");
        Setup panel=*bass;panel.useProgrammer=false;
        const std::vector<Fixture> fixtures {
            {"bass_panel_first_48000",panel,48000,T::retrigOn1,true,1.123291366},
            {"bass_panel_retrigger_48000",panel,48000,T::retrigOn2,true,1.440924029},
            {"bass_panel_first_96000",panel,96000,T::retrigOn1,true,1.106078572},
            {"bass_panel_retrigger_96000",panel,96000,T::retrigOn2,true,1.365308570},
            {"funky_tabs_off_44100",*funky,44100,T::singleOff,false,1.626889135},
            {"funky_tabs_off_48000",*funky,48000,T::singleOff,false,1.291310019},
            {"funky_tabs_off_96000",*funky,96000,T::singleOff,false,1.220958165}
        };
        verifyPrereg(argv[2],argv[4],fixtures);
        mutationOracles();
        std::ofstream csv(argv[6],std::ios::binary);
        require((bool)csv,"cannot open trial CSV");
        csv.imbue(std::locale::classic());
        csv<<std::setprecision(17);
        csv<<"fixture,rate,variant,event_frame,original_reference,maxjump,event_minus_omit_maxjump,peak_frame_offset,ratio_to_full_event,pre_event_pcm_mismatches,nonfinite,zero_time_vca_charge_bit_changes\n";
        int successes=0,renders=0;
        for(const auto& fixture:fixtures)trial(fixture,csv,successes,renders);
        require(renders==63,"trial run departed from 63 preregistered engine renders");
        csv.flush();require((bool)csv,"failed to write complete trial CSV");
        std::cerr<<"RESULT primary_audio_carrier_suppression_successes="<<successes
                 <<"/7 prereg_hypothesis="<<(successes>=6?"SUPPORTED":"FALSIFIED")
                 <<" source_tree=7dd159d491012fd47b46c0aa651e86ea959ca289"
                 <<" engine_renders="<<renders<<" csv_rows=77\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"CLICK_M41_VCA_PATH_FAILED: "<<ex.what()<<'\n';
        return 1;
    }
}
