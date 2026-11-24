// Rendered distinctions between the authored banjo, koto and harp materials.
// These checks establish spectral/decay behavior, not perceptual equivalence
// to a recorded instrument. Standard/unsupported routes retain their samples.
#include "core/InstrumentBody.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {
using namespace cs80::body;
constexpr double pi=3.1415926535897932384626433832795;
int checks=0, failures=0;
void check(bool ok,const char* message) { ++checks; if(!ok){++failures;std::printf("FAIL: %s\n",message);} }
double hz(double note){return 440.*std::exp2((note-69.)/12.);}
double db(double x){return 20.*std::log10(std::max(x,1.e-15));}
struct Audio {
    double rate; std::vector<float> samples;
    int at(double t) const {return std::clamp((int)std::lround(t*rate),0,(int)samples.size());}
    double rms(double first,double last) const {
        double sum=0; const int begin=at(first),end=at(last);
        for(int i=begin;i<end;++i)sum+=(double)samples[(size_t)i]*samples[(size_t)i];
        return std::sqrt(sum/std::max(1,end-begin));
    }
};
Audio render(int model,int material,double note,double hostRate=48000,float velocity=.85f,
             float colour=.65f,float decay=1.f,float size=1.f,double duration=2.4,
             std::uint32_t seed=0x7812a3u,bool omitted=false) {
    Audio a{hostRate*2.,{}}; a.samples.resize((size_t)std::lround(duration*a.rate));
    InstrumentBody body; body.prepare(a.rate,hostRate);
    if(omitted) body.noteOn(model,(float)note,velocity,colour,decay,seed,size);
    else body.noteOn(model,(float)note,velocity,colour,decay,seed,size,standard,material);
    bool finite=true; double peak=0.;
    for(auto& x:a.samples){x=body.tick((float)hz(note),colour);finite=finite&&std::isfinite(x);peak=std::max(peak,std::abs((double)x));}
    check(finite&&peak<.78,"plucked material remains finite within the excitation bound");
    return a;
}
struct Spectrum {
    double rate,weights=0.; std::vector<double> samples;
    Spectrum(const Audio& a,double begin=.025,double end=.175):rate(a.rate){
        int first=a.at(begin),last=a.at(end);samples.resize((size_t)(last-first));
        for(int i=first;i<last;++i){double w=.5-.5*std::cos(2*pi*(i-first)/(last-first-1));weights+=w;samples[(size_t)(i-first)]=w*a.samples[(size_t)i];}
    }
    double tone(double frequency)const{
        const double angle=2*pi*frequency/rate,c=std::cos(angle),s=std::sin(angle);
        double r=1.,im=0.,rs=0.,is=0.;
        for(double x:samples){rs+=r*x;is+=im*x;double nr=r*c-im*s;im=im*c+r*s;r=nr;}
        return 2*std::hypot(rs,is)/weights;
    }
    std::array<double,12> shape(double frequency)const{
        std::array<double,12> result{};double sum=0.;
        for(size_t i=0;i<result.size();++i){result[i]=tone(frequency*(i+1));sum+=result[i]*result[i];}
        for(auto& x:result)x/=std::sqrt(std::max(sum,1.e-24));
        return result;
    }
    double highFraction(double frequency)const{
        double low=0.,high=0.;for(int h=1;h<=12;++h){double x=tone(h*frequency);(h>=4?high:low)+=x*x;}
        return high/std::max(high+low,1.e-24);
    }
};
double distance(const Audio& a,const Audio& b,double pitch){
    const auto x=Spectrum(a).shape(pitch),y=Spectrum(b).shape(pitch);double d=0.;
    for(size_t i=0;i<x.size();++i)d+=(x[i]-y[i])*(x[i]-y[i]);return std::sqrt(d);
}
void standardAndFallback(){
    for(double rate:{44100.,48000.,96000.}) for(int model:{nylon,steel,bass})
        for(double note:{36.,60.,84.}) {
            const auto oldCall=render(model,0,note,rate,.7f,.5f,1.f,1.f,.12,21,true);
            const auto zero=render(model,0,note,rate,.7f,.5f,1.f,1.f,.12,21);
            const auto invalid=render(model,999,note,rate,.7f,.5f,1.f,1.f,.12,21);
            const auto unrelated=render(model,gong,note,rate,.7f,.5f,1.f,1.f,.12,21);
            check(oldCall.samples==zero.samples&&zero.samples==invalid.samples&&zero.samples==unrelated.samples,
                  "omitted, zero, invalid and incompatible materials preserve identical standard audio");
        }
}
void materialIdentity(){
    double minimumDistance=1.e9,minimumRange=1.e9; double banjoTailMax=-1.e9,harpTailMin=1.e9;
    for(double rate:{44100.,48000.,96000.}) for(double note:{48.,60.,72.})
        for(int material:{banjo,koto,harp}) {
            const int model=material==harp?nylon:steel;
            const auto a=render(model,material,note,rate);
            const auto base=render(model,0,note,rate);
            const double d=distance(a,base,hz(note));minimumDistance=std::min(minimumDistance,d);
            check(d>.10,"material differs from gain-matched generic string harmonic shape");
            const auto soft=render(model,material,note,rate,.25f);
            const auto firm=render(model,material,note,rate,.9f);
            const double range=db(firm.rms(.025,.175)/soft.rms(.025,.175));minimumRange=std::min(minimumRange,range);
            check(range>3.0,"soft pluck retains less string excitation energy than firm pluck");
            check(Spectrum(firm).highFraction(hz(note))>Spectrum(soft).highFraction(hz(note)),
                  "firm pluck increases upper partial energy proportion independently of gain");
            const double relativeTail=db(a.rms(1.9,2.3)/a.rms(.025,.175))-db(base.rms(1.9,2.3)/base.rms(.025,.175));
            if(material==banjo){banjoTailMax=std::max(banjoTailMax,relativeTail);check(relativeTail < -5.,"banjo prompt energy drains faster than steel guitar body");}
            if(material==harp){harpTailMin=std::min(harpTailMin,relativeTail);check(relativeTail>2.,"harp lower aftersound outlasts short nylon body at equal initial level");}
            std::printf("MATERIAL %s rate %.0f note %.0f shape-distance %.6f touch %.6f dB tail-relative %.6f dB\n",materialName(material),rate,note,d,range,relativeTail);
        }
    std::printf("SUMMARY minimum gain-matched shape distance %.6f; minimum touch %.6f dB; banjo longest relative tail %.6f dB; harp shortest relative tail %.6f dB\n",minimumDistance,minimumRange,banjoTailMax,harpTailMin);
}

Audio pitchGesture(int material, double from, double to, double hostRate, bool returnHome)
{
    const int model = material == harp ? nylon : steel;
    Audio a {2. * hostRate, {}};
    a.samples.resize((size_t) std::lround(.45 * a.rate));
    InstrumentBody body;
    body.prepare(a.rate, hostRate);
    body.noteOn(model, (float) from, .85f, .65f, 1.f, 12345, 1.f, standard, material);
    bool finite = true;
    double peak = 0.;
    for (size_t i = 0; i < a.samples.size(); ++i)
    {
        const double time = i / a.rate;
        const double slide = returnHome ? std::clamp((time - .08) / .025, 0., 1.)
                                            * (1. - std::clamp((time - .15) / .025, 0., 1.)) : 1.;
        const double note = from + (to - from) * slide;
        const float value = body.tick((float) hz(note), .65f);
        a.samples[i] = value;
        finite = finite && std::isfinite(value);
        peak = std::max(peak, std::abs((double) value));
    }
    check(finite && peak < .78, "plucked pitch gesture stays finite and bounded");
    return a;
}
void movingResonances()
{
    double worstDestination = 0., worstReturn = 0.;
    for (double rate : {44100., 48000., 96000.})
        for (int material : {banjo, koto, harp})
        {
            // Immediately sounding a different pitch isolates the radiation
            // profile before register-dependent losses have greatly diverged.
            const auto moved = pitchGesture(material, 60, 72, rate, false);
            const auto fresh = pitchGesture(material, 72, 72, rate, false);
            const Spectrum a(moved, .012, .060), b(fresh, .012, .060);
            const double pitch = hz(72), a1 = a.tone(pitch), b1 = b.tone(pitch);
            for (int h : {2,3,4,5})
            {
                const double expected = b.tone(h * pitch) / b1;
                if (expected < .003) continue;
                const double error = std::abs(db((a.tone(h * pitch) / a1) / expected));
                worstDestination = std::max(worstDestination, error);
                check(error < .65, "moving plucked harmonics retain the destination body response");
            }
            const auto returned = pitchGesture(material, 60, 72, rate, true);
            const auto steady = pitchGesture(material, 60, 60, rate, true);
            const Spectrum c(returned, .26, .40), d(steady, .26, .40);
            const double root = hz(60), c1 = c.tone(root), d1 = d.tone(root);
            for (int h : {2,3,4,5})
            {
                const double expected = d.tone(h * root) / d1;
                if (expected < .003) continue;
                const double error = std::abs(db((c.tone(h * root) / c1) / expected));
                worstReturn = std::max(worstReturn, error);
                check(error < .08, "returning to onset pitch restores the captured body response");
            }
        }
    std::printf("PITCH maximum destination-response error %.6f dB; return-response error %.6f dB\n",
                worstDestination, worstReturn);
}
void edgesAndRepeatability(){
    for(double rate:{44100.,48000.,96000.}) for(int material:{banjo,koto,harp})
        for(double note:{0.,24.,96.,127.}) for(float colour:{0.f,1.f}) {
            const int model=material==harp?nylon:steel;
            auto a=render(model,material,note,rate,1.f,colour,4.f,2.5f,.25,9182);
            auto repeat=render(model,material,note,rate,1.f,colour,4.f,2.5f,.25,9182);
            check(a.samples==repeat.samples,"identical seed and controls reproduce the same material audio");
        }
}
}
int main(){standardAndFallback();materialIdentity();movingResonances();edgesAndRepeatability();std::printf("Plucked materials: %d checks, %d failures\n",checks,failures);return failures?1:0;}
