#pragma once
// Accepted native JUCE VST3 event path. kLegacyMIDICCOutEvent input is not
// claimed as a universal DAW transport; direct normal JUCE MIDI has a separate
// supplemental witness. All numerical bounds come from the existing contract.
#include <pluginterfaces/vst/ivstmidicontrollers.h>
namespace {
void processProgram(Instance& instance,int start,int count,Changes* changes,std::vector<float>& output,
                    int pc,int bank,bool sendProgram)
{
    std::vector<float> left(count),right(count);float* channels[]{left.data(),right.data()};
    AudioBusBuffers bus{};bus.numChannels=2;bus.channelBuffers32=channels;
    Events events;
    const auto legacy=[&](int absolute,int controller,int value){
        if(absolute<start||absolute>=start+count)return;
        Event e{};e.busIndex=0;e.sampleOffset=absolute-start;e.type=Event::kLegacyMIDICCOutEvent;
        e.midiCCOut.channel=0;e.midiCCOut.controlNumber=static_cast<uint8>(controller);e.midiCCOut.value=static_cast<int8>(value);
        events.events.push_back(e);
    };
    if(sendProgram){legacy(pc,0,bank);legacy(pc,32,0);legacy(pc,kCtrlProgramChange,0);}
    const auto note=[&](int absolute,int pitch,bool on){
        if(absolute<start||absolute>=start+count)return;
        Event e{};e.busIndex=0;e.sampleOffset=absolute-start;e.type=on?Event::kNoteOnEvent:Event::kNoteOffEvent;
        if(on){e.noteOn.channel=0;e.noteOn.pitch=pitch;e.noteOn.velocity=.75f;e.noteOn.noteId=pitch;}
        else{e.noteOff.channel=0;e.noteOff.pitch=pitch;e.noteOff.noteId=pitch;}
        events.events.push_back(e);
    };
    note(17,48,true);note(18,52,true);note(pc+1,60,true);note(pc+2,64,true);
    for(int pitch:{48,52,60,64})note(1536,pitch,false);
    std::stable_sort(events.events.begin(),events.events.end(),[](auto& a,auto& b){return a.sampleOffset<b.sampleOffset;});
    ProcessContext context{};context.sampleRate=testSampleRate;context.projectTimeSamples=start;
    ProcessData data{};data.processMode=testProcessMode;data.symbolicSampleSize=kSample32;data.numSamples=count;
    data.numOutputs=1;data.outputs=&bus;data.inputEvents=&events;data.inputParameterChanges=changes;data.processContext=&context;
    require(instance.audio->process(data)==kResultOk,"native program audio process");
    for(int i=0;i<count;++i){output.push_back(left[i]);output.push_back(right[i]);}
}
struct ProgramRender{std::vector<float> audio;double policy,parity,volume;};
ProgramRender renderProgram(Module& module,cs80::ParamValues p,int pc,int bank,bool automation,bool split,
                            bool sendProgram=true)
{
    Instance instance(module,p);
    Curves curves;
    for(size_t lane=0;lane<curves.size();++lane){
        const int key=cs80::kSampleAccurateParameters[lane];
        curves[lane]={{0,normalized(key,p[key])},{255,validGeneratedNormalized(.67-double(lane)*.013)},{1023,validGeneratedNormalized(.31+double(lane)*.008)}};
    }
    for(const auto& curve:curves)checkGeneratedCurve(curve);
    std::vector<float> output;
    for(int start=0;start<1024;){
        const int count=split&&start<pc?pc-start:1024-start;
        Changes changes;
        if(automation)for(size_t lane=0;lane<curves.size();++lane){
            const int key=cs80::kSampleAccurateParameters[lane];Curve points;
            // Explicit first values avoid interpreting a recalled preset as a
            // new implicit origin in the same-policy partition control.
            points.emplace_back(0,absoluteValue(curves[lane],normalized(key,p[key]),start));
            for(const auto& point:curves[lane])if(point.first>start&&point.first<start+count)
                points.emplace_back(point.first-start,point.second);
            if(points.back().first!=count-1)points.emplace_back(count-1,absoluteValue(curves[lane],normalized(key,p[key]),start+count-1));
            changes.add(parameterId(cs80::paramId(key)),std::move(points));
        }
        processProgram(instance,start,count,&changes,output,pc,bank,sendProgram);start+=count;
    }
    processProgram(instance,1024,1024,nullptr,output,pc,bank,sendProgram);
    connectController(instance);
    return{std::move(output),readSelector(instance,cs80::P_sourcePerformance),
           readSelector(instance,cs80::P_parityMode),readSelector(instance,cs80::P_volume)};
}
double regionDifference(const std::vector<float>& a,const std::vector<float>& b,int begin,int end){
    double maximum=0;for(int i=begin*2;i<end*2;++i)maximum=std::max(maximum,std::abs(double(a[i])-b[i]));return maximum;
}
}

namespace {
void testVst3ProgramBoundaries(Module& module,double audioTolerance,double normalizedTolerance)
{
    auto p=cs80::defaultValues();p[cs80::P_parityMode]=0;p[cs80::P_sourcePerformance]=1;p[cs80::P_volume]=.11f;
    p[cs80::P_drift]=p[cs80::P_voiceDispersion]=p[cs80::P_analogNoise]=0;
    for(int bank:{0,1}) {
        for(int pc:{0,1,64,256,700}) {
            const auto actual=renderProgram(module,p,pc,bank,true,false);
            const auto reference=renderProgram(module,p,pc,bank,true,true);
            const auto before=regionDifference(actual.audio,reference.audio,0,pc);
            const auto error=difference(actual.audio,reference.audio);
            std::cout<<"PROGRAM circuit_recall bank="<<bank<<" offset="<<pc<<" before_error="<<before
                     <<" max_audio_error="<<error<<" actual_policy="<<actual.policy
                     <<" reference_policy="<<reference.policy<<" actual_volume="<<actual.volume
                     <<" reference_volume="<<reference.volume<<std::endl;
            check(actual.policy==1&&reference.policy==1&&actual.parity==0&&reference.parity==0,
                  "both factory banks retain the fixed circuit engine at every program offset");
            check(error<=audioTolerance,
                  "recall in either bank preserves queued sample timing across the program boundary");
            check(std::abs(actual.volume-double(.31f))<=normalizedTolerance
                      &&std::abs(reference.volume-double(.31f))<=normalizedTolerance,
                  "recall in either bank preserves the independently specified final host endpoint");
        }
        const auto full=renderProgram(module,p,64,bank,false,false);
        const auto split=renderProgram(module,p,64,bank,false,true);
        const auto error=difference(full.audio,split.audio);
        std::cout<<"CONTROL no_queue bank="<<bank<<" error="<<error<<" policy="<<full.policy<<std::endl;
        check(error<=audioTolerance,"native program-only rendering is partition-neutral");
        check(full.policy==1&&full.parity==0&&split.policy==1&&split.parity==0,
              "program-only recalls in both banks retain canonical circuit state");
        const auto omitted=renderProgram(module,p,64,bank,false,false,false);
        const auto positive=difference(full.audio,omitted.audio);
        std::cout<<"CONTROL program_audible bank="<<bank<<" difference="<<positive<<std::endl;
        check(positive>.00001,"native Program Change produces an audible change in each bank");

        // The historical engine bits may still arrive in old host sessions.
        // Their initial values must not suppress MIDI program handling, reset
        // held notes, flatten automation, or change the recalled instrument.
        const auto circuit=renderProgram(module,p,64,bank,true,false);
        for(int retired:{cs80::P_sourcePerformance,cs80::P_parityMode}) {
            auto stale=p;
            stale[retired]=retired==cs80::P_sourcePerformance?0.0f:1.0f;
            const auto restored=renderProgram(module,stale,64,bank,true,false);
            // A queue on every sound control can completely override the
            // recalled voicing. Keep that precedence check above, then use
            // an unautomated positive control to hear Program Change itself.
            const auto recalledSound=renderProgram(module,stale,64,bank,false,false);
            const auto withoutProgram=renderProgram(module,stale,64,bank,false,false,false);
            std::cout<<"CONTROL retired_initial_policy bank="<<bank<<" id="<<cs80::paramId(retired)
                     <<" error="<<difference(restored.audio,circuit.audio)
                     <<" program_difference="<<difference(recalledSound.audio,withoutProgram.audio)<<std::endl;
            check(restored.policy==1&&restored.parity==0,
                  "retired initial policy requests normalize through real VST3 program recall");
            check(restored.audio==circuit.audio,
                  "obsolete initial policy bits leave queued program playback byte-identical");
            check(difference(recalledSound.audio,withoutProgram.audio)>.00001,
                  "obsolete initial isolation request cannot suppress an audible Program Change");
        }
    }
    const auto invalid=renderProgram(module,p,64,3,true,false);
    const auto absent=renderProgram(module,p,64,3,true,false,false);
    const auto ignoredError=difference(invalid.audio,absent.audio);
    std::cout<<"CONTROL invalid_program error="<<ignoredError<<std::endl;
    check(ignoredError==0,"invalid bank/program does not cancel host curves");
}
}
