#define INIT_CLASS_IID
#include "native_observer_render_encoding.hpp"
#include <iostream>
#include <iomanip>
using namespace cs80_observer;
int main(int argc,char** argv){try{
    require(argc==2,"one case required");std::string mode=argv[1];RenderProtocol p;p.block=2;p.frames=3;p.legacyCCFixtureConvention=true;
    Event on{};on.type=Event::kNoteOnEvent;on.noteOn.channel=2;on.noteOn.pitch=60;on.noteOn.tuning=-0.0f;on.noteOn.velocity=.5f;on.noteOn.length=2;on.noteOn.noteId=-1;
    Event off{};off.type=Event::kNoteOffEvent;off.noteOff.channel=2;off.noteOff.pitch=60;off.noteOff.tuning=1.5f;off.noteOff.velocity=.25f;off.noteOff.noteId=-1;
    Event cc{};cc.type=Event::kLegacyMIDICCOutEvent;cc.midiCCOut.controlNumber=1;cc.midiCCOut.channel=2;cc.midiCCOut.value=64;
    p.events={{0,on},{1,off},{2,cc}};p.parameters={{0,7,-0.0},{2,8,1}};
    RenderObservation o;o.callbacks={{0,2,kResultOk,1},{2,1,kResultOk,3}};o.interleaved={0,-0.0f,1,-1,.25f,-.25f};o.outputParameters={{0,9,.25},{1,9,.5},{0,10,.75},{2,9,1}};
    bool invalid=mode!="protocol" && mode!="observation";
    if(mode=="callback-short")o.callbacks.pop_back();
    else if(mode=="callback-extra")o.callbacks.push_back(o.callbacks.back());
    else if(mode=="callback-start")o.callbacks[1].start=1;
    else if(mode=="callback-count")o.callbacks[1].count=2;
    else if(mode=="callback-result")o.callbacks[1].returned=kResultFalse;
    else if(mode=="callback-mask")o.callbacks[1].silenceFlags=4;
    else if(mode=="pcm-short")o.interleaved.pop_back();
    else if(mode=="pcm-extra")o.interleaved.push_back(0);
    else if(mode=="pcm-nan")o.interleaved[0]=std::numeric_limits<float>::quiet_NaN();
    else if(mode=="output-end")o.outputParameters.back().frame=3;
    else if(mode=="output-negative")o.outputParameters[0].frame=-1;
    else if(mode=="output-id")o.outputParameters[0].id=kNoParamId;
    else if(mode=="output-value")o.outputParameters[0].value=2;
    else if(mode=="output-nan")o.outputParameters[0].value=std::numeric_limits<double>::quiet_NaN();
    else if(mode=="output-block-order")std::swap(o.outputParameters.front(),o.outputParameters.back());
    else if(mode=="output-queue-order")std::swap(o.outputParameters[0],o.outputParameters[1]);
    else if(mode=="output-queue-repeat")o.outputParameters.insert(o.outputParameters.begin()+3,{1,9,.5});
    else if(mode=="output-block-bound")o.outputParameters.assign(4097,{0,9,.5});
    else if(mode=="output-total-bound")o.outputParameters.assign(65537,{0,9,.5});
    else if(mode=="protocol-invalid")p.events[0].value.ppqPosition=1;
    else require(!invalid,"unknown case");
    bool rejected=false;Bytes encoded;
    try{encoded=mode=="protocol"?encodeRenderProtocol(p):encodeRenderObservation(p,o);}catch(const std::exception&){rejected=true;}
    require(rejected==invalid,"expected structural acceptance/rejection");
    for(auto v:encoded.data)std::cout<<std::hex<<std::setfill('0')<<std::setw(2)<<int(v);
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
