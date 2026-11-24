// A23 evidence probe for source c5549f7d095a5c5f49a5ad22ec0ccc819245d201.
// Unmodified, category-confirmed real presets only.
// Compile separately against each source revision's complete headers/library.
#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
using namespace cs80;
struct Event { double time; int kind; int note; float value; };
struct Case { std::string id, name, role; int rate; double duration; std::vector<Event> events; };
const FactoryPreset& preset(const std::string& name)
{
    for (const auto& p : programLibrary())
        if (p.name == name && p.category == "Leads") return p;
    throw std::runtime_error("missing Leads preset: " + name);
}
int main(int argc, char** argv)
{
    if (argc != 4) return 2;
    const std::filesystem::path out(argv[1]);
    const std::string scope(argv[2]), source(argv[3]);
    std::filesystem::create_directories(out);
    std::vector<Case> cases;
    if (scope == "all")
    {
        for (const auto& item : std::vector<std::pair<std::string,std::string>>{
                 {"overlap_touch","Touch Lead"}, {"overlap_original","Ribbon Lead"}})
            cases.push_back({item.first,item.second,"overlap",48000,2.7,
                {{0,0,60,.72f},{.6,0,67,.62f},{1.15,1,67,0},{1.6,1,60,0}}});
        cases.push_back({"ribbon_return","Ribbon Glide Lead","ribbon",48000,2.7,
            {{0,0,60,.85f},{.6,2,0,.35f},{1.1,2,0,0},{1.8,1,60,0}}});
        cases.push_back({"glide_both_directions","Glide Lead","glide",48000,4.0,
            {{0,0,55,.85f},{.5,1,55,0},{.5,0,67,.85f},{1.8,1,67,0},
             {1.8,0,55,.85f},{3.1,1,55,0}}});
        cases.push_back({"pressure_vibrato_return","Aftertouch Vibrato Lead","vibrato",48000,3.2,
            {{0,0,67,.55f},{.6,3,0,1},{1.6,3,0,0},{2.4,1,67,0}}});
        cases.push_back({"pressure_required_original","Electric Leads 136","pressure",48000,3.3,
            {{0,0,67,.55f},{.5,3,0,.3f},{1.1,3,0,1},{1.8,3,0,0},{2.5,1,67,0}}});
        for (const auto& item : std::vector<std::pair<std::string,std::string>>{
                 {"modern_drive_release","Drive Lead"}, {"modern_space_release","Space Lead"}})
            cases.push_back({item.first,item.second,"modern_release",48000,4.0,
                {{0,0,67,.85f},{.5,3,0,.7f},{1.6,1,67,0}}});
    }
    for (int rate : {44100,96000})
        for (const auto& item : std::vector<std::pair<std::string,std::string>>{
                 {"reed","Reed Lead"}, {"brass","Neo-Noir Brass Lead"}})
            cases.push_back({item.first+"_"+std::to_string(rate),item.second,"body_rate",rate,3.0,
                {{0,0,67,.7f},{.6,3,0,.8f},{1.1,2,0,.2f},{1.5,2,0,0},{2.0,1,67,0}}});
    std::ofstream meta(out/"cases.csv"), trace(out/"trace.csv");
    meta << "id,name,role,source,rate,duration,frames,peak,nonfinite,over_full_scale\n";
    trace << "id,frame,slot,note,active,held,pressure,ribbon,amp0,hz0,amp1,hz1\n";
    meta << std::setprecision(12); trace << std::setprecision(12);
    for (const auto& c : cases)
    {
        const auto& p = preset(c.name);
        auto engine = std::make_unique<Engine>();
        engine->prepare(c.rate); engine->setParams(p.values);
        const int frames = (int)std::llround(c.duration*c.rate), step=c.rate/100;
        std::vector<float> audio((size_t)frames*2);
        size_t event=0; int nextTrace=step, at=0;
        while (at<frames)
        {
            while (event<c.events.size() && (int)std::llround(c.events[event].time*c.rate)==at)
            {
                const auto e=c.events[event++];
                if (e.kind==0) engine->noteOn(e.note,e.value,1);
                else if (e.kind==1) engine->noteOff(e.note,1);
                else if (e.kind==2) engine->setRibbon(e.value);
                else engine->channelPressure(1,e.value);
            }
            int end=std::min({at+127,frames,nextTrace});
            if (event<c.events.size()) end=std::min(end,(int)std::llround(c.events[event].time*c.rate));
            if (end<=at) throw std::runtime_error("nonadvancing event schedule");
            engine->render(audio.data()+2*at,end-at); at=end;
            if (at==nextTrace || at==frames)
            {
                for (int slot=0;slot<8;++slot)
                {
                    const auto v=engine->getVoiceInfo(slot);
                    if (!v.active && !v.held) continue;
                    const auto a=engine->getRankInfo(slot,0), b=engine->getRankInfo(slot,1);
                    trace << c.id << ',' << at << ',' << slot << ',' << v.note << ','
                          << v.active << ',' << v.held << ',' << v.pressure << ',' << v.ribbonRatio << ','
                          << a.ampEnvelope << ',' << a.oscillatorHz << ',' << b.ampEnvelope << ',' << b.oscillatorHz << '\n';
                }
                nextTrace+=step;
            }
        }
        double peak=0; int nonfinite=0, over=0;
        for (float v:audio)
        { if (!std::isfinite(v)) ++nonfinite; else peak=std::max(peak,std::abs((double)v)); if(std::abs(v)>1) ++over; }
        std::ofstream pcm(out/(c.id+".f32"),std::ios::binary);
        pcm.write(reinterpret_cast<const char*>(audio.data()),(std::streamsize)(audio.size()*sizeof(float)));
        meta << c.id << ',' << c.name << ',' << c.role << ',' << source << ',' << c.rate << ','
             << c.duration << ',' << frames << ',' << peak << ',' << nonfinite << ',' << over << '\n';
        std::cout << c.id << ": " << c.name << ", " << frames << " frames, peak=" << peak
                  << ", nonfinite=" << nonfinite << ", over=" << over << '\n';
    }
}
