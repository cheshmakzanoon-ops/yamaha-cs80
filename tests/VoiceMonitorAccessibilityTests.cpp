#include "plugin/PluginProcessor.h"
#include "plugin/StudioDrawers.h"
#include "VoiceMonitorFocusChecks.h"
#include <iostream>
namespace {
int checks=0,failures=0;
void check(bool ok,const char* text){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<text<<'\n';}}
void render(Cs80AudioProcessor& p,juce::MidiBuffer& midi){juce::AudioBuffer<float> audio(2,32);audio.clear();p.processBlock(audio,midi);}
}
int main(){
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-voice-accessibility");
    Cs80AudioProcessor p(fixture.getFile());p.prepareToPlay(48000,32);
    cs80::ui::ScopeDrawer drawer(p);drawer.setVisible(true);drawer.setSize(420,470);
    auto* status=drawer.findChildWithID("voice_monitor_status");
    check(status!=nullptr,"actual drawer has accessible status");if(!status)return 1;
    // No native peer in this fixture; inspect the virtual handler factory.
    auto handler=status->createAccessibilityHandler();
    check(handler!=nullptr && handler->getRole()==juce::AccessibilityRole::staticText,"read-only static text role factory");
    check(!status->getWantsKeyboardFocus(),"status adds no keyboard stop");
    bool interceptSelf=true,interceptChildren=true;
    status->getInterceptsMouseClicks(interceptSelf,interceptChildren);
    check(!interceptSelf && !interceptChildren,"status never intercepts pointer input");
    check(status->getTitle().contains("Settings illustrations for channel I"),"settings summary does not claim measurements");
    check(cs80::test::voiceMonitorFocusMatches(drawer,false),"settings focus order unchanged");
    drawer.setLiveInspection(true);
    check(status->getTitle().contains("Physical slot 1, rank I") && status->getTitle().contains("Waiting for audio callback"),"new request waits with correct identity");
    check(cs80::test::voiceMonitorFocusMatches(drawer,true),"live focus order unchanged");
    juce::MidiBuffer midi;render(p,midi);drawer.setChannel(0);
    check(status->getTitle().contains("Inactive slot. Values unavailable"),"inactive slot has no numeric values");
    midi.addEvent(juce::MidiMessage::noteOn(1,60,.8f),0);render(p,midi);drawer.setChannel(0);
    const auto reading=p.getVoiceInspection();
    check(reading.active && reading.rendered,"normal audio fixture produced active rendered reading");
    check(status->getTitle().contains("Key held") && status->getTitle().contains("MIDI note 60, channel 1"),"active note identity exposed");
    check(status->getTitle().contains("oscillator " + juce::String(reading.oscillatorHz,2) + " Hz") && status->getTitle().contains("VCA envelope " + juce::String(reading.amplitude,4)),"summary matches actual published values");
    drawer.setChannel(1);
    check(status->getTitle().contains("rank II") && status->getTitle().contains("Waiting for audio callback") && !status->getTitle().contains("oscillator"),"selection never relabels stale numeric values");
    midi.clear();render(p,midi);
    juce::Thread::sleep(350);juce::Timer::callPendingTimersSynchronously();
    check(status->getTitle().contains("MIDI note 60") && !status->getTitle().contains("Waiting"),"timer alone refreshes new callback for selected rank");
    drawer.setInspectedSlot(15);midi.clear();render(p,midi);drawer.setInspectedSlot(15);
    check(status->getTitle().contains("Physical slot 16") && status->getTitle().contains("not available in 8-voice mode"),"unavailable extension explicit");
    drawer.setVisible(false);drawer.setVisible(true);
    check(status->getTitle().contains("Waiting for audio callback"),"reopen does not expose stale response");
    drawer.setLiveInspection(false);drawer.setChannel(1);
    check(status->getTitle().contains("Settings illustrations for channel II") && !status->getTitle().contains("Live inspection"),"settings replaces live summary");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
