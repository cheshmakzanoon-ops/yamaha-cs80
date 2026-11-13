// Bounded supported native histories for fixed modern spring/plate/tape wet/dry tails.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen112Roster.h"
#include "Vst3Frozen115Roster.h"
#include <fstream>
namespace {
using NamedCurves = std::vector<std::pair<int, Curve>>;
int renderCount=0;
// Fixture-local long ordinary note history; historical Instance::process is unchanged.
void processHeld(Instance& instance, int start, int count, Changes* changes, std::vector<float>& output)
{
    require(start >= 0 && count > 0 && count <= 1024 && start + count <= 65536, "bounded long process");
    std::vector<float> left(size_t(count), 0.f), right(size_t(count), 0.f);
    float* channels[] {left.data(), right.data()};
    AudioBusBuffers bus {}; bus.numChannels = 2; bus.channelBuffers32 = channels;
    Events events;
    for (int key = 0; key < 2; ++key) {
        for (bool on : {true, false}) {
            const int absolute = (on ? 17 : 8192) + key;
            if (absolute < start || absolute >= start + count) continue;
            Event event {}; event.busIndex = 0; event.sampleOffset = absolute - start;
            event.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
            if (on) { event.noteOn.channel = 0; event.noteOn.pitch = int16(48 + key);
                event.noteOn.velocity = .75f; event.noteOn.noteId = 48 + key; }
            else { event.noteOff.channel = 0; event.noteOff.pitch = int16(48 + key);
                event.noteOff.velocity = 0.f; event.noteOff.noteId = 48 + key; }
            events.events.push_back(event);
        }
    }
    std::stable_sort(events.events.begin(), events.events.end(),
        [](const Event& a, const Event& b) { return a.sampleOffset < b.sampleOffset; });
    ProcessContext context {}; context.sampleRate = testSampleRate; context.projectTimeSamples = start;
    ProcessData data {}; data.processMode = testProcessMode; data.symbolicSampleSize = kSample32;
    data.numSamples = count; data.numOutputs = 1; data.outputs = &bus;
    data.inputEvents = &events; data.inputParameterChanges = changes; data.processContext = &context;
    require(instance.audio->process(data) == kResultOk, "long ordinary VST3 processing");
    for (int i = 0; i < count; ++i) { output.push_back(left[size_t(i)]); output.push_back(right[size_t(i)]); }
}


std::vector<float> renderSession(Module& module,const cs80::ParamValues& p,
                                const NamedCurves& curves,int block,bool flatten=false,bool nullChanges=false)
{
    ++renderCount; Instance instance(module,p,13,13);
    std::vector<float> audio; audio.reserve(131072);
    for(int start=0;start<65536;start+=block) {
        const int count=std::min(block,65536-start); Changes changes;
        for(const auto& [parameter,curve]:curves) {
            const double initial=normalized(parameter,p[parameter]); Curve points;
            if(!flatten) for(const auto& point:curve)
                if(point.first>=start && point.first<start+count)points.emplace_back(point.first-start,point.second);
            if(points.empty() || points.back().first!=count-1)
                points.emplace_back(flatten?0:count-1,absoluteValue(curve,initial,start+count-1));
            changes.add(parameterId(cs80::paramId(parameter)),std::move(points));
        }
        processHeld(instance,start,count,nullChanges?nullptr:&changes,audio);
    }
    (void)difference(audio,audio);require(audio.size()==131072,"complete stereo capture");return audio;
}
bool exactAudio(const std::vector<float>& a,const std::vector<float>& b)
{(void)difference(a,b);return std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;}
void capture(const std::filesystem::path& path,const std::vector<float>& audio)
{(void)difference(audio,audio);std::ofstream f(path,std::ios::binary);require(bool(f),"capture open");f.write(reinterpret_cast<const char*>(audio.data()),std::streamsize(audio.size()*sizeof(float)));require(bool(f),"capture complete");}
}
int main(int argc,char**argv) {
 try {
    require(argc==7,"MODULE OUTPUT HOST_CONTRACT FX_CONTRACT RATE MODE");
    const bool compat=false;
    const std::string rate(argv[5]),mode(argv[6]);
    require(rate=="48000","supported rate");
    require(mode=="realtime" || mode=="offline","supported mode");
    testSampleRate=std::stod(rate);testProcessMode=mode=="offline"?kOffline:kRealtime;
    const ws1::Tolerances host(argv[3],"host_automation"),limits(argv[4],"host_automation_fx_modern_tail");
    require(cs80::kSampleAccurateParameters.size()>=112,"112 prefix available");
    require(std::equal(cs80::kFrozen112Roster.begin(),cs80::kFrozen112Roster.end(),cs80::kSampleAccurateParameters.begin()),"literal112 prefix preserved");
    constexpr std::array mixes{int(cs80::P_fx1Mix),int(cs80::P_fx2Mix),int(cs80::P_fx3Mix)};
    for(size_t i=0;i<mixes.size();++i)require(cs80::paramId(mixes[i])=="fx"+std::to_string(i+1)+"_mix","stable FX mix IDs");
    require(limits["key_parameter_count"]==115,"115 lane contract");
    if(!compat){require(cs80::kSampleAccurateParameters.size()>=115,"115 capability prefix");
      require(std::equal(cs80::kFrozen115Roster.begin(),cs80::kFrozen115Roster.end(),cs80::kSampleAccurateParameters.begin()),"literal115 prefix preserved");
      require(std::equal(mixes.begin(),mixes.end(),cs80::kSampleAccurateParameters.begin()+112),"mixes at112..114");}
    std::cout<<"compiled_lanes="<<cs80::kSampleAccurateParameters.size()<<"\n";
    const std::filesystem::path out(argv[2]);require(!std::filesystem::exists(out),"fresh capture directory");std::filesystem::create_directory(out);
    Module module(argv[1]);
        auto parameters = cs80::defaultValues();
        parameters[cs80::P_sourcePerformance] = 1;
        parameters[cs80::P_parityMode] = 0;
        parameters[cs80::P_drift] = parameters[cs80::P_voiceDispersion] = parameters[cs80::P_analogNoise] = 0;
        parameters[cs80::P_kbdLevelLow] = 0;
        parameters[cs80::P_kbdLevelHigh] = 0;
        parameters[cs80::P_kbdBrillianceLow] = parameters[cs80::P_kbdBrillianceHigh] = 0;
        parameters[cs80::P_portamento] = 0;
        parameters[cs80::P_initBend] = 0;
        parameters[cs80::P_ribbonCcEnable] = 0;
        parameters[cs80::P_sustainMode] = 0;
        parameters[cs80::P_glideMode] = 0;
        parameters[cs80::P_glideAssign] = 0;
        parameters[cs80::P_footswitchPlugged] = parameters[cs80::P_footswitchPressed] = 0;
        parameters[cs80::P_sustainAssign] = 0;
        parameters[cs80::P_macro1] = parameters[cs80::P_macro2]
            = parameters[cs80::P_macro3] = parameters[cs80::P_macro4] = .5f;
        parameters[cs80::P_arpOn] = parameters[cs80::P_kasScanTiming] = 0;
        parameters[cs80::P_lfo1Depth] = parameters[cs80::P_lfo2Depth] = parameters[cs80::P_lfo3Depth] = 0;
        for (int i = cs80::P_mod1Amount; i <= cs80::P_mod16Amount; ++i) parameters[i] = 0;
        parameters[cs80::P_mix] = .5f;
        parameters[cs80::P_masterSustain] = 0;
        parameters[cs80::P_keyRelease] = 0.f;
        parameters[cs80::P_ribbon] = 0;
        // Neutral motion macro above yields Engine envTimeScale == 1; dispersion is zero.
        parameters[cs80::P_mpeOn] = parameters[cs80::P_pressureCurve] = parameters[cs80::P_pressureThreshold] = 0;
        for (int rank : {0, 1}) {
            parameters[cs80::chParam(rank, cs80::CP_vLevel)] = .35f;
            parameters[cs80::chParam(rank, cs80::CP_vA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_vD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_vS)] = .7f;
            parameters[cs80::chParam(rank, cs80::CP_vR)] = .005f;
            parameters[cs80::chParam(rank, cs80::CP_fA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_fD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_fAl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fIl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fR)] = .30f;
            parameters[cs80::chParam(rank, cs80::CP_lpfCutoff)] = 4000;
            parameters[cs80::chParam(rank, cs80::CP_hpfCutoff)] = 25;
            parameters[cs80::chParam(rank, cs80::CP_lpfRes)] = parameters[cs80::chParam(rank, cs80::CP_hpfRes)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_vcfLevel)] = .78f;
            parameters[cs80::chParam(rank, cs80::CP_pulse)] = 1;
            parameters[cs80::chParam(rank, cs80::CP_saw)] = parameters[cs80::chParam(rank, cs80::CP_sine)] = parameters[cs80::chParam(rank, cs80::CP_noise)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_pw)] = .5f;
            parameters[cs80::chParam(rank, cs80::CP_pwmSpeed)] = 3;
            parameters[cs80::chParam(rank, cs80::CP_pwmDepth)] = 0;
            for (int local : {cs80::CP_initBrill, cs80::CP_initLevel, cs80::CP_afterBrill, cs80::CP_afterLevel})
                parameters[cs80::chParam(rank, local)] = 0;
        }

        // Pin deterministic compatibility SUB oscillator: sine=0, 8 Hz, no external input.
        parameters[cs80::P_subOriginalMode] = 0;
        parameters[cs80::P_subWave] = 0;
        parameters[cs80::P_subSpeed] = 8;
        parameters[cs80::P_subExtLevel] = 0;
        parameters[cs80::P_subVco] = parameters[cs80::P_subVcf] = parameters[cs80::P_subVca] = 0;
        parameters[cs80::P_subAtSpeed] = parameters[cs80::P_subAtVco] = parameters[cs80::P_subAtVcf] = 0;

        // Explicitly neutralize all other modulation and effects.
        parameters[cs80::P_modWheelSub] = parameters[cs80::P_modWheelVcf] = 0;
        parameters[cs80::P_ringAmount] = parameters[cs80::P_chorusOn] = 0;
        parameters[cs80::P_fx1Mix] = parameters[cs80::P_fx2Mix] = parameters[cs80::P_fx3Mix] = 0;
        parameters[cs80::P_fx1Type] = parameters[cs80::P_fx2Type] = parameters[cs80::P_fx3Type] = 0;
        parameters[cs80::P_velocityCurve] = 0;
        parameters[cs80::P_lfo1Rate] = 18; parameters[cs80::P_lfo2Rate] = 19; parameters[cs80::P_lfo3Rate] = 20;
        parameters[cs80::P_lfo1Wave] = parameters[cs80::P_lfo2Wave] = parameters[cs80::P_lfo3Wave] = 0;

        for(int i=0;i<3;++i)parameters[cs80::P_mseg1Rate+i]=0.f;

    parameters[cs80::P_voiceMode]=0;
    parameters[cs80::P_lfo1Dest]=parameters[cs80::P_lfo2Dest]=parameters[cs80::P_lfo3Dest]=0;
    parameters[cs80::P_ringSpeed]=13;
    parameters[cs80::P_fx1Model]=parameters[cs80::P_fx2Model]=parameters[cs80::P_fx3Model]=0;
    parameters[cs80::P_fxGainCompensation]=0;
    parameters[cs80::P_fx1Oversampling]=parameters[cs80::P_fx2Oversampling]=parameters[cs80::P_fx3Oversampling]=2;
    parameters[cs80::P_fx1ParamA]=parameters[cs80::P_fx2ParamA]=parameters[cs80::P_fx3ParamA]=.5f;
    parameters[cs80::P_fx1ParamB]=parameters[cs80::P_fx2ParamB]=parameters[cs80::P_fx3ParamB]=.5f;
    int scenarios=0,captures=0;
    for(int scenario=0;scenario<4;++scenario){
      auto p=parameters;NamedCurves curves,constant;
      constexpr std::array models{int(cs80::P_fx1Model),int(cs80::P_fx2Model),int(cs80::P_fx3Model)};
      constexpr std::array selected{2.f,3.f,4.f};
      for(int i=0;i<3;++i)if(scenario==i || scenario==3){
        const int parameter=mixes[size_t(i)];p[parameter]=.10f;p[models[size_t(i)]]=selected[size_t(i)];
        const double neutral=normalized(parameter,.10f);const int shift=scenario==3?37*i:0;
        curves.push_back({parameter,{{0,neutral},{1023,neutral},{2047+shift,normalized(parameter,.35f)},
          {4095+shift,normalized(parameter,.15f)},{8191+shift,normalized(parameter,.45f)},
          {12287+shift,normalized(parameter,.20f)},{32767+shift,normalized(parameter,.40f)},{49151+shift,normalized(parameter,.20f)},{65535,neutral}}});
        constant.push_back({parameter,{{0,neutral},{65535,neutral}}});
      }
      const std::string id="fx-case"+std::to_string(scenario);
      auto save=[&](const std::string& suffix,const std::vector<float>& audio){capture(out/(id+"-"+suffix+".pcm"),audio);++captures;};
      const auto neutral=renderSession(module,p,{},256),nullAudio=renderSession(module,p,{},256,false,true);
      check(exactAudio(neutral,nullAudio),"empty/null noqueue identity");save("noqueue",neutral);save("null",nullAudio);
      NamedCurves preceding;
      for(int prior:{int(cs80::P_volume),int(cs80::P_masterTune),int(cs80::P_tuningRef),int(cs80::P_mod16Amount)}){
        const double v=normalized(prior,p[prior]);preceding.push_back({prior,{{0,v},{1023,v},{4095,.4},{65535,.6}}});}
      save("preceding112",renderSession(module,p,preceding,127));
      auto legacy=p;legacy[cs80::P_sourcePerformance]=0;
      const auto old=renderSession(module,legacy,curves,256),oldFlat=renderSession(module,legacy,curves,256,true);
      check(exactAudio(old,oldFlat),"Legacy final-point flattening");save("legacy",old);save("legacy-flat",oldFlat);
      auto pure=p;pure[cs80::P_parityMode]=1;
      const auto pureAudio=renderSession(module,pure,curves,256),pureFixed=renderSession(module,pure,constant,256);
      check(exactAudio(pureAudio,pureFixed),"PURE effects inert");save("pure",pureAudio);save("pure-fixed",pureFixed);
      if(!compat){
        const auto scalar=renderSession(module,p,curves,1);save("scalar",scalar);double error=0;
        for(int block:{7,64,256,1024}){
          const auto forward=renderSession(module,p,curves,block);save("block"+std::to_string(block),forward);error=std::max(error,difference(scalar,forward));
          if(scenario==3){auto reverse=curves;std::reverse(reverse.begin(),reverse.end());
            const auto reversed=renderSession(module,p,reverse,block);save("reverse"+std::to_string(block),reversed);
            check(exactAudio(forward,reversed),"combined reversed queue order exact");}
        }
        check(error<=host["audio_max_abs_error"],"FX mix queues match scalar reference");
        const auto fixed=renderSession(module,p,constant,1);save("constant",fixed);
        auto dryParams=p;dryParams[cs80::P_fx1Mix]=dryParams[cs80::P_fx2Mix]=dryParams[cs80::P_fx3Mix]=0.f;
        const auto dry=renderSession(module,dryParams,{},256);save("dry",dry);
        const double dryTail=difference({dry.begin()+32768,dry.end()},std::vector<float>(dry.size()-32768,0.f));
        const double tail=difference({scalar.begin()+32768,scalar.end()},{dry.begin()+32768,dry.end()});
        const double tailMotion=difference({scalar.begin()+32768,scalar.end()},{fixed.begin()+32768,fixed.end()});
        check(dryTail<=limits["maximum_dry_tail"],"dry release silent in tail window");
        check(tail>limits["minimum_tail_effect"],"fixed effect has delayed output after release");
        check(tailMotion>limits["minimum_tail_motion"],"mix trajectory affects released effect tail");
        std::cout<<id<<" dry_tail="<<dryTail<<" tail="<<tail<<" tail_motion="<<tailMotion<<"\n";
        check(std::memcmp(scalar.data(),fixed.data(),2048*sizeof(float))==0,"initial1024-frame bytehold");
        const double effect=difference({scalar.begin()+2048,scalar.begin()+28672},{fixed.begin()+2048,fixed.begin()+28672});
        check(effect>limits["minimum_held_effect"],"positive fixed-type FX mix held effect");
        if(scenario==3)for(int i=0;i<3;++i){auto held=curves;held[size_t(i)]=constant[size_t(i)];
          const auto witness=renderSession(module,p,held,1);save("held-lane"+std::to_string(i),witness);
          const double perLane=difference({scalar.begin()+2048,scalar.begin()+28672},{witness.begin()+2048,witness.begin()+28672});
          check(perLane>limits["minimum_held_effect"],"combined per-slot mix motion audible");
          std::cout<<"combined_slot="<<i<<" held_effect="<<perLane<<"\n";}
        std::cout<<id<<" notes=2 max_error="<<error<<" held_effect="<<effect<<"\n";
      }
      ++scenarios;
    }
    std::cout<<"scenarios="<<scenarios<<" renders="<<renderCount<<" captures="<<captures
             <<" failures="<<failures<<" mode="<<(compat?"compat-only":"full")<<"\n";
    return failures?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}
