// Causal follow-up for retained A42 probe assumptions. No factory edits.
// Non-MPE note-on resets its local pressure; a subsequent channel-pressure
// event updates all active addressed voices. TWS pressure release is smoothed.
#define main a42_initial_probe_main
#include "a42-lead-phrase-probe.cpp"
#undef main

namespace {
void channelContract(const char* name) {
    auto engine=std::make_unique<Engine>();engine->prepare(rate);engine->setParams(preset(name).values);
    std::array<float,256> block{};double peak=0;bool finite=true;int total=0;
    auto advance=[&](double seconds){int frames=int(std::llround(seconds*rate));for(int at=0;at<frames;){int n=std::min(128,frames-at);engine->render(block.data(),n);for(int i=0;i<2*n;++i){finite=finite&&std::isfinite(block[size_t(i)]);peak=std::max(peak,std::abs(double(block[size_t(i)])));}at+=n;}total+=frames;};
    int matched=0;
    for(int k=0;k<12;++k) {
        const int note=60+k%5;
        // Reproduce the initial probe's incorrect pressure-inheritance assumption.
        engine->channelPressure(1,.6f);engine->noteOn(note,.8f,1);advance(.02);
        bool found=false;
        for(int v=0;v<8;++v){auto x=engine->getVoiceInfo(v);if(x.active&&x.held&&x.note==note){found=true;check(x.pressure==0,std::string(name)+": new non-MPE note resets local pressure");++matched;}}
        check(found,std::string(name)+": accepted note exists");
        engine->channelPressure(1,.6f);advance(.02);
        for(int v=0;v<8;++v){auto x=engine->getVoiceInfo(v);if(x.active&&x.held&&x.note==note)check(x.pressure==.6f,std::string(name)+": subsequent channel event reaches new note");}
        engine->noteOff(note,1);advance(.13);
    }
    advance(4.0);
    check(matched==12,std::string(name)+": all12 accepted-note pressure resets observed");
    check(finite&&peak<1,std::string(name)+": finite channel-event phrase with headroom");
    check(engine->activeVoices()==0,std::string(name)+": channel-event phrase releases every voice");
    ++renders;stereoFrames+=total;greatestPeak=std::max(greatestPeak,peak);
    std::printf("CHANNEL_CONTRACT,%s,notes=%d,stereo_frames=%d,peak_dbfs=%.9f,final_active=%d\n",name,matched,total,db(peak),engine->activeVoices());
}
void pressureFade(const char* name) {
    const auto a=render(preset(name),{ev(0,on,60,.8f),ev(1,poly,60,.85f),ev(2,on,67,.8f),ev(3,poly,60,0),ev(4.5,poly,67,.85f),ev(6,poly,67,0),ev(8.5,off,60),ev(8.5,off,67)},10,std::string(name)+" settled pressure release");
    const double first=a.rms(1.4,1.9), firstSettled=a.rms(4.0,4.4),second=a.rms(5.4,5.9),secondSettled=a.rms(7.5,8.0);
    check(first>1e-4&&second>1e-4,std::string(name)+": both per-note gestures remain audible");
    check(firstSettled<1e-6&&secondSettled<1e-6,std::string(name)+": pressure-off held keys settle below -120 dBFS");
    check(firstSettled<first*1e-4&&secondSettled<second*1e-4,std::string(name)+": pressure-off fall exceeds 80 dB while both keys remain held");
    std::printf("PRESSURE_FADE,%s,first_dbfs=%.9f,first_settled_dbfs=%.9f,first_change_db=%.9f,second_dbfs=%.9f,second_settled_dbfs=%.9f,second_change_db=%.9f\n",name,db(first),db(firstSettled),db(firstSettled/first),db(second),db(secondSettled),db(secondSettled/second));
}
}
int main() {
    try {
        channelContract("Velocity Lead");channelContract("Pressure Lead");
        pressureFade("Pressure Lead");pressureFade("Electric Leads 136");
    } catch(const std::exception& e){check(false,e.what());}
    std::printf("SUMMARY,checks=%d,failures=%d,renders=%d,ownership_checks=%d,stereo_frames=%lld,max_peak_dbfs=%.9f\n",checks,failures,renders,ownershipChecks,stereoFrames,db(greatestPeak));
    return failures?1:0;
}
