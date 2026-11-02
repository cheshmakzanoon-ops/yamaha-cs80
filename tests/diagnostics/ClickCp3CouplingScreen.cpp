// Source-visible CP3 input capacitor vs inferred post-IG00151 coupling.
// Preregistered first: commit 4743032. Standalone shadow simulation; no src edit.
#include "core/Vca.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <locale>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
struct Fixture { int rate; double reference; };
enum class Stimulus { pitched, dc, zero, heldPitched };
struct Scenario {
    const char* label;
    Stimulus stimulus;
    double frequency;
    double amplitude;
};
struct Stereo {
    std::vector<float> l,r;
    int rate=0;
    long first=0;
    std::uint64_t nonfinite=0;
};
struct DoublePath { Stereo existing, extraInputCap; };

class IdealCp3InputCap {
public:
    explicit IdealCp3InputCap(int rate)
        : coefficient(-std::expm1(-1.0 / (cs80::kIg00151CouplingSeconds * rate))) {}
    float process(float input) {
        // Ideal stiff CP3 driving a 1uF nonpolarized cap and idealized 100k
        // resistance. The actual M41 source and IC input impedance are open.
        capacitor += ((double)input - capacitor) * coefficient;
        return (float)((double)input - capacitor);
    }
private:
    double coefficient=0.0,capacitor=0.0;
};

float envelope(int frame,int rate,bool gated) {
    if(!gated)return 1.0f;
    if(frame<0)return 0.0f;
    const long attack=(long)std::ceil(.004*rate);
    const double u=std::min<long>(frame,attack)/(double)attack;
    return (float)(.5-.5*std::cos(std::numbers::pi*u));
}
float inputSignal(int frame,int rate,const Scenario& scenario) {
    if(scenario.stimulus==Stimulus::zero)return 0.0f;
    if(scenario.stimulus==Stimulus::dc)return 1.0f;
    return (float)(scenario.amplitude*std::sin(2.0*std::numbers::pi*scenario.frequency*(double)frame/rate));
}

DoublePath run(const Fixture& fixture,const Scenario& scenario) {
    const int rate=fixture.rate;
    const long warmup=rate;
    const long after=(long)std::llround(.012*rate);
    const bool gated=scenario.stimulus!=Stimulus::heldPitched;
    cs80::Ig00151Vca current,withCp3;
    current.prepare(rate);withCp3.prepare(rate);
    current.reset(1.0f);withCp3.reset(1.0f);
    IdealCp3InputCap cp3(rate);
    DoublePath result;
    for(auto* r:{&result.existing,&result.extraInputCap}) {
        r->rate=rate;r->first=warmup;
        r->l.reserve((std::size_t)(warmup+after));
        r->r.reserve((std::size_t)(warmup+after));
    }
    for(int n=-(int)warmup;n<(int)after;++n) {
        const float x=inputSignal(n,rate,scenario);
        const float drive=envelope(n,rate,gated);
        const float normal=current.process(x,1.0f,0.0f,drive);
        const float coupled=withCp3.process(cp3.process(x),1.0f,0.0f,drive);
        const std::array<float,2> values {normal,coupled};
        for(int path=0;path<2;++path) {
            auto* r=path?&result.extraInputCap:&result.existing;
            const float y=values[(std::size_t)path];
            r->nonfinite+=(std::uint64_t)!std::isfinite(y);
            const float safe=std::isfinite(y)?y:0.0f;
            r->l.push_back(safe);r->r.push_back(safe);
        }
    }
    require(result.existing.nonfinite==0&&result.extraInputCap.nonfinite==0,
            "nonfinite shadow-VCA sample");
    return result;
}

long countChanged(const Stereo& a,const Stereo& b,long from,long end) {
    require(a.l.size()==b.l.size() && a.r.size()==b.r.size(),"PCM size mismatch");
    require(from>=0&&end>=from&&end<=(long)a.l.size(),"PCM bounds mismatch");
    long changed=0;
    for(long i=from;i<end;++i) {
        const auto index=(std::size_t)i;
        changed+=std::bit_cast<std::uint32_t>(a.l[index])!=std::bit_cast<std::uint32_t>(b.l[index]);
        changed+=std::bit_cast<std::uint32_t>(a.r[index])!=std::bit_cast<std::uint32_t>(b.r[index]);
    }
    return changed;
}
double maxJump(const Stereo& r,long from,long end) {
    double peak=0.0;
    for(long i=std::max(1L,from);i<end;++i) {
        const auto index=(std::size_t)i,prev=index-1;
        peak=std::max({peak,std::fabs((double)r.l[index]-r.l[prev]),
                       std::fabs((double)r.r[index]-r.r[prev])});
    }
    return peak;
}
double maxDerivativeDifference(const Stereo& a,const Stereo& b,long from,long end) {
    double peak=0.0;
    for(long i=std::max(1L,from);i<end;++i) {
        const auto index=(std::size_t)i,prev=index-1;
        peak=std::max({peak,
            std::fabs(((double)a.l[index]-a.l[prev])-((double)b.l[index]-b.l[prev])),
            std::fabs(((double)a.r[index]-a.r[prev])-((double)b.r[index]-b.r[prev]))});
    }
    return peak;
}
double peakAbs(const Stereo& r,long from,long end) {
    double peak=0.0;
    for(long i=from;i<end;++i) {
        const auto index=(std::size_t)i;
        peak=std::max({peak,(double)std::fabs(r.l[index]),(double)std::fabs(r.r[index])});
    }
    return peak;
}

void mutationOracles() {
    Stereo a,b;
    a.rate=b.rate=48000;
    a.l={.01f,.02f,.03f,.04f,.05f};
    a.r={.01f,.02f,.03f,.04f,.05f};
    b=a;
    require(countChanged(a,b,0,5)==0,"identical negative rejected");
    b.l[1]=std::nextafter(b.l[1],std::numeric_limits<float>::infinity());
    require(countChanged(a,b,0,3)==1,"single-ULP pre-event mutation escaped");
    b=a;b.l[3]+=.20f;
    require(countChanged(a,b,2,5)==1,"single-sample glitch escaped");
    b.r[4]+=.20f;
    require(countChanged(a,b,2,5)==2,"two-sample glitch escaped");
    require(maxDerivativeDifference(a,b,2,5)>.19,"PCM impulse was not measured");
}

void verifyPrereg(const std::string& filename,const std::string& topology) {
    std::ifstream file(filename);
    require((bool)file,"preregistered coupling contract unavailable");
    const std::string contract((std::istreambuf_iterator<char>(file)),{});
    require(contract.find("\"status\": \"REGISTERED\"")!=std::string::npos,
            "CP3 coupling trial was not preregistered");
    require(contract.find("\"required_scenario_rows\": 42")!=std::string::npos,
            "trial rows departed from preregistered plan");
    require(contract.find("7dd159d491012fd47b46c0aa651e86ea959ca289")!=std::string::npos,
            "frozen source hash missing");
    require(contract.find("7fa78c747afeabf3ee25cf506c67c287a4cf0f5dbcf3e631be485566ca5b70c8")!=std::string::npos,
            "prior path experiment identity missing");
    std::ifstream more(topology);
    require((bool)more,"two-capacitor topology addendum unavailable");
    const std::string addendum((std::istreambuf_iterator<char>(more)),{});
    require(addendum.find("SOURCE_REFINEMENT_REGISTERED_BEFORE_FIRST_TRIAL")!=std::string::npos
            &&addendum.find("4743032")!=std::string::npos
            &&addendum.find("O(33)")!=std::string::npos,
            "M41 source correction not registered before shadow trial");
}

void write(std::ostream& out,const Fixture& f,const char* label,const char* path,
           const Scenario& scenario,const Stereo& r,double originalJump) {
    const long begin=r.first,end=begin+(long)std::llround(.008*f.rate);
    const double jump=maxJump(r,begin,end);
    const double ratio=originalJump>0.0?jump/originalJump:0.0;
    const double step=std::fabs((double)r.l[(std::size_t)begin]-r.l[(std::size_t)begin-1]);
    out<<f.rate<<','<<label<<','<<path<<','<<scenario.frequency<<','<<scenario.amplitude<<','
       <<f.reference<<','<<jump<<','<<jump/f.reference<<','
       <<peakAbs(r,begin,end)<<','<<step<<','<<ratio<<','<<r.nonfinite<<'\n';
    require(std::isfinite(jump)&&std::isfinite(ratio),"nonfinite shadow metric");
}

void writeImpulse(std::ostream& out,const Fixture& f,const char* name,
                  const Scenario& pitched,const Stereo& original,long offset,int channel,
                  double originalJump) {
    Stereo glitch=original;
    const long first=glitch.first,index=first+offset;
    require(index>first && index<first+(long)std::llround(.008*f.rate),
            "impulse outside preregistered event window");
    if(channel==0)glitch.l[(std::size_t)index]+=.20f;
    else glitch.r[(std::size_t)index]+=.20f;
    require(countChanged(original,glitch,0,first)==0,"impulse corrupted pre-event PCM");
    require(countChanged(original,glitch,first,(long)original.l.size())==1,
            "positive corrupted more than one digital sample");
    require(maxDerivativeDifference(original,glitch,first,
                                   first+(long)std::llround(.008*f.rate))>.19,
            "positive impulse not detected in event window");
    write(out,f,name,channel==0?"left_one_sample_positive":"right_one_sample_positive",
          pitched,glitch,originalJump);
}

int trial(std::ostream& out) {
    constexpr std::array<Fixture,3> rates {{{44100,.037371031939983368},
        {48000,.037026423960924149},{96000,.029931630939245224}}};
    const std::array<Scenario,6> scenarios {{{"c1_pitched_220",Stimulus::pitched,220.0,.10},
        {"c1_pitched_1760",Stimulus::pitched,1760.0,.20},
        {"c1_dc_gate",Stimulus::dc,0.0,1.0},
        {"c1_zero_audio_gate",Stimulus::zero,0.0,0.0},
        {"held_no_event_220",Stimulus::heldPitched,220.0,.10},
        {"held_no_event_1760",Stimulus::heldPitched,1760.0,.20}}};
    int rows=0,pitchedPass=0,dcPass=0,smoothAlerts=0;
    for(const auto& f:rates) {
        for(const auto& sc:scenarios) {
            const auto p=run(f,sc);
            const long first=p.existing.first;
            const long end=first+(long)std::llround(.008*f.rate);
            const double originalJump=maxJump(p.existing,first,end);
            const double capJump=maxJump(p.extraInputCap,first,end);
            if(sc.stimulus==Stimulus::pitched||sc.stimulus==Stimulus::dc||sc.stimulus==Stimulus::zero) {
                require(std::bit_cast<std::uint32_t>(p.existing.l[(std::size_t)first])==
                        std::bit_cast<std::uint32_t>(p.existing.l[(std::size_t)first-1]),
                        "existing VCA C1 gate has a sample step");
                require(std::bit_cast<std::uint32_t>(p.extraInputCap.l[(std::size_t)first])==
                        std::bit_cast<std::uint32_t>(p.extraInputCap.l[(std::size_t)first-1]),
                        "input-coupled VCA C1 gate has a sample step");
            }
            if(sc.stimulus==Stimulus::zero)
                require(countChanged(p.existing,p.extraInputCap,0,(long)p.existing.l.size())==0,
                        "zero-input VCA control/pedestal was mutated by CP3 input capacitor");
            if(sc.stimulus==Stimulus::pitched&&sc.frequency==1760.0) {
                require(originalJump>0.0,"vacuous 1760Hz pitched negative");
                const double ratio=capJump/originalJump;
                pitchedPass+=(ratio>=.95&&ratio<=1.05);
                smoothAlerts+=(originalJump/f.reference>1.0);
                // Two separately located one-sample digital positives use the
                // original augmented pitch PCM, not Engine or source code.
                writeImpulse(out,f,"known_impulse_plus_020_at_1frame",sc,p.extraInputCap,
                             1,0,originalJump);++rows;
                writeImpulse(out,f,"known_impulse_plus_020_at_6ms",sc,p.extraInputCap,
                             (long)std::llround(.006*f.rate),1,originalJump);++rows;
                std::cerr<<std::setprecision(10)<<"pitched "<<f.rate
                         <<" ratio_augmented="<<ratio
                         <<" c1_smooth_native_score="<<originalJump/f.reference<<'\n';
            }
            if(sc.stimulus==Stimulus::dc) {
                const double originalPeak=peakAbs(p.existing,first,end);
                require(originalPeak>0.0,"vacuous DC test");
                const double ratio=peakAbs(p.extraInputCap,first,end)/originalPeak;
                dcPass+=(ratio<=.02);
                std::cerr<<"dc "<<f.rate<<" peak_ratio_input_cap_over_existing="<<ratio<<'\n';
            }
            write(out,f,sc.label,"current_output_coupling",sc,p.existing,originalJump);++rows;
            write(out,f,sc.label,"additional_cp3_input_coupling",sc,p.extraInputCap,originalJump);++rows;
        }
    }
    require(rows==42,"shadow screen did not produce 42 preregistered rows");
    std::cerr<<"RESULT pitched_ratio_within_5percent="<<pitchedPass<<"/3"
             <<" dc_location_discriminator="<<dcPass<<"/3"
             <<" c1_smooth_1760_alert_rates="<<smoothAlerts<<"/3"
             <<" prereg_screen="<<(pitchedPass==3&&dcPass==3&&smoothAlerts>=1?"SUPPORTED":"FALSIFIED")
             <<" rows="<<rows<<'\n';
    return pitchedPass==3&&dcPass==3&&smoothAlerts>=1 ? 0:2;
}
} // namespace

int main(int argc,char** argv) {
    try {
        require(argc==7 && std::string(argv[1])=="--prereg" &&
                std::string(argv[3])=="--topology" && std::string(argv[5])=="--out",
                "usage: ClickCp3CouplingScreen --prereg CONTRACT --topology ADDENDUM --out CSV");
        verifyPrereg(argv[2],argv[4]);
        mutationOracles();
        std::ofstream out(argv[6],std::ios::binary);
        require((bool)out,"cannot create CP3 coupling screen CSV");
        out.imbue(std::locale::classic());
        out<<std::setprecision(17);
        out<<"rate,case,path,input_hz,input_amplitude,reference,maxjump,normalized_score,peak_abs,gate_pcm_step,ratio_to_original,nonfinite\n";
        const int status=trial(out);
        out.flush();require((bool)out,"failed to write coupling screen CSV");
        return status;
    } catch(const std::exception& error) {
        std::cerr<<"CP3_COUPLING_SCREEN_FAILED: "<<error.what()<<'\n';
        return 1;
    }
}
