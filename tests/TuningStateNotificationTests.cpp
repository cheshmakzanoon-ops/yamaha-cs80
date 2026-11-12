#include "plugin/PluginProcessor.h"
#include "plugin/KeyboardTuningPage.h"

#include <functional>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
using Page = cs80::ui::modern_layer::KeyboardTuningPage;
int checks = 0, failures = 0;
void check(bool value, const char* text)
{
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL: " << text << '\n'; }
}
const juce::String scale = "! valid fixture\nNotification twelve-tone\n12\n100.0\n200.0\n300.0\n400.0\n500.0\n600.0\n700.0\n800.0\n900.0\n1000.0\n1100.0\n2/1\n";
const juce::String replacement = "! valid replacement\nReplacement twelve-tone\n12\n101.0\n201.0\n301.0\n401.0\n501.0\n601.0\n701.0\n801.0\n901.0\n1001.0\n1101.0\n2/1\n";
const juce::String mapping432 = "0\n0\n127\n60\n69\n432.0\n12\n";
const juce::String mapping444 = "0\n0\n127\n60\n69\n444.0\n12\n";
juce::MemoryBlock saved(Cs80AudioProcessor& host)
{
    juce::MemoryBlock result;
    host.getStateInformation(result);
    return result;
}
juce::ValueTree tuning(const juce::MemoryBlock& state)
{
    auto xml = juce::XmlDocument::parse(juce::String::fromUTF8(
        static_cast<const char*>(state.getData()), static_cast<int>(state.getSize())));
    return xml == nullptr ? juce::ValueTree() : juce::ValueTree::fromXml(*xml).getChildWithName("WS3_TUNING");
}
std::vector<float> parameters(Cs80AudioProcessor& host)
{
    std::vector<float> values;
    for (auto* parameter : host.getParameters()) values.push_back(parameter->getValue());
    return values;
}
struct Probe final : juce::AudioProcessorListener
{
    explicit Probe(Cs80AudioProcessor& p) : host(p) { host.addListener(this); }
    ~Probe() override { host.removeListener(this); }
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override { ++parameterChanges; }
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& details) override
    {
        ++notifications;
        dirty += details.nonParameterStateChanged ? 1 : 0;
        unrelatedFlags += details.latencyChanged || details.parameterInfoChanged || details.programChanged ? 1 : 0;
        wrongThread |= !juce::MessageManager::getInstance()->isThisTheMessageThread();
        // Real hosts can synchronously save state, read metadata or destroy an
        // editor in this callback. No Scala lock or page access may span it.
        observed = saved(host);
        description = host.getScalaDescription();
        hasScale = host.hasScalaTuning();
        if (onChange) onChange();
    }
    void reset()
    {
        notifications = dirty = unrelatedFlags = parameterChanges = 0;
        wrongThread = false;
        observed.reset();
        description.clear();
        hasScale = false;
    }
    Cs80AudioProcessor& host;
    int notifications = 0, dirty = 0, unrelatedFlags = 0, parameterChanges = 0;
    bool wrongThread = false, hasScale = false;
    juce::MemoryBlock observed;
    juce::String description;
    std::function<void()> onChange;
};
void timers()
{
    juce::Thread::sleep(120);
    juce::Timer::callPendingTimersSynchronously();
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile root("-tuning-state-notification");
    for (bool pure : { false, true }) for (int mode : { 0, 1, 2 })
    {
        auto host = std::make_unique<Cs80AudioProcessor>(root.getFile());
        host->setPure(pure);
        host->setParamValue(cs80::P_tuningMode, static_cast<float>(mode));
        Page page(*host);
        timers();
        Probe probe(*host);
        const auto originalParameters = parameters(*host);
        const auto metadata = host->getPresetMetadata();
        auto changed = [&](const char* name, const std::function<bool()>& action)
        {
            const auto before = saved(*host);
            probe.reset();
            check(action(), "valid user tuning action succeeds");
            const auto after = saved(*host);
            std::cout << name << ": state-changed=" << (before != after) << ", dirty=" << probe.dirty << '\n';
            check(before != after, "fixture changes saved session state");
            check(probe.notifications == 1 && probe.dirty == 1 && probe.unrelatedFlags == 0,
                  "changed user action emits only one non-parameter-state notification");
            check(probe.observed == after && !probe.wrongThread,
                  "synchronous host callback can save committed state on message thread");
            check(probe.parameterChanges == 0 && parameters(*host) == originalParameters,
                  "tuning action does not manufacture parameter edits or change tuning mode");
            const auto current = host->getPresetMetadata();
            check(current.name == metadata.name && current.factoryIndex == metadata.factoryIndex,
                  "session tuning action preserves preset identity");
        };
        auto unchanged = [&](const std::function<bool()>& action, bool success)
        {
            const auto before = saved(*host);
            probe.reset();
            check(action() == success, "unchanged action reports its expected result");
            check(saved(*host) == before && probe.notifications == 0 && probe.parameterChanges == 0,
                  "unchanged or rejected user action does not dirty the session");
        };
        unchanged([&] { return page.loadKeyboardMappingText(mapping432).isEmpty(); }, false);
        unchanged([&] { page.clearTuning(); return true; }, true);
        changed("SCL", [&] { return page.loadScalaText(scale).isEmpty(); });
        check(probe.hasScale && probe.description == "Notification twelve-tone", "notification observes loaded scale metadata");
        const auto beforeIdenticalLoad = host->scalaTuningRevision();
        unchanged([&] { return page.loadScalaText(scale).isEmpty(); }, true);
        unchanged([&] { return page.loadKeyboardMappingText(mapping432, beforeIdenticalLoad).isEmpty(); }, false);
        changed("Comment-only SCL", [&] { return page.loadScalaText(scale + "! saved comment\n").isEmpty(); });
        changed("Restore SCL text", [&] { return page.loadScalaText(scale).isEmpty(); });
        changed("KBM", [&] { return page.loadKeyboardMappingText(mapping432).isEmpty(); });
        check(tuning(probe.observed).getProperty("scl").toString() == scale
              && tuning(probe.observed).getProperty("kbm").toString() == mapping432,
              "host save callback receives exact loaded source texts");
        unchanged([&] { return page.loadKeyboardMappingText(mapping432).isEmpty(); }, true);
        changed("Default mapping", [&] { return page.loadKeyboardMappingText({}).isEmpty(); });
        unchanged([&] { return page.loadKeyboardMappingText({}).isEmpty(); }, true);
        changed("SCL and KBM", [&] { return page.loadScalaText(scale, mapping444).isEmpty(); });
        unchanged([&] { return page.loadScalaText(scale, mapping444).isEmpty(); }, true);
        const auto staleRevision = host->scalaTuningRevision();
        changed("Replace SCL", [&] { return page.loadScalaText(replacement).isEmpty(); });
        check(tuning(saved(*host)).getProperty("kbm").toString().isEmpty(), "full scale replacement retains default mapping semantics");
        unchanged([&] { return page.loadKeyboardMappingText(mapping432, staleRevision).isEmpty(); }, false);
        const auto withScale = saved(*host);
        changed("CLEAR", [&] { page.clearTuning(); return true; });
        check(!probe.hasScale && probe.description.isEmpty() && !tuning(probe.observed).isValid(),
              "clear notification observes complete source removal");
        unchanged([&] { page.clearTuning(); return true; }, true);
        const auto withoutScale = saved(*host);
        for (const auto* state : { &withScale, &withoutScale })
        {
            probe.reset();
            host->setStateInformation(state->getData(), static_cast<int>(state->getSize()));
            check(saved(*host) == *state && probe.dirty == 0, "valid host session restoration does not dirty itself");
        }
        probe.reset(); timers();
        check(probe.notifications == 0, "timer refresh does not duplicate tuning notifications");
        host->prepareToPlay(48000, 32);
        probe.reset();
        juce::AudioBuffer<float> audio(2, 32); juce::MidiBuffer midi;
        host->processBlock(audio, midi);
        check(probe.dirty == 0, "audio application does not emit user-state notifications");
    }

    // Every modified page method must survive synchronous page destruction.
    for (int action : { 0, 1, 2 })
    {
        auto host = std::make_unique<Cs80AudioProcessor>(root.getFile());
        if (action != 0) check(host->loadScalaTuning(scale).isEmpty(), "destruction fixture loads a valid scale");
        auto page = std::make_unique<Page>(*host);
        auto* target = page.get();
        Probe probe(*host);
        probe.onChange = [&] { page.reset(); };
        bool ok = true;
        if (action == 0) ok = target->loadScalaText(scale).isEmpty();
        if (action == 1) ok = target->loadKeyboardMappingText(mapping432).isEmpty();
        if (action == 2) target->clearTuning();
        check(ok && page == nullptr && probe.dirty == 1, "host listener can destroy page during each action notification");
        check(probe.observed == saved(*host), "page destruction preserves complete committed state");
    }
    std::cout << "Tuning state notifications: " << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
