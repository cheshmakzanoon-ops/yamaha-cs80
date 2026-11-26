// Audible contracts for the existing factory mallets. These establish touch,
// tuned-mode identity, ringing aftersound and damping, not acoustic equivalence.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/InstrumentBodyVariants.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace cs80;
constexpr double sampleRate = 48000.0;
constexpr double pi = 3.14159265358979323846;
int checks = 0, failures = 0;
void check(bool pass, const std::string& message)
{
    ++checks;
    if (!pass) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
double db(double x) { return 20.0 * std::log10(std::max(x, 1.e-12)); }
const FactoryPreset& preset(const char* name)
{
    for (const auto& p : curatedLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("Missing preset: ") + name);
}
struct Audio
{
    std::vector<float> samples;
    int frame(double t) const { return std::clamp((int) std::llround(t*sampleRate), 0, (int) samples.size()/2); }
    double rms(double begin, double end) const
    {
        const int first = frame(begin), last = frame(end);
        double power = 0.0;
        for (int i=first; i<last; ++i)
            power += .5 * (double(samples[2*i])*samples[2*i] + double(samples[2*i+1])*samples[2*i+1]);
        return std::sqrt(power/std::max(1,last-first));
    }
};
Audio render(const ParamValues& v, int note, float velocity, double duration, double releaseAt = -1.0)
{
    Engine engine;
    engine.prepare(sampleRate);
    auto dryP = v; dryP[P_fxMasterBypass] = 1.0f; engine.setParams(dryP);
    engine.noteOn(note,velocity,1);
    Audio a;
    a.samples.resize(2*(int)std::llround(sampleRate*duration));
    bool released = false, finite = true;
    double peak = 0.0;
    const int releaseFrame = releaseAt < 0 ? (int)a.samples.size() : a.frame(releaseAt);
    for (int offset=0; offset<(int)a.samples.size()/2;)
    {
        if (!released && offset>=releaseFrame) { engine.noteOff(note,1); released=true; }
        int count=std::min(256,(int)a.samples.size()/2-offset);
        if (!released) count=std::min(count,releaseFrame-offset);
        engine.render(a.samples.data()+2*offset,count);
        offset+=count;
    }
    for (float x:a.samples) { finite=finite&&std::isfinite(x); peak=std::max(peak,std::abs(double(x))); }
    check(finite&&peak<1.0,"factory mallet render remains finite and below full scale");
    return a;
}
double tone(const Audio& a, double begin, double end, double hz)
{
    const int first=a.frame(begin),last=a.frame(end),count=last-first;
    std::vector<std::array<double,2>> window(count);
    double weights=0.0, strongest=0.0;
    for(int i=0;i<count;++i)
    {
        const double w=.5-.5*std::cos(2*pi*i/(count-1));
        weights+=w;
        window[i]={a.samples[2*(first+i)]*w,a.samples[2*(first+i)+1]*w};
    }
    for(int cents=-20;cents<=20;cents+=5)
    {
        const double c=2*std::cos(2*pi*hz*std::exp2(cents/1200.0)/sampleRate);
        std::array<double,2> previous{},before{};
        for(const auto& x:window) for(int ch=0;ch<2;++ch)
        {
            const double next=x[ch]+c*previous[ch]-before[ch];
            before[ch]=previous[ch];previous[ch]=next;
        }
        double power=0.0;
        for(int ch=0;ch<2;++ch) power+=std::max(0.0,previous[ch]*previous[ch]+before[ch]*before[ch]-c*previous[ch]*before[ch]);
        strongest=std::max(strongest,2*std::sqrt(.5*power)/weights);
    }
    return strongest;
}
void checkTouch()
{
    int count=0;
    for(const auto& p:curatedLibrary()) if(p.category=="Bells")
    {
        ++count;
        const auto soft=render(p.values,60,.25f,.75);
        const auto firm=render(p.values,60,.85f,.75);
        const double contrast=db(firm.rms(.01,.25)/soft.rms(.01,.25));
        check(contrast>6.5,p.name+": firm strike is audibly stronger than soft strike");
        check(soft.rms(.01,.25)>1.e-5,p.name+": soft mallet remains audible");
        std::printf("TOUCH %-19s %.3f dB\n",p.name.c_str(),contrast);
    }
    check(count==23,"all 23 existing curated Bells programs are covered");
}
void materials()
{
    for(int note:{48,60,72})
    {
        const double key=440.0*std::exp2((note-69)/12.0);
        for(const char* name:{"Xylophone","Steel Drum"})
        {
            const auto& p=preset(name);
            auto wrong=p.values;
            wrong[P_body1Variant]=wrong[P_body2Variant]=body::standard;
            const auto full=render(p.values,note,.9f,1.5);
            const auto oldMaterial=render(wrong,note,.9f,1.5);
            const bool wood=std::string(name)=="Xylophone";
            const double f=key*(wood?2.0:1.0);
            const double fundamental=tone(full,.025,.13,f);
            const double third=tone(full,.025,.13,3*f);
            const double wrongThird=tone(oldMaterial,.025,.13,3*f);
            const double ratio=db(third/fundamental);
            check(ratio>-25.0,std::string(name)+": tracked third mode is audible");
            check(db(third/wrongThird)>12.0,std::string(name)+": tuned third exceeds the wrong-material control");
            if(wood)
            {
                const double fourth=tone(full,.025,.13,4*f);
                check(db(third/fourth)>12.0,"Xylophone: third-tuned wood is distinct from a fourth-tuned marimba");
                check(db(full.rms(.8,1.2)/full.rms(.01,.13))<-25.0,"Xylophone: compact wood resonance clears repeated notes");
            }
            else
            {
                check(db(tone(full,.025,.13,2*f)/fundamental)>-18.0,"Steel Drum: octave shell mode accompanies its twelfth");
                check(db(full.rms(.5,.75)/full.rms(.01,.13))>-22.0,"Steel Drum: shell rings after the mallet contact");
            }
            std::printf("MATERIAL %-12s note %d third %.3f dB; control difference %.3f dB\n",name,note,ratio,db(third/wrongThird));
        }
    }
}
void damping()
{
    const auto& vibes=preset("Vibraphone");
    const auto held=render(vibes.values,60,.85f,2.0);
    const auto released=render(vibes.values,60,.85f,2.0,.7);
    const double dampingDb=db(released.rms(1.05,1.25)/held.rms(1.05,1.25));
    check(dampingDb<-30.0,"Vibraphone: key release promptly damps the ringing bar");
    check(held.rms(1.05,1.25)>1.e-4,"Vibraphone: held bar is still audible at the damping comparison");
    std::printf("VIBRAPHONE DAMPING %.3f dB relative to held note\n",dampingDb);
    const auto& marimba=preset("Marimba");
    const auto& xylo=preset("Xylophone");
    const auto m=render(marimba.values,72,.85f,1.5);
    const auto x=render(xylo.values,60,.85f,1.5); // same sounding fundamental
    const double marimbaTail=db(m.rms(.65,.85)/m.rms(.01,.13));
    const double xyloTail=db(x.rms(.65,.85)/x.rms(.01,.13));
    check(marimbaTail-xyloTail>6.0,"Marimba: fuller wood aftersound than the same-pitch Xylophone");
    std::printf("WOOD AFTERSOUND marimba %.3f dB; xylophone %.3f dB\n",marimbaTail,xyloTail);
}
}
int main()
{
    try { checkTouch(); materials(); damping(); }
    catch(const std::exception& e) { check(false,e.what()); }
    std::printf("MalletPresetRefinement: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
