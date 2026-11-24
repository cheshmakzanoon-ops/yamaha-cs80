// Actual Engine audio contracts for the existing FX programs. These verify
// contour, cadence, noise-band and pressure distinctions; they do not establish
// equivalence to recorded weather, machinery, or a particular hardware unit.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using namespace cs80;
constexpr double rate = 44100.0;
int failures = 0, checks = 0;
void check(bool condition, const std::string& message)
{
    ++checks;
    if (!condition) { ++failures; std::printf("FAIL: %s\n", message.c_str()); }
}
double db(double value) { return 20.0 * std::log10(std::max(value, 1.e-14)); }
const FactoryPreset& preset(const char* name)
{
    for (const auto& p : curatedLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("Missing FX preset: ") + name);
}
struct Audio
{
    std::vector<float> samples;
    int frame(double time) const { return std::clamp((int) std::lround(time * rate), 0, (int) samples.size()/2); }
    double rms(double begin, double end) const
    {
        double sum = 0.0;
        const int first = frame(begin), last = frame(end);
        for (int i=first; i<last; ++i)
            sum += .5 * (double(samples[2*i])*samples[2*i] + double(samples[2*i+1])*samples[2*i+1]);
        return std::sqrt(sum / std::max(1,last-first));
    }
};
Audio render(const ParamValues& values, double seconds, float pressure = 0.0f, float velocity = .9f, int note = 60)
{
    auto engine = std::make_unique<Engine>();
    engine->prepare(rate); engine->setParams(values);
    engine->noteOn(note,velocity,1); engine->channelPressure(1,pressure);
    Audio a; a.samples.resize(2*(int)std::lround(rate*seconds));
    for(int at=0;at<(int)a.samples.size()/2;)
    {
        const int count=std::min(256,(int)a.samples.size()/2-at);
        engine->render(a.samples.data()+2*at,count); at+=count;
    }
    double peak=0.0; bool finite=true;
    for(float x:a.samples) { finite=finite&&std::isfinite(x); peak=std::max(peak,std::abs(double(x))); }
    check(finite && peak<1.0,"complete FX audio render remains finite with headroom");
    return a;
}
// Pairing identical deterministic carriers removes their random window-energy
// fluctuation and exposes the gain motion introduced by a modulation change.
std::vector<double> motion(const Audio& moving,const Audio& control,double begin,double end,double step=.025)
{
    std::vector<double> values;
    for(double t=begin;t+step<=end;t+=step)
        values.push_back(db(moving.rms(t,t+step)/std::max(control.rms(t,t+step),1.e-14)));
    return values;
}
double deviation(const std::vector<double>& values)
{
    double sum=0.0, squares=0.0;
    for(double x:values) { sum+=x; squares+=x*x; }
    return std::sqrt(std::max(0.0,squares/values.size()-std::pow(sum/values.size(),2)));
}
double differenceRms(const Audio& a,const Audio& b,double begin,double end)
{
    double sum=0.0;
    for(int i=a.frame(begin)*2;i<a.frame(end)*2;++i) { const double d=a.samples[i]-b.samples[i]; sum+=d*d; }
    return std::sqrt(sum/std::max(1,2*(a.frame(end)-a.frame(begin))));
}
double highShare(const Audio& a,double begin,double end)
{
    // One-pole 1500 Hz split: a broad crack must have much more upper-band
    // energy than the rumble. Filter warm-up starts at the beginning of audio.
    const double k=1.0-std::exp(-2.0*3.141592653589793*1500.0/rate);
    std::array<double,2> low{}; double highPower=0.0,totalPower=0.0;
    for(int i=0;i<a.frame(end);++i) for(int ch=0;ch<2;++ch)
    {
        const double x=a.samples[2*i+ch]; low[ch]+=k*(x-low[ch]);
        if(i>=a.frame(begin)) { highPower+=(x-low[ch])*(x-low[ch]); totalPower+=x*x; }
    }
    return highPower/std::max(totalPower,1.e-20);
}
void naturalContours()
{
    const auto& wind=preset("Wind"); auto flat=wind.values;
    flat[P_lfo1Depth]=flat[P_lfo2Depth]=flat[P_lfo3Depth]=0.0f;
    const auto gust=render(wind.values,12.0), still=render(flat,12.0);
    const auto gain=motion(gust,still,1.0,12.0);
    const double gustDeviation=deviation(gain);
    double repeatingError=0.0; const int lag=(int)std::lround((1.0/.7)/.025);
    for(int i=lag;i<(int)gain.size();++i) repeatingError+=std::pow(gain[i]-gain[i-lag],2);
    repeatingError=std::sqrt(repeatingError/(gain.size()-lag));
    check(gustDeviation>.25,"Wind: audible gusts differ from an otherwise identical static noise bed");
    check(repeatingError>.25,"Wind: gain contour does not repeat with the old .70 Hz pulse");
    std::printf("WIND gust deviation %.3f dB; old-period difference %.3f dB\n",gustDeviation,repeatingError);

    const auto surf=render(preset("Surf").values,10.0);
    double crest=0.0,crestAt=0.0;
    for(double t=.05;t<4.0;t+=.05) { const double r=surf.rms(t,t+.05); if(r>crest) { crest=r; crestAt=t; } }
    const double rise=db(crest/surf.rms(0.0,.05)),drain=db(surf.rms(8.5,9.5)/crest);
    check(rise>8.0 && crestAt>.12,"Surf: the wave swells into its crest instead of striking immediately");
    check(drain<-15.0,"Surf: held one-shot wave drains away after its crest");
    auto spray=preset("Surf").values, surge=spray;
    spray[P_mix]=1.0f; surge[P_mix]=0.0f;
    const auto high=render(spray,5.0),low=render(surge,5.0);
    const double foamFall=db(high.rms(3.8,4.5)/high.rms(.5,1.5));
    const double surgeFall=db(low.rms(3.8,4.5)/low.rms(.5,1.5));
    check(foamFall<surgeFall-15.0,"Surf: separate bright foam subsides before the low wash");
    std::printf("SURF rise %.3f dB, crest %.2f s, drain %.3f dB; foam/low falls %.3f / %.3f dB\n",rise,crestAt,drain,foamFall,surgeFall);

    const auto thunder=render(preset("Thunder").values,7.0);
    const double crack=highShare(thunder,.002,.055),roll=highShare(thunder,.4,.9);
    const double decay=db(thunder.rms(5.8,6.8)/thunder.rms(.15,.55));
    check(crack>roll*5.0,"Thunder: broadband crack resolves into a darker low rumble");
    check(decay<-12.0,"Thunder: held rumble keeps losing energy after the impact");
    auto evenRoll=preset("Thunder").values;
    evenRoll[P_lfo1Depth]=evenRoll[P_lfo2Depth]=evenRoll[P_lfo3Depth]=0.0f;
    const auto even=render(evenRoll,7.0);
    const double irregular=deviation(motion(thunder,even,.3,6.5));
    check(irregular>.25,"Thunder: rolling intensity differs audibly from a mechanically even decay");
    std::printf("THUNDER high-band crack/roll %.5f / %.5f; tail %.3f dB; roll variation %.3f dB\n",crack,roll,decay,irregular);
}
void rainContinuity()
{
    const auto& rain=preset("Rain"); auto flat=rain.values,stepped=rain.values;
    flat[P_lfo1Depth]=0.0f; stepped[P_lfo1Wave]=5.0f;
    const auto smooth=render(rain.values,2.5),control=render(flat,2.5),hold=render(stepped,2.5);
    const auto a=motion(smooth,control,.2,2.5,.002),b=motion(hold,control,.2,2.5,.002);
    double smoothEdge=0.0,steppedEdge=0.0;
    for(size_t i=1;i<a.size();++i) { smoothEdge=std::max(smoothEdge,std::abs(a[i]-a[i-1])); steppedEdge=std::max(steppedEdge,std::abs(b[i]-b[i-1])); }
    check(deviation(a)>.1,"Rain: the continuous bed retains audible fine level motion");
    check(smoothEdge<steppedEdge*.5,"Rain: smooth motion avoids the sharp whole-bed steps of a sample-and-hold control");
    std::printf("RAIN smooth/stepped maximum two-ms gain changes %.4f / %.4f dB\n",smoothEdge,steppedEdge);
}
void clockAndPressure()
{
    const auto clock=render(preset("Clockwork").values,2.0);
    double early=0.0,late=0.0,body=0.0;
    for(int beat=2;beat<11;++beat) { const double t=beat/6.0; early+=std::pow(clock.rms(t+.005,t+.035),2); late+=std::pow(clock.rms(t+.105,t+.145),2); body+=std::pow(clock.rms(t+.055,t+.075),2); }
    const double gap=db(std::sqrt(late/early)), tickFall=db(std::sqrt(body/early));
    check(gap<-25.0,"Clockwork: short ticks fall silent between six-per-second strikes");
    check(tickFall<-10.0,"Clockwork: each tick decays before a square-gated tone would close");
    std::printf("CLOCKWORK within-tick decay %.3f dB; between-tick attenuation %.3f dB\n",tickFall,gap);

    const auto& breath=preset("Breath FX");
    const auto soft=render(breath.values,1.5,0.0f),pressed=render(breath.values,1.5,1.0f);
    const double contrast=db(pressed.rms(.5,1.4)/soft.rms(.5,1.4));
    check(contrast>9.0,"Breath FX: pressure produces a pronounced whisper-to-exhale dynamic range");
    std::printf("BREATH pressure range %.3f dB\n",contrast);

    const auto& robot=preset("Robot Voice");auto noVowel=robot.values;noVowel[P_body1Mix]=0.0f;
    const auto vowel=render(robot.values,1.1,.5f),pulse=render(noVowel,1.1,.5f);
    const double change=db(differenceRms(vowel,pulse,.25,1.0)/pulse.rms(.25,1.0));
    check(change>-8.0,"Robot Voice: voiced material makes an audible difference after the ring and filters");
    std::printf("ROBOT voiced-material residual %.3f dB relative to pulse control\n",change);
}
}
int main()
{
    try
    {
        int count=0;
        for(const auto& p:curatedLibrary()) if(p.category=="FX") ++count;
        check(count==24,"all 24 existing FX addresses remain present");
        naturalContours(); rainContinuity(); clockAndPressure();
    }
    catch(const std::exception& e) { check(false,e.what()); }
    std::printf("FxRefinementTests: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
