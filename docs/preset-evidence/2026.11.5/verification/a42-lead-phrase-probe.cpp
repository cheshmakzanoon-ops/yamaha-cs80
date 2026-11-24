// A42: additional whole-preset phrase/controller checks. This diagnostic is
// separate from the registered short-note lead suite and changes no factory data.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cs80;
namespace {
constexpr int rate = 48000;
int checks = 0, failures = 0, renders = 0, ownershipChecks = 0;
long long stereoFrames = 0;
double greatestPeak = 0;
void check(bool ok, const std::string& label) {
    ++checks; if (!ok) { ++failures; std::printf("FAIL,%s\n", label.c_str()); }
}
double db(double x) { return 20*std::log10(std::max(x,1e-15)); }
const FactoryPreset& preset(const char* name) {
    for (const auto& p : programLibrary()) if (p.name == name) return p;
    throw std::runtime_error(std::string("missing preset: ")+name);
}
enum Kind { off, on, channel, poly, ribbon, marker };
struct Event { int frame; Kind kind; int note; float value; };
Event ev(double t, Kind kind, int note=0, float value=0) {
    return {int(std::llround(t*rate)),kind,note,value};
}
struct Audio {
    std::vector<float> data;
    double peak=0;
    int active=0, held=0;
    double rms(double a,double b) const {
        const int from=int(std::llround(a*rate)),to=int(std::llround(b*rate));
        double sum=0;for(int i=2*from;i<2*to;++i)sum+=double(data[size_t(i)])*data[size_t(i)];
        return std::sqrt(sum/(2*(to-from)));
    }
    double pitch(double start,double seconds,double expected) const {
        int begin=int(start*rate), count=int(seconds*rate);
        int low=int(std::floor(rate/(1.18*expected))), high=int(std::ceil(rate/(.84*expected)));
        std::vector<double> c(size_t(high+2));
        for(int lag=low-1;lag<=high+1;++lag) {
            double ab=0,aa=0,bb=0;
            for(int n=begin;n<begin+count;++n) {
                double a=.5*(data[size_t(2*n)]+data[size_t(2*n+1)]);
                double b=.5*(data[size_t(2*(n+lag))]+data[size_t(2*(n+lag)+1)]);
                ab+=a*b;aa+=a*a;bb+=b*b;
            }
            c[size_t(lag)]=ab/std::sqrt(std::max(aa*bb,1e-24));
        }
        int best=low;for(int n=low+1;n<=high;++n)if(c[size_t(n)]>c[size_t(best)])best=n;
        double left=c[size_t(best-1)],middle=c[size_t(best)],right=c[size_t(best+1)];
        double d=left-2*middle+right;
        double offset=std::abs(d)>1e-12?.5*(left-right)/d:0;
        return rate/(best+std::clamp(offset,-.5,.5));
    }
};
Audio render(const FactoryPreset& p,std::vector<Event> events,double seconds,const std::string& label) {
    auto engine=std::make_unique<Engine>();engine->prepare(rate);engine->setParams(p.values);
    std::stable_sort(events.begin(),events.end(),[](auto a,auto b){return a.frame<b.frame;});
    Audio audio;const int frames=int(std::llround(seconds*rate));audio.data.resize(size_t(2*frames));
    std::set<int> held;std::map<int,float> pressure;float channelValue=0;
    size_t index=0;int inspect=-1;
    for(int at=0;at<frames;) {
        while(index<events.size()&&events[index].frame==at) {
            const auto e=events[index++];
            if(e.kind==on){engine->noteOn(e.note,e.value,1);held.insert(e.note);pressure[e.note]=channelValue;}
            else if(e.kind==off){engine->noteOff(e.note,1);held.erase(e.note);pressure.erase(e.note);}
            else if(e.kind==channel){engine->channelPressure(1,e.value);channelValue=e.value;for(int n:held)pressure[n]=e.value;}
            else if(e.kind==poly){engine->polyPressure(e.note,e.value,1);if(held.contains(e.note))pressure[e.note]=e.value;}
            else if(e.kind==ribbon)engine->setRibbon(e.value);
            inspect=at+rate/100;
        }
        if(at==inspect) {
            std::set<int> actual;int count=0;bool pressureOk=true;
            for(int v=0;v<8;++v){auto voice=engine->getVoiceInfo(v);if(voice.active&&voice.held){actual.insert(voice.note);++count;pressureOk=pressureOk&&pressure.contains(voice.note)&&std::abs(voice.pressure-pressure[voice.note])<1e-6;}}
            check(actual==held&&count==int(held.size()),label+": held-note ownership");++ownershipChecks;
            check(pressureOk,label+": pressure belongs to the addressed held notes");
            inspect=-1;
        }
        int next=frames;if(index<events.size())next=std::min(next,events[index].frame);if(inspect>at)next=std::min(next,inspect);
        int count=std::min(137,next-at);if(count<=0)throw std::runtime_error("nonadvancing stream");
        engine->render(audio.data.data()+2*at,count);at+=count;
    }
    bool finite=true;long long clipped=0;
    for(float x:audio.data){finite=finite&&std::isfinite(x);audio.peak=std::max(audio.peak,std::abs(double(x)));clipped+=std::abs(x)>=1;}
    audio.active=engine->activeVoices();for(int v=0;v<8;++v)audio.held+=engine->getVoiceInfo(v).held;
    check(finite,label+": finite output");check(clipped==0,label+": no full-scale samples");
    check(audio.active==0&&audio.held==0,label+": all released voices retire");
    ++renders;stereoFrames+=frames;greatestPeak=std::max(greatestPeak,audio.peak);
    std::printf("RENDER,%s,%.3f,%.9f,%.9f,%d,%d,%lld\n",label.c_str(),seconds,db(audio.peak),db(audio.rms(seconds-.4,seconds-.01)),audio.active,audio.held,clipped);
    std::fflush(stdout);return audio;
}
void phrase(const char* name,bool overlap) {
    std::vector<Event> events {ev(0,channel,0,.60f)};
    const std::array<int,14> line {60,64,67,72,67,64,60,59,62,65,69,65,62,60};
    for(size_t i=0;i<line.size();++i) {
        const double t=i*.32;
        events.push_back(ev(t,on,overlap?line[i]:60,i%2==0?.35f:.9f));
        events.push_back(ev(t+(overlap?.39:.22),off,overlap?line[i]:60));
    }
    events.push_back(ev(1.65,channel,0,.2f));events.push_back(ev(3.25,channel,0,.8f));
    auto a=render(preset(name),events,8,std::string(name)+(overlap?" overlap":" repeat"));
    check(a.rms(.05,4.5)>1e-5,std::string(name)+": phrase remains audible");
}
void pressurePair(const char* name) {
    std::vector<Event> both {ev(0,on,60,.8f),ev(1,poly,60,.85f),ev(2,on,67,.8f),ev(3,poly,60,0),ev(4,poly,67,.85f),ev(5,poly,67,0),ev(5.8,off,60),ev(5.8,off,67)};
    auto control=both;for(auto& e:control)if(e.note==67)e.kind=marker;
    auto a=render(preset(name),both,8,std::string(name)+" poly-pressure two keys");
    auto b=render(preset(name),control,8,std::string(name)+" poly-pressure one-key control");
    double residual=0,denom=0,maxDifference=0;
    for(int i=int(2.2*rate)*2;i<int(2.8*rate)*2;++i){double x=a.data[size_t(i)],y=b.data[size_t(i)];residual+=(x-y)*(x-y);denom+=y*y;maxDifference=std::max(maxDifference,std::abs(x-y));}
    residual=std::sqrt(residual/std::max(denom,1e-24));
    const double initial=a.rms(.3,.8), first=a.rms(1.4,1.9), quiet=a.rms(3.5,3.9),second=a.rms(4.4,4.9),returned=a.rms(5.4,5.75);
    check(initial<1e-6,std::string(name)+": initial unpressed key is silent");
    check(first>1e-4&&second>1e-4,std::string(name)+": each independently pressed key sounds");
    check(quiet<1e-6&&returned<1e-6,std::string(name)+": releasing per-note pressure returns both held keys to silence");
    check(residual<1e-5,std::string(name)+": unpressed second key adds no measurable pitched signal");
    std::printf("POLY,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.12g,%.12g\n",name,db(initial),db(first),db(quiet),db(second),db(returned),residual,maxDifference);
}
void ribbonReturn() {
    const auto a=render(preset("Ribbon Glide Lead"),{ev(0,on,60,.85f),ev(.6,ribbon,0,.30f),ev(1.3,ribbon,0,0),ev(2,ribbon,0,-.25f),ev(2.7,ribbon,0,0),ev(3.5,off,60)},5.5,"Ribbon Glide Lead ribbon return");
    constexpr double c4=261.625565;
    const double before=a.pitch(.4,.08,c4),up=a.pitch(1,.08,c4*1.3),center=a.pitch(1.7,.08,c4),down=a.pitch(2.4,.08,c4*.75),end=a.pitch(3.1,.08,c4);
    check(std::abs(up/before-1.3)<.015,"ribbon positive ratio");
    check(std::abs(down/before-.75)<.015,"ribbon negative ratio");
    check(std::abs(1200*std::log2(center/before))<5&&std::abs(1200*std::log2(end/before))<5,"ribbon returns to center pitch after both directions");
    std::printf("RIBBON,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n",before,up,center,down,end,1200*std::log2(center/before),1200*std::log2(end/before));
}
}
int main() {
    try {
        for(const char* name:{"Velocity Lead","Brass Lead","Reed Lead","Whistle Lead","Fifth Lead","Octave Lead","Glide Lead","Glissando Lead","Noir Lead","Ribbon Glide Lead"}){phrase(name,false);phrase(name,true);}
        pressurePair("Pressure Lead");pressurePair("Electric Leads 136");ribbonReturn();
    } catch(const std::exception& e){check(false,e.what());}
    std::printf("SUMMARY,checks=%d,failures=%d,renders=%d,ownership_checks=%d,stereo_frames=%lld,max_peak_dbfs=%.9f\n",checks,failures,renders,ownershipChecks,stereoFrames,db(greatestPeak));
    return failures?1:0;
}
