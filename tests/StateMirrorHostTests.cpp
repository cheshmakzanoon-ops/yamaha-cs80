#include "plugin/PluginProcessor.h"
#include "plugin/ParameterStateMirror.h"
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <limits>
#include <thread>

namespace {
int checks=0,failures=0;
void check(bool ok,const char* message) {++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<message<<'\n';}}
struct Interleave final : juce::ValueTree::Listener {
    juce::RangedAudioParameter& parameter;
    bool editTree=false,armed=true;
    cs80::UndoableParameterState* recursiveSync=nullptr;
    int calls=0;
    float newer=0;
    explicit Interleave(juce::RangedAudioParameter& p,bool tree):parameter(p),editTree(tree){}
    void valueTreePropertyChanged(juce::ValueTree& tree,const juce::Identifier& property) override {
        if(!armed || property!=juce::Identifier("value"))return;
        armed=false;++calls;
        if(editTree)tree.setProperty("value",parameter.convertFrom0to1(.8f),nullptr);
        else parameter.setValueNotifyingHost(.8f);
        newer=parameter.getValue();
        if(recursiveSync)recursiveSync->syncNow();
    }
};
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-state-mirror");
    auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    for(int key:std::array<int,4>{cs80::P_volume,cs80::chParam(0,cs80::CP_hpfCutoff),cs80::P_tuningMode,cs80::P_sourcePerformance})
    for(bool treeEdit:{false,true}) for(bool recurse:{false,true}) {
        auto* parameter=host->getAPVTS().getParameter(cs80::paramId(key));
        auto& state=host->getUndoableState();
        parameter->setValueNotifyingHost(.2f);state.syncNow();
        parameter->setValueNotifyingHost(.6f);
        auto child=host->getAPVTS().state.getChildWithProperty("id",parameter->getParameterID());
        Interleave injection(*parameter,treeEdit);
        if(recurse)injection.recursiveSync=&state;
        child.addListener(&injection);
        state.syncNow();child.removeListener(&injection);
        check(injection.calls==1,"controlled update interleaves with the mirror write");
        check(parameter->getValue()==injection.newer,"mirror never overwrites a newer parameter or tree edit");
        state.syncNow();
        check(parameter->getValue()==injection.newer,"subsequent sync preserves newer parameter");
        check(juce::approximatelyEqual((float)child.getProperty("value"),parameter->convertFrom0to1(injection.newer)),
              "next sync brings the tree to the newer parameter");
    }
    {
        auto other=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
        juce::ValueTree root("test"),a("PARAM"),b("PARAM");
        root.appendChild(a,nullptr);root.appendChild(b,nullptr);
        a.setProperty("value",.4f,nullptr);b.setProperty("value",.6f,nullptr);
        using Scope=cs80::ParameterStateMirrorScope;
        const auto* owner=&host->getAPVTS();const auto* second=&other->getAPVTS();
        check(!Scope::suppresses(owner,a,"value"),"no mirror is active outside scope");
        {
            Scope outer(owner,a,.4f);
            check(Scope::suppresses(owner,a,"value"),"matching owner child and value are suppressed");
            check(!Scope::suppresses(second,a,"value") && !Scope::suppresses(owner,b,"value"),"other owner and child are not suppressed");
            check(!Scope::suppresses(owner,a,"id"),"other properties retain ordinary processing");
            {
                Scope inner(second,b,.6f);
                check(Scope::suppresses(owner,a,"value") && Scope::suppresses(second,b,"value"),"nested scopes preserve each owner's mirror");
            }
            check(Scope::suppresses(owner,a,"value") && !Scope::suppresses(second,b,"value"),"inner scope removal restores outer scope");
            a.setProperty("value",.8f,nullptr);
            check(!Scope::suppresses(owner,a,"value"),"genuine newer tree value is not suppressed");
        }
        check(!Scope::suppresses(owner,a,"value"),"scope always restores inactive state");
        {
            Scope temporary(owner,root.getChild(0),.8f);
            check(Scope::suppresses(owner,a,"value"),"scope owns temporary child handle safely");
            {
                Scope invalid(owner,a,std::numeric_limits<float>::quiet_NaN());
                check(Scope::suppresses(owner,a,"value"),"nonfinite inner scope leaves outer scope intact");
            }
            bool crossThread=true;
            std::thread worker([&]{crossThread=Scope::suppresses(owner,a,"value");});worker.join();
            check(!crossThread,"mirror scope never leaks to another thread");
            root.removeChild(a,nullptr);
            check(!Scope::suppresses(owner,a,"value"),"changed parent identity is not suppressed");
        }
        check(!Scope::suppresses(owner,a,"value"),"temporary-handle scope restores inactive state");
    }
    auto& state=host->getUndoableState();
    auto* volume=host->getAPVTS().getParameter(cs80::paramId(cs80::P_volume));
    volume->setValueNotifyingHost(.2f);state.syncNow();const float before=volume->getValue();
    state.getHistory().clearUndoHistory();
    {
        cs80::UndoableParameterState::ScopedUserEdit edit(state,"mirror history control");
        volume->setValueNotifyingHost(.7f);
    }
    const float after=volume->getValue();
    check(state.undo() && volume->getValue()==before,"ordinary undo still drives parameter from tree");
    check(state.redo() && volume->getValue()==after,"ordinary redo still drives parameter from tree");
    std::cout<<"State mirror host: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1:0;
}
