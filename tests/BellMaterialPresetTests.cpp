// Audible contracts for the existing factory mallets. These establish touch,
// tuned-mode identity, ringing aftersound and damping, not acoustic equivalence.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/InstrumentBodyMaterials.h"

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
    engine.setParams(v);
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
    check(finite&&peak<1.0,"material-preset render stays finite and below full scale");
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

struct Entry {const char* name; int material; double footage; double identityRatio;};
constexpr Entry entries[] {
    {"Tubular Bells",body::tubular,1.0,2.0},
    {"Gong",body::gong,.5,3.153},
    {"Temple Bell",body::temple,.5,2.361},
    {"Agogo",body::agogo,2.0,1.873},
    {"Music Box",body::musicBox,2.0,6.2669}
};
void materialIdentity()
{
    for(const auto& entry:entries)
    {
        const auto& p=preset(entry.name);
        check(p.values[P_body1Material]==entry.material && p.values[P_body2Material]==entry.material,
              p.name+": both same-pitch ranks retain the selected material");
        check(p.values[P_ringDepth]==0.0f && p.values[P_ringAmount]==0.0f,
              p.name+": natural material has no unrelated fixed ring carrier");
        for(int note:{48,60,72})
        {
            const auto soft=render(p.values,note,.25f,2.2);
            const auto firm=render(p.values,note,.85f,2.2);
            auto wrong=p.values; wrong[P_body1Material]=wrong[P_body2Material]=body::defaultMaterial;
            const auto control=render(wrong,note,.85f,2.2);
            const double contrast=db(firm.rms(.01,.25)/soft.rms(.01,.25));
            check(contrast>6.5,p.name+": firm impact is stronger than gentle impact");
            check(soft.rms(.01,.25)>1.e-5,p.name+": soft strike remains audible in all three registers");
            const double begin=entry.material==body::gong?.65:.025;
            const double end=entry.material==body::gong?1.5:.25;
            const double key=440*std::exp2((note-69)/12.0)*entry.footage;
            const double shaped=tone(firm,begin,end,key*entry.identityRatio)/firm.rms(begin,end);
            const double generic=tone(control,begin,end,key*entry.identityRatio)/control.rms(begin,end);
            const double change=db(shaped/generic);
            check(change>9.0,p.name+": characteristic mode survives full Engine and gain-matched generic control");
            std::printf("MATERIAL %-14s note %d touch %.3f dB normalized mode change %.3f dB\n",entry.name,note,contrast,change);
            if(entry.material==body::tubular)
            {
                const double nominal=tone(firm,.04,.3,2*key), missing=tone(firm,.04,.3,key);
                check(db(nominal/missing)>18.0,"Tubular Bells: octave nominal dominates absent strike-frequency fundamental");
                const double second=tone(firm,.04,.3,key*2.8795313186);
                check(db(second/nominal)>-16.0,"Tubular Bells: upper nominal group remains audible");
            }
            if(entry.material==body::musicBox)
            {
                const double tooth=tone(firm,.015,.16,6.2669*key);
                const double bar=tone(firm,.015,.16,2.756*key);
                check(db(tooth/bar)>12.0,"Music Box: cantilever ping replaces free-bar first overtone");
            }
        }
    }
}
void envelopes()
{
    const auto& gong=preset("Gong");
    for(int note:{48,60,72})
    {
        const auto a=render(gong.values,note,.85f,8);
        const double f=440*std::exp2((note-69)/12.)*.5;
        const double early=tone(a,.025,.14,3.153*f)/tone(a,.025,.14,f);
        const double late=tone(a,.8,1.5,3.153*f)/tone(a,.8,1.5,f);
        const double growth=db(late/early);
        check(growth>6.0,"Gong: upper plate modes develop after the initial low impact");
        check(a.rms(5,6)>1.e-5,"Gong: independent plate aftersound remains audible on a held key");
        std::printf("GONG note %d normalized upper bloom %.3f dB; 5-6s RMS %.3f dBFS\n",note,growth,db(a.rms(5,6)));
    }
    const auto agogo=render(preset("Agogo").values,48,.85f,3.0);
    const auto temple=render(preset("Temple Bell").values,72,.85f,8.0);
    const double a=db(agogo.rms(1.5,2)/agogo.rms(.02,.18));
    const double t=db(temple.rms(1.5,2)/temple.rms(.02,.18));
    check(t-a>18.0,"Temple Bell: same-pitch shell aftersound outlasts compact agogo by a material margin");
    check(temple.rms(5,6)>1.e-5,"Temple Bell: long modal aftersound remains audible");
    std::printf("SAME-PITCH TAIL agogo %.3f dB temple %.3f dB\n",a,t);
    const auto tube=render(preset("Tubular Bells").values,60,.85f,8.0);
    check(tube.rms(4,5)>1.e-5,"Tubular Bells: tube keeps ringing beyond the contact rank");
    const auto music=render(preset("Music Box").values,60,.85f,5.0);
    check(music.rms(1,1.5)>1.e-5,"Music Box: tooth has ringing body rather than only a click");
    check(db(music.rms(3,4)/music.rms(.02,.18))<-15.0,"Music Box: plucked tooth decays while the key remains held");
}
}
int main()
{
    try { materialIdentity(); envelopes(); }
    catch(const std::exception& e) { check(false,e.what()); }
    std::printf("BellMaterialPresets: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
