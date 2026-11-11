#pragma once
#include "plugin/ModernModulationUi.h"
#include "plugin/PluginProcessor.h"

namespace cs80::tests
{
template<class Check>
void testMsegEditorLifecycle(Check check)
{
    juce::TemporaryFile fixture("-cs80-mseg-lifecycle");
    auto host = std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    using cs80::ui::modern_layer::MsegEditor;
    for (int kind = 0; kind < 4; ++kind)
        for (bool returnToOriginal : {false, true})
    {
        cs80::modern::MsegShape source;
        source.count=4; source.points[0]={0.f,0.f,0.f}; source.points[1]={.25f,.7f,0.f};
        source.points[2]={.6f,.3f,0.f}; source.points[3]={1.f,0.f,0.f};
        source.loop=cs80::modern::MsegLoop::sustain; source.loopStart=1; source.loopEnd=2;
        auto other=source; other.points[1].level=.5f;
        host->getModernLayer().setShape(0,source); host->getModernLayer().setShape(1,other);
        MsegEditor editor(*host); editor.setSize(500,240); editor.setMsegIndex(0);
        auto event = [&](juce::Point<float> position) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.f,0.f,0.f,0.f,0.f,
                &editor,&editor,juce::Time::getCurrentTime(),position,juce::Time::getCurrentTime(),1,false);
        };
        const auto start = kind==0 ? editor.pointPosition(1)
            : kind==1 ? editor.toScreen(.4,.1)
            : editor.loopMarkerArea(kind==3).getCentre();
        const auto target = kind==2 ? editor.pointPosition(0) : kind==3 ? editor.pointPosition(3)
            : start + juce::Point<float>(20.f,-25.f);
        editor.mouseDown(event(start));
        editor.setMsegIndex(1);
        if (returnToOriginal) editor.setMsegIndex(0);
        const auto before0=host->getModernLayer().getShape(0), before1=host->getModernLayer().getShape(1);
        editor.mouseDrag(event(target)); editor.mouseUp(event(target));
        check(host->getModernLayer().getShape(0)==before0 && host->getModernLayer().getShape(1)==before1,
              "selection change cancels old point, curve and loop drags including A-to-B-to-A");
        const auto selectedBefore=editor.getShape();
        const auto fresh=editor.pointPosition(1), moved=fresh+juce::Point<float>(0.f,-10.f);
        editor.mouseDown(event(fresh)); editor.mouseDrag(event(moved)); editor.mouseUp(event(moved));
        check(editor.getShape()!=selectedBefore && host->getModernLayer().getShape(editor.getMsegIndex())==editor.getShape(),
              "fresh drag after selection change still edits the selected envelope");
    }
}
}
