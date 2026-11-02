// Deterministic, unprocessed A/B audition slices from the FROZEN audit engine.
// No fade, normalization, limiter, resampling, gate or source model change.
#define main cs80_original_engine_audit_main
#include "../../src/tools/engine_audit.cpp"
#undef main
#include <bit>
#include <cstdint>

namespace {

enum class Identity { firstOn, retrigger, singleOff, steal };
struct Candidate {
    const char* stem;
    Setup setup;
    int rate;
    double event;
    Identity identity;
    int removedCount;
};

void bytes(std::ofstream& out, const char* literal, std::size_t length) {
    out.write(literal, (std::streamsize) length);
}
void u16(std::ofstream& out, std::uint16_t value) {
    for (int i = 0; i < 2; ++i) out.put((char) ((value >> (i * 8)) & 255));
}
void u32(std::ofstream& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) out.put((char) ((value >> (i * 8)) & 255));
}

// WAVE_FORMAT_IEEE_FLOAT, 32-bit stereo, exact host sample bits, with fact.
void writeWav(const fs::path& path, const Rendered& a, const Rendered& b,
              long first, long last, int mode) {
    require(first >= 0 && last > first && last <= (long) a.l.size()
            && a.l.size() == b.l.size(), "bad audition slice");
    const std::uint32_t dataBytes = (std::uint32_t) ((last - first) * 2 * sizeof(float));
    std::ofstream out(path, std::ios::binary);
    require((bool) out, "cannot write exact WAV");
    bytes(out,"RIFF",4); u32(out, 48 + dataBytes); bytes(out,"WAVE",4);
    bytes(out,"fmt ",4); u32(out,16); u16(out,3); u16(out,2);
    u32(out,(std::uint32_t)a.rate); u32(out,(std::uint32_t)a.rate*8); u16(out,8); u16(out,32);
    bytes(out,"fact",4); u32(out,4); u32(out,(std::uint32_t)(last-first));
    bytes(out,"data",4); u32(out,dataBytes);
    for (long i = first; i < last; ++i)
        for (int c : {0,1}) {
            const float event = c ? a.r[(std::size_t)i] : a.l[(std::size_t)i];
            const float omitted = c ? b.r[(std::size_t)i] : b.l[(std::size_t)i];
            const float x = mode == 0 ? event : mode == 1 ? omitted : event-omitted;
            require(std::isfinite(x), "nonfinite WAV sample");
            u32(out, std::bit_cast<std::uint32_t>(x));
        }
    require((bool)out, "incomplete WAV write");
}

bool target(const Event& e, const Candidate& s) {
    if (e.t != s.event) return false;
    if (s.identity == Identity::singleOff) return !e.on && e.note == 60;
    if (s.identity == Identity::firstOn || s.identity == Identity::retrigger)
        return e.on && e.note == 60;
    return e.on && (e.note == 48 || e.note == 52 || e.note == 55 || e.note == 60);
}

void renderCandidate(const Candidate& candidate, const fs::path& output, std::ostream& manifest) {
    const auto full = phrase();
    auto omitted = full;
    const auto it = std::remove_if(omitted.begin(), omitted.end(),
                                   [&](const Event& e){return target(e,candidate);});
    const int count = (int)std::distance(it, omitted.end());
    require(count == candidate.removedCount, "unexpected target event identity/count");
    omitted.erase(it, omitted.end());
    const int rate = candidate.rate;
    const long eventFrame = (long) std::ceil(candidate.event * rate / kBlock) * kBlock;
    const long first = eventFrame - (long) std::llround(.075 * rate);
    const long last = eventFrame + (long) std::llround(.300 * rate);
    const auto baseline = render(candidate.setup, rate, full, candidate.event + .32);
    const auto control = render(candidate.setup, rate, omitted, candidate.event + .32);
    require(!baseline.nonfinite && !control.nonfinite, "non-finite exact engine PCM");
    long prefixMismatch = 0;
    for (long i = 0; i < eventFrame; ++i)
        for (int c : {0,1}) {
            const auto& a = c ? baseline.r : baseline.l;
            const auto& b = c ? control.r : control.l;
            prefixMismatch += std::bit_cast<std::uint32_t>(a[(std::size_t)i]) !=
                              std::bit_cast<std::uint32_t>(b[(std::size_t)i]);
        }
    require(prefixMismatch == 0, "audition A/B differs before its omitted event");
    const double ref = candidate.identity == Identity::singleOff
        ? std::max(maxJump(baseline, eventFrame-(long)(.06*rate),eventFrame),1.e-7)
        : candidate.identity == Identity::steal
          ? std::max(maxJump(baseline,idx(baseline,T::bigOn),idx(baseline,T::bigOff)),1.e-7)
          : std::max(maxJump(baseline,idx(baseline,T::chordOn),idx(baseline,T::chordOff)),1.e-7);
    const long post = (long) std::llround(.008 * rate);
    const double originalScore = maxJump(baseline,eventFrame,eventFrame+post)/ref;
    const double omittedScore = maxJump(control,eventFrame,eventFrame+post)/ref;
    double eventOnly = 0, sqBase=0, sqControl=0, sqDiff=0, peakBase=0, peakDiff=0;
    const long n = last-first;
    for(long i=first;i<last;++i)
        for(int c:{0,1}) {
            const float a = c?baseline.r[(std::size_t)i]:baseline.l[(std::size_t)i];
            const float b = c?control.r[(std::size_t)i]:control.l[(std::size_t)i];
            const double d = (double)(a-b);
            sqBase+=(double)a*a; sqControl+=(double)b*b; sqDiff+=d*d;
            peakBase=std::max(peakBase,std::fabs((double)a));
            peakDiff=std::max(peakDiff,std::fabs(d));
        }
    for(long i=eventFrame;i<eventFrame+post;++i)
        for(int c:{0,1}) {
            const auto& a=c?baseline.r:baseline.l;
            const auto& b=c?control.r:control.l;
            const std::size_t j=(std::size_t)i,p=(std::size_t)(i-1);
            eventOnly=std::max(eventOnly,std::fabs((double)(a[j]-a[p])-(double)(b[j]-b[p])));
        }
    const std::string stem=candidate.stem;
    for (int mode=0;mode<3;++mode)
        writeWav(output/(stem+(mode==0?"_A_event.wav":mode==1?"_B_omission.wav":"_C_difference.wav")),
                 baseline,control,first,last,mode);
    manifest << stem << ',' << rate << ',' << candidate.event << ',' << eventFrame << ','
             << first << ',' << last << ',' << count << ',' << prefixMismatch << ','
             << originalScore << ',' << omittedScore << ',' << (eventOnly/ref) << ','
             << std::sqrt(sqBase/(2*n)) << ',' << std::sqrt(sqControl/(2*n)) << ','
             << std::sqrt(sqDiff/(2*n)) << ',' << peakBase << ',' << peakDiff << '\n';
    manifest.flush();
    std::cerr << "rendered " << stem << " rate=" << rate << " original=" << originalScore
              << " omitted=" << omittedScore << " exact_prefix=" << prefixMismatch << '\n';
}
} // namespace

int main(int argc,char**argv) {
    try {
        require(argc==2, "usage: ClickAuditionAB NEW_EMPTY_DIRECTORY");
        const fs::path output=argv[1];
        require(!fs::exists(output), "output directory already exists; refusing to overwrite clips");
        require(fs::create_directories(output), "cannot create clip output directory");
        std::ofstream manifest(output/"manifest.csv");
        require((bool)manifest, "cannot write manifest");
        manifest << "stem,rate,event_seconds,event_frame,first_frame,last_frame,omitted_events,pre_pcm_bit_mismatches,"
                    "original_score,omitted_score,event_only_max_jump_score,baseline_clip_rms,omitted_clip_rms,"
                    "difference_clip_rms,baseline_clip_peak,difference_clip_peak\n";
        manifest << std::setprecision(12);
        auto programs=programSetups(true),tabs=tabSetups(true);
        auto bass=std::find_if(programs.begin(),programs.end(),[](const Setup& s){return s.name=="Bass Stack";});
        auto funky=std::find_if(tabs.begin(),tabs.end(),[](const Setup& s){return s.name=="Funky 1 + Funky 2";});
        auto clav=std::find_if(tabs.begin(),tabs.end(),[](const Setup& s){return s.name=="Clavichord 1 + Clavichord 2";});
        require(bass!=programs.end()&&funky!=tabs.end()&&clav!=tabs.end(),"missing frozen fixtures");
        auto physical=*bass; physical.useProgrammer=false;
        const std::vector<Candidate> scenarios {
            {"bass_panel_first_48000",physical,48000,T::retrigOn1,Identity::firstOn,1},
            {"bass_panel_retrigger_48000",physical,48000,T::retrigOn2,Identity::retrigger,1},
            {"bass_panel_first_96000",physical,96000,T::retrigOn1,Identity::firstOn,1},
            {"bass_panel_retrigger_96000",physical,96000,T::retrigOn2,Identity::retrigger,1},
            {"funky_tabs_noteoff_44100",*funky,44100,T::singleOff,Identity::singleOff,1},
            {"funky_tabs_noteoff_48000",*funky,48000,T::singleOff,Identity::singleOff,1},
            {"funky_tabs_noteoff_96000",*funky,96000,T::singleOff,Identity::singleOff,1},
            {"clav_tabs_steal_96000",*clav,96000,T::stealOn,Identity::steal,4}
        };
        for(const auto& item:scenarios)renderCandidate(item,output,manifest);
        return 0;
    }catch(const std::exception& err){std::cerr<<"ClickAuditionAB: "<<err.what()<<'\n';return 1;}
}
