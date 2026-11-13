// Bounded supported native histories for existing voice-dispersion amount and one preceding volume lane.
#define CS80_SUPPORTED_SUB_AUTOMATION_ONLY 1
#include "Vst3AutomationTests.cpp"
#include "Vst3Frozen118Roster.h"
#include <fstream>
namespace {
using NamedCurves = std::vector<std::pair<int, Curve>>;
int renderCount=0;
int noteOffFrame=28672;
// Fixture-local long ordinary note history; historical Instance::process is unchanged.
void processHeld(Instance& instance, int start, int count, Changes* changes, std::vector<float>& output)
{
    require(start >= 0 && count > 0 && count <= 1024 && start + count <= 32768, "bounded long process");
    std::vector<float> left(size_t(count), 0.f), right(size_t(count), 0.f);
    float* channels[] {left.data(), right.data()};
    AudioBusBuffers bus {}; bus.numChannels = 2; bus.channelBuffers32 = channels;
    Events events;
    for (int key = 0; key < 3; ++key) {
        for (bool on : {true, false}) {
            const int absolute = (on ? 17 : noteOffFrame) + key;
            if (absolute < start || absolute >= start + count) continue;
            Event event {}; event.busIndex = 0; event.sampleOffset = absolute - start;
            event.type = on ? Event::kNoteOnEvent : Event::kNoteOffEvent;
            if (on) { event.noteOn.channel = 0; event.noteOn.pitch = int16(48 + 4 * key);
                event.noteOn.velocity = .75f; event.noteOn.noteId = 48 + 4 * key; }
            else { event.noteOff.channel = 0; event.noteOff.pitch = int16(48 + 4 * key);
                event.noteOff.velocity = 0.f; event.noteOff.noteId = 48 + 4 * key; }
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
    std::vector<float> audio; audio.reserve(65536);
    for(int start=0;start<32768;start+=block) {
        const int count=std::min(block,32768-start); Changes changes;
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
    (void)difference(audio,audio);require(audio.size()==65536,"complete stereo capture");return audio;
}
bool exactAudio(const std::vector<float>& a,const std::vector<float>& b)
{(void)difference(a,b);return std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;}
void capture(const std::filesystem::path& path,const std::vector<float>& audio)
{(void)difference(audio,audio);std::ofstream f(path,std::ios::binary);require(bool(f),"capture open");f.write(reinterpret_cast<const char*>(audio.data()),std::streamsize(audio.size()*sizeof(float)));require(bool(f),"capture complete");}
}
int main(int argc,char**argv) {
 try {
    require(argc==7 || argc==8,"MODULE OUTPUT HOST_CONTRACT DISPERSION_CONTRACT RATE MODE [--compat-only]");
    const bool compat=argc==8;if(compat)require(std::string(argv[7])=="--compat-only","explicit compatibility mode");
    const std::string rate(argv[5]),mode(argv[6]);
    require(rate=="44100" || rate=="48000" || rate=="96000","supported rate");
    require(mode=="realtime" || mode=="offline","supported mode");
    testSampleRate=std::stod(rate);testProcessMode=mode=="offline"?kOffline:kRealtime;
    const ws1::Tolerances host(argv[3],"host_automation"),limits(argv[4],"host_automation_dispersion");
    require(cs80::kSampleAccurateParameters.size()>=118,"118 prefix available");
    require(std::equal(cs80::kFrozen118Roster.begin(),cs80::kFrozen118Roster.end(),cs80::kSampleAccurateParameters.begin()),"literal118 prefix preserved");
    require(cs80::paramId(cs80::P_voiceDispersion)=="voice_dispersion","stable existing voice-dispersion amount ID");
    require(limits["key_parameter_count"]==119,"119 lane contract");
    if(!compat){require(cs80::kSampleAccurateParameters.size()==119,"exact119 capability");
      require(cs80::kSampleAccurateParameters[118]==cs80::P_voiceDispersion,"voice dispersion at118");}
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
        parameters[cs80::P_arpOn] = 0; parameters[cs80::P_kasScanTiming] = 0;
        parameters[cs80::P_arpMode]=0; parameters[cs80::P_arpOctaves]=1; parameters[cs80::P_arpRate]=8;
        parameters[cs80::P_lfo1Depth] = parameters[cs80::P_lfo2Depth] = parameters[cs80::P_lfo3Depth] = 0;
        for (int i = cs80::P_mod1Amount; i <= cs80::P_mod16Amount; ++i) parameters[i] = 0;
        parameters[cs80::P_mix] = .5f;
        parameters[cs80::P_masterSustain] = 0;
        parameters[cs80::P_keyRelease] = .02f;
        parameters[cs80::P_ribbon] = 0;
        // Neutral motion macro above yields Engine envTimeScale == 1; dispersion is zero.
        parameters[cs80::P_mpeOn] = parameters[cs80::P_pressureCurve] = parameters[cs80::P_pressureThreshold] = 0;
        for (int rank : {0, 1}) {
            parameters[cs80::chParam(rank, cs80::CP_vLevel)] = .35f;
            parameters[cs80::chParam(rank, cs80::CP_vA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_vD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_vS)] = .7f;
            parameters[cs80::chParam(rank, cs80::CP_vR)] = .02f;
            parameters[cs80::chParam(rank, cs80::CP_fA)] = .001f;
            parameters[cs80::chParam(rank, cs80::CP_fD)] = .002f;
            parameters[cs80::chParam(rank, cs80::CP_fAl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fIl)] = 0;
            parameters[cs80::chParam(rank, cs80::CP_fR)] = .02f;
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
    // Fixed drive is inert at zero mix except in preceding118 compatibility curves.
    parameters[cs80::P_fx1Type]=5;
    parameters[cs80::P_fxGainCompensation]=0;
    parameters[cs80::P_fx1Oversampling]=parameters[cs80::P_fx2Oversampling]=parameters[cs80::P_fx3Oversampling]=2;
    int scenarios=0,captures=0;
    for(int scenario=0;scenario<4;++scenario){
      auto p=parameters; p[cs80::P_voiceDispersion]=.10f; const bool releaseHistory=scenario>=2; noteOffFrame=releaseHistory?8192:28672;
      if(releaseHistory)for(int rank:{0,1}){p[cs80::chParam(rank,cs80::CP_vR)]=.5f;p[cs80::chParam(rank,cs80::CP_fR)]=.5f;} NamedCurves curves,constant;
      const int dispersion=int(cs80::P_voiceDispersion),volume=int(cs80::P_volume);
      const double initial=normalized(dispersion,.10f);
      curves.push_back({dispersion,{{0,initial},{1023,initial},{4095,normalized(dispersion,.70f)},
          {8191,normalized(dispersion,.40f)},{16383,normalized(dispersion,.25f)},
          {24575,normalized(dispersion,.60f)},{32767,initial}}});
      constant.push_back({dispersion,{{0,initial},{32767,initial}}});
      if(scenario%2==1){
        p[volume]=.60f; const double v=normalized(volume,p[volume]);
        curves.push_back({volume,{{0,v},{1023,v},{4132,normalized(volume,.75f)},
          {8228,normalized(volume,.50f)},{16420,normalized(volume,.70f)},
          {24612,normalized(volume,.55f)},{32767,v}}});
        constant.push_back({volume,{{0,v},{32767,v}}});
      }
      if(releaseHistory){
        curves[0].second={{0,initial},{10239,initial},{16383,normalized(dispersion,.70f)},
            {24575,normalized(dispersion,.25f)},{32767,normalized(dispersion,.60f)}};
        if(scenario%2==1){const double v=normalized(volume,p[volume]);curves[1].second={{0,v},{10239,v},
            {16420,normalized(volume,.75f)},{24612,normalized(volume,.50f)},{32767,normalized(volume,.70f)}};}
      }
      const std::string id="dispersion-case"+std::to_string(scenario);
      auto save=[&](const std::string& suffix,const std::vector<float>& audio){capture(out/(id+"-"+suffix+".pcm"),audio);++captures;};
      // These five compatibility captures NEVER submit the new dispersion lane.
      const auto neutral=renderSession(module,p,{},256),nullAudio=renderSession(module,p,{},256,false,true);
      check(exactAudio(neutral,nullAudio),"empty/null noqueue identity");save("noqueue",neutral);save("null",nullAudio);
      NamedCurves preceding;
      for(int prior:{int(cs80::P_volume),int(cs80::P_masterTune),int(cs80::P_tuningRef),int(cs80::P_mod16Amount),int(cs80::P_fx1Mix),int(cs80::P_arpRate),int(cs80::P_analogNoise),int(cs80::P_drift)}){
        const double v=normalized(prior,p[prior]);preceding.push_back({prior,{{0,v},{1023,v},{8191,.4},{32767,.6}}});}
      auto priorState=p;priorState[cs80::P_arpOn]=1;
      save("preceding118",renderSession(module,priorState,preceding,127));
      auto legacy=p;legacy[cs80::P_sourcePerformance]=0;
      save("legacy-prior",renderSession(module,legacy,preceding,256));
      auto pure=p;pure[cs80::P_parityMode]=1;
      save("pure-prior",renderSession(module,pure,preceding,256));
      if(!compat){
        const auto old=renderSession(module,legacy,curves,256),oldFlat=renderSession(module,legacy,curves,256,true);
        check(exactAudio(old,oldFlat),"Legacy final-point flattening");save("legacy",old);save("legacy-flat",oldFlat);
        // Hold ONLY the new dispersion lane. Existing volume must still move in combined controls.
        auto dispersionHeld=curves;dispersionHeld[0]=constant[0];
        const auto pureAudio=renderSession(module,pure,curves,256),pureFixed=renderSession(module,pure,dispersionHeld,256);
        check(exactAudio(pureAudio,pureFixed),"PURE voice-dispersion amount inert with same prior controls");save("pure",pureAudio);save("pure-dispersion-held",pureFixed);
        const auto scalar=renderSession(module,p,curves,1);save("scalar",scalar);double error=0;
        for(int block:{7,64,256,1024}){
          const auto forward=renderSession(module,p,curves,block);save("block"+std::to_string(block),forward);error=std::max(error,difference(scalar,forward));
          if(scenario%2==1){auto reverse=curves;std::reverse(reverse.begin(),reverse.end());
            const auto reversed=renderSession(module,p,reverse,block);save("reverse"+std::to_string(block),reversed);
            check(exactAudio(forward,reversed),"combined reversed queue order exact");}
        }
        check(error<=std::min(host["audio_max_abs_error"],limits["audio_max_abs_error"]),"voice-dispersion amount queues match scalar reference");
        const auto fixed=renderSession(module,p,constant,1);save("constant",fixed);
        check(std::memcmp(scalar.data(),fixed.data(),2048*sizeof(float))==0,"initial1024-frame bytehold");
        const double effect=difference({scalar.begin()+2048,scalar.begin()+57344},{fixed.begin()+2048,fixed.begin()+57344});
        check(effect>limits["minimum_dispersion_effect"],"positive voice-dispersion amount held-chord effect");
        if(releaseHistory){
          check(std::memcmp(scalar.data(),fixed.data(),20480*sizeof(float))==0,"release trajectories exact through10239 including noteoffs");
          const double tail=difference({scalar.begin()+32768,scalar.begin()+57344},{fixed.begin()+32768,fixed.begin()+57344});
          check(tail>limits["minimum_dispersion_effect"],"positive post-release dispersion witness");}
        if(scenario%2==1)for(int i=0;i<2;++i){auto held=curves;held[size_t(i)]=constant[size_t(i)];
          const auto witness=renderSession(module,p,held,1);save("held-lane"+std::to_string(i),witness);
          const int startEffect=releaseHistory?32768:2048;
          const double perLane=difference({scalar.begin()+startEffect,scalar.begin()+57344},{witness.begin()+startEffect,witness.begin()+57344});
          check(perLane>limits["minimum_dispersion_effect"],"combined individual lane motion measurable");
          std::cout<<"combined_lane="<<i<<" held_effect="<<perLane<<"\n";}
        std::cout<<id<<" notes=3 max_error="<<error<<" held_effect="<<effect<<"\n";
      }
      ++scenarios;
    }
    require(captures==(compat?20:72),"exact predeclared retained capture count");
    std::cout<<"scenarios="<<scenarios<<" renders="<<renderCount<<" captures="<<captures
             <<" failures="<<failures<<" mode="<<(compat?"compat-only":"full")<<"\n";
    return failures?1:0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}
