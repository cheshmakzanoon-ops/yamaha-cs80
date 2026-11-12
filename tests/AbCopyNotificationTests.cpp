#include "plugin/PluginProcessor.h"
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
void flush() { juce::Thread::sleep(130); juce::Timer::callPendingTimersSynchronously(); }
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
        observed = save(host); // Reentrant host serialization must see completed copy.
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
    juce::TemporaryFile temp("-ab-copy-notification");
    Cs80AudioProcessor p(temp.getFile());
    flush();
    Listener listener(p);
    const int active = p.getAbSlot(), inactive = 1 - active;
    auto action = [&](auto fn, int expected)
    {
        listener.reset();
        fn();
        check(listener.dirty == 0, "no synchronous host dirty callback inside copy");
        flush();
        listener.expect(expected);
        check(p.getAbSlot() == active, "copy leaves active slot unchanged");
    };

    action([&] { p.abCopyCurrentTo(inactive); }, 1);
    check(p.isAbSlotStored(inactive), "inactive copy stored");
    const auto first = save(p);
    action([&] { p.abCopyCurrentTo(inactive); }, 0);
    check(save(p) == first, "identical inactive copy leaves saved state unchanged");
    action([&] { p.abCopyCurrentTo(active); }, 0);
    check(save(p) == first, "active-slot copy is not serialized");
    action([&] { p.abCopyCurrentTo(-1); p.abCopyCurrentTo(2); }, 0);
    check(save(p) == first, "invalid destinations leave saved state unchanged");

    p.setParamValue(cs80::P_masterTune, 0.2f);
    flush();
    const float sounding = p.getParamValue(cs80::P_masterTune);
    const auto beforeCopy = save(p);
    action([&] { p.abCopyCurrentTo(inactive); }, 1);
    check(p.getParamValue(cs80::P_masterTune) == sounding, "copy does not change sounding parameter");
    check(save(p) != beforeCopy, "changed sound copy changes serialized inactive slot");
    const auto changed = save(p);
    action([&] { p.abCopyCurrentTo(active); }, 0);
    check(save(p) == changed, "changed active snapshot remains absent from saved state");

    action([&]
    {
        p.setParamValue(cs80::P_masterTune, 0.3f);
        p.abCopyCurrentTo(inactive);
        p.setParamValue(cs80::P_masterTune, 0.4f);
        p.abCopyCurrentTo(inactive);
    }, 1);
    check(std::abs(p.getParamValue(cs80::P_masterTune) - 0.4f) < 1.0e-5f, "coalesced copies preserve latest sound");

    p.setParamValue(cs80::P_masterTune, 0.5f);
    flush();
    action([&]
    {
        Cs80AudioProcessor::ScopedUserEdit edit(p.getUndoableState(), "Copy test");
        p.abCopyCurrentTo(inactive);
    }, 1);
    action([&]
    {
        Cs80AudioProcessor::ScopedUserEdit edit(p.getUndoableState(), "Identical copy test");
        p.abCopyCurrentTo(inactive);
    }, 0);

    // A modern-only change is serialized in the inactive snapshot too.
    auto modern = p.getModernLayer().getConfig();
    modern.msegs[0].sync = !modern.msegs[0].sync;
    p.getModernLayer().setConfig(modern);
    flush();
    const auto beforeModernCopy = save(p);
    action([&] { p.abCopyCurrentTo(inactive); }, 1);
    check(p.getModernLayer().getConfig() == modern, "copy preserves sounding modern config");
    check(save(p) != beforeModernCopy, "modern-only copy changes inactive saved state");
    action([&] { p.abCopyCurrentTo(inactive); }, 0);

    // A host listener may copy again after serializing. That new change must
    // be deferred to a later timer turn, never recursively delivered or lost.
    p.setParamValue(cs80::P_masterTune, 0.6f);
    flush();
    listener.reset();
    bool once = false;
    listener.callback = [&]
    {
        if (!once)
        {
            once = true;
            p.setParamValue(cs80::P_masterTune, 0.7f);
            p.abCopyCurrentTo(inactive);
        }
    };
    p.abCopyCurrentTo(inactive);
    check(listener.dirty == 0, "reentrant case begins deferred");
    flush();
    check(listener.dirty == 1, "reentrant copy not delivered recursively");
    listener.callback = {};
    listener.reset();
    flush();
    listener.expect(1);
    check(p.getAbSlot() == active, "reentrant copy leaves active slot unchanged");

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
