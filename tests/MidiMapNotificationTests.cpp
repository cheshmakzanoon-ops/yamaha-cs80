#include "plugin/PluginProcessor.h"
#include <functional>
#include <iostream>
#include <thread>

namespace {
int checks = 0, failures = 0;

void check(bool ok, const char* message)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::cerr << "FAIL " << message << '\n';
    }
}

void flush()
{
    juce::Thread::sleep(130);
    juce::Timer::callPendingTimersSynchronously();
}

juce::MemoryBlock save(Cs80AudioProcessor& p)
{
    juce::MemoryBlock b;
    p.getStateInformation(b);
    return b;
}

struct Listener : juce::AudioProcessorListener
{
    Cs80AudioProcessor& host;
    int dirty = 0, other = 0;
    bool wrongThread = false;
    juce::MemoryBlock observed;
    std::function<void()> callback;

    explicit Listener(Cs80AudioProcessor& p) : host(p) { p.addListener(this); }
    ~Listener() { host.removeListener(this); }

    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}

    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails& d) override
    {
        dirty += d.nonParameterStateChanged;
        other += d.programChanged || d.parameterInfoChanged || d.latencyChanged;
        wrongThread |= !juce::MessageManager::getInstance()->isThisTheMessageThread();
        observed = save(host);
        if (callback)
            callback();
    }

    void reset()
    {
        dirty = other = 0;
        wrongThread = false;
        observed.reset();
    }

    void expect(int n)
    {
        check(dirty == n, "exact coalesced dirty count");
        check(other == 0 && !wrongThread, "message-thread dirty-only notification");
        if (n)
            check(observed == save(host), "host sees final saved mapping");
    }
};
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile temp("-midi-map");
    Cs80AudioProcessor p(temp.getFile());
    p.prepareToPlay(48000, 64);
    flush();
    Listener l(p);
    auto& m = p.getMidiLearn();
    int target = p.hostParameterIndex(cs80::P_masterTune);
    int other = p.hostParameterIndex(cs80::P_masterSustain);

    auto action = [&](auto f, int n)
    {
        l.reset();
        f();
        check(l.dirty == 0, "host not called inside mutation");
        flush();
        l.expect(n);
    };

    action([&] { m.armLearn(target); }, 0);
    action([&] { m.cancelLearn(); }, 0);
    action([&] { m.assign(20, target); }, 1);
    action([&] { m.assign(20, target); }, 0);
    action([&] { m.assign(21, target); }, 1);
    check(m.parameterFor(20) == -1 && m.parameterFor(21) == target, "moving mapping");
    action([&] { m.assign(21, other); }, 1);
    check(m.parameterFor(21) == other, "displaced mapping");
    action([&]
    {
        m.clearController(20);
        m.clearParameter(-1);
        m.assign(0, target);
    }, 0);
    action([&] { m.clearParameter(other); }, 1);
    action([&] { m.clearParameter(other); }, 0);
    action([&]
    {
        m.assign(20, target);
        m.assign(21, other);
        m.clearController(20);
    }, 1);
    action([&] { m.clearAll(); }, 1);
    action([&]
    {
        m.armLearn(target);
        m.clearAll();
    }, 0);

    // Learning the same parameter value still changes serialized mapping.
    p.getParameters()[target]->setValueNotifyingHost(64.f / 127.f);
    flush();
    action([&]
    {
        m.armLearn(target);
        juce::AudioBuffer<float> b(2, 64);
        b.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 22, 64), 0);
        std::thread audio([&] { p.processBlock(b, midi); });
        audio.join();
    }, 1);
    action([&]
    {
        m.armLearn(target);
        juce::AudioBuffer<float> b(2, 64);
        b.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 22, 64), 0);
        std::thread audio([&] { p.processBlock(b, midi); });
        audio.join();
    }, 0);

    const auto state = save(p);
    action([&]
    {
        m.clearAll();
        p.setStateInformation(state.getData(), int(state.getSize()));
    }, 0);
    check(m.parameterFor(22) == target, "restore replaces map silently");
    action([&] { p.setStateInformation(state.getData(), int(state.getSize())); }, 0);

    // Accepted restore cancels pending mutations; subsequent edits notify again.
    action([&] { m.clearController(22); }, 1);

    // Reentrant edit is delivered on a later timer, not lost or recursively called.
    l.reset();
    bool once = false;
    l.callback = [&]
    {
        if (!once)
        {
            once = true;
            m.assign(24, other);
        }
    };
    m.assign(23, target);
    flush();
    check(l.dirty == 1, "first coalesced notification");
    l.callback = {};
    l.reset();
    flush();
    l.expect(1);

    // Pure blocks learning but explicit forgetting remains a serialized user edit.
    p.setParamValue(cs80::P_parityMode, 1);
    flush();
    action([&]
    {
        m.armLearn(target);
        juce::AudioBuffer<float> b(2, 64);
        b.clear();
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 25, 64), 0);
        std::thread audio([&] { p.processBlock(b, midi); });
        audio.join();
    }, 0);
    check(m.parameterFor(25) == -1, "PURE no new map");
    action([&] { m.clearAll(); }, 1);

    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
