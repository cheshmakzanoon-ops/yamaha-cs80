#include "plugin/PluginProcessor.h"
#include "plugin/ModernModulationUi.h"
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

namespace
{
using namespace cs80::ui::modern_layer;
int checks = 0, failures = 0;
void check(bool ok, const char* message)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
void timers()
{
    juce::Thread::sleep(130);
    juce::Timer::callPendingTimersSynchronously();
}
juce::MemoryBlock save(Cs80AudioProcessor& host)
{
    juce::MemoryBlock result;
    host.getStateInformation(result);
    return result;
}
struct Probe final : juce::AudioProcessorListener
{
    explicit Probe(Cs80AudioProcessor& p) : host(p) { host.addListener(this); }
    ~Probe() override { host.removeListener(this); }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override { ++parameters; }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& d) override
    {
        ++notifications;
        dirty += d.nonParameterStateChanged;
        program += d.programChanged;
        other += d.latencyChanged || d.parameterInfoChanged;
        wrongThread |= !juce::MessageManager::getInstance()->isThisTheMessageThread();
        observed = save(host); // host may save reentrantly at this boundary
        if (callback) callback();
    }
    void reset() { notifications = dirty = program = other = parameters = 0; wrongThread = false; observed.reset(); }
    void expect(int count, bool parameterEdit = false)
    {
        check(notifications == count && dirty == count, "exact coalesced dirty notification count");
        check(program == 0 && other == 0 && !wrongThread, "dirty-only notification on message thread");
        if (!parameterEdit) check(parameters == 0, "side-state edit does not fabricate parameter notifications");
        if (count) check(observed == save(host), "reentrant save sees latest complete state");
    }
    Cs80AudioProcessor& host;
    int notifications=0, dirty=0, program=0, other=0, parameters=0;
    bool wrongThread=false;
    juce::MemoryBlock observed;
    std::function<void()> callback;
};
juce::ToggleButton* toggle(MsegPage& page, const char* name)
{
    for (auto* child : page.getChildren())
        if (auto* b = dynamic_cast<juce::ToggleButton*>(child); b && b->getButtonText() == name) return b;
    return nullptr;
}
juce::ComboBox* combo(MsegPage& page, const char* title)
{
    for (auto* child : page.getChildren())
        if (auto* b = dynamic_cast<juce::ComboBox*>(child); b && b->getTitle() == title) return b;
    return nullptr;
}
void click(MsegPage& page, const char* name)
{
    auto* button = toggle(page, name);
    check(button != nullptr, "actual MSEG toggle found");
    if (button) { button->setToggleState(!button->getToggleState(), juce::dontSendNotification); button->onClick(); }
}
std::function<void()> menuAction(const juce::PopupMenu& menu, const juce::String& text)
{
    for (juce::PopupMenu::MenuItemIterator it(menu); it.next();)
        if (it.getItem().text == text && it.getItem().isEnabled) return it.getItem().action;
    return {};
}
juce::MouseEvent mouse(juce::Component& c, juce::Point<float> p)
{
    return {juce::Desktop::getInstance().getMainMouseSource(), p,
        juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier), 1.f, 0.f, 0.f, 0.f, 0.f,
        &c, &c, juce::Time::getCurrentTime(), p, juce::Time::getCurrentTime(), 1, false};
}
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile root("-modern-state-notification");
    Cs80AudioProcessor host(root.getFile());
    host.setPure(false);
    auto page = std::make_unique<MsegPage>(host);
    page->setSize(1000, 400);
    ModMatrixPage matrix(host);
    matrix.setSize(1000, 600);
    timers();
    Probe probe(host);
    auto changed = [&](auto action)
    {
        probe.reset();
        const auto before = host.getModernLayer().getConfig();
        action();
        check(before != host.getModernLayer().getConfig(), "valid component action changed modulation state");
        check(probe.notifications == 0, "notification deferred beyond component mutation stack");
        timers(); probe.expect(1);
        probe.reset(); timers(); probe.expect(0);
    };
    for (int slot = 0; slot < 3; ++slot)
    {
        page->selectMseg(slot);
        changed([&] { click(*page, "Sync"); });
        changed([&] { click(*page, "Bipolar"); });
        for (const auto* title : { "MSEG loop mode", "MSEG trigger mode", "MSEG shape preset" })
        {
            auto* box = combo(*page, title);
            check(box != nullptr, "actual MSEG choice found");
            if (box) changed([&] { box->setSelectedId(box->getSelectedId() == 2 ? 1 : 2, juce::sendNotificationSync); });
        }
    }
    for (int row = 0; row < 16; ++row)
    {
        for (auto* box : { &matrix.sourceBox(row), &matrix.viaBox(row), &matrix.destinationBox(row), &matrix.targetBox(row) })
            changed([&] { box->setSelectedId(2, juce::sendNotificationSync); });
        probe.reset();
        matrix.sourceBox(row).onChange(); // explicit unchanged callback
        timers(); probe.expect(0);
    }
    page->selectMseg(0); // refresh cached graph after other edits
    auto& graph = page->getEditor();
    changed([&] {
        const auto point = graph.pointPosition(1);
        graph.mouseDown(mouse(graph, point));
        graph.mouseDrag(mouse(graph, point.translated(4.f, 12.f)));
        graph.mouseUp(mouse(graph, point.translated(4.f, 12.f)));
    });
    page->selectMseg(0);
    changed([&] {
        const auto point = graph.toScreen(.37, .53);
        graph.mouseDown(mouse(graph, point)); graph.mouseUp(mouse(graph, point));
        graph.mouseDoubleClick(mouse(graph, point));
    });
    // Valid typed point editing exercises the separate numeric-commit path.
    page->showPointValues();
    auto* form = page->getPointValuesEditor();
    check(form != nullptr, "actual point-values overlay opens");
    if (form)
    {
        juce::TextEditor* level = nullptr;
        juce::Button* apply = nullptr;
        for (auto* child : form->getChildren())
        {
            if (child->getTitle() == "Point normalized level") level = dynamic_cast<juce::TextEditor*>(child);
            if (child->getTitle() == "Apply MSEG point values") apply = dynamic_cast<juce::Button*>(child);
        }
        check(level != nullptr && apply != nullptr, "real numeric form controls found");
        if (level && apply) changed([&] { level->setText("0.42", false); apply->onClick(); });
    }
    auto invoke = [&](std::function<void()> action) { check(bool(action), "actual menu action available"); if (action) action(); };
    changed([&] { invoke(menuAction(page->buildShapeCopyMenu(), "Copy shape to MSEG 2")); });
    changed([&] { invoke(menuAction(matrix.buildRouteActionsMenu(0), "Clear route and amount")); });
    changed([&] { invoke(menuAction(matrix.buildRouteActionsMenu(1), "Copy route + amount to slot 1")); });

    // Multiple real actions share one notification; future timer intervals remain live.
    probe.reset(); click(*page, "Sync"); click(*page, "Bipolar");
    timers(); probe.expect(1);
    changed([&] { click(*page, "Sync"); });

    // Coalescing does not discard the existing program-display request.
    probe.reset(); click(*page, "Sync"); host.getUndoableState().requestProgramDisplayUpdate();
    timers();
    check(probe.notifications == 1 && probe.dirty == 1 && probe.program == 1 && probe.other == 0,
          "program and side-state flags coexist in one delivery");

    // Route edits form explicit transactions. Replays dirty only when side-state changes.
    host.getUndoManager().clearUndoHistory();
    changed([&] { matrix.sourceBox(0).setSelectedId(3, juce::sendNotificationSync); });
    const auto edited = host.getModernLayer().getConfig();
    changed([&] { check(host.undo(), "real route undo succeeds"); });
    changed([&] { check(host.redo(), "real route redo succeeds"); });
    check(host.getModernLayer().getConfig() == edited, "redo restores route");
    probe.reset(); check(!host.redo(), "empty redo rejected"); timers(); probe.expect(0);
    {
        Cs80AudioProcessor::ScopedUserEdit edit(host.getUndoableState(), "Active edit");
        check(!host.undo(), "undo during active edit rejected");
    }
    timers(); probe.expect(0);

    bool backgroundUndo = true, backgroundRedo = true;
    std::thread background([&] { backgroundUndo = host.undo(); backgroundRedo = host.redo(); });
    background.join();
    check(!backgroundUndo && !backgroundRedo, "background replay remains rejected");
    check(host.getModernLayer().getConfig() == edited, "background replay preserves state");
    timers(); probe.expect(0);

    // Parameter-only amount edit/replay uses its normal parameter notification,
    // not the new non-parameter channel.
    host.getUndoManager().clearUndoHistory();
    probe.reset();
    auto* amount = host.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::P_mod1Amount));
    amount->beginChangeGesture(); amount->setValueNotifyingHost(amount->convertTo0to1(.375f)); amount->endChangeGesture();
    timers(); probe.expect(0, true);
    check(probe.parameters > 0, "amount change retains parameter host notifications");
    probe.reset(); check(host.undo(), "parameter-only undo succeeds"); timers(); probe.expect(0, true);
    check(probe.parameters > 0, "parameter-only undo retains parameter notifications");
    probe.reset(); check(host.redo(), "parameter-only redo succeeds"); timers(); probe.expect(0, true);

    // A new edit made reentrantly during host delivery remains pending for the
    // next timer; the current exchange must not erase it after the callback.
    probe.reset(); click(*page, "Sync");
    bool once = false;
    probe.callback = [&] { if (!once) { once = true; click(*page, "Bipolar"); } };
    timers();
    check(probe.notifications == 1 && probe.dirty == 1 && once, "first delivery admits reentrant editor action");
    probe.callback = {}; probe.reset(); timers(); probe.expect(1);
    probe.reset(); timers(); probe.expect(0);

    // Programmatic setup and host restore are not editor actions.
    probe.reset();
    auto config = host.getModernLayer().getConfig(); config.msegs[0].sync = !config.msegs[0].sync;
    host.getModernLayer().setConfig(config); timers(); probe.expect(0);
    const auto restored = save(host);
    probe.reset(); host.setStateInformation(restored.getData(), static_cast<int>(restored.getSize()));
    timers(); probe.expect(0);
    page->selectMseg(0);
    probe.reset(); click(*page, "Sync");
    host.setStateInformation(restored.getData(), static_cast<int>(restored.getSize()));
    timers(); probe.expect(0);
    check(host.getModernLayer().getConfig() == config, "accepted restore supersedes pending edit");
    page->selectMseg(0);
    changed([&] { click(*page, "Sync"); }); // future edits are not suppressed

    // Component lifetime does not own committed-state notification delivery.
    probe.reset(); click(*page, "Sync"); page.reset(); timers(); probe.expect(1);
    page = std::make_unique<MsegPage>(host); page->setSize(1000, 400);
    probe.reset(); click(*page, "Sync");
    probe.callback = [&] { page.reset(); };
    timers(); probe.expect(1);
    check(page == nullptr, "host listener can destroy page at deferred delivery");
    probe.callback = {};
    probe.reset(); timers(); probe.expect(0);
    std::cout << "Modern state notification checks=" << checks << " failures=" << failures << '\n';
    return failures ? 1 : 0;
}
