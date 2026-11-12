#include "plugin/PluginProcessor.h"
#include "plugin/StudioDrawers.h"
#include "VoiceMonitorFocusChecks.h"
#include <iostream>
namespace {
struct DirtyListener : juce::AudioProcessorListener
{
    int changes = 0;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override { ++changes; }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override { ++changes; }
};
juce::MemoryBlock save(Cs80AudioProcessor& processor)
{
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    return state;
}
}
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-voice-monitor-focus");
    Cs80AudioProcessor processor(fixture.getFile());
    cs80::ui::ScopeDrawer drawer(processor);
    drawer.setVisible(true);
    drawer.setSize(420, 470);
    int checks = 0, failures = 0;
    for (bool live : {false, true, false})
    {
        drawer.setLiveInspection(live);
        ++checks;
        if (!cs80::test::voiceMonitorFocusMatches(drawer, live)) ++failures;
        juce::KeyboardFocusTraverser traverser;
        for (auto* c : traverser.getAllComponents(&drawer))
            std::cout << c->getTitle() << '\n';
    }
    // Exercise real UI callbacks, not processor telemetry setters. Inspection is
    // observational: it must not modify session bytes or mark the host dirty.
    juce::Thread::sleep(350);
    juce::Timer::callPendingTimersSynchronously();
    const auto saved = save(processor);
    DirtyListener listener;
    processor.addListener(&listener);
    auto check = [&](bool ok) { ++checks; if (!ok) ++failures; };
    auto click = [&](const char* id)
    {
        auto* button = dynamic_cast<juce::Button*>(drawer.findChildWithID(id));
        check(button != nullptr);
        if (button && button->onClick) button->onClick();
    };
    click("scope_live");
    check(cs80::test::voiceMonitorFocusMatches(drawer, true));
    click("scope_previous_slot");
    check(drawer.getInspectedSlot() == 15);
    click("scope_next_slot");
    check(drawer.getInspectedSlot() == 0);
    juce::KeyboardFocusTraverser traverser;
    const auto liveOrder = traverser.getAllComponents(&drawer);
    if (liveOrder.size() >= 2)
    {
        auto* rank = dynamic_cast<juce::Button*>(liveOrder[1]);
        check(rank != nullptr);
        if (rank && rank->onClick) rank->onClick();
    }
    else check(false);
    check(drawer.getChannel() == 1);
    int closes = 0;
    drawer.onClose = [&] { ++closes; drawer.setVisible(false); };
    click("scope_close");
    check(closes == 1 && !drawer.isVisible());
    drawer.setVisible(true);
    check(cs80::test::voiceMonitorFocusMatches(drawer, true));
    check(!drawer.keyPressed(juce::KeyPress('x')) && closes == 1);
    check(drawer.keyPressed(juce::KeyPress(juce::KeyPress::escapeKey)) && closes == 2 && !drawer.isVisible());
    drawer.setVisible(true);
    click("scope_live");
    check(cs80::test::voiceMonitorFocusMatches(drawer, false));
    juce::Thread::sleep(350);
    juce::Timer::callPendingTimersSynchronously();
    check(listener.changes == 0);
    check(save(processor) == saved);
    processor.removeListener(&listener);
    if (argc == 2)
    {
        juce::File directory(argv[1]);
        directory.createDirectory();
        processor.selectHardwareSource(1, 6);
        drawer.setChannel(1);
        for (bool live : {false, true})
        for (int width : {420, 294})
        {
            drawer.setLiveInspection(live);
            drawer.setSize(width, juce::roundToInt(470.0 * width / 420));
            const auto image = drawer.createComponentSnapshot(drawer.getLocalBounds());
            juce::FileOutputStream stream(directory.getChildFile(juce::String(live ? "voice-live-" : "voice-settings-") + juce::String(width) + ".png"));
            stream.setPosition(0);
            stream.truncate();
            check(juce::PNGImageFormat().writeImageToStream(image, stream));
        }
    }

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
