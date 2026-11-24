// Full factory-patch checks for distinct existing plucked material voices.
// These measure authored spectrum, touch, ringing and damper behavior; they do
// not assert equality to recordings of physical instruments.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include "core/InstrumentBodyMaterials.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace cs80;
namespace
{
constexpr double rate = 48000.0, pi = 3.1415926535897932384626433832795;
int checks = 0, failures = 0;
void check(bool ok, const std::string& label)
{
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL: %s\n", label.c_str()); }
}
double db(double x) { return 20.0 * std::log10(std::max(x, 1.e-15)); }
const FactoryPreset& preset(const char* name)
{
    const auto& bank = curatedLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(), [name](const auto& p) { return p.name == name; });
    if (found == bank.end()) throw std::runtime_error(std::string("Missing existing preset: ") + name);
    return *found;
}
struct Audio
{
    std::vector<float> data;
    double rms(double start, double end) const
    {
        const auto first = static_cast<size_t>(std::llround(start * rate)) * 2;
        const auto last = static_cast<size_t>(std::llround(end * rate)) * 2;
        double sum = 0;
        for (size_t n=first;n<last;++n) sum += static_cast<double>(data[n]) * data[n];
        return std::sqrt(sum / (last-first));
    }
    double tone(double frequency) const
    {
        // Early sustained string window, independent of any post-render gain.
        const int first=static_cast<int>(.025*rate), last=static_cast<int>(.30*rate);
        const double angle=2*pi*frequency/rate, c=std::cos(angle), s=std::sin(angle);
        double real=1, imaginary=0, realSum=0, imaginarySum=0, weight=0;
        for(int n=first;n<last;++n)
        {
            const double w=.5-.5*std::cos(2*pi*(n-first)/(last-first-1));
            const double value=.5*(data[static_cast<size_t>(2*n)]+data[static_cast<size_t>(2*n+1)])*w;
            realSum+=real*value; imaginarySum+=imaginary*value; weight+=w;
            const double next=real*c-imaginary*s; imaginary=imaginary*c+real*s; real=next;
        }
        return 2*std::hypot(realSum,imaginarySum)/weight;
    }
};
Audio render(const FactoryPreset& p,int note,float velocity,double held,double tail=0,bool pedal=false)
{
    auto e=std::make_unique<Engine>(); e->prepare(rate); e->setParams(p.values);
    e->noteOn(note,velocity,1);
    if(pedal)e->controlChange(64,1.0f,1);
    const int releaseAt=static_cast<int>(std::llround(held*rate));
    const int total=static_cast<int>(std::llround((held+tail)*rate));
    Audio out;out.data.resize(static_cast<size_t>(total*2));
    for(int at=0;at<total;)
    {
        if(at==releaseAt)e->noteOff(note,1);
        const int count=std::min(128,(at<releaseAt?releaseAt:total)-at);
        e->render(out.data.data()+2*at,count);at+=count;
    }
    bool finite=true;double peak=0;
    for(float x:out.data){finite=finite&&std::isfinite(x);peak=std::max(peak,std::abs(static_cast<double>(x)));}
    check(finite&&peak>1.e-6&&peak<.88,p.name+": finite audible full patch retains headroom");
    return out;
}
double shapeDistance(const Audio& a,const Audio& b,double pitch,bool tine)
{
    constexpr std::array<double,16> ordinary {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    constexpr std::array<double,16> lamella {1,2,3,4,5,5.3,5.5,5.7,5.9,6,7,9,11,13,14.8,17};
    const auto& ratios=tine?lamella:ordinary;
    std::array<double,16> aa{},bb{};double sumA=0,sumB=0;
    for(size_t i=0;i<ratios.size();++i){aa[i]=a.tone(pitch*ratios[i]);bb[i]=b.tone(pitch*ratios[i]);sumA+=aa[i];sumB+=bb[i];}
    double result=0;
    for(size_t i=0;i<ratios.size();++i)result+=std::abs(aa[i]/sumA-bb[i]/sumB);
    return result;
}
}
int main()
{
    try
    {
        for(const char* name:{"Harp","Koto","Banjo","Kalimba"})
        {
            const auto& p=preset(name);
            for(int note:{48,60,72})
            {
                std::array<double,3> effort{};size_t v=0;
                for(float velocity:{.25f,.55f,.85f})
                {
                    const auto a=render(p,note,velocity,1.6,.6);
                    effort[v++]=a.rms(.015,.45);
                    const double tail=db(a.rms(1.3,1.6)/a.rms(.015,.15));
                    check(tail < -6.0,p.name+": plucked body loses energy while the key is held");
                    std::printf("REGISTER %s %d %.2f %.6f %.6f\n",name,note,velocity,db(a.rms(.015,.45)),tail);
                }
                const double range=db(effort[2]/effort[0]);
                check(effort[0]<effort[1]&&effort[1]<effort[2],p.name+": three touch levels increase monotonically");
                check(range>8.0&&range<18.0,p.name+": useful touch without silent soft strikes");
                std::printf("TOUCH %s %d %.6f\n",name,note,range);
            }
            // The control has identical patch, pitch, filters, envelopes and
            // gain. Only the new material is disabled. Normalizing the entire
            // spectral vector removes a uniform gain change from this test.
            auto control=p;
            for(int rank=0;rank<2;++rank)control.values[bodyMaterialParam(rank)]=body::defaultMaterial;
            const auto authored=render(p,60,.85f,.4), generic=render(control,60,.85f,.4);
            const double distance=shapeDistance(authored,generic,261.6255653005986,std::string(name)=="Kalimba");
            check(distance>.12,p.name+": material changes the full-patch spectrum after equal-level comparison");
            std::printf("SHAPE %s %.9f\n",name,distance);

            // Sustain retains the body's natural losses; it must not freeze
            // an unchanging sample or start a fresh excitation at note-off.
            const auto released=render(p,60,.85f,.5,1.5,false);
            const auto sustained=render(p,60,.85f,.5,1.5,true);
            const double pedalGain=db(sustained.rms(1.3,1.6)/released.rms(1.3,1.6));
            check(pedalGain>.35,p.name+": sustain postpones key damping of the decaying body");
            check(sustained.rms(1.3,1.6)<sustained.rms(.02,.20),p.name+": pedal-held body still decays physically");
            std::printf("PEDAL %s %.6f\n",name,pedalGain);
        }
    }
    catch(const std::exception& e){check(false,e.what());}
    std::printf("Plucked material preset tests: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
