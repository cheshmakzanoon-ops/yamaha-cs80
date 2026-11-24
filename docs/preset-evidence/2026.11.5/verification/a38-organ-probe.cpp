// A38 independent, bounded organ phrase/registration diagnostic.
// Reuses the unmodified compiled Engine. No production parameters are changed.
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
using namespace cs80;
namespace {
constexpr int rate = 48000;
constexpr double pi = 3.1415926535897932384626433832795;
struct Audio {
    std::vector<float> x;
    double rms(double begin, double end) const {
        const int a = (int) std::llround(begin * rate), b = (int) std::llround(end * rate);
        double e = 0;
        for (int i = 2*a; i < 2*b; ++i) e += (double)x[i]*x[i];
        return std::sqrt(e / (2*(b-a)));
    }
    double peak() const { double p=0; for(float v:x) { if (!std::isfinite(v)) throw std::runtime_error("nonfinite audio"); p=std::max(p,std::abs((double)v)); } return p; }
    double difference(const Audio& b) const { double p=0; for(size_t i=0;i<x.size();++i)p=std::max(p,std::abs((double)x[i]-b.x[i]));return p; }
};
double db(double v) { return 20*std::log10(std::max(v,1e-15)); }
const FactoryPreset& find(const char* name) {
    for(const auto& p:programLibrary())if(p.name==name)return p;
    throw std::runtime_error(std::string("missing preset: ")+name);
}
Audio phrase(const FactoryPreset& p, bool swappedVelocity, bool pressure) {
    auto e=std::make_unique<Engine>(); e->prepare(rate);e->setParams(p.values);
    Audio a;a.x.resize(2*105600); // 2.2 seconds, including 600 ms final tail.
    constexpr std::array<int,4> first{48,55,60,64}, second{50,57,62,65};
    // Different chords overlap for 40 ms, exercising all eight voices. The
    // original chord is repeated after a 120 ms gap without resetting Engine.
    constexpr int events[]{0,12000,24000,25920,36000,47040,52800,62400,72000,76800,105600};
    int next=0;
    for(int at=0;at<105600;) {
        if(at==events[next]) {
            if(at==0||at==52800)for(int n:first)e->noteOn(n,swappedVelocity?.9f:.2f,1);
            if(at==24000)for(int n:second)e->noteOn(n,swappedVelocity?.2f:.9f,1);
            if(at==25920||at==76800)for(int n:first)e->noteOff(n,1);
            if(at==47040)for(int n:second)e->noteOff(n,1);
            if(pressure&&(at==12000||at==62400))e->channelPressure(1,1.f);
            if(pressure&&(at==36000||at==72000))e->channelPressure(1,0.f);
            ++next;
        }
        const int count=std::min(128,events[next]-at);
        e->render(a.x.data()+2*at,count);at+=count;
    }
    return a;
}
Audio single(const FactoryPreset& p,int rank,int note,float pressure=0) {
    auto values=p.values;if(rank>=0)values[P_mix]=(float)rank;
    auto e=std::make_unique<Engine>();e->prepare(rate);e->setParams(values);
    e->noteOn(note,.7f,1);e->channelPressure(1,pressure);
    Audio a;a.x.resize(2*57600);
    for(int at=0;at<57600;at+=128)e->render(a.x.data()+2*at,std::min(128,57600-at));
    return a;
}
double tone(const Audio& a,double hz,double from=.5,double to=1.1) {
    const int start=(int)(from*rate),stop=(int)(to*rate);
    double real[2]{},imag[2]{},weight=0;
    for(int i=start;i<stop;++i) {
        const double w=.5-.5*std::cos(2*pi*(i-start)/(stop-start-1));
        const double angle=2*pi*hz*(i-start)/rate;
        for(int ch=0;ch<2;++ch) {real[ch]+=w*a.x[2*i+ch]*std::cos(angle);imag[ch]+=w*a.x[2*i+ch]*std::sin(angle);}
        weight+=w;
    }
    return std::sqrt(real[0]*real[0]+imag[0]*imag[0]+real[1]*real[1]+imag[1]*imag[1])/weight;
}
}
int main() {
    try {
        std::printf("{\"revision\":\"%s\",\"sample_rate\":%d,\"phrases\":[",curatedLibraryRevision,rate);
        int count=0;
        for(const auto& p:programLibrary())if(p.category=="Organ"||p.name=="Tone I: Organ 1"||p.name=="Tone II: Organ 2") {
            const auto plain=phrase(p,false,false),velocity=phrase(p,true,false),pressure=phrase(p,false,true);
            const double peak=std::max({plain.peak(),velocity.peak(),pressure.peak()});
            if(count++)std::printf(",");
            std::printf("{\"name\":\"%s\",\"peak_dbfs\":%.9f,\"first_held_dbfs\":%.9f,\"second_held_dbfs\":%.9f,\"repeated_held_dbfs\":%.9f,\"tail_dbfs\":%.9f,\"tail_relative_db\":%.9f,\"velocity_max_absolute_difference\":%.12g,\"pressure_max_absolute_difference\":%.12g}",p.name.c_str(),db(peak),db(plain.rms(.3,.48)),db(plain.rms(.7,.9)),db(plain.rms(1.3,1.5)),db(plain.rms(1.8,2.0)),db(plain.rms(1.8,2.0)/plain.rms(1.3,1.5)),plain.difference(velocity),plain.difference(pressure));
        }
        std::printf("],\"registration\":[");count=0;
        // Independent nominal ratios are explicit, rather than deriving the
        // expectation from the preset's footage parameter under test.
        struct Registration {const char* name;double lower;double upper;};
        constexpr Registration regs[]{
            {"Flute Organ 8+4",1,2},{"Flute Organ 16+8",.5,1},
            {"Chapel Organ",1,4},{"Church Organ",.5,2},{"Fifth Organ",1,1.5}
        };
        for(const auto& c:regs)for(int rank=0;rank<2;++rank) {
            const auto a=single(find(c.name),rank,60);
            const double ratio=rank==0?c.lower:c.upper,expected=261.6255653005986*ratio;
            double wrong=0;for(double r:{.5,1.,1.5,2.,4.})if(r!=ratio)wrong=std::max(wrong,tone(a,261.6255653005986*r));
            if(count++)std::printf(",");
            std::printf("{\"name\":\"%s\",\"rank\":%d,\"expected_ratio\":%.3f,\"expected_hz\":%.9f,\"expected_line_dbfs\":%.9f,\"fundamental_over_other_tested_footages_db\":%.9f,\"peak_dbfs\":%.9f}",c.name,rank+1,ratio,expected,db(tone(a,expected)),db(tone(a,expected)/wrong),db(a.peak()));
        }
        std::printf("],\"modern_pressure\":[");count=0;
        const auto& modern=find("Tremolo LFO Organ");
        for(int rank=-1;rank<2;++rank) {
            const auto plain=single(modern,rank,60,0),pressed=single(modern,rank,60,1);
            if(count++)std::printf(",");
            std::printf("{\"rank\":%d,\"gain_db\":%.9f,\"max_absolute_difference\":%.12g,\"octave_to_root_change_db\":%.9f,\"peak_dbfs\":%.9f}",rank+1,db(pressed.rms(.5,1.0)/plain.rms(.5,1.0)),plain.difference(pressed),db((tone(pressed,523.2511306011972)/tone(pressed,261.6255653005986))/(tone(plain,523.2511306011972)/tone(plain,261.6255653005986))),db(std::max(plain.peak(),pressed.peak())));
        }
        std::printf("]}\n");
        return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"A38: %s\n",e.what());return 1;}
}
