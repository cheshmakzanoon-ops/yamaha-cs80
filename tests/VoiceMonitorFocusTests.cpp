#include "plugin/PluginProcessor.h"
#include "plugin/StudioDrawers.h"
#include "VoiceMonitorFocusChecks.h"
#include <iostream>
int main()
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
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
