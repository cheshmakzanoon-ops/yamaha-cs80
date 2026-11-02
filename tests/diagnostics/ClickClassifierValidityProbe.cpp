// Post-render PCM classifier-validity controls. Preregistered FIRST in fd192a7.
// Deliberate digital single-sample corruption never touches Engine or src/.
#define main cs80_original_engine_audit_main
#include "../../src/tools/engine_audit.cpp"
#undef main

#include <bit>
#include <limits>

namespace {

enum class EventKind { firstOn, retrigger, off, steal };
struct Fixture {
    const char* id;
    const char* coordinate;
    Setup setup;
    int rate;
    double seconds;
    EventKind kind;
    int noteCount;
};

enum class Control { omission, smooth220, smooth1760, impulseL005, impulseL020,
                     impulseR005, impulseR020, originalSource };
struct ControlSpec { Control control; const char* label; const char* classification; };

constexpr std::array<ControlSpec,8> controls {{{Control::omission,"no_target_event","negative_no_event"},
    {Control::smooth220,"c1_220hz_pitched_attack","negative_c1_smooth_pitch"},
    {Control::smooth1760,"c1_1760hz_pitched_attack","negative_c1_smooth_pitch"},
    {Control::impulseL005,"impulse_left_1frame_005","positive_one_sample_impulse"},
    {Control::impulseL020,"impulse_left_1frame_020","positive_one_sample_impulse"},
    {Control::impulseR005,"impulse_right_6ms_005","positive_one_sample_impulse"},
    {Control::impulseR020,"impulse_right_6ms_020","positive_one_sample_impulse"},
    {Control::originalSource,"unchanged_original_source_event_pcm","unlabeled_source_observation"}}};

long blockStart(double t, int rate) {
    return (long)std::ceil(t*rate/kBlock)*kBlock;
}

bool matchesEvent(const Event& e, const Fixture& f) {
    if (e.t != f.seconds) return false;
    if (f.kind == EventKind::off) return !e.on && e.note==60;
    if (f.kind == EventKind::firstOn || f.kind==EventKind::retrigger)
        return e.on && e.note==60;
    return e.on && (e.note==48 || e.note==52 || e.note==55 || e.note==60);
}

std::size_t bitMismatches(const Rendered& a, const Rendered& b, long begin, long end) {
    require(a.l.size() == b.l.size() && a.r.size() == b.r.size(), "PCM shape mismatched");
    require(begin>=0 && end>=begin && end<=(long)a.l.size(), "bad comparison interval");
    std::size_t diff=0;
    for(long i=begin;i<end;++i) {
        const auto j=(std::size_t)i;
        diff += std::bit_cast<std::uint32_t>(a.l[j]) != std::bit_cast<std::uint32_t>(b.l[j]);
        diff += std::bit_cast<std::uint32_t>(a.r[j]) != std::bit_cast<std::uint32_t>(b.r[j]);
    }
    return diff;
}

bool oneSamplePositive(const Rendered& original,const Rendered& changed,long eventFrame) {
    return bitMismatches(original,changed,eventFrame,(long)original.l.size())==1;
}
bool c1StartContinuous(const Rendered& original,const Rendered& changed,long eventFrame) {
    return std::bit_cast<std::uint32_t>(original.l[(std::size_t)eventFrame])==
               std::bit_cast<std::uint32_t>(changed.l[(std::size_t)eventFrame]) &&
           std::bit_cast<std::uint32_t>(original.r[(std::size_t)eventFrame])==
               std::bit_cast<std::uint32_t>(changed.r[(std::size_t)eventFrame]);
}
bool referenceUnmodified(double observed, double frozen) {
    return std::bit_cast<std::uint64_t>(observed)==std::bit_cast<std::uint64_t>(frozen);
}

void mutationOracles() {
    Rendered r,copy;
    r.rate=48000;
    r.l={.1f,.2f,.3f,.4f,.5f,.6f};
    r.r={-.1f,-.2f,-.3f,-.4f,-.5f,-.6f};
    copy=r;
    require(bitMismatches(r,copy,0,3)==0,"valid exact prefix control rejected");
    copy.l[1]=std::nextafter(copy.l[1],std::numeric_limits<float>::infinity());
    require(bitMismatches(r,copy,0,3)!=0,"mutated pre-event sample escaped rejection");
    copy=r;copy.l[4]+=.05f;
    require(oneSamplePositive(r,copy,3),"valid one-sample positive control rejected");
    copy.r[5]+=.05f;
    require(!oneSamplePositive(r,copy,3),"two-sample digital glitch escaped rejection");
    copy=r;
    require(c1StartContinuous(r,copy,3),"valid continuous onset rejected");
    copy.l[3]+=.05f;
    require(!c1StartContinuous(r,copy,3),"nonzero smooth onset escaped rejection");
    require(referenceUnmodified(.023,.023),"unchanged score reference rejected");
    require(!referenceUnmodified(.023+.001,.023),"changed score reference escaped rejection");
}

void applySmoothPitch(Rendered& x,long f,double frequency,double amp) {
    const long L=(long)std::ceil(.004*x.rate);
    for(long n=0;f+n<(long)x.l.size();++n) {
        const double u=std::min(n,L)/(double)L;
        const double envelope=.5-.5*std::cos(std::numbers::pi*u);
        const double oscillator=std::sin(2.0*std::numbers::pi*frequency*n/x.rate);
        x.l[(std::size_t)(f+n)]+=(float)(amp*envelope*oscillator);
    }
}

void applySingleSample(Rendered& x,long f,int channel,double amplitude) {
    const std::size_t i=(std::size_t)f;
    if (channel==0)x.l[i]+=(float)amplitude;
    else x.r[i]+=(float)amplitude;
}

void requirePrereg(const std::string& filename,const std::vector<Fixture>& fixtures) {
    std::ifstream input(filename);
    require((bool)input,"preregistered classifier contract missing");
    const std::string registered((std::istreambuf_iterator<char>(input)),{});
    require(registered.find("\"status\": \"REGISTERED\"")!=std::string::npos,
            "classifier contract not registered before trial");
    require(registered.find("7dd159d491012fd47b46c0aa651e86ea959ca289")!=std::string::npos,
            "source tree preregistration changed");
    require(registered.find("\"required_total_rows\": 88")!=std::string::npos,
            "registered expected row count changed");
    for(const auto& f:fixtures)
        require(registered.find(std::string("\"id\": \"")+f.id+"\"")!=std::string::npos,
                "fixture identity missing from preregistration");
}

void trial(const Fixture& f,std::ostream& out,int& rows) {
    const auto events=phrase();
    auto omitted=events;
    const auto it=std::remove_if(omitted.begin(),omitted.end(),[&](const Event& e){return matchesEvent(e,f);});
    const int erased=(int)std::distance(it,omitted.end());
    require(erased==f.noteCount,"registered target omission count mismatch");
    omitted.erase(it,omitted.end());
    const long frame=blockStart(f.seconds,f.rate),window=(long)std::llround(.008*f.rate);
    const double seconds=f.seconds+.020;
    const auto original=render(f.setup,f.rate,events,seconds);
    const auto noEvent=render(f.setup,f.rate,omitted,seconds);
    require(!original.nonfinite&&!noEvent.nonfinite,"render generated non-finite PCM");
    require(bitMismatches(original,noEvent,0,frame)==0,"original/omission prefix changed");
    const double ref=f.kind==EventKind::off
        ? std::max(maxJump(original,frame-(long)(.06*f.rate),frame),1.e-7)
        : f.kind==EventKind::steal
          ? std::max(maxJump(original,idx(original,T::bigOn),idx(original,T::bigOff)),1.e-7)
          : std::max(maxJump(original,idx(original,T::chordOn),idx(original,T::chordOff)),1.e-7);
    require(ref>0.0&&std::isfinite(ref),"original score denominator invalid");
    const double frozenRef=ref;
    const double omissionScore=maxJump(noEvent,frame,frame+window)/ref;
    const double sourceEventScore=maxJump(original,frame,frame+window)/ref;
    for(const auto& variantSpec:controls) {
        Rendered candidate=noEvent;
        switch(variantSpec.control) {
            case Control::omission:break;
            case Control::smooth220:applySmoothPitch(candidate,frame,220.0,.1);break;
            case Control::smooth1760:applySmoothPitch(candidate,frame,1760.0,.2);break;
            case Control::impulseL005:applySingleSample(candidate,frame+1,0,.05);break;
            case Control::impulseL020:applySingleSample(candidate,frame+1,0,.20);break;
            case Control::impulseR005:applySingleSample(candidate,frame+(long)std::llround(.006*f.rate),1,.05);break;
            case Control::impulseR020:applySingleSample(candidate,frame+(long)std::llround(.006*f.rate),1,.20);break;
            case Control::originalSource:candidate=original;break;
        }
        require(referenceUnmodified(ref,frozenRef),"original score reference mutated");
        const auto prefix=bitMismatches(noEvent,candidate,0,frame);
        require(prefix==0,"variant unexpectedly changed pre-event PCM");
        const auto changed=bitMismatches(noEvent,candidate,frame,(long)candidate.l.size());
        if (variantSpec.control==Control::omission)require(changed==0,"no-event negative changed PCM");
        if (variantSpec.control==Control::smooth220||variantSpec.control==Control::smooth1760)
            require(c1StartContinuous(noEvent,candidate,frame),"smooth musical onset has nonzero first sample");
        if (variantSpec.control==Control::impulseL005||variantSpec.control==Control::impulseL020||
            variantSpec.control==Control::impulseR005||variantSpec.control==Control::impulseR020)
            require(oneSamplePositive(noEvent,candidate,frame),"single-sample impulse not independent support");
        const double score=maxJump(candidate,frame,frame+window)/ref;
        out<<f.id<<','<<f.coordinate<<','<<f.rate<<','<<f.seconds<<','<<variantSpec.label
           <<','<<variantSpec.classification<<','<<prefix<<','<<changed<<','<<ref
           <<','<<(score*ref)<<','<<score<<','<<(score>1.0?1:0)
           <<','<<omissionScore<<','<<(score-omissionScore)<<','<<sourceEventScore<<'\n';
        ++rows;
    }
    out.flush();
    std::cerr<<"validity fixture "<<f.id<<" rate="<<f.rate
             <<" original_score="<<sourceEventScore
             <<" no_event_score="<<omissionScore<<'\n';
}

} // namespace

int main(int argc,char**argv) {
    try {
        const bool stdoutMode=argc==4&&std::string(argv[1])=="--prereg"&&std::string(argv[3])=="--stdout";
        const bool outFileMode=argc==5&&std::string(argv[1])=="--prereg"&&std::string(argv[3])=="--out";
        require(stdoutMode||outFileMode,
                "usage: ClickClassifierValidityProbe --prereg REGISTERED_JSON --stdout | --out NEW_CSV");
        mutationOracles();
        auto programs=programSetups(true),tabs=tabSetups(true);
        auto bass=std::find_if(programs.begin(),programs.end(),[](const Setup& s){return s.name=="Bass Stack";});
        auto funky=std::find_if(tabs.begin(),tabs.end(),[](const Setup& s){return s.name=="Funky 1 + Funky 2";});
        auto clav=std::find_if(tabs.begin(),tabs.end(),[](const Setup& s){return s.name=="Clavichord 1 + Clavichord 2";});
        require(bass!=programs.end()&&funky!=tabs.end()&&clav!=tabs.end(),"registered source fixtures missing");
        auto physical=*bass;physical.useProgrammer=false;
        const auto electrical=*bass;
        const std::vector<Fixture> fixtures {
            {"bass_panel_retrigger_44100","original_physical_panel",physical,44100,T::retrigOn2,EventKind::retrigger,1},
            {"bass_panel_first_48000","original_physical_panel",physical,48000,T::retrigOn1,EventKind::firstOn,1},
            {"bass_panel_retrigger_48000","original_physical_panel",physical,48000,T::retrigOn2,EventKind::retrigger,1},
            {"bass_panel_first_96000","original_physical_panel",physical,96000,T::retrigOn1,EventKind::firstOn,1},
            {"bass_panel_retrigger_96000","original_physical_panel",physical,96000,T::retrigOn2,EventKind::retrigger,1},
            {"bass_programmer_retrigger_48000","electrical_programmer",electrical,48000,T::retrigOn2,EventKind::retrigger,1},
            {"bass_programmer_retrigger_96000","electrical_programmer",electrical,96000,T::retrigOn2,EventKind::retrigger,1},
            {"funky_tabs_off_44100","tone_tabs",*funky,44100,T::singleOff,EventKind::off,1},
            {"funky_tabs_off_48000","tone_tabs",*funky,48000,T::singleOff,EventKind::off,1},
            {"funky_tabs_off_96000","tone_tabs",*funky,96000,T::singleOff,EventKind::off,1},
            {"clav_tabs_steal_96000","tone_tabs",*clav,96000,T::stealOn,EventKind::steal,4}
        };
        require(fixtures.size()==11,"registered fixture count changed");
        requirePrereg(argv[2],fixtures);
        std::ofstream file;
        if (outFileMode) {
            require(!fs::exists(argv[4]),"refusing to overwrite existing classifier evidence CSV");
            file.open(argv[4]);
            require((bool)file,"cannot create classifier evidence CSV");
        }
        std::ostream& out=outFileMode?static_cast<std::ostream&>(file):std::cout;
        out<<"fixture,coordinate,rate,event_seconds,control,control_class,pre_event_pcm_bit_mismatches,changed_output_samples,original_reference_jump,post_window_max_jump,candidate_score,alert_gt_1,omission_baseline_score,score_increment_vs_omission,source_event_score\n";
        out<<std::setprecision(12);
        int rows=0;
        for(const auto& fixture:fixtures)trial(fixture,out,rows);
        require(rows==88,"registered outcome row count mismatch");
        std::cerr<<"classifier validity mutation oracles PASS; exact rows="<<rows<<'\n';
        return 0;
    }catch(const std::exception& err){std::cerr<<"ClickClassifierValidityProbe: "<<err.what()<<'\n';return 1;}
}
