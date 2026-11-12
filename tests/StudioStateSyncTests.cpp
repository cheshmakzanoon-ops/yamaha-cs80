#include "plugin/PluginProcessor.h"
#include "plugin/StudioMount.h"
#include <iostream>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
juce::MemoryBlock save(Cs80AudioProcessor& p)
{
    juce::MemoryBlock data;
    p.getStateInformation(data);
    return data;
}
void restore(Cs80AudioProcessor& p, const juce::MemoryBlock& data)
{
    p.setStateInformation(data.getData(), static_cast<int>(data.getSize()));
}
void flush()
{
    juce::Thread::sleep(130);
    juce::Timer::callPendingTimersSynchronously();
}
struct Notes : juce::MidiKeyboardState::Listener
{
    int ons = 0, offs = 0;
    void handleNoteOn(juce::MidiKeyboardState*, int, int, float) override { ++ons; }
    void handleNoteOff(juce::MidiKeyboardState*, int, int, float) override { ++offs; }
};
struct Host : juce::AudioProcessorListener
{
    int dirty = 0;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& d) override
    {
        dirty += d.nonParameterStateChanged;
    }
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile temp("-studio-sync");
    Cs80AudioProcessor p(temp.getFile());
    auto& state = p.getEditorState();
    p.setParamValue(cs80::P_parityMode, 0);
    state.qwertyEnabled = false;
    state.controllersShown = true;
    state.circuitNotes = true;
    const auto restored = save(p);
    state.qwertyEnabled = true;
    state.controllersShown = false;
    state.circuitNotes = false;
    cs80::ui::StudioMount studio(p);
    studio.sync();
    check(studio.qwerty.isEnabled(), "initial keyboard enabled");
    check(!studio.areControllersShown() && !studio.controllers.isVisible(), "initial controllers hidden");
    check(!cs80::ui::circuitNotesInTooltips(), "initial notes hidden");

    Notes notes;
    p.getKeyboardState().addListener(&notes);
    studio.qwerty.isKeyDown = [](int) { return true; };
    studio.qwerty.textEntryFocused = [] { return false; };
    check(studio.qwerty.keyPressed(juce::KeyPress('A', {}, 'a'), nullptr), "enabled keyboard accepts note");
    check(notes.ons == 1 && studio.qwerty.heldCount() == 1, "held keyboard note tracked");
    restore(p, restored);
    // State restoration may itself release keyboard state. Measure the mount's
    // remaining held-key cleanup separately, rather than assume restore behavior.
    studio.sync();
    check(!studio.qwerty.isEnabled(), "open editor follows restored QWERTY flag");
    check(studio.qwerty.heldCount() == 0 && !p.getKeyboardState().isNoteOn(1, 60), "restored disable leaves no held note");
    check(notes.offs >= 1, "held note receives noteOff");
    check(studio.areControllersShown() && studio.controllers.isVisible(), "restored controller preference visible outside PURE");
    check(cs80::ui::circuitNotesInTooltips(), "open editor follows restored circuit notes flag");
    const int offs = notes.offs;
    for (int i = 0; i < 4; ++i) studio.sync();
    check(notes.offs == offs, "unchanged sync emits no repeated noteOff");
    check(!studio.qwerty.keyPressed(juce::KeyPress('A', {}, 'a'), nullptr), "disabled keyboard rejects note");

    // Setter state and local caches must survive the next periodic sync.
    studio.setQwertyEnabled(true);
    studio.setControllersShown(false);
    studio.setCircuitNotesShown(false);
    studio.sync();
    check(state.qwertyEnabled.load() && studio.qwerty.isEnabled(), "QWERTY setter survives sync");
    check(!state.controllersShown.load() && !studio.areControllersShown() && !studio.controllers.isVisible(), "controller setter survives sync");
    check(!state.circuitNotes.load() && !cs80::ui::circuitNotesInTooltips(), "notes setter survives sync");
    const auto setterState = save(p);
    restore(p, restored);
    studio.sync();
    restore(p, setterState);
    studio.sync();
    check(studio.qwerty.isEnabled() && !studio.areControllersShown() && !cs80::ui::circuitNotesInTooltips(), "opposite valid restore updates all flags again");

    // Restored controller intent remains latent in PURE and returns afterward.
    p.setParamValue(cs80::P_parityMode, 1);
    state.controllersShown = true;
    const auto pureState = save(p);
    studio.setControllersShown(false);
    restore(p, pureState);
    studio.sync();
    check(studio.areControllersShown() && !studio.controllers.isVisible(), "PURE hides restored wanted controllers");
    p.setParamValue(cs80::P_parityMode, 0);
    studio.sync();
    check(studio.controllers.isVisible(), "leaving PURE reveals restored controller intent");
    studio.setControllersShown(false);
    p.setParamValue(cs80::P_parityMode, 1);
    studio.sync();
    p.setParamValue(cs80::P_parityMode, 0);
    studio.sync();
    check(!studio.controllers.isVisible(), "PURE roundtrip preserves unwanted controllers");

    // Tooltips remain global. Unchanged mounts must not fight each other's
    // most recent explicit preference application on every timer tick.
    juce::TemporaryFile secondTemp("-studio-sync-second");
    Cs80AudioProcessor second(secondTemp.getFile());
    second.getEditorState().circuitNotes = true;
    {
        cs80::ui::StudioMount secondStudio(second);
        check(cs80::ui::circuitNotesInTooltips(), "second mount applies initial notes preference");
        studio.sync();
        check(cs80::ui::circuitNotesInTooltips(), "unchanged first mount does not overwrite global preference");
        secondStudio.setCircuitNotesShown(false);
        state.circuitNotes = true;
        studio.sync();
        check(cs80::ui::circuitNotesInTooltips(), "changed first preference applies");
        secondStudio.sync();
        check(cs80::ui::circuitNotesInTooltips(), "setter updates second mount cache, avoids repeated global write");
    }

    flush();
    Host host;
    p.addListener(&host);
    restore(p, setterState);
    flush(); // Existing 8 Hz mount timer must consume restored preferences too.
    check(studio.qwerty.isEnabled() && !studio.areControllersShown()
          && !cs80::ui::circuitNotesInTooltips(), "periodic sync consumes restored preferences");
    check(host.dirty == 0, "accepted restore and UI synchronization do not dirty host");
    p.removeListener(&host);
    p.getKeyboardState().removeListener(&notes);
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
