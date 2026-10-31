#include "plugin/PluginProcessor.h"
#include "plugin/ModernModulationUi.h"
#include <bit>
#include <cstdio>
#include <memory>
static juce::MouseEvent mouse(juce::Component& c, juce::Point<float> at, juce::Point<float> down) {
    const auto now = juce::Time::getCurrentTime();
    return { juce::Desktop::getInstance().getMainMouseSource(), at,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
        &c, &c, now, down, now, 1, at != down };
}
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    auto host = std::make_unique<Cs80AudioProcessor>();
    auto& state = host->getUndoableState();
    auto* volume = state.getParameter(Cs80AudioProcessor::parameterId(cs80::P_volume));
    volume->beginChangeGesture(); volume->setValueNotifyingHost(0.625f); volume->endChangeGesture();
    state.getParameter(Cs80AudioProcessor::parameterId(cs80::P_mix))->setValueNotifyingHost(0.25f);
    cs80::ui::modern_layer::MsegEditor editor(*host); editor.setSize(640, 240);
    const auto start = editor.pointPosition(1), finish = start + juce::Point<float>(30.0f, 20.0f);
    editor.mouseDown(mouse(editor, start, start));
    editor.mouseDrag(mouse(editor, (start + finish) * 0.5f, start));
    editor.mouseDrag(mouse(editor, finish, start)); editor.mouseUp(mouse(editor, finish, start));
    const auto edited = host->getModernLayer().getShape(0);
    {
        const juce::ScopedLock treeLock(state.treeLock()); state.syncNow();
        std::printf("stored points: %s\n", state.state.getChildWithName("MODERN_LAYER").getChildWithName("MSEG").getProperty("points").toString().toRawUTF8());
    }
    const bool undone = host->undo(), redone = host->redo();
    const auto replayed = host->getModernLayer().getShape(0);
    std::printf("undo=%d redo=%d shape_equal=%d volume=%a\n", undone, redone, edited == replayed, (double) volume->getValue());
    for (int i = 0; i < edited.count; ++i) {
        const auto a=edited.points[(size_t)i], b=replayed.points[(size_t)i];
        std::printf("point %d time: %.12g [0x%08x] -> %.12g [0x%08x]; level: %.12g [0x%08x] -> %.12g [0x%08x]; curve: %.12g -> %.12g\n",
            i, (double)a.time, std::bit_cast<unsigned>(a.time), (double)b.time, std::bit_cast<unsigned>(b.time),
            (double)a.level, std::bit_cast<unsigned>(a.level), (double)b.level, std::bit_cast<unsigned>(b.level), (double)a.curve, (double)b.curve);
    }
}
