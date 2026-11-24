// A44: bounded long-window behavior of existing environmental and mechanical FX.
// Compiled with source6094 headers and its frozen core. No production data changed.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <stdexcept>
#include <string>

using namespace cs80;
namespace {
constexpr int rate = 48000;
struct Case { const char* id; const char* name; int seconds; float pressure; int control; bool repeated; int hold; };
constexpr std::array<Case,13> cases {{
    {"wind-moving","Wind",36,0,0,false,36},
    {"wind-flat","Wind",36,0,1,false,36},
    {"wind-periodic-control","Wind",36,0,2,false,36},
    {"rain-moving","Rain",36,0,0,false,36},
    {"rain-flat","Rain",36,0,1,false,36},
    {"rain-fine-flat","Rain",36,0,3,false,36},
    {"surf-repeated","Surf",40,0,0,true,12},
    {"thunder-repeated","Thunder",40,0,0,true,12},
    {"helicopter-soft-pressure","Helicopter",12,0,0,false,12},
    {"helicopter-full-pressure","Helicopter",12,1,0,false,12},
    {"siren-soft-pressure","Siren",12,0,0,false,12},
    {"siren-full-pressure","Siren",12,1,0,false,12},
    {"clockwork-long","Clockwork",16,0,0,false,12}
}};
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::filesystem::path output(argv[1]);
    std::filesystem::create_directories(output);
    std::ofstream summary(output/"summary.csv"), settings(output/"settings.csv");
    summary << "case,name,host_index,seconds,frames,pressure,peak,rms,nonfinite,over_full_scale,active_at_end\n" << std::setprecision(17);
    settings << "case";
    for(int i=0;i<P_numParams;++i) settings << ',' << paramId(i);
    settings << '\n' << std::setprecision(9);
    try {
        for(const auto& c:cases) {
            const auto& bank=programLibrary();
            auto it=std::find_if(bank.begin(),bank.end(),[&](const auto& p){return p.name==c.name;});
            if(it==bank.end()) throw std::runtime_error("Missing preset");
            auto values=it->values;
            if(c.control==1 || c.control==2) {
                values[P_lfo1Depth]=values[P_lfo2Depth]=values[P_lfo3Depth]=0;
                if(c.control==2) {
                    values[P_lfo2Depth]=it->values[P_lfo2Depth];
                    values[P_lfo2Wave]=1; values[P_lfo2Rate]=.7f;
                }
            }
            if(c.control==3) values[P_lfo1Depth]=0;
            settings << c.id;
            for(int i=0;i<P_numParams;++i) settings << ',' << values[i];
            settings << '\n'; settings.flush();
            auto e=std::make_unique<Engine>();e->prepare(rate);e->setParams(values);
            e->noteOn(60,.9f,1);e->channelPressure(1,c.pressure);
            std::ofstream pcm(output/(std::string(c.id)+".f32"),std::ios::binary);
            std::ofstream trace(output/(std::string(c.id)+"-trace.csv"));
            trace << "frame,sub_value,sub_rate,active\n" << std::setprecision(9);
            std::array<float,512> block{};
            const int frames=c.seconds*rate;
            double peak=0,sum=0;long long bad=0,clipped=0;
            for(int at=0;at<frames;) {
                if(at==c.hold*rate) e->noteOff(60,1);
                if(c.repeated && at==16*rate) {e->noteOn(60,.9f,1);e->channelPressure(1,c.pressure);}
                if(c.repeated && at==28*rate) e->noteOff(60,1);
                int next=frames;
                for(int event:{c.hold*rate,c.repeated?16*rate:frames,c.repeated?28*rate:frames}) if(event>at) next=std::min(next,event);
                const int count=std::min({256,frames-at,next-at});
                e->render(block.data(),count);
                pcm.write(reinterpret_cast<const char*>(block.data()),2*count*sizeof(float));
                for(int i=0;i<2*count;++i) {
                    const double x=block[i];
                    if(!std::isfinite(x)) {++bad;continue;}
                    if(std::abs(x)>=1.0)++clipped;
                    peak=std::max(peak,std::abs(x));sum+=x*x;
                }
                at+=count;
                trace << at << ',' << e->getSubOscillatorValue() << ',' << e->getSubOscillatorRate() << ',' << e->activeVoices() << '\n';
            }
            if(!pcm||!trace)throw std::runtime_error("Output write failed");
            summary << c.id << ',' << c.name << ',' << (it-bank.begin()) << ',' << c.seconds << ',' << frames << ',' << c.pressure << ',' << peak << ',' << std::sqrt(sum/(2*frames)) << ',' << bad << ',' << clipped << ',' << e->activeVoices() << '\n';
            summary.flush();
            std::printf("%s: %d frames, peak %.6f dBFS, nonfinite %lld, full-scale %lld, active %d\n",c.id,frames,20*std::log10(std::max(peak,1.e-15)),bad,clipped,e->activeVoices());std::fflush(stdout);
        }
    } catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
    return 0;
}
