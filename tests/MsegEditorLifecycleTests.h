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
    // Deleting a point while its drag is active invalidates the drag's point index.
    for (auto keyCode : {juce::KeyPress::deleteKey, juce::KeyPress::backspaceKey})
    {
        cs80::modern::MsegShape source;
        source.count = 4;
        source.points[0] = {0.f, 0.f, 0.f}; source.points[1] = {.25f, .7f, 0.f};
        source.points[2] = {.6f, .3f, 0.f}; source.points[3] = {1.f, 0.f, 0.f};
        host->getModernLayer().setShape(0, source);
        MsegEditor editor(*host); editor.setSize(500, 240); editor.setMsegIndex(0);
        auto event = [&](juce::Point<float> position) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
                juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.f,0.f,0.f,0.f,0.f,
                &editor,&editor,juce::Time::getCurrentTime(),position,juce::Time::getCurrentTime(),1,false);
        };
        const auto start = editor.pointPosition(1), target = start + juce::Point<float>(0.f, -20.f);
        editor.mouseDown(event(start));
        const bool handled = editor.keyPressed(juce::KeyPress(keyCode));
        check(!handled && host->getModernLayer().getShape(0) == source,
              "Delete and Backspace cannot invalidate an active point drag");
        editor.mouseDrag(event(target)); editor.mouseUp(event(target));
        const auto moved = host->getModernLayer().getShape(0);
        check(moved.count == source.count && moved.points[1].level != source.points[1].level
              && moved.points[2] == source.points[2],
              "the admitted drag continues to edit its original point after rejected deletion");
        check(editor.keyPressed(juce::KeyPress(keyCode))
              && host->getModernLayer().getShape(0).count == source.count - 1,
              "Delete and Backspace still remove the selected point after drag completion");
    }

    for (int buttons : {juce::ModifierKeys::rightButtonModifier, juce::ModifierKeys::middleButtonModifier,
                        juce::ModifierKeys::leftButtonModifier})
    {
        cs80::modern::MsegShape source;
        source.count = 4;
        source.points[0] = {0.f, 0.f, 0.f}; source.points[1] = {.25f, .7f, 0.f};
        source.points[2] = {.6f, .3f, 0.f}; source.points[3] = {1.f, 0.f, 0.f};
        host->getModernLayer().setShape(0, source);
        MsegEditor editor(*host); editor.setSize(500, 240); editor.setMsegIndex(0);
        auto event = [&](juce::Point<float> position) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
                juce::ModifierKeys(buttons), 1.f,0.f,0.f,0.f,0.f,
                &editor,&editor,juce::Time::getCurrentTime(),position,juce::Time::getCurrentTime(),1,false);
        };
        const bool left = buttons == juce::ModifierKeys::leftButtonModifier;
        const auto start = editor.pointPosition(1), moved = start + juce::Point<float>(0.f, -20.f);
        editor.mouseDown(event(start)); editor.mouseDrag(event(moved)); editor.mouseUp(event(moved));
        check((host->getModernLayer().getShape(0) != source) == left,
              "only left-button gestures move envelope points");
        editor.mouseDoubleClick(event(editor.pointPosition(1)));
        check(editor.getShape().count == (left ? 3 : 4),
              "only left-button double clicks delete envelope points");
        const auto beforeInsert = editor.getShape();
        editor.mouseDoubleClick(event(editor.toScreen(.45, .9)));
        check(editor.getShape().count == beforeInsert.count + (left ? 1 : 0),
              "only left-button double clicks insert envelope points");
    }

    // A graph gesture owns the revision it displayed. External edits, recall and Undo
    // must not be overwritten by the cached points or loop flags on the next event.
    auto& layer = host->getModernLayer();
    auto& undo = host->getUndoManager();
    cs80::modern::MsegShape source;
    source.count = 4;
    source.points[0] = {0.f, .1f, .15f}; source.points[1] = {.25f, .7f, -.2f};
    source.points[2] = {.6f, .3f, .1f}; source.points[3] = {1.f, .2f, 0.f};
    source.loop = cs80::modern::MsegLoop::sustain; source.loopStart = 1; source.loopEnd = 2;
    auto replacement = source;
    replacement.count = 2; replacement.points[1] = {2.f, .4f, 0.f};
    replacement.loopStart = 0; replacement.loopEnd = 1;
    replacement.sync = replacement.bipolar = true;
    replacement.trigger = cs80::modern::MsegTrigger::mono;
    replacement = cs80::modern::sanitizeShape(replacement);
    auto event = [](MsegEditor& editor, juce::Point<float> position) {
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
            juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.f,0.f,0.f,0.f,0.f,
            &editor,&editor,juce::Time::getCurrentTime(),position,juce::Time::getCurrentTime(),1,false);
    };
    for (int slot = 0; slot < cs80::modern::kMsegCount; ++slot)
        for (int kind = 0; kind < 4; ++kind)
            for (int external = 0; external < 6; ++external)
                for (bool deliverChange : {false, true})
    {
        layer.resetToDefaults();
        layer.setShape(slot, replacement);
        juce::MemoryBlock saved;
        host->getStateInformation(saved);
        layer.setShape(slot, source);
        MsegEditor editor(*host); editor.setSize(500, 240); editor.setMsegIndex(slot);
        undo.clearUndoHistory();
        const auto start = kind == 0 ? editor.pointPosition(2) : kind == 1 ? editor.toScreen(.4, .1)
            : editor.loopMarkerArea(kind == 3).getCentre();
        const auto target = kind == 2 ? editor.pointPosition(0) : kind == 3 ? editor.pointPosition(3)
            : start + juce::Point<float>(10.f, -15.f);
        editor.mouseDown(event(editor, start));
        editor.mouseDrag(event(editor, target));
        check(layer.getShape(slot) != source, "each graph gesture initially edits its intended field");
        // Delivery of our own publication must not cancel a continuing gesture.
        layer.sendSynchronousChangeMessage();
        const auto edited = layer.getShape(slot);
        const auto continued = kind == 2 ? editor.pointPosition(2) : kind == 3 ? editor.pointPosition(1)
            : target + juce::Point<float>(-20.f, -20.f);
        if (external == 0) layer.setShape(slot, replacement);
        else if (external == 1)
        {
            auto settings = edited; settings.sync = !settings.sync; settings.bipolar = !settings.bipolar;
            settings.trigger = cs80::modern::MsegTrigger::free;
            layer.setShape(slot, settings);
        }
        else if (external == 2)
        {
            auto config = layer.getConfig();
            config.slots[0].source = cs80::modern::ModSource::velocity;
            config.msegs[(std::size_t) ((slot + 1) % cs80::modern::kMsegCount)] = replacement;
            layer.setConfig(config);
        }
        else if (external == 3)
            check(host->undo() && layer.getShape(slot) == source, "Undo restores the shape during the active drag");
        else if (external == 4)
        {
            layer.setShape(slot, replacement); layer.setShape(slot, edited);
        }
        else host->setStateInformation(saved.getData(), (int) saved.getSize());
        const auto expected = layer.getConfig(); const auto revision = layer.getRevision();
        if (deliverChange) layer.sendSynchronousChangeMessage();
        editor.mouseDrag(event(editor, continued));
        editor.mouseUp(event(editor, continued));
        check(layer.getConfig() == expected && layer.getRevision() == revision,
              "external shape/settings/routing/Undo/A-B-A/session changes cancel stale drags without writes");
        check(editor.getShape() == expected.msegs[(std::size_t) slot],
              "canceled drag immediately displays the published shape");
        const auto fresh = editor.pointPosition(1), moved = fresh + juce::Point<float>(0.f, -10.f);
        editor.mouseDown(event(editor, fresh)); editor.mouseDrag(event(editor, moved)); editor.mouseUp(event(editor, moved));
        check(layer.getShape(slot) != expected.msegs[(std::size_t) slot] && editor.getShape() == layer.getShape(slot),
              "a fresh gesture works after external state invalidated the old gesture");
        check(host->undo() && layer.getConfig() == expected,
              "Undo of the fresh gesture restores the external state, not the canceled cached shape");
    }

    for (int kind = 0; kind < 4; ++kind)
        for (int interruption = 0; interruption < 3; ++interruption)
    {
        layer.setShape(0, source);
        MsegEditor editor(*host); editor.setSize(500, 240); editor.setMsegIndex(0);
        const auto start = kind == 0 ? editor.pointPosition(2) : kind == 1 ? editor.toScreen(.4, .1)
            : editor.loopMarkerArea(kind == 3).getCentre();
        const auto first = kind == 2 ? editor.pointPosition(0) : kind == 3 ? editor.pointPosition(3)
            : start + juce::Point<float>(10.f, -10.f);
        const auto second = kind == 2 ? editor.pointPosition(2) : kind == 3 ? editor.pointPosition(1)
            : start + juce::Point<float>(20.f, -20.f);
        editor.mouseDown(event(editor, start)); editor.mouseDrag(event(editor, first));
        const auto edited = layer.getShape(0);
        if (interruption == 0)
        {
            layer.sendSynchronousChangeMessage();
            editor.mouseDrag(event(editor, second)); editor.mouseUp(event(editor, second));
            check(layer.getShape(0) != edited && layer.getShape(0) == editor.getShape(),
                  "own publication notifications preserve continued point, curve and loop drags");
        }
        else if (interruption == 1)
        {
            layer.setShape(0, replacement);
            editor.mouseUp(event(editor, first));
            check(layer.getShape(0) == replacement && editor.getShape() == replacement
                  && editor.viewLength() == replacement.endTime() * 1.12f,
                  "release without another drag refreshes pending external state and thaws the time axis");
        }
        else
        {
            editor.setEnabled(false); editor.setEnabled(true);
            editor.mouseDrag(event(editor, second)); editor.mouseUp(event(editor, second));
            check(layer.getShape(0) == edited, "disable and re-enable cannot revive an interrupted graph gesture");
        }
    }

    // Events can reach the component before ChangeBroadcaster delivers a new state.
    // Never reinterpret a click or a selected point against unseen replacement topology.
    for (int action = 0; action < 4; ++action)
    {
        layer.setShape(0, source);
        MsegEditor editor(*host); editor.setSize(500, 240); editor.setMsegIndex(0);
        const auto oldPoint = editor.pointPosition(2);
        editor.mouseDown(event(editor, oldPoint)); editor.mouseUp(event(editor, oldPoint));
        layer.setShape(0, replacement);
        const auto revision = layer.getRevision();
        if (action == 0)
        {
            editor.mouseDown(event(editor, oldPoint));
            editor.mouseDrag(event(editor, oldPoint + juce::Point<float>(0.f, -20.f)));
            editor.mouseUp(event(editor, oldPoint));
        }
        else if (action == 1) editor.mouseDoubleClick(event(editor, oldPoint));
        else check(!editor.keyPressed(juce::KeyPress(action == 2 ? juce::KeyPress::deleteKey : juce::KeyPress::backspaceKey)),
                   "a stale selected point cannot be removed before state refresh");
        check(layer.getShape(0) == replacement && layer.getRevision() == revision && editor.getShape() == replacement,
              "stale mouse-down, double-click, Delete and Backspace refresh without overwriting new topology");
    }

    // JUCE delivers mouseUp before mouseDoubleClick for the same second click.
    // Refreshing on release must not make that canceled sequence valid again.
    for (int interruption = 0; interruption < 5; ++interruption)
    {
        layer.setShape(0, source);
        MsegEditor editor(*host); editor.setSize(500, 240); editor.setMsegIndex(0);
        const auto oldPoint = editor.pointPosition(2);
        if (interruption == 0) layer.setShape(0, replacement);
        editor.mouseDown(event(editor, oldPoint));
        if (interruption == 1 || interruption == 2)
        {
            layer.setShape(0, replacement);
            if (interruption == 2) layer.sendSynchronousChangeMessage();
        }
        if (interruption == 3) { editor.setEnabled(false); editor.setEnabled(true); }
        if (interruption == 4) { editor.setMsegIndex(1); editor.setMsegIndex(0); }
        const auto expected = layer.getConfig(); const auto revision = layer.getRevision();
        editor.mouseUp(event(editor, oldPoint)); editor.mouseDoubleClick(event(editor, oldPoint));
        check(layer.getConfig() == expected && layer.getRevision() == revision,
              "mouse-up refresh, selection and enablement cancellation also reject the sequence's following double-click");
        const auto fresh = editor.toScreen(.45, .95);
        const auto count = editor.getShape().count;
        editor.mouseDown(event(editor, fresh)); editor.mouseUp(event(editor, fresh));
        editor.mouseDoubleClick(event(editor, fresh));
        check(editor.getShape().count == count + 1 && layer.getShape(0) == editor.getShape(),
              "a fresh double-click sequence inserts a point after canceled pointer input");
    }

}
}
