#include "plugin/PluginProcessor.h"
#include "plugin/StudioMount.h"
#include <functional>
#include <cmath>
#include <iostream>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
// Allow the headless timer thread to finish its pending-message wait before one dispatch.
void flush() { juce::Thread::sleep(350); juce::Timer::callPendingTimersSynchronously(); }
juce::MemoryBlock save(Cs80AudioProcessor& p)
{
    juce::MemoryBlock data;
    p.getStateInformation(data);
    return data;
}
struct Listener : juce::AudioProcessorListener
{
    Cs80AudioProcessor& host;
    int dirty = 0, other = 0;
    bool wrongThread = false;
    juce::MemoryBlock observed;
    std::function<void()> callback;
    explicit Listener(Cs80AudioProcessor& p) : host(p) { host.addListener(this); }
    ~Listener() { host.removeListener(this); }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& d) override
    {
        dirty += d.nonParameterStateChanged;
        other += d.programChanged || d.parameterInfoChanged || d.latencyChanged;
        wrongThread |= !juce::MessageManager::getInstance()->isThisTheMessageThread();
        observed = save(host); // Reentrant host serialization must see completed preference edit.
        if (callback) callback();
    }
    void reset() { dirty = other = 0; wrongThread = false; observed.reset(); }
    void expect(int count)
    {
        check(dirty == count, "expected coalesced dirty count");
        check(other == 0 && !wrongThread, "dirty-only delivery on message thread");
        if (count) check(observed == save(host), "reentrant host save sees final state");
    }
};
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile temp("-studio-preference");
    Cs80AudioProcessor p(temp.getFile());
    cs80::ui::StudioMount studio(p);
    flush();
    Listener listener(p);
    studio.tour.setSteps({ { "Test tour", "One valid tour step", {} } });
    int actionNumber = 0;
    auto action = [&](auto edit, int expected)
    {
        listener.reset();
        const int before = failures;
        ++actionNumber;
        edit();
        check(listener.dirty == 0, "preference edit does not call host synchronously");
        flush();
        listener.expect(expected);
        if (failures != before) std::cerr << "Action " << actionNumber << " dirty=" << listener.dirty << " expected=" << expected << '\n';
    };

    studio.qwerty.isKeyDown = [](int) { return true; };
    studio.qwerty.textEntryFocused = [] { return false; };
    check(studio.qwerty.keyPressed(juce::KeyPress('A', {}, 'a'), nullptr), "enabled QWERTY plays note");
    check(studio.qwerty.heldCount() == 1 && p.getKeyboardState().isNoteOn(1, 60), "keyboard note held before edit");
    action([&] { studio.setQwertyEnabled(false); }, 1);
    check(!studio.qwerty.isEnabled() && studio.qwerty.heldCount() == 0
          && !p.getKeyboardState().isNoteOn(1, 60), "preference disable releases held note");
    action([&] { studio.setQwertyEnabled(false); }, 0);
    action([&] { studio.setControllersShown(true); }, 1);
    action([&] { studio.setControllersShown(true); }, 0);
    action([&] { studio.setCircuitNotesShown(true); }, 1);
    action([&] { studio.setCircuitNotesShown(true); }, 0);
    check(studio.tour.getPeer() == nullptr, "tour has no native peer");
    action([&] { studio.tour.start(); studio.tour.dismiss(); }, 1);
    check(p.getEditorState().tourSeen.load(), "actual peerless dismissal records session flag");
    action([&] { studio.tour.start(); studio.tour.dismiss(); }, 0);

    // All four serialized fields coalesce into one final-state notification.
    action([&]
    {
        studio.setQwertyEnabled(true);
        studio.setControllersShown(false);
        studio.setCircuitNotesShown(false);
        p.setEditorTourSeen(false);
    }, 1);
    action([&]
    {
        p.setEditorQwertyEnabled(true);
        p.setEditorControllersShown(false);
        p.setEditorCircuitNotes(false);
        p.setEditorTourSeen(false);
    }, 0);
    action([&] { studio.sync(); studio.sync(); }, 0);

    p.setParamValue(cs80::P_parityMode, 1);
    flush();
    action([&] { studio.setControllersShown(true); }, 1);
    check(studio.areControllersShown() && !studio.controllers.isVisible(), "PURE hides but saves desired controllers");
    p.setParamValue(cs80::P_parityMode, 0);
    action([&] { studio.sync(); }, 0);
    check(studio.controllers.isVisible(), "leaving PURE reveals saved preference without dirty update");

    // Only normal accepted self-generated state is used here.
    const auto ownState = save(p);
    action([&]
    {
        studio.setQwertyEnabled(false);
        studio.setControllersShown(false);
        studio.setCircuitNotesShown(true);
        p.setEditorTourSeen(true);
        p.setStateInformation(ownState.getData(), static_cast<int>(ownState.getSize()));
        studio.sync();
    }, 0);
    check(studio.qwerty.isEnabled() && studio.areControllersShown()
          && !p.getEditorState().circuitNotes.load() && !p.getEditorState().tourSeen.load(),
          "accepted restore supersedes pending preference edits");
    action([&]
    {
        p.setStateInformation(ownState.getData(), static_cast<int>(ownState.getSize()));
        studio.sync();
    }, 0);
    action([&] { studio.setCircuitNotesShown(true); }, 1);

    // An edit during the host callback waits for the next notification turn.
    listener.reset();
    bool once = false;
    listener.callback = [&]
    {
        if (!once)
        {
            once = true;
            studio.setControllersShown(false);
        }
    };
    studio.setCircuitNotesShown(false);
    check(listener.dirty == 0, "reentrant test starts deferred");
    flush();
    check(listener.dirty == 1, "listener edit is not delivered recursively");
    listener.callback = {};
    listener.reset();
    flush();
    listener.expect(1);
    action([&] { studio.sync(); }, 0);

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
