#pragma once

// ws5-modern UI self-test, run by CS80Snapshots --self-test (src/tools/ui_snapshot.cpp): every
// modern layer page constructs, paints at the Studio sizes and responds to real mouse,
// keyboard and combo interaction, external state changes refresh it, and a refresh never
// writes the APVTS state. Snapshots are written beside the other UI captures.

#include "ModernLayerHostTests.h"
#include "plugin/ModernModulationUi.h"

namespace cs80::tests
{
namespace modern_ui_detail
{
namespace mui = cs80::ui::modern_layer;
}

template <typename Check, typename Flush>
void testModernLayerUi(Check check, Flush flush, const juce::File& snapshotDir)
{
    using namespace modern_ui_detail;
    juce::TemporaryFile root("-cs80-modern-ui");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    host.prepareToPlay(48000.0, 256);
    auto& layer = host.getModernLayer();
    const auto stateBefore = host.getAPVTS().copyState();

    // Construct and paint every page at the Studio sizes; a refresh must not write state.
    auto paintCheck = [&](juce::Component& page, const char* name, int w, int h)
    {
        page.setSize(w, h);
        page.addToDesktop(0);
        page.setVisible(true);
        flush(60);
        const auto image = page.createComponentSnapshot(page.getLocalBounds(), true, 1.0f);
        check(image.isValid() && image.getWidth() == w && distinctColours(image) > 12, "modern layer page paints");
        std::function<void(juce::Component&)> inside = [&](juce::Component& parent)
        {
            for (auto* child : parent.getChildren())
            {
                if (! child->isVisible()) continue;
                if (dynamic_cast<juce::ComboBox*>(child) || dynamic_cast<juce::Slider*>(child) || dynamic_cast<juce::Button*>(child))
                    check(! child->getBounds().isEmpty() && parent.getLocalBounds().contains(child->getBounds()),
                          "modern layer control has bounds inside its page");
                inside(*child);
            }
        };
        inside(page);
        juce::FileOutputStream stream(snapshotDir.getChildFile(juce::String(name) + "-" + juce::String(w) + "x" + juce::String(h) + ".png"));
        if (stream.openedOk())
        {
            stream.setPosition(0);
            stream.truncate();
            juce::PNGImageFormat().writeImageToStream(image, stream);
        }
    };
    {
        mui::ModernModulationPage modulation(host);
        mui::KeyboardModulationPage keyboard(host);
        mui::ModernFxPage fxPage(host);
        paintCheck(modulation, "modern-modulation", 1100, 640);
        paintCheck(modulation, "modern-modulation", 1560, 820);
        paintCheck(keyboard, "modern-keyboard", 1100, 360);
        paintCheck(fxPage, "modern-fx", 1100, 380);
        flush(120);
        check(stateBefore.isEquivalentTo(host.getAPVTS().copyState()), "constructing and painting the pages writes no state");

        // MSEG editor: drag a point, bend a segment, add and delete points, move the loop end.
        auto& editor = modulation.getMsegPage().getEditor();
        const auto before = layer.getShape(0);
        const auto p1 = editor.pointPosition(1);
        drag(editor, p1, p1 + juce::Point<float>(40.0f, 30.0f));
        auto shape = layer.getShape(0);
        check(shape.points[1].time > before.points[1].time && shape.points[1].level < before.points[1].level,
              "dragging an MSEG point moves it in the session state");
        const auto segmentMid = (editor.pointPosition(2) + editor.pointPosition(3)) * 0.5f + juce::Point<float>(0.0f, 4.0f);
        const float curveBefore = shape.points[2].curve;
        drag(editor, segmentMid, segmentMid - juce::Point<float>(0.0f, 40.0f));
        check(layer.getShape(0).points[2].curve != curveBefore, "dragging a segment bends its curve");
        const int count = layer.getShape(0).count;
        const auto empty = editor.toScreen(0.5 * (layer.getShape(0).points[2].time + layer.getShape(0).points[3].time), 0.95);
        editor.mouseDoubleClick(mouse(editor, empty, empty, 2));
        check(layer.getShape(0).count == count + 1, "double-clicking empty space adds an MSEG point");
        const auto added = editor.pointPosition(3);
        editor.mouseDoubleClick(mouse(editor, added, added, 2));
        check(layer.getShape(0).count == count, "double-clicking a point deletes it");
        const auto marker = editor.loopMarkerArea(true).getCentre();
        drag(editor, marker, juce::Point<float>(editor.pointPosition(3).x, marker.y));
        check(layer.getShape(0).loopEnd == 3, "dragging the loop-end flag snaps it to a point");
        editor.mouseDown(mouse(editor, editor.pointPosition(1), editor.pointPosition(1)));
        editor.mouseUp(mouse(editor, editor.pointPosition(1), editor.pointPosition(1)));
        check(editor.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)) && layer.getShape(0).count == count - 1,
              "Delete removes the selected point");

        // Matrix: combos write the routing, the slider writes the amount parameter, and an
        // external change (state restore, another page) refreshes the rows.
        auto& matrix = modulation.getMatrixPage();
        matrix.sourceBox(0).setSelectedId((int) ModSource::lfo1 + 1, juce::sendNotificationSync);
        matrix.destinationBox(0).setSelectedId((int) ModDestination::lpCutoff + 1, juce::sendNotificationSync);
        matrix.targetBox(0).setSelectedId((int) ModTarget::rankII + 1, juce::sendNotificationSync);
        matrix.viaBox(0).setSelectedId((int) ModSource::modWheel + 1, juce::sendNotificationSync);
        const ModSlot expected { ModSource::lfo1, ModSource::modWheel, ModDestination::lpCutoff, ModTarget::rankII };
        check(layer.getSlot(0) == expected, "matrix combos write the slot routing");
        matrix.amountSlider(0).setValue(-0.4, juce::sendNotificationSync);
        check(std::abs(host.getParamValue(P_mod1Amount) + 0.4f) < 1e-4f, "matrix amount slider writes its host parameter");
        layer.setSlot(5, { ModSource::ribbon, ModSource::none, ModDestination::pan, ModTarget::both });
        flush(80);
        check(matrix.sourceBox(5).getSelectedId() == (int) ModSource::ribbon + 1
                  && matrix.destinationBox(5).getSelectedId() == (int) ModDestination::pan + 1,
              "matrix rows follow external routing changes");

        // MSEG page controls write the shape options.
        layer.setShape(1, defaultShape(1));
        modulation.getMsegPage().selectMseg(1);
        check(modulation.getMsegPage().getEditor().getMsegIndex() == 1, "MSEG selector switches the edited envelope");

        // Keyboard and FX pages drive their parameters.
        keyboard.curveSlider(0).setValue(0.5, juce::sendNotificationSync);
        check(std::abs(host.getParamValue(P_modVelocityCurve) - 0.5f) < 1e-4f, "velocity curve slider writes its parameter");
        fxPage.modelBox(1).setSelectedId((int) fx::Model::tape + 1, juce::sendNotificationSync);
        check(std::abs(host.getParamValue(P_fx2Model) - (float) fx::Model::tape) < 1e-4f, "FX model combo writes its parameter");
        fxPage.loudnessMatchButton().triggerClick();
        flush(60);
        check(host.getParamValue(P_fxGainCompensation) >= 0.5f, "loudness-match switch writes its parameter");

        // A state restore refreshes the open editor.
        juce::MemoryBlock blank;
        Cs80AudioProcessor fresh(root.getFile().getChildFile("presets"));
        fresh.getStateInformation(blank);
        host.setStateInformation(blank.getData(), (int) blank.getSize());
        flush(120);
        check(modulation.getMsegPage().getEditor().getShape() == defaultShape(1)
                  && matrix.sourceBox(0).getSelectedId() == 1,
              "restoring a state refreshes the open modern layer pages");
    }
    root.getFile().deleteRecursively();
}

} // namespace cs80::tests
