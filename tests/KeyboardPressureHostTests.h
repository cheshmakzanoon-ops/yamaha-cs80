#pragma once
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <chrono>
#include <cstdio>
#include <typeinfo>

namespace cs80::tests
{
namespace pressure_detail
{
inline float pressure(Cs80AudioProcessor& host, int note, int channel = 1, bool heldOnly = true)
{
    for (int i = 0; i < 8; ++i)
    {
        const auto v = host.getEngine().getVoiceInfo(i);
        if (v.active && (!heldOnly || v.held) && v.note == note && v.channel == channel) return v.pressure;
    }
    return -1.0f;
}
inline juce::MidiKeyboardComponent* keyboard(juce::Component& root)
{
    if (auto* result = dynamic_cast<juce::MidiKeyboardComponent*>(&root)) return result;
    for (auto* child : root.getChildren()) if (auto* result = keyboard(*child)) return result;
    return nullptr;
}
}

// Exercise the real processor handoff while KAS allocation is deferred. A
// zero-frame callback publishes events without advancing the scan clock; each
// 128-frame callback then exceeds the nominal 1 ms scan at 48 kHz. No sleeps,
// direct Engine note calls or synthetic pressure ownership are needed.
template <class Check>
void testKeyboardScannerReleaseOwnershipHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-pressure-scanner-host");
    for (bool pure : { false, true })
    {
        Cs80AudioProcessor host(fixture.getFile());
        host.setEngineMode(Cs80AudioProcessor::EngineMode::circuit);
        host.setParamValue(P_parityMode, pure ? 1.0f : 0.0f);
        host.setParamValue(P_kasScanTiming, 1.0f);
        host.setParamValue(P_oversampling, 0.0f);
        host.setParamValue(P_arpOn, 0.0f);
        host.setParamValue(P_voiceMode, 0.0f);
        host.setParamValue(P_tuningMode, 0.0f);
        host.setParamValue(P_mpeOn, 0.0f);
        host.setParamValue(P_analogNoise, 0.0f);
        host.setParamValue(P_masterSustain, 0.0f);
        host.setParamValue(P_sustainMode, 0.0f);
        host.setParamValue(chParam(0, CP_vR), 0.5f);
        host.setParamValue(chParam(1, CP_vR), 0.5f);
        host.prepareToPlay(48000.0, 128);
        host.serviceKeyboardUI();
        juce::MidiBuffer midi;
        auto tick = [&](int frames = 128)
        {
            juce::AudioBuffer<float> audio(2, frames);
            audio.clear();
            host.processBlock(audio, midi);
            midi.clear();
        };
        auto require = [&](bool condition, const char* message)
        {
            const auto label = juce::String(pure ? "PURE scanner: " : "Source scanner: ") + message;
            check(condition, label.toRawUTF8());
        };
        auto strike = [&](int note)
        { return host.captureKeyboardGesture(1, note, [&] { host.getKeyboardState().noteOn(1, note, 0.8f); }); };
        auto release = [&](int note) { host.getKeyboardState().noteOff(1, note, 0.0f); };
        auto hostOn = [&](int note, int channel = 1)
        { midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 100), 0); };
        auto hostOff = [&](int note, int channel = 1)
        { midi.addEvent(juce::MidiMessage::noteOff(channel, note), 0); };
        auto held = [&](int note = -1, int channel = -1)
        {
            int count = 0;
            for (int i = 0; i < 8; ++i)
            {
                const auto voice = host.getEngine().getVoiceInfo(i);
                count += voice.active && voice.held && (note < 0 || voice.note == note)
                    && (channel < 0 || voice.channel == channel);
            }
            return count;
        };
        auto zeroPressure = [&]
        {
            for (int i = 0; i < 8; ++i)
            {
                const auto voice = host.getEngine().getVoiceInfo(i);
                if (voice.active && voice.pressure != 0.0f) return false;
            }
            return true;
        };
        auto reset = [&]
        {
            host.setParamValue(P_kasScanTiming, 1.0f);
            host.requestPanicFromUI(); tick(0); host.serviceKeyboardUI();
        };
        tick(); // Settle the selected policy before any captured UI gesture.
        host.serviceKeyboardUI();

        for (int frames : { 0, 128 })
        {
            reset();
            const auto token = strike(60); release(60);
            tick(frames); tick();
            require(token && held() == 0,
                    "same-batch UI onset/off leaves no held key after the next scan");
        }
        reset();
        const auto deferred = strike(60);
        host.setKeyPressureFromUI(deferred, 0.8f); tick(0);
        require(deferred && held() == 0, "zero-frame dispatch queues rather than allocates a UI onset");
        tick();
        require(held(60, 1) == 1 && pressure_detail::pressure(host, 60) == 0.0f,
                "scanned UI onset sounds without claiming immediate pressure ownership");
        host.setKeyPressureFromUI(deferred, 0.9f); tick();
        require(pressure_detail::pressure(host, 60) == 0.0f,
                "a later pressure event cannot turn a deferred token into an allocation proof");
        release(60); tick(0); tick();
        require(held() == 0, "cross-block UI release reaches its scanned voice");

        // A ninth UI key can wait, but must neither release nor pressure an
        // already-held host note of the same pitch when the UI key is lifted.
        reset();
        for (int note = 60; note < 68; ++note) hostOn(note);
        tick();
        const auto refused = strike(60); host.setKeyPressureFromUI(refused, 0.9f); tick(0); tick();
        require(refused && held() == 8 && zeroPressure(), "refused UI onset cannot pressure eight host-held voices");
        release(60); tick(0); tick();
        require(held() == 8 && held(60, 1) == 1,
                "refused UI release cancels only its waiting key, not the host voice");
        hostOff(61); tick();
        require(held() == 7 && held(60, 1) == 1,
                "cancelled waiting UI key cannot reappear when a circuit becomes free");

        reset(); hostOn(60, 2); tick();
        const auto alias = strike(60); host.setKeyPressureFromUI(alias, 0.7f); tick();
        release(60); tick();
        require(alias && held(60, 1) == 0 && held(60, 2) == 1 && zeroPressure(),
                "UI release preserves the equal-pitch host voice on another channel");

        // Existing newest-onset semantics leave the older UI allocation alone
        // after a successful same-pitch host onset. Its stale UI off must not
        // release the newer host allocation; two host offs clean up both keys.
        reset();
        const auto superseded = strike(60); tick(0);
        hostOn(60); tick(0); tick();
        require(superseded && held(60, 1) == 2, "queued host takeover obtains a second same-pitch circuit");
        release(60); host.setKeyPressureFromUI(superseded, 0.9f); tick(0); tick();
        require(held(60, 1) == 2 && zeroPressure(), "stale UI release and pressure preserve a newer host onset");
        hostOff(60); hostOff(60); tick();
        require(held() == 0, "ordinary host releases retain the existing newest-onset behavior");

        // A refused host onset is a waiting key, not a takeover of the UI
        // circuit. Cancelling that host key must retain the UI release bit.
        reset();
        for (int note = 60; note < 68; ++note) strike(note);
        tick(0); tick(); hostOn(60); tick(0); tick();
        hostOff(60); tick(0); tick();
        require(held() == 8 && held(60, 1) == 1,
                "host off cancels its refused waiting onset without releasing the UI circuit");
        release(60); tick(0); tick();
        require(held() == 7 && held(60, 1) == 0,
                "cancelling a refused host key does not revoke the earlier UI release");

        reset();
        for (int note = 60; note < 68; ++note) strike(note);
        tick(0); tick(); hostOn(60); tick(0);
        release(60); tick(0); tick();
        bool reconsideredHost = false;
        for (int i = 0; i < 8; ++i)
        {
            const auto voice = host.getEngine().getVoiceInfo(i);
            reconsideredHost |= voice.active && voice.held && voice.note == 60
                && voice.channel == 1 && voice.velocity == 0.0f;
        }
        require(held() == 8 && held(60, 1) == 1 && reconsideredHost,
                "UI release frees its circuit for a still-waiting host key with zero initial touch");
        hostOff(60); tick();
        require(held() == 7 && held(60, 1) == 0, "reconsidered host key still follows ordinary host release");

        reset();
        const auto old = strike(60); release(60);
        const auto fresh = strike(60);
        host.setKeyPressureFromUI(old, 0.9f); host.setKeyPressureFromUI(fresh, 0.4f);
        tick(0); tick();
        require(old && fresh && !(old == fresh) && held(60, 1) == 1 && zeroPressure(),
                "queued UI release/restrike keeps only the fresh held onset without false pressure claims");
        release(60); tick();
        require(held() == 0, "queued retrigger retains its own eventual UI release");

        // Scan disable must drain an older onset before its UI off, including
        // an off/restrike pair captured while that old scan is still pending.
        for (bool retrigger : { false, true })
        {
            reset(); strike(60); tick(0);
            host.setParamValue(P_kasScanTiming, 0.0f);
            release(60);
            Cs80AudioProcessor::KeyPressureToken next;
            if (retrigger) next = strike(60);
            tick(0); tick();
            require(held(60, 1) == (retrigger ? 1 : 0) && (!retrigger || next),
                    "scan disable preserves pending UI onset/off/restrike order");
            if (retrigger) { release(60); tick(); }
            require(held() == 0, "no UI key stays held after the disabled scan drains");
        }
        reset(); strike(60); tick();
        host.setParamValue(P_kasScanTiming, 0.0f); release(60); tick();
        require(held() == 0, "already-sounding scanned UI voice releases after scan disable");
        const auto immediate = strike(62); host.setKeyPressureFromUI(immediate, 0.4f); tick();
        require(immediate && std::abs(pressure_detail::pressure(host, 62) - 0.4f) < 1e-6f,
                "scan-disabled immediate allocation retains existing pressure ownership");
        release(62); tick();

        // Released UI pressure origins outlive their current gesture. Reuse
        // that exact circuit through the scanner, then allocate a fresh UI key
        // elsewhere: a saved old origin must not disguise the host alias.
        host.setParamValue(P_kasScanTiming, 0.0f);
        host.prepareToPlay(48000.0, 128); host.serviceKeyboardUI();
        const auto slotOrigin = strike(60);
        host.setKeyPressureFromUI(slotOrigin, 0.2f); tick(0);
        const auto seededSlot = host.getEngine().getVoiceInfo(0);
        require(slotOrigin && seededSlot.active && seededSlot.held && seededSlot.note == 60
                    && std::abs(seededSlot.pressure - 0.2f) < 1e-6f,
                "immediate UI onset establishes pressure ownership in physical slot zero");
        release(60); tick(0);
        bool advancedSlots = true;
        for (int note = 61; note < 68; ++note)
        {
            hostOn(note); hostOff(note); tick(0);
            const auto voice = host.getEngine().getVoiceInfo(note - 60);
            advancedSlots &= voice.active && !voice.held && voice.note == note;
        }
        require(advancedSlots && held() == 0,
                "seven host onset/off pairs advance the allocator back to the old UI slot");
        host.setParamValue(P_kasScanTiming, 1.0f); hostOn(60); tick(0); tick();
        const auto reusedSlot = host.getEngine().getVoiceInfo(0);
        require(held() == 1 && reusedSlot.active && reusedSlot.held && reusedSlot.note == 60
                    && reusedSlot.channel == 1 && reusedSlot.pressure == 0.0f,
                "deferred host onset reuses the released UI slot with zero pressure");
        host.setParamValue(P_kasScanTiming, 0.0f);
        const auto freshAlias = strike(60);
        const bool aliasPressureAccepted = host.setKeyPressureFromUI(freshAlias, 0.9f);
        tick(0);
        const auto aliasSlot = host.getEngine().getVoiceInfo(1);
        require(freshAlias && !(slotOrigin == freshAlias) && aliasPressureAccepted
                    && held(60, 1) == 2 && aliasSlot.active && aliasSlot.held && aliasSlot.note == 60
                    && zeroPressure(),
                "scan toggles cannot reuse a stale slot origin to pressure the host alias");
        release(60); tick(0);
        require(held() == 1 && host.getEngine().getVoiceInfo(0).held && zeroPressure(),
                "fresh UI alias still releases without touching the reused host slot");
        hostOff(60); tick(0);
        require(held() == 0, "host alias releases normally after the stale-origin pressure check");

        // Fill only the 64-event scanner, not the 256-event UI mailbox. The
        // UI overflow event flushes a HOST onset into the before/after snapshot,
        // so that snapshot must not accidentally claim the host's pressure.
        reset(); hostOn(60);
        for (int i = 1; i < kas::Scanner::kCapacity; ++i) hostOff(127, 16);
        tick(0);
        const auto overflowing = strike(60); host.setKeyPressureFromUI(overflowing, 0.9f); tick(0);
        require(overflowing && held(60, 1) == 2 && zeroPressure(),
                "UI scanner overflow cannot claim a host voice allocated by the flush");
        release(60); tick();
        require(held(60, 1) == 1 && zeroPressure(), "overflowed UI voice releases without touching the host voice");
        hostOff(60); tick();
        require(held() == 0, "host voice preserved through UI overflow still releases normally");

        // Conversely, a host overflow flush can allocate an earlier UI onset
        // while the host itself is refused. That is not a host takeover.
        reset();
        for (int note = 60; note < 68; ++note) strike(note);
        tick(0);
        for (int i = 8; i < kas::Scanner::kCapacity; ++i) hostOff(127, 16);
        hostOn(60); tick(0);
        require(held() == 8, "host scanner overflow flushes the eight earlier UI onsets");
        hostOff(60); tick();
        require(held() == 8, "overflow-refused host cancellation leaves the UI allocations held");
        release(60); tick();
        require(held() == 7 && held(60, 1) == 0,
                "host overflow snapshot cannot revoke an earlier UI release obligation");

        // Every lifecycle reset must discard pending scanner events, sounding
        // UI ownership and refused waiting keys. The queued old off must not
        // reach a fresh host note after the UI acknowledges the new epoch.
        for (int lifecycle = 0; lifecycle < 3; ++lifecycle)
            for (int state = 0; state < 3; ++state)
            {
                reset();
                if (state == 2)
                {
                    for (int note = 48; note < 56; ++note) hostOn(note);
                    tick();
                }
                const auto stale = strike(60); tick(0);
                if (state != 0) tick();
                require(stale && held(60, 1) == (state == 1 ? 1 : 0)
                            && held() == (state == 2 ? 8 : state == 1 ? 1 : 0),
                        "lifecycle fixture reaches its queued, sounding or waiting UI state");
                release(60);
                if (lifecycle == 0) host.prepareToPlay(48000.0, 128);
                if (lifecycle == 1) { host.releaseResources(); host.prepareToPlay(48000.0, 128); }
                if (lifecycle == 2) { host.requestPanicFromUI(); tick(0); }
                host.serviceKeyboardUI();
                require(!host.setKeyPressureFromUI(stale, 0.9f), "lifecycle reset rejects the old UI pressure token");
                hostOn(60); tick(); tick();
                require(held() == 1 && held(60, 1) == 1 && zeroPressure(),
                        "reset scanner/waiting/UI state cannot revive or release a fresh host note");
                const auto next = strike(62); tick(); release(62); tick();
                require(next && held() == 1 && held(60, 1) == 1,
                        "fresh post-reset UI gesture releases while the host note remains held");
                hostOff(60); tick();
                require(held() == 0, "no old queued or waiting UI key reappears after the fresh host release");
            }
    }
}

// Real processor dispatch, including zero-frame callbacks; no synthetic direct
// pressure-to-host mailbox. All storage paths are isolated unused temp fixtures.
template <class Check>
void testKeyboardPressureOwnershipHost(Check check)
{
    juce::TemporaryFile fixture("-cs80-pressure-host");
    for (int frames : { 0, 32 })
    {
        Cs80AudioProcessor host(fixture.getFile());
        // Plan 1.7 (ws2): fresh instances default to the circuit engine; this fixture
        // exercises the legacy-start contract, so it selects Legacy (compat) explicitly.
        host.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);
        host.setParamValue(P_analogNoise, 0.0f);
        host.setParamValue(P_mpeOn, 0.0f);
        host.setParamValue(P_masterSustain, 0.0f);
        host.setParamValue(chParam(0, CP_vR), 0.5f);
        host.setParamValue(chParam(1, CP_vR), 0.5f);
        host.prepareToPlay(48000.0, 32);
        host.serviceKeyboardUI();
        juce::AudioBuffer<float> audio(2, frames);
        juce::MidiBuffer midi;
        auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
        auto strike = [&](int note)
        { return host.captureKeyboardGesture(1, note, [&] { host.getKeyboardState().noteOn(1, note, 0.8f); }); };
        auto hostOn = [&](int note, int channel = 1)
        { midi.addEvent(juce::MidiMessage::noteOn(channel, note, (juce::uint8) 100), 0); };
        auto reset = [&]
        {
            host.requestPanicFromUI(); tick(); host.serviceKeyboardUI();
        };
        const auto old = strike(60);
        host.setKeyPressureFromUI(old, 0.2f);
        host.getKeyboardState().noteOff(1, 60, 0.0f);
        const auto fresh = strike(60);
        host.setKeyPressureFromUI(old, 0.9f);
        tick();
        check(old && fresh && !(old == fresh) && pressure_detail::pressure(host, 60) == 0.0f,
              "same-epoch release/restrike rejects old gesture pressure after the fresh onset");
        host.setKeyPressureFromUI(fresh, 0.35f); tick();
        check(std::abs(pressure_detail::pressure(host, 60) - 0.35f) < 1e-6f,
              "fresh same-pitch UI gesture remains usable beside attributed UI release tails");
        reset();
        const auto zeroVelocity = host.captureKeyboardGesture(1, 60, [&]
        { host.getKeyboardState().noteOn(1, 60, 0.001f); });
        tick();
        check(!zeroVelocity && pressure_detail::pressure(host, 60) < 0.0f,
              "JUCE velocity quantization to note-off never supplies a pressure onset token");

        for (int lifecycle = 0; lifecycle < 3; ++lifecycle)
        {
            reset(); const auto token = strike(60); tick();
            if (lifecycle == 0) host.prepareToPlay(48000.0, 32);
            if (lifecycle == 1) { host.releaseResources(); host.prepareToPlay(48000.0, 32); }
            if (lifecycle == 2) { host.requestPanicFromUI(); tick(); }
            host.serviceKeyboardUI();
            hostOn(60); tick();
            check(!host.setKeyPressureFromUI(token, 0.95f), "late pressure token is rejected after lifecycle reset and UI acknowledgment");
            tick();
            check(pressure_detail::pressure(host, 60) == 0.0f, "old UI gesture cannot pressure a fresh same-pitch host voice");
        }
        for (int mode : { P_sourcePerformance, P_parityMode })
        {
            reset();
            const auto token = strike(60); host.setKeyPressureFromUI(token, 0.8f);
            host.setParamValue(mode, 1.0f); hostOn(64); tick();
            check(pressure_detail::pressure(host, 60) < 0.0f && pressure_detail::pressure(host, 64) == 0.0f,
                  "effective mode transition discards captured old UI onset/pressure but accepts fresh host MIDI");
            host.serviceKeyboardUI();
            check(!host.setKeyPressureFromUI(token, 1.0f), "effective mode transition revokes the old token after acknowledgment");
            const auto next = strike(67); host.setKeyPressureFromUI(next, 0.4f); tick();
            check(std::abs(pressure_detail::pressure(host, 67) - 0.4f) < 1e-6f,
                  "fresh post-mode-ack UI gesture retains pressure control");
            host.setParamValue(mode, 0.0f); tick(); host.serviceKeyboardUI();
        }

        reset();
        for (int note = 60; note < 68; ++note) hostOn(note);
        tick();
        const auto refused = strike(60); host.setKeyPressureFromUI(refused, 0.9f); tick();
        check(refused && host.getEngine().activeVoices() == 8 && pressure_detail::pressure(host, 60) == 0.0f,
              "queue-accepted ninth onset never claims or pressures a refused host voice");
        reset();
        for (int note = 60; note < 68; ++note) strike(note);
        tick(); hostOn(60); tick(); // All eight are held: host allocation is refused.
        host.getKeyboardState().noteOff(1, 60, 0.0f); tick();
        check(pressure_detail::pressure(host, 60) < 0.0f,
              "refused ninth host onset cannot revoke the accepted UI key release and strand its voice");

        for (bool released : { false, true })
        {
            reset(); hostOn(60, 2); tick();
            if (released) { midi.addEvent(juce::MidiMessage::noteOff(2, 60), 0); tick(); }
            const auto ui = strike(60); host.setKeyPressureFromUI(ui, 0.7f); tick();
            check(pressure_detail::pressure(host, 60) == 0.0f
                      && pressure_detail::pressure(host, 60, 2, false) == 0.0f,
                  "UI pressure conservatively avoids cross-channel host pitch aliases including release tails");
            midi.addEvent(juce::MidiMessage::aftertouchChange(2, 60, 71), 0); tick();
            check(std::abs(pressure_detail::pressure(host, 60, 2, false) - 71.0f / 127.0f) < 1e-6f,
                  "host MIDI poly-aftertouch keeps its existing pitch-group semantics");
        }
        reset();
        const auto superseded = strike(60); tick(); hostOn(60); tick();
        host.setKeyPressureFromUI(superseded, 0.8f); tick();
        bool unchanged = true;
        for (int i = 0; i < 8; ++i)
        { const auto v = host.getEngine().getVoiceInfo(i); if (v.active && v.note == 60) unchanged &= v.pressure == 0.0f; }
        check(unchanged, "later host onset revokes the earlier UI gesture before pressure dispatch");
        for (int cc : { 120, 123 })
        {
            reset(); const auto channelMode = strike(60); tick();
            midi.addEvent(juce::MidiMessage::controllerEvent(1, cc, 0), 0); tick();
            hostOn(60); tick(); host.setKeyPressureFromUI(channelMode, 0.9f); tick();
            check(pressure_detail::pressure(host, 60) == 0.0f,
                  "host CC120/123 revokes UI pressure before a subsequent same-pitch host strike");
        }

        reset();
        const auto beforeArp = strike(60); tick();
        host.setParamValue(P_arpOn, 1.0f); tick();
        const auto duringArp = strike(62); tick();
        host.setKeyPressureFromUI(beforeArp, 0.9f); host.setKeyPressureFromUI(duringArp, 0.9f); tick();
        host.setParamValue(P_arpOn, 0.0f); tick();
        host.setKeyPressureFromUI(duringArp, 0.9f); tick();
        bool arpSafe = true;
        for (int i = 0; i < 8; ++i)
        { const auto v = host.getEngine().getVoiceInfo(i); if (v.active) arpSafe &= v.pressure == 0.0f; }
        check(arpSafe, "effective arp and both arp edges revoke direct-pointer pressure attribution");

        reset(); hostOn(72, 2); tick();
        const auto flood = strike(60); tick();
        bool overflow = false;
        for (int i = 0; i < 300; ++i) overflow |= !host.setKeyPressureFromUI(flood, 0.5f);
        tick();
        check(overflow && pressure_detail::pressure(host, 60) < 0.0f
                  && pressure_detail::pressure(host, 72, 2) == 0.0f,
              "bounded pressure flood uses UI-only recovery and preserves unrelated host-held notes");
        host.serviceKeyboardUI();
        check(!host.setKeyPressureFromUI(flood, 0.9f), "overflow acknowledgment does not revive an old gesture");
    }
    testKeyboardScannerReleaseOwnershipHost(check);
}

template <class Check>
void testKeyboardPointerPressure(Check check)
{
    juce::TemporaryFile fixture("-cs80-pressure-pointer");
    Cs80AudioProcessor host(fixture.getFile());
    host.prepareToPlay(48000.0, 32);
    // ws8's first-run tour opens by itself once the editor is in a real window and
    // covers the keybed (macOS CI found cs80::ui::TourOverlay at the key point
    // whenever window activation took longer than the tour's first timer tick).
    // This fixture is about pointer pressure, so the player has seen the tour.
    host.getEditorState().tourSeen = true;
    Cs80AudioProcessorEditor editor(host);
    // Keep the supported minimum layout, not the screenshot's 1600x1000.
    // On smaller desktops the upper panel may extend off-screen deliberately:
    // this fixture exposes its tested keybed, not a claim that the whole UI fits.
    editor.setSize(1120, 700);
    const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
    check(display != nullptr, "pointer-pressure fixture has a real display for native pointer coordinates");
    const auto usable = display != nullptr ? display->userArea : juce::Rectangle<int>();
    editor.setTopLeftPosition(usable.getX() + (usable.getWidth() - editor.getWidth()) / 2,
                             usable.getBottom() - editor.getHeight());
    // JUCE keyboard hit testing calls reallyContains(), which recurses through
    // Component::contains to the top-level native peer. A detached component
    // cannot receive a real pointer note, even when its bounds look correct.
    // A command-line fixture is not the active application (macOS CI reports
    // foreground=0 even after asking), so another process's window could stay
    // ordered above it. The fixture tests pointer pressure through the real hit
    // test, not ordering against other applications, so its window floats above
    // normal windows; it also asks to be activated before ordering the editor front.
    editor.setAlwaysOnTop(true);
    editor.addToDesktop(0);
    editor.setVisible(true);
    juce::Process::makeForegroundProcess();
    editor.toFront(true);
    auto* keyboard = pressure_detail::keyboard(editor);
    check(keyboard != nullptr, "actual editor contains the pointer-pressure keyboard");
    if (keyboard == nullptr) return;
    check(editor.getPeer() != nullptr && editor.isShowing() && keyboard->isShowing(),
          "pointer-pressure fixture owns a visible native editor peer for JUCE hit testing");
    auto positionFor = [&](int note, float depth)
    {
        const auto key = keyboard->getRectangleForKey(note);
        return juce::Point<float>(key.getCentreX(), keyboard->getHeight() * depth);
    };
    auto nativePointsReady = [&]
    {
        for (int note : { 60, 62, 64 })
            for (float depth : { 0.65f, 0.85f, 0.95f })
            {
                const auto position = positionFor(note, depth);
                if (!usable.contains(keyboard->localPointToGlobal(position.roundToInt()))
                    || keyboard->getNoteAndVelocityAtPosition(position).note != note) return false;
            }
        return true;
    };
    // macOS additionally checks the actual frontmost NSWindow at each screen
    // point. Let native realization/ordering settle through the normal loop;
    // do not bypass hit testing or inject a pretend note into the keyboard.
    // Window-server activation and ordering are asynchronous on macOS: wait up to
    // 10 s (formerly 2 s), re-ordering the editor front each half second; the
    // frontmost and hit-test assertions themselves are unchanged.
    const auto settleStart = std::chrono::steady_clock::now();
    const auto deadline = settleStart + std::chrono::seconds(10);
    auto nextFront = settleStart + std::chrono::milliseconds(500);
    while (!nativePointsReady() && std::chrono::steady_clock::now() < deadline)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil(20);
        if (std::chrono::steady_clock::now() >= nextFront)
        {
            editor.toFront(true);
            nextFront += std::chrono::milliseconds(500);
        }
    }
    const auto settleMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - settleStart).count();
    check(nativePointsReady(), "pointer-pressure native window is frontmost at all tested on-screen key points");
    const auto firstScreenPoint = keyboard->localPointToGlobal(positionFor(60, 0.65f).roundToInt());
    // Diagnostic evidence for the hit test: every desktop window JUCE knows of, and
    // which component the editor itself finds at the first key point.
    {
        auto& desktop = juce::Desktop::getInstance();
        for (int i = 0; i < desktop.getNumComponents(); ++i)
            if (auto* window = desktop.getComponent(i))
                std::printf("Pointer fixture desktop window %d: %s '%s' %d,%d %dx%d visible=%d\n", i,
                            typeid(*window).name(), window->getName().toRawUTF8(), window->getX(), window->getY(),
                            window->getWidth(), window->getHeight(), (int) window->isVisible());
        const auto local = editor.getLocalPoint(nullptr, firstScreenPoint);
        auto* found = editor.getComponentAt(local);
        std::printf("Pointer fixture component at key60: %s '%s' (keyboard=%d)\n",
                    found != nullptr ? typeid(*found).name() : "none",
                    found != nullptr ? found->getName().toRawUTF8() : "",
                    (int) (found == keyboard || (found != nullptr && keyboard->isParentOf(found))));
        auto* peer = editor.getPeer();
        const auto peerPoint = peer != nullptr ? peer->globalToLocal(firstScreenPoint.toFloat()).roundToInt() : juce::Point<int>();
        std::printf("Pointer fixture peer contains key60=%d foreground=%d settle=%lld ms\n",
                    (int) (peer != nullptr && peer->contains(peerPoint, true)),
                    (int) juce::Process::isForegroundProcess(), (long long) settleMs);
    }
    std::printf("Pointer fixture: editor=%d,%d %dx%d; usable=%d,%d %dx%d; key60 screen=%d,%d hit=%d\n",
                editor.getX(), editor.getY(), editor.getWidth(), editor.getHeight(),
                usable.getX(), usable.getY(), usable.getWidth(), usable.getHeight(),
                firstScreenPoint.x, firstScreenPoint.y,
                keyboard->getNoteAndVelocityAtPosition(positionFor(60, 0.65f)).note);
    juce::AudioBuffer<float> audio(2, 0);
    juce::MidiBuffer midi;
    auto tick = [&] { host.processBlock(audio, midi); midi.clear(); };
    auto event = [&](int note, float depth, bool down)
    {
        const auto position = positionFor(note, depth);
        check(usable.contains(keyboard->localPointToGlobal(position.roundToInt())),
              "pointer-pressure fixture event lies inside the actual usable display");
        check(keyboard->getNoteAndVelocityAtPosition(position).note == note,
              "pointer-pressure fixture point resolves to the intended key through real JUCE hit testing");
        return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position,
            juce::ModifierKeys(down ? juce::ModifierKeys::leftButtonModifier : 0),
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, keyboard, keyboard, juce::Time::getCurrentTime(),
            position, juce::Time::getCurrentTime(), 1, true);
    };
    keyboard->mouseDown(event(60, 0.65f, true));
    keyboard->mouseDrag(event(60, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 60) - 0.5f) < 1e-5f,
          "real JUCE mouse-down onset capture preserves first same-block pointer pressure");
    keyboard->mouseDrag(event(62, 0.65f, true));
    keyboard->mouseDrag(event(62, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 62) - 0.5f) < 1e-5f
              && pressure_detail::pressure(host, 60) < 0.0f,
          "real cross-key drag captures the new JUCE onset rather than reusing the old token");
    keyboard->mouseExit(event(62, 0.85f, true));
    keyboard->mouseEnter(event(62, 0.65f, true));
    keyboard->mouseDrag(event(62, 0.65f, true));
    keyboard->mouseDrag(event(62, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 62) - 0.5f) < 1e-5f,
          "real same-pitch exit/re-entry captures a fresh onset after JUCE releases the previous key");
    host.requestPanicFromUI(); tick(); host.serviceKeyboardUI();
    midi.addEvent(juce::MidiMessage::noteOn(1, 62, (juce::uint8) 100), 0); tick();
    keyboard->mouseDrag(event(62, 0.95f, true)); tick();
    check(pressure_detail::pressure(host, 62) == 0.0f,
          "ongoing same-key pointer drag cannot adopt a fresh host note after reset and acknowledgment");
    keyboard->mouseUp(event(62, 0.95f, false)); tick();
    keyboard->mouseDown(event(64, 0.65f, true));
    keyboard->mouseDrag(event(64, 0.85f, true)); tick();
    check(std::abs(pressure_detail::pressure(host, 64) - 0.5f) < 1e-5f,
          "lifting and making a genuinely fresh pointer strike restores pressure after reset");
    keyboard->mouseUp(event(64, 0.85f, false)); tick();
}
}
