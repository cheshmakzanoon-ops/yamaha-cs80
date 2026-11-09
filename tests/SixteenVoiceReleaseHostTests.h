#pragma once
#include "plugin/PluginProcessor.h"
#include <cmath>
#include <memory>
#include <initializer_list>

namespace cs80::tests
{
// Real processBlock dispatch without an editor, native peer or external DAW.
template<class Check> void testSixteenVoiceReleaseHost(Check check)
{
    juce::TemporaryFile fixture("-sixteen-release-host");
    const auto held = [](const Engine& e, int note, int channel = 2) {
        int result=0; for(int i=0;i<8;++i) { const auto v=e.getVoiceInfo(i); result+=v.active&&v.held&&v.note==note&&v.channel==channel; } return result;
    };
    const auto pedal = [](const Engine& e, int note) {
        int result=0; for(int i=0;i<8;++i) { const auto v=e.getVoiceInfo(i); result+=v.active&&v.pedalHeld&&v.note==note&&v.channel==2; } return result;
    };
    auto make = [&](bool circuit, int mode=1) {
        auto p=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        p->setEngineMode(circuit?Cs80AudioProcessor::EngineMode::circuit:Cs80AudioProcessor::EngineMode::legacy);
        for(int id:std::initializer_list<int>{P_parityMode,P_kasScanTiming,P_arpOn,P_mpeOn,P_analogNoise,P_voiceDispersion,P_drift,P_masterSustain,P_sustainMode,P_velocityCurve}) p->setParamValue(id,0);
        p->setParamValue(P_voiceMode,static_cast<float>(mode)); p->setParamValue(P_keyRelease,.5f);
        p->prepareToPlay(48000,128);p->serviceKeyboardUI();return p;
    };
    for(bool circuit:{false,true}) for(int frames:{0,32}) {
        auto p=make(circuit);juce::MidiBuffer midi;
        auto tick=[&](int n=-1) {
            juce::AudioBuffer<float> audio(2,n<0?frames:n); audio.clear();p->processBlock(audio,midi);midi.clear();
            bool finite=true;for(int c=0;c<2;++c)for(int i=0;i<audio.getNumSamples();++i)finite&=std::isfinite(audio.getSample(c,i));
            check(finite,"processor writes finite release output");return audio;
        };
        auto on=[&](int note,int channel=2,int at=0){midi.addEvent(juce::MidiMessage::noteOn(channel,note,(juce::uint8)100),at);};
        auto off=[&](int note,int channel=2,int at=0){midi.addEvent(juce::MidiMessage::noteOff(channel,note),at);};
        auto count=[&](int a,int b){check(held(p->getEngine(),60)==a&&held(p->getExtensionEngine(),60)==b,"exact primary/extension repeated-note held counts");};
        auto fill=[&]{for(int note=60;note<68;++note)on(note);tick();};
        auto reset=[&]{p->requestPanicFromUI();tick(0);p->serviceKeyboardUI();};
        tick();check(p->getEngine().canCoordinateImmediateReleaseWith(p->getExtensionEngine()),"processor binds one cold clock that survives prepare");
        fill();on(60,2,7);tick();count(1,1);
        off(60,3,21);tick();count(1,1);
        off(60,2,21);tick();count(1,0);off(60);tick();count(0,0);off(60);tick();count(0,0);
        // Extension's local age is deliberately much larger than primary's.
        reset();fill();for(int i=0;i<40;++i){on(90);tick(0);off(90);tick(0);}on(60);tick();
        off(61);tick();on(60);tick();count(2,1);
        off(60);tick();count(1,1);off(60);tick();count(1,0);off(60);tick();count(0,0);
        // Zero-velocity Note On takes the same release route and consumes CC88.
        reset();fill();on(60);tick();
        midi.addEvent(juce::MidiMessage::controllerEvent(2,88,90),0);
        midi.addEvent(juce::MidiMessage::noteOn(2,60,(juce::uint8)0),0);tick();count(1,0);
        on(70);tick();bool ordinaryVelocity=false;
        for(int i=0;i<8;++i) { const auto v=p->getExtensionEngine().getVoiceInfo(i); if(v.active&&v.held&&v.note==70&&v.channel==2) ordinaryVelocity=v.velocity==100.0f*(1.0f/127.0f); }
        check(ordinaryVelocity,"zero-velocity Note On consumes CC88 before the next ordinary strike");
        // Pedal retains sound, not physical ownership of the released onset.
        reset();fill();on(60);tick();midi.addEvent(juce::MidiMessage::controllerEvent(2,64,127),0);off(60);tick();count(1,0);
        check(pedal(p->getExtensionEngine(),60)==1,"one released extension onset remains pedal-held");
        off(60);tick();count(0,0);check(pedal(p->getEngine(),60)==1,"second off releases only primary into pedal hold");
        midi.addEvent(juce::MidiMessage::controllerEvent(2,64,0),0);tick();
        check(pedal(p->getEngine(),60)==0&&pedal(p->getExtensionEngine(),60)==0,"pedal-up clears both independently released holds");
        // UI extension onset releases without cutting the older host onset.
        reset();fill();
        const auto token=p->captureKeyboardGesture(2,60,[&]{p->getKeyboardState().noteOn(2,60,.8f);});tick();
        check(static_cast<bool>(token),"UI repeated onset has a real gesture token");count(1,1);
        p->getKeyboardState().noteOff(2,60,0);tick();count(1,0);
        // A newer host strike revokes the existing UI alias in the processor.
        reset();p->captureKeyboardGesture(2,60,[&]{p->getKeyboardState().noteOn(2,60,.8f);});tick();
        for(int note=61;note<68;++note)on(note);tick();on(60);tick();count(1,1);
        p->getKeyboardState().noteOff(2,60,0);tick();count(1,1);
        off(60);tick();count(1,0);off(60);tick();count(0,0);
        // Primary ownership survives disabling and reactivating the extension.
        reset();fill();on(60);tick();p->setParamValue(P_voiceMode,0);tick();
        check(held(p->getEngine(),60)==1,"eight-voice transition retains primary onset");
        p->setParamValue(P_voiceMode,1);tick();on(60);tick();count(1,1);off(60);tick();count(1,0);
        p->prepareToPlay(48000,128);tick();count(0,0);fill();on(60);tick();off(60);tick();count(1,0);
        p->setParamValue(P_voiceMode,0);tick(0); // Exclude extension tails from the audio witness.
        const auto sounding=tick(256);bool nonzero=false;
        for(int c=0;c<2;++c)for(int i=0;i<sounding.getNumSamples();++i)nonzero|=sounding.getSample(c,i)!=0;
        check(p->getEngine().getVoiceMode()==0&&nonzero,"release fixture retains non-silent primary-instrument output");
        p->setParamValue(P_voiceMode,1);tick(0);
        // Within one callback, MIDI arrival order still distinguishes strikes.
        reset();for(int note=60;note<68;++note)on(note);on(60);off(60);tick();count(1,0);
        if(circuit) {
            reset();fill();for(int note=60;note<68;++note)on(note);tick();on(60);tick();count(1,1);
            off(60);tick();count(1,1); // Matching seventeenth waiter only.
            off(60);tick();count(1,0);off(60);tick();count(0,0);
            off(61);tick();count(0,0);
            // Prior scanner ownership is intentionally not promoted to a
            // guessed musical order when scan timing is turned off.
            reset();p->setParamValue(P_kasScanTiming,1);tick(0);
            for(int note=60;note<68;++note)on(note);on(60);tick(128);count(1,1);
            p->setParamValue(P_kasScanTiming,0);tick(0);off(60);tick();count(0,0);
            check(!p->getEngine().immediateReleaseCandidate(60,2,false).order,"old scanner broadcast leaves no qualified matching onset");
            reset();p->setParamValue(P_kasScanTiming,1);tick(0);
            for(int note=60;note<68;++note)on(note);tick(128);count(1,0);
            p->setParamValue(P_kasScanTiming,0);tick(0);on(60);tick();count(1,1);
            check(p->getEngine().canCoordinateImmediateReleaseWith(p->getExtensionEngine()),"mixed-provenance fixture has empty scanners and immediate modes");
            check(p->getEngine().immediateReleaseCandidate(60,2,false).order==0
                  && p->getExtensionEngine().immediateReleaseCandidate(60,2,false).order>0,
                  "only primary candidate has deferred provenance");
            off(60);tick();count(0,0); // Unknown provenance, not equal-order tie, forces fallback.
        }
    }
    for(bool circuit:{false,true}) for(int mode:{2,3,4}) for(bool partial:{false,true}) {
        auto p=make(circuit,partial?0:mode);juce::AudioBuffer<float> audio(2,0);juce::MidiBuffer midi;
        auto tick=[&]{p->processBlock(audio,midi);midi.clear();};
        auto on=[&](int note){midi.addEvent(juce::MidiMessage::noteOn(2,note,(juce::uint8)100),0);};
        auto off=[&]{midi.addEvent(juce::MidiMessage::noteOff(2,60),0);tick();};
        tick();if(partial){for(int note=40;note<47;++note)on(note);tick();p->setParamValue(P_voiceMode,static_cast<float>(mode));tick();}
        on(60);tick();const int group=partial?1:(1<<(mode-1));
        check(held(p->getEngine(),60)==group,"primary unison group is allocated before sixteen-mode change");
        if(!partial){for(int note=61;note<68;++note)on(note);tick();}
        p->setParamValue(P_voiceMode,1);tick();on(60);tick();
        check(held(p->getExtensionEngine(),60)==1,"unison transition actually routes the new onset to extension");
        off();check(held(p->getEngine(),60)==group&&held(p->getExtensionEngine(),60)==0,"first off preserves complete old unison group");
        off();check(held(p->getEngine(),60)==0,"second off releases entire old group despite live mode change");
    }
}
} // namespace cs80::tests
