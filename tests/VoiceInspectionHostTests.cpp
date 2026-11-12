#include "plugin/PluginProcessor.h"
#include "plugin/StudioDrawers.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
int checks=0, failures=0;
void check(bool ok,const char* text) { ++checks; if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';} }
void process(Cs80AudioProcessor& p,int n,juce::MidiBuffer& midi) { juce::AudioBuffer<float> b(2,n);b.clear();p.processBlock(b,midi); }
void snapshot(Cs80AudioProcessor& p,int slot,int rank,bool rendered) {
    const auto r=p.getVoiceInspection(); const auto& e=slot<8?p.getEngine():p.getExtensionEngine();
    check(r.ready && cs80::VoiceInspection::slot(r.request)==slot && cs80::VoiceInspection::rank(r.request)==rank,"echoed identity");
    const auto v=e.getVoiceInfo(slot%8);
    check(r.active==v.active && r.note==v.note && r.channel==v.channel && r.held==v.held && r.pedalHeld==v.pedalHeld,"current slot fields match render-owned getter");
    check(r.rendered==(rendered && v.active),"rank availability");
    if(r.rendered){ const auto k=e.getRankInfo(slot%8,rank);
        check(r.amplitude==k.ampEnvelope && r.filter==k.filterEnvelope && r.oscillatorHz==k.oscillatorHz && r.hpfHz==k.hpfHz && r.lpfHz==k.lpfHz,"exact cached rank values");
    }
}
}
int main(int argc,char**argv) {
    cs80::VoiceInspection t;
    check(!t.read().ready,"initial unavailable");t.request(15,1,true);
    cs80::VoiceInspectionReading a; a.request=t.requested();a.ready=a.available=a.active=a.rendered=true;a.capacity=16;a.note=80;a.channel=16;
    a.amplitude=.25f;a.filter=-.75f;a.oscillatorHz=440;a.hpfHz=125;a.lpfHz=24000;
    auto b=a;b.note=31;b.channel=2;b.amplitude=.75f;b.filter=.5f;b.oscillatorHz=110;b.hpfHz=80;b.lpfHz=900;
    t.publish(a);check(t.read()==a,"exact tuple");
    t.request(0,0,true);check(!t.read().ready,"selection pending");t.request(15,1,true);check(!t.read().ready,"ABA selection pending");
    a.request=b.request=t.requested();t.publish(a);
    std::atomic<bool> done{false};std::thread writer([&]{for(int i=0;i<20000;++i){t.publish(a);t.publish(b);}done=true;});
    bool coherent=true;while(!done){auto r=t.read();coherent &= !r.ready || r==a || r==b;}writer.join();check(coherent,"atomic tuples do not tear");
    t.reset();check(!t.read().ready,"reset unavailable");
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-voice-inspection");
    for(bool circuit:{false,true}) for(bool sixteen:{false,true}) {
        Cs80AudioProcessor p(fixture.getFile());p.setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
        p.setParamValue(cs80::P_voiceMode,sixteen?1:0);p.prepareToPlay(48000,128);
        juce::MidiBuffer m;for(int i=0;i<(sixteen?16:8);++i)m.addEvent(juce::MidiMessage::noteOn(1,48+i,.7f),0);
        p.requestVoiceInspection(0,0,true);process(p,128,m);
        for(int slot=0;slot<(sixteen?16:8);++slot)for(int rank=0;rank<2;++rank){p.requestVoiceInspection(slot,rank,true);m.clear();process(p,32,m);snapshot(p,slot,rank,true);process(p,0,m);snapshot(p,slot,rank,false);}
        p.requestVoiceInspection(15,1,true);m.clear();process(p,32,m);check(p.getVoiceInspection().capacity==(sixteen?16:8),"applied capacity");check(p.getVoiceInspection().available==sixteen,"unavailable extension");
        p.requestVoiceInspection(0,0,true);m.clear();m.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);m.addEvent(juce::MidiMessage::noteOff(1,48),1);process(p,32,m);snapshot(p,0,0,true);
        m.clear();m.addEvent(juce::MidiMessage::controllerEvent(1,120,0),0);process(p,32,m);check(!p.getVoiceInspection().active && !p.getVoiceInspection().rendered,"panic suppresses stale values");
        p.releaseResources();check(!p.getVoiceInspection().ready,"release invalidates");p.prepareToPlay(48000,128);check(!p.getVoiceInspection().ready,"prepare invalidates");
    }
    {
        Cs80AudioProcessor p(fixture.getFile());p.setParamValue(cs80::P_voiceMode,1);p.prepareToPlay(48000,128);
        p.requestVoiceInspection(8,1,true);juce::MidiBuffer m;for(int i=0;i<9;++i)m.addEvent(juce::MidiMessage::noteOn(1,48+i,.7f),0);
        process(p,128,m);snapshot(p,8,1,true);
        p.setParamValue(cs80::P_parityMode,1);m.clear();process(p,32,m);check(p.getVoiceInspection().capacity==8 && !p.getVoiceInspection().available,"PURE hides extension");
        p.setParamValue(cs80::P_parityMode,0);process(p,32,m);check(p.getVoiceInspection().capacity==16,"stored sixteen restored after PURE");
        p.setParamValue(cs80::P_voiceMode,0);process(p,32,m);check(!p.getVoiceInspection().available,"16 to 8 invalidates extension");
        p.requestVoiceInspection(0,0,true);m.clear();m.addEvent(juce::MidiMessage::noteOn(2,77,.7f),32);process(p,32,m);
        check(p.getVoiceInspection().ready && !p.getVoiceInspection().rendered,"event at final boundary suppresses cached rank values");
        m.clear();process(p,32,m);snapshot(p,0,0,true);
        p.requestVoiceInspection(1,1,true);check(!p.getVoiceInspection().ready,"processor selection pending");
        p.requestVoiceInspection(0,0,true);check(!p.getVoiceInspection().ready,"processor selection ABA pending");
        process(p,32,m);snapshot(p,0,0,true);
        p.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);process(p,0,m);check(!p.getVoiceInspection().rendered,"policy no-render suppresses cache");
    }
    // Same processor state, identical stimulus; inspection is read-only.
    Cs80AudioProcessor off(fixture.getFile()),on(fixture.getFile());juce::MemoryBlock state;off.getStateInformation(state);on.setStateInformation(state.getData(),int(state.getSize()));
    off.prepareToPlay(48000,128);on.prepareToPlay(48000,128);on.requestVoiceInspection(0,0,true);
    for(int block=0;block<24;++block){on.requestVoiceInspection(block%16,block%2,true);(void)on.getVoiceInspection();juce::AudioBuffer<float> x(2,128),y(2,128);x.clear();y.clear();juce::MidiBuffer mx,my;if(block==0){mx.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);my=mx;}if(block==12){mx.addEvent(juce::MidiMessage::noteOff(1,60),32);my=mx;}off.processBlock(x,mx);on.processBlock(y,my);for(int c=0;c<2;++c)check(std::memcmp(x.getReadPointer(c),y.getReadPointer(c),128*sizeof(float))==0,"inspection PCM neutrality");}
    {
        cs80::ui::ScopeDrawer drawer(on);drawer.setVisible(true);drawer.setSize(420,470);drawer.setLiveInspection(true);drawer.setInspectedSlot(0);drawer.setChannel(0);
        juce::MidiBuffer m;m.addEvent(juce::MidiMessage::noteOn(1,67,.8f),0);process(on,128,m);juce::Thread::sleep(80);juce::Timer::callPendingTimersSynchronously();
        check(on.getVoiceInspection().ready,"drawer owns enabled request");
        auto* previous=dynamic_cast<juce::Button*>(drawer.findChildWithID("scope_previous_slot"));
        auto* next=dynamic_cast<juce::Button*>(drawer.findChildWithID("scope_next_slot"));
        check(previous && next,"slot controls exposed");
        if(previous && next){previous->onClick();check(drawer.getInspectedSlot()==15 && !on.getVoiceInspection().ready,"previous wraps to extension pending");next->onClick();check(drawer.getInspectedSlot()==0 && !on.getVoiceInspection().ready,"next wraps to primary pending");}
        m.clear();process(on,32,m);juce::Thread::sleep(80);juce::Timer::callPendingTimersSynchronously();

        drawer.setVisible(false);check(!on.getVoiceInspection().ready,"hidden drawer invalidates request");
        drawer.setVisible(true);check(!on.getVoiceInspection().ready,"shown drawer waits for fresh callback");
        process(on,32,m);juce::Thread::sleep(80);juce::Timer::callPendingTimersSynchronously();
        check(on.getVoiceInspection().ready,"shown drawer resumes inspection");
        if(argc==2){juce::File dir(argv[1]);dir.createDirectory();for(bool live:{true,false}) {drawer.setLiveInspection(live);for(int width:{420,315}){drawer.setSize(width,juce::roundToInt(470.0*width/420));auto image=drawer.createComponentSnapshot(drawer.getLocalBounds());auto file=dir.getChildFile(juce::String(live?"voice-live-":"voice-settings-")+juce::String(width)+".png");juce::FileOutputStream stream(file);stream.setPosition(0);stream.truncate();check(juce::PNGImageFormat().writeImageToStream(image,stream),"offscreen image export");}}}
        drawer.setLiveInspection(false);check(!on.getVoiceInspection().ready,"disabled request cannot relabel live response");
        if(argc==2) {
            on.selectHardwareSource(1,6);drawer.setChannel(1);
            for(int width:{420,315}) {drawer.setSize(width,juce::roundToInt(470.0*width/420));auto image=drawer.createComponentSnapshot(drawer.getLocalBounds());
                auto file=juce::File(argv[1]).getChildFile("voice-settings-tone-"+juce::String(width)+".png");juce::FileOutputStream stream(file);stream.setPosition(0);stream.truncate();
                check(juce::PNGImageFormat().writeImageToStream(image,stream),"long source-caption image");}
        }
    }
    { cs80::ui::ScopeDrawer drawer(on);drawer.setVisible(true);drawer.setLiveInspection(true);
      juce::MidiBuffer m;process(on,32,m);check(on.getVoiceInspection().ready,"destructor fixture enabled"); }
    check(!on.getVoiceInspection().ready,"destroyed drawer invalidates request");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
