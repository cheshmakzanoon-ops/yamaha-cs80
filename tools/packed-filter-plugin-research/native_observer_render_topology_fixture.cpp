#define INIT_CLASS_IID
#include "native_observer_render_encoding.hpp"
#include <iostream>
using namespace cs80_observer;
struct TopologyFake final:IAudioProcessor {
    std::string mutation;bool disabled=true;int calls=0;AudioBusBuffers alternate{};float sample=0;float* alternateChannels[1]{nullptr};
    tresult PLUGIN_API queryInterface(const TUID id,void** out)override{return observerQuery(this,id,IAudioProcessor::iid,out);}
    uint32 PLUGIN_API addRef()override{return 1;}uint32 PLUGIN_API release()override{return 1;}
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement*,int32,SpeakerArrangement*,int32)override{return kResultOk;}
    tresult PLUGIN_API getBusArrangement(BusDirection,int32,SpeakerArrangement&)override{return kResultOk;}
    tresult PLUGIN_API canProcessSampleSize(int32)override{return kResultOk;}
    uint32 PLUGIN_API getLatencySamples()override{return 0;}
    tresult PLUGIN_API setupProcessing(ProcessSetup&)override{return kResultOk;}
    tresult PLUGIN_API setProcessing(TBool)override{return kResultOk;}
    uint32 PLUGIN_API getTailSamples()override{return 0;}
    tresult PLUGIN_API process(ProcessData& d)override{
        ++calls;require(d.numOutputs==1 && d.outputs && d.outputs->numChannels==2,"stereo output");
        if(disabled)require(d.numInputs==1 && d.inputs && d.inputs->numChannels==1 && d.inputs->channelBuffers32 && d.inputs->channelBuffers32[0]==nullptr && d.inputs->silenceFlags==0,"inactive mono descriptor every callback");
        else require(d.numInputs==0 && d.inputs==nullptr,"legacy absent input");
        for(int i=0;i<d.numSamples;++i){d.outputs->channelBuffers32[0][i]=float(d.processContext->projectTimeSamples+i);d.outputs->channelBuffers32[1][i]=-float(d.processContext->projectTimeSamples+i);}
        if(mutation=="input-count")d.numInputs=0;
        if(mutation=="inputs-null")d.inputs=nullptr;
        if(mutation=="inputs-pointer")d.inputs=&alternate;
        if(mutation=="channels-zero")d.inputs->numChannels=0;
        if(mutation=="channels-two")d.inputs->numChannels=2;
        if(mutation=="array-null")d.inputs->channelBuffers32=nullptr;
        if(mutation=="array-pointer")d.inputs->channelBuffers32=alternateChannels;
        if(mutation=="sample-nonnull")d.inputs->channelBuffers32[0]=&sample;
        if(mutation=="silence")d.inputs->silenceFlags=1;
        return kResultOk;
    }
};
std::uint32_t read32(const std::vector<std::uint8_t>& b,std::size_t at){require(at+4<=b.size(),"decode bounds");std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(b[at+i])<<(8*i);return n;}
int main(int argc,char** argv){try{
 require(argc==2,"case");const std::string mode=argv[1];RenderProtocol p;p.block=4;p.frames=10;p.topology=AudioInputTopology::DisabledMonoExternalInput;TopologyFake f;
 if(mode=="encoding"){
  RenderProtocol old=p;old.topology=AudioInputTopology::ZeroAudioInput;const auto a=encodeRenderProtocol(old).data,b=encodeRenderProtocol(p).data;
  require(read32(a,8)==1 && read32(b,8)==2 && read32(b,12)==2 && b.size()==a.size()+4,"versioned explicit profile");
  require(std::equal(a.begin(),a.begin()+8,b.begin()) && std::equal(a.begin()+12,a.end(),b.begin()+16),"v1 remaining fields unchanged");return 0;
 }
 if(mode=="invalid-zero" || mode=="invalid-three"){
  p.topology=static_cast<AudioInputTopology>(mode=="invalid-zero"?0:3);bool renderRejected=false,encodeRejected=false;
  try{(void)renderObserved(f,p);}catch(...){renderRejected=true;}try{(void)encodeRenderProtocol(p);}catch(...){encodeRejected=true;}
  require(renderRejected && encodeRejected && f.calls==0,"unknown profile rejected before native callback");return 0;
 }
 if(mode=="legacy"){p.topology=AudioInputTopology::ZeroAudioInput;f.disabled=false;}
 const bool positive=mode=="valid" || mode=="legacy";if(!positive)f.mutation=mode;
 bool rejected=false;RenderObservation result;try{result=renderObserved(f,p);}catch(const std::exception& e){rejected=true;require(std::string(e.what())=="process buffer descriptor mutation","precise mutation rejection");}
 require(rejected!=positive,"exact outcome");
 if(positive){require(f.calls==3 && result.interleaved.size()==20,"complete partial-final-block render");for(int i=0;i<10;++i)require(result.interleaved[i*2]==float(i) && result.interleaved[i*2+1]==-float(i),"exact PCM");const auto encoded=encodeRenderObservation(p,result);require(!encoded.data.empty(),"encoded observation includes selected protocol");}
 else {require(f.calls==1,"terminal first callback mutation");}return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
