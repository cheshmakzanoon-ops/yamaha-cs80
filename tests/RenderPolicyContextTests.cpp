#include "plugin/PluginProcessor.h"
#include "plugin/StudioView.h"
#include "plugin/ParameterContextButton.h"
#include <iostream>
namespace {
int checks=0, failures=0;
void check(bool ok,const char* name) { ++checks; if(!ok){++failures;std::cerr<<"FAIL: "<<name<<'\n';} }
juce::Component* find(juce::Component& root,const char* id) {
    if(root.getComponentID()==id)return &root;
    for(auto* child:root.getChildren())if(auto* found=find(*child,id))return found;
    return nullptr;
}
std::function<void()> action(const juce::PopupMenu& menu,const char* prefix) {
    for(juce::PopupMenu::MenuItemIterator it(menu);it.next();)
        if(it.getItem().text.startsWith(prefix) && it.getItem().isEnabled)return it.getItem().action;
    return {};
}
juce::MouseEvent mouse(juce::Component& c,int modifiers) {
    const juce::Point<float> p(10,10);
    return {juce::Desktop::getInstance().getMainMouseSource(),p,juce::ModifierKeys(modifiers),
        1.f,0.f,0.f,0.f,0.f,&c,&c,juce::Time::getCurrentTime(),p,juce::Time::getCurrentTime(),1,false};
}
void click(cs80::ui::ParameterContextButton& button) {
    button.mouseDown(mouse(button,juce::ModifierKeys::leftButtonModifier));
    button.mouseUp(mouse(button,0));
}
struct Listener:juce::AudioProcessorListener {
    int values=0, begins=0, ends=0;
    void audioProcessorParameterChanged(juce::AudioProcessor*,int,float) override {++values;}
    void audioProcessorChanged(juce::AudioProcessor*,const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*,int) override {++begins;}
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*,int) override {++ends;}
};
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-render-policy-context");
    Cs80AudioProcessor host(fixture.getFile());
    auto studio=std::make_unique<cs80::ui::StudioView>(host);
    studio->setSize(1600,936);studio->setVisible(true);studio->showTab(cs80::ui::StudioTab::settings);
    for(const char* id:{"source_performance","parity_mode","render_policy_legacy"})
        check(find(*studio,id)==nullptr,"retired engine policies have no selectable Studio control");
    for(int core:{int(cs80::P_sourcePerformance),int(cs80::P_parityMode)}) {
        auto* parameter=host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(core));
        check(parameter && parameter->isBoolean() && !parameter->isAutomatable(),
              "reserved engine field retains its Boolean host identity without automation");
    }

    // Exercise the same real context/click/undo/lifetime paths through the
    // independent SUB waveform-set choice, which remains an editable control.
    studio->showTab(cs80::ui::StudioTab::modulation,1);
    auto* button=dynamic_cast<cs80::ui::ParameterContextButton*>(find(*studio,"sub_original_mode"));
    check(button!=nullptr,"actual SUB waveform-set control uses the context helper");
    if(!button)return 1;
    constexpr int core=cs80::P_subOriginalMode;
    auto* parameter=host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(core));
    check(button->getContextParameterId()==Cs80AudioProcessor::parameterId(core),"bound ID matches attachment");
    check(button->getContextHostParameterIndex()==parameter->getParameterIndex(),"real host index");
    check(button->getTooltip().contains("original functions") && button->getTooltip().contains("extended waveform set"),
          "waveform-set tooltip describes the actual independent choice");
    const float nonDefault=parameter->convertFrom0to1(parameter->getDefaultValue())<.5f?1.f:0.f;
    auto canonical=[&] {
        return host.getParamValue(cs80::P_sourcePerformance)==1 && host.getParamValue(cs80::P_parityMode)==0
            && host.getEngineMode()==Cs80AudioProcessor::EngineMode::circuit && !host.isPure();
    };
    for(int sourceState:{0,1}) for(int isolationState:{0,1}) {
        host.setParamValue(core,0);
        host.getUndoableState().syncNow();host.getUndoManager().clearUndoHistory();
        Listener policyListener;host.addListener(&policyListener);
        host.setParamValue(cs80::P_sourcePerformance,float(sourceState));
        host.setParamValue(cs80::P_parityMode,float(isolationState));
        host.getUndoableState().syncNow();
        check(canonical() && host.getParamValue(core)==0,"obsolete requests leave the circuit policy and SUB choice unchanged");
        check(policyListener.values==0 && policyListener.begins==0 && policyListener.ends==0,
              "obsolete policy setters emit no parameter edit or gesture");
        check(!host.getUndoManager().canUndo(),"obsolete policy writes create no undo step");
        host.removeListener(&policyListener);

        click(*button);
        check(host.getParamValue(core)==1 && canonical(),"ordinary click changes only the editable SUB waveform set");
        host.getUndoableState().syncNow();
        check(host.getUndoManager().canUndo(),"real SUB click creates an undo step");
        check(host.getUndoableState().undo() && host.getParamValue(core)==0 && canonical(),
              "one undo restores the prior SUB choice without changing engine policy");
        check(!host.getUndoManager().canUndo(),"real SUB click is one undo transaction");
        check(host.getUndoableState().redo() && host.getParamValue(core)==1 && canonical(),
              "redo restores the edited SUB choice on the same circuit engine");
        click(*button);
        check(host.getParamValue(core)==0 && canonical(),"ordinary second click toggles the waveform set off");

        int menus=0;
        const auto originalMenu=button->onContextMenu;
        button->onContextMenu=[&]{++menus;};
        check(button->keyPressed(juce::KeyPress(juce::KeyPress::F10Key,juce::ModifierKeys::shiftModifier,0)),
              "Shift F10 opens the real waveform-set context");
        button->mouseDown(mouse(*button,juce::ModifierKeys::rightButtonModifier));
        button->mouseUp(mouse(*button,0));
        check(menus==2 && host.getParamValue(core)==0 && canonical(),"context pointer and key do not toggle the waveform set");
        button->onContextMenu=originalMenu;

        host.setParamValue(core,nonDefault);
        Listener listener;host.addListener(&listener);
        auto reset=action(button->buildContextMenu(),"Reset to default");
        check(bool(reset),"reset offered");if(reset)reset();
        check(parameter->getValue()==parameter->getDefaultValue() && canonical(),"reset changes only the actual waveform-set default");
        check(listener.values==1 && listener.begins==1 && listener.ends==1,"one balanced reset gesture");
        if(reset)reset();
        check(listener.values==1 && listener.begins==1 && listener.ends==1,"repeated reset is silent");
        host.removeListener(&listener);

        auto learn=action(button->buildContextMenu(),"MIDI learn");
        check(bool(learn),"MIDI learn remains available after obsolete policy requests");if(learn)learn();
        check(host.getMidiLearn().learningParameter()==parameter->getParameterIndex(),"learn binds the actual waveform-set parameter");
        auto cancel=action(button->buildContextMenu(),"Cancel MIDI learn");
        check(bool(cancel),"cancel offered");if(cancel)cancel();
        check(host.getMidiLearn().learningParameter()<0,"cancel clears the active request");

        auto retained=action(button->buildContextMenu(),"Reset to default");
        host.setParamValue(core,nonDefault);
        studio->setEnabled(false);if(retained)retained();
        check(host.getParamValue(core)==nonDefault,"retained action cannot edit a disabled ancestor");
        check(!action(button->buildContextMenu(),"Reset to default"),"disabled ancestor prevents reset");
        studio->setEnabled(true);
    }
    auto oldReset=action(button->buildContextMenu(),"Reset to default");
    check(bool(oldReset),"a real context reset exists before editor destruction");
    studio.reset();
    host.setParamValue(core,nonDefault);
    if(oldReset)oldReset();
    check(host.getParamValue(core)==nonDefault && canonical(),"destroyed editor leaves its retained action inert");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
