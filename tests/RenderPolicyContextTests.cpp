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
    auto* source=dynamic_cast<cs80::ui::ParameterContextButton*>(find(*studio,"source_performance"));
    auto* parity=dynamic_cast<cs80::ui::ParameterContextButton*>(find(*studio,"parity_mode"));
    auto* legacy=dynamic_cast<cs80::ui::ParameterContextButton*>(find(*studio,"render_policy_legacy"));
    check(source && parity && legacy,"actual policy controls use helper");
    if(!source || !parity || !legacy)return 1;
    check(legacy->getContextHostParameterIndex()==-1 && legacy->buildContextMenu().getNumItems()==0,"composite legacy has no misleading context");
    check(source->getTooltip().contains("stored SOURCE PERFORMANCE") && parity->getTooltip().contains("source request is retained"),"tooltips explain stored/effective semantics");
    std::function<void()> retained;
    for(int i=0;i<2;++i) {
        auto* button=i==0?source:parity;
        const int core=i==0?cs80::P_sourcePerformance:cs80::P_parityMode;
        const int other=i==0?cs80::P_parityMode:cs80::P_sourcePerformance;
        auto* parameter=host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(core));
        check(button->getContextParameterId()==Cs80AudioProcessor::parameterId(core),"bound ID matches attachment");
        check(button->getContextHostParameterIndex()==parameter->getParameterIndex(),"real host index");
        host.setParamValue(other,0);host.setParamValue(core,0);
        click(*button);
        check(host.getParamValue(core)==1 && host.getParamValue(other)==0,"ordinary click changes only bound stored request");
        click(*button);
        check(host.getParamValue(core)==0 && host.getParamValue(other)==0,"ordinary second click toggles request off");
        int menus=0;
        const auto originalMenu=button->onContextMenu;
        button->onContextMenu=[&]{++menus;};
        check(button->keyPressed(juce::KeyPress(juce::KeyPress::F10Key,juce::ModifierKeys::shiftModifier,0)),"Shift F10 opens policy request context");
        button->mouseDown(mouse(*button,juce::ModifierKeys::rightButtonModifier));
        button->mouseUp(mouse(*button,0));
        check(menus==2 && host.getParamValue(core)==0 && host.getParamValue(other)==0,"context pointer and key do not select policy");
        button->onContextMenu=originalMenu;
        for(float otherValue:{0.f,1.f}) {
            host.setParamValue(other,otherValue);
            host.setParamValue(core,parameter->convertFrom0to1(parameter->getDefaultValue())<.5f?1.f:0.f);
            Listener listener;host.addListener(&listener);
            auto reset=action(button->buildContextMenu(),"Reset to default");
            check(bool(reset),"reset offered");if(reset)reset();
            check(parameter->getValue()==parameter->getDefaultValue(),"reset actual default");
            check(host.getParamValue(other)==otherValue,"reset retains other request");
            check(listener.values==1 && listener.begins==1 && listener.ends==1,"one balanced reset gesture");
            if(reset)reset();
            check(listener.values==1 && listener.begins==1 && listener.ends==1,"repeated reset silent");
            host.removeListener(&listener);
        }
        host.setParamValue(cs80::P_parityMode,0);
        auto learn=action(button->buildContextMenu(),"MIDI learn");check(bool(learn),"learn offered outside PURE");if(learn)learn();
        check(host.getMidiLearn().learningParameter()==parameter->getParameterIndex(),"learn binds actual request");
        auto cancel=action(button->buildContextMenu(),"Cancel MIDI learn");check(bool(cancel),"cancel offered");if(cancel)cancel();
        check(host.getMidiLearn().learningParameter()<0,"cancel clears request");
        host.setParamValue(cs80::P_parityMode,1);
        check(!action(button->buildContextMenu(),"MIDI learn"),"learn unavailable in PURE");
        host.setParamValue(cs80::P_parityMode,0);
        retained=action(button->buildContextMenu(),"Reset to default");
        host.setParamValue(core,parameter->convertFrom0to1(parameter->getDefaultValue())<.5f?1.f:0.f);
        const float beforeDisabled=host.getParamValue(core);
        studio->setEnabled(false);if(retained)retained();
        check(host.getParamValue(core)==beforeDisabled,"retained action cannot edit disabled ancestor");
        check(!action(button->buildContextMenu(),"Reset to default"),"disabled ancestor prevents reset");
        studio->setEnabled(true);
    }
    host.setParamValue(cs80::P_sourcePerformance,1);host.setParamValue(cs80::P_parityMode,1);
    legacy->onClick();
    check(host.getParamValue(cs80::P_sourcePerformance)==0 && host.getParamValue(cs80::P_parityMode)==0,"legacy still clears both requests");
    auto oldReset=action(source->buildContextMenu(),"Reset to default");
    studio.reset();
    auto* sourceParameter=host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_sourcePerformance));
    const float nonDefault=sourceParameter->convertFrom0to1(sourceParameter->getDefaultValue())<.5f?1.f:0.f;
    host.setParamValue(cs80::P_sourcePerformance,nonDefault);
    if(oldReset)oldReset();
    check(host.getParamValue(cs80::P_sourcePerformance)==nonDefault,"destroyed editor leaves retained action inert");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
