#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace
{
// Schema validation must not read the live ValueTree handle while another
// state-loading thread may replace it. This identifier never changes.
const juce::Identifier stateType { "CS80_PARAMS" };

// JUCE 8.0.6 AudioParameterBool stores an unsnapped host float internally,
// while APVTS caches its snapped 0/1 value. A same-bucket state restore may
// therefore skip the setter. Publish the existing boolean semantics through
// getValue too, so host, APVTS, DSP and saved state cannot disagree.
// Use only for actual Bool parameters; float switches/choices stay untouched.
// IDs, order, defaults, version hints and the >= 0.5 boolean meaning are unchanged.
class CanonicalBoolParameter final : public juce::AudioParameterBool
{
public:
    using juce::AudioParameterBool::AudioParameterBool;
    float getValue() const override { return get() ? 1.0f : 0.0f; }
};

juce::File makeUserPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("CS80Reference")
        .getChildFile("Presets");
}
} // namespace

Cs80AudioProcessor::Cs80AudioProcessor(const juce::File& presetDirectory)
    : AudioProcessor(BusesProperties()
          .withInput("External Modulation", juce::AudioChannelSet::mono(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      presetDirectoryOverride(presetDirectory),
      apvts(*this, nullptr, stateType, createParameterLayout())
{
    keyboardState.addListener(this);
    for (int i = 0; i < cs80::P_numParams; ++i)
        parameterValues[(size_t) i] = apvts.getRawParameterValue(parameterId(i));
    for (int channel = 0; channel < 2; ++channel)
        hardwareSources[(size_t) channel] = apvts.getRawParameterValue(hardwareSourceParameterId(channel));
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (isMemoryControl(local))
                hardwareMemories[(size_t) bank][(size_t) local] = apvts.getRawParameterValue(memoryParameterId(bank, local));
    for (int channel = 0; channel < 2; ++channel)
        for (int tone = 0; tone < 11; ++tone)
        {
            cs80::ParamValues values;
            cs80::applyHardwareTone(values, channel, tone);
            for (int local = 0; local < cs80::CP_count; ++local)
                hardwareFactoryValues[(size_t) channel][(size_t) tone][(size_t) local] = values[cs80::chParam(channel, local)];
        }
    // Fresh instances: factory program 0 with the original SUB panel, on the
    // circuit engine (plan 1.7).
    applyFreshInstancePatch();
    cached = currentValues();
    engine.setParams(cached);
    engine.prepare(48000.0);
    // Every oversampling factor's decimator delays 63 host samples, so report it
    // as soon as the processor exists. prepareToPlay and any later quality or
    // offline change re-report it for hosts that query only after a change.
    setLatencySamples(engine.getLatencySamples());
    presetName = (getFactoryPresets().empty() ? juce::String("Init Patch")
                 : juce::String(getFactoryPresets()[0].name)) + " / Original SUB";
    currentProgram = 0;
}

Cs80AudioProcessor::~Cs80AudioProcessor()
{
    keyboardState.removeListener(this);
}

void Cs80AudioProcessor::handleNoteOn(juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    if (!std::isfinite(velocity))
    {
        cs80::KeyboardGestureCapture::attempted(this, channel, note, {});
        return;
    }
    // Match the stock indirect MIDI route, including 7-bit velocity quantisation
    // and note-on velocity zero being interpreted as note-off by handleMidi.
    const auto message = juce::MidiMessage::noteOn(channel, note, velocity);
    KeyPressureToken token;
    keyboardEvents.push(channel, note, message.getFloatVelocity(), message.isNoteOn(),
                        juce::Time::getMillisecondCounter(), &token);
    cs80::KeyboardGestureCapture::attempted(this, channel, note, token);
}

void Cs80AudioProcessor::handleNoteOff(juce::MidiKeyboardState*, int channel, int note, float velocity)
{
    keyboardEvents.push(channel, note, velocity, false, juce::Time::getMillisecondCounter());
}

void Cs80AudioProcessor::serviceKeyboardUI()
{
    jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
    if (keyboardEvents.needsAcknowledgment())
    {
        const auto epoch = keyboardEvents.requestedEpoch();
        keyboardState.reset();
        keyboardEvents.acknowledge(epoch);
    }
    // false still clears JUCE's indirect queue, without constructing injected MIDI.
    keyboardDiscardBuffer.clear();
    keyboardState.processNextMidiBuffer(keyboardDiscardBuffer, 0, 1, false);
}

void Cs80AudioProcessor::resetKeyboardHandoff() noexcept
{
    keyboardEvents.requestReset();
    uiHeldMasks.fill(0);
    keyPressureOwnership.clear();
    for (auto& mask : hostKeyMasks) mask.store(0, std::memory_order_relaxed);
}

void Cs80AudioProcessor::observeHostMidi(const juce::MidiMessage& message) noexcept
{
    const unsigned bit = 1u << (message.getChannel() - 1);
    if (message.isNoteOn())
    {
        hostKeyMasks[(size_t) message.getNoteNumber()].fetch_or(bit, std::memory_order_relaxed);
        keyPressureOwnership.hostNote(message.getNoteNumber());
    }
    else if (message.isNoteOff())
    {
        const auto note = (size_t) message.getNoteNumber();
        hostKeyMasks[note].fetch_and(~bit, std::memory_order_relaxed);
        uiHeldMasks[note] &= ~bit;
        keyPressureOwnership.hostNote((int) note);
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        keyPressureOwnership.clear(); // Conservative: channel modes revoke UI pressure attribution.
        for (size_t note = 0; note < uiHeldMasks.size(); ++note)
        {
            hostKeyMasks[note].fetch_and(~bit, std::memory_order_relaxed);
            uiHeldMasks[note] &= ~bit;
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout Cs80AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout](int index, int versionHint = 1) {
        const auto& d = cs80::paramDef(index);
        juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
        juce::AudioParameterFloatAttributes attributes;
        if (d.unit != nullptr && *d.unit != 0)
            attributes = attributes.withLabel(d.unit);
        const auto id = parameterId(index);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, versionHint }, juce::String(d.name), range, d.def, attributes));
    };

    for (int i = 0; i < cs80::P_numGlobals; ++i)
        add(i);
    for (int ch = 0; ch < 2; ++ch)
        for (int p = 0; p < cs80::CP_count; ++p)
            add(cs80::chParam(ch, p));

    // Append only: the original 122 host parameter indexes/IDs remain stable.
    for (int channel = 0; channel < 2; ++channel)
    {
        juce::StringArray choices;
        for (int tone = 0; tone < 11; ++tone)
            choices.add(juce::String(cs80::hardwareToneName(tone, channel)));
        choices.add("Memory " + juce::String(channel + 1));
        choices.add("Memory " + juce::String(channel + 3));
        choices.add("Panel");
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { hardwareSourceParameterId(channel), 2 },
            channel == 0 ? "Channel I source" : "Channel II source", choices, 13));
    }
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
        {
            if (!isMemoryControl(local)) continue;
            const auto& d = cs80::kChannelDefs[local];
            const auto id = juce::ParameterID { memoryParameterId(bank, local), 2 };
            const auto name = "Memory " + juce::String(bank + 1) + " " + d.group + " " + d.name;
            if (local == cs80::CP_saw || local == cs80::CP_pulse)
                layout.add(std::make_unique<CanonicalBoolParameter>(id, name, d.def >= 0.5f));
            else
            {
                auto attributes = juce::AudioParameterFloatAttributes().withLabel(d.unit);
                layout.add(std::make_unique<juce::AudioParameterFloat>(id, name,
                    juce::NormalisableRange<float>(d.min, d.max, 0.0f, d.skew), d.def, attributes));
            }
        }
    // Core extension index122 is NOT host index122: keep all 228 existing
    // host indexes stable, and use a later AU version hint to preserve AU order.
    add(cs80::P_subAtVcf, 3); // shipped host228 / AUhint3 stays fixed
    layout.add(std::make_unique<CanonicalBoolParameter>(
        juce::ParameterID { parameterId(cs80::P_subOriginalMode), 4 },
        "Original SUB mode", true));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { parameterId(cs80::P_subOriginalFunction), 4 },
        "Original SUB function",
        juce::StringArray { "Sine", "Saw down", "Saw up", "Square", "Noise", "EXT" }, 0));
    add(cs80::P_subOriginalSpeed, 4);
    add(cs80::P_subExtLevel, 4);

    // Keyboard contours, footswitch/assigners, EXP-WAH and output switch. AU
    // version hint 5 keeps them after every previously shipped parameter.
    for (int index = cs80::P_kbdBrillianceLow; index <= cs80::P_kbdLevelHigh; ++index)
        add(index, 5);
    for (int index : { cs80::P_footswitchPlugged, cs80::P_footswitchPressed,
                       cs80::P_sustainAssign, cs80::P_glideAssign })
        layout.add(std::make_unique<CanonicalBoolParameter>(
            juce::ParameterID { parameterId(index), 5 },
            juce::String(cs80::paramDef(index).name), cs80::paramDef(index).def >= 0.5f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { parameterId(cs80::P_footMode), 5 },
        "Foot controller mode", juce::StringArray { "EXP", "EXP-WAH" }, 0));
    layout.add(std::make_unique<CanonicalBoolParameter>(
        juce::ParameterID { parameterId(cs80::P_outputLevel), 5 },
        "Output HIGH (on) / LOW (off)", true));
    // PURE CS-80 (plan 1.7): the W0 isolation contract, opt-in, appended after
    // all 243 shipped host IDs. ID, index and AU hint are unchanged.
    layout.add(std::make_unique<CanonicalBoolParameter>(
        juce::ParameterID { parameterId(cs80::P_parityMode), 6 }, "PURE CS-80", false));
    // ENGINE (plan 1.7): on = Circuit, off = Legacy (compat); a sound-model
    // choice, not a change to isolation. Host244/AU hint 7 unchanged. Fresh
    // instances use the circuit engine; the CORE default stays Legacy so
    // default-ParamValues renders, the legacy factory bank and old sessions keep
    // their sound (normaliseRestoredState restores missing values by schema).
    layout.add(std::make_unique<CanonicalBoolParameter>(
        juce::ParameterID { parameterId(cs80::P_sourcePerformance), 7 }, "Engine", true,
        juce::AudioParameterBoolAttributes()
            .withStringFromValueFunction([](bool circuit, int) { return juce::String(circuit ? "Circuit" : "Legacy (compat)"); })
            .withValueFromStringFunction([](const juce::String& text)
            {
                const auto t = text.trim();
                return t.startsWithIgnoreCase("c") || t.equalsIgnoreCase("on") || t == "1";
            })));

    // [ws1-voice] append this workstream's host parameters below this line, AU version hint 8.

    // [ws2-engine] append this workstream's host parameters below this line, AU version hint 9.
    // Numerical quality, a session setting rather than part of a sound.
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { parameterId(cs80::P_oversampling), 9 }, "Oversampling quality",
        juce::StringArray { "Eco (2x)", "Standard", "HQ (8x)" },
        (int) cs80::paramDef(cs80::P_oversampling).def));

    // [ws3-keyboard] append this workstream's host parameters below this line, AU version hint 10.

    // [ws4-circuits] append this workstream's host parameters below this line, AU version hint 11.

    // [ws5-modern] append this workstream's host parameters below this line, AU version hint 12.

    // [ws6-library] append this workstream's host parameters below this line, AU version hint 13.

    // [ws7-panel] append this workstream's host parameters below this line, AU version hint 14.

    // [ws8-studio] append this workstream's host parameters below this line, AU version hint 15.

    return layout;
}

bool Cs80AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.inputBuses.size() != 1 || layouts.outputBuses.size() != 1) return false;
    const auto input = layouts.getMainInputChannelSet();
    const auto output = layouts.getMainOutputChannelSet();
    return (input.isDisabled() || input == juce::AudioChannelSet::mono())
        && (output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo());
}

void Cs80AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    outputMeter.reset();
    // Offline bounces render the circuit engine at HQ (Oversampling.h).
    engine.setNonRealtime(isNonRealtime());
    engine.prepare(sampleRate);
    readParameters();
    const auto previousPolicy = engine.getRenderPolicy();
    engine.setControlFrame({ cached, cachedHardwareSources });
    if (engine.getRenderPolicy() != previousPolicy)
    {
        engine.setExpression(expressionValue.load(std::memory_order_relaxed));
        // The unconditional lifecycle handoff reset below also revokes gestures.
    }
    // prepare() resets the engine's expression to full; restore the pedal position.
    pendingExpression.store(expressionValue.load(std::memory_order_relaxed), std::memory_order_relaxed);
    setLatencySamples(engine.getLatencySamples());
    interleaved.assign((size_t) juce::jmax(4096, samplesPerBlock) * 2, 0.0f);
    externalScratch.assign(interleaved.size() / 2, 0.0f);
    externalInputEnabled.store(getTotalNumInputChannels() == 1, std::memory_order_relaxed);
    resetKeyboardHandoff();
}

void Cs80AudioProcessor::releaseResources()
{
    outputMeter.reset();
    engine.reset();
    resetKeyboardHandoff();
}

void Cs80AudioProcessor::readParameters()
{
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        if (auto* p = parameterValues[(size_t) i])
            cached.v[(size_t) i] = p->load(std::memory_order_relaxed);
    }
    for (int channel = 0; channel < 2; ++channel)
    {
        const int source = getHardwareSource(channel);
        cachedHardwareSources[(size_t) channel] = source;
        if (source == 13) continue;
        for (int local = 0; local < cs80::CP_count; ++local)
        {
            // Footage is outside the original programmer; pan/filter bypass
            // are software extensions and remain live independent controls.
            if (local == cs80::CP_feet || local == cs80::CP_pan
                || local == cs80::CP_hpfOn || local == cs80::CP_lpfOn) continue;
            cached.v[(size_t) cs80::chParam(channel, local)] = source < 11
                ? hardwareFactoryValues[(size_t) channel][(size_t) source][(size_t) local]
                : getMemoryValue(channel + (source == 12 ? 2 : 0), local);
        }
    }
}

cs80::ParamValues Cs80AudioProcessor::currentValues() const
{
    cs80::ParamValues values;
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        const auto id = parameterId(i);
        if (auto* p = apvts.getRawParameterValue(id))
            values.v[(size_t) i] = p->load();
        else
            values.v[(size_t) i] = cs80::paramDef(i).def;
    }
    return values;
}

float Cs80AudioProcessor::getParamValue(int index) const
{
    if (index < 0 || index >= cs80::P_numParams)
        return 0.0f;
    if (auto* p = apvts.getRawParameterValue(parameterId(index)))
        return p->load();
    return cs80::paramDef(index).def;
}

void Cs80AudioProcessor::setParamValue(int index, float physicalValue)
{
    if (index < 0 || index >= cs80::P_numParams)
        return;
    if (auto* p = apvts.getParameter(parameterId(index)))
    {
        const float normalised = p->convertTo0to1(physicalValue);
        p->setValueNotifyingHost(normalised);

    }
}

void Cs80AudioProcessor::applyParameterValues(const cs80::ParamValues& values)
{
    applyValues(values);
}

bool Cs80AudioProcessor::isPhysicalSetupParameter(int index) noexcept
{
    // The oversampling quality is a session setting, not part of a sound.
    return index == cs80::P_footswitchPlugged || index == cs80::P_footswitchPressed
        || index == cs80::P_footMode || index == cs80::P_outputLevel
        || index == cs80::P_oversampling;
}

void Cs80AudioProcessor::applyValues(const cs80::ParamValues& values)
{
    resetHardwareSources();
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        // A preset recalls the sound, not what is plugged into the jack, the
        // pedal contact, the EXP/WAH selector or the rear output switch.
        // Isolation is a session policy, not a sound: presets cannot disengage it.
        if (isPhysicalSetupParameter(i) || i == cs80::P_parityMode)
            continue;
        if (auto* p = apvts.getParameter(parameterId(i)))
            p->setValueNotifyingHost(p->convertTo0to1(values[i]));
    }
    // The next audio block snapshots APVTS atomics; never mutate DSP from the GUI.
}

Cs80AudioProcessor::PresetMetadata Cs80AudioProcessor::getPresetMetadata() const
{
    const juce::ScopedLock lock(presetMetadataLock);
    return { presetName, currentProgram.load(std::memory_order_relaxed) };
}

void Cs80AudioProcessor::setPresetMetadata(const juce::String& name, int index)
{
    const juce::ScopedLock lock(presetMetadataLock);
    presetName = name;
    currentProgram.store(index, std::memory_order_relaxed);
}

void Cs80AudioProcessor::setPresetName(const juce::String& name)
{
    {
        const juce::ScopedLock lock(presetMetadataLock);
        presetName = name;
    }
    // Host callbacks may reenter metadata getters; never hold the lock here.
    updateHostDisplay();
}

void Cs80AudioProcessor::setParameterGesture(int index, float physicalValue)
{
    if (auto* p = apvts.getParameter(parameterId(index)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(physicalValue));
        p->endChangeGesture();
    }
}

void Cs80AudioProcessor::setEngineMode(EngineMode mode)
{
    setParameterGesture(cs80::P_sourcePerformance, mode == EngineMode::circuit ? 1.0f : 0.0f);
}

Cs80AudioProcessor::EngineMode Cs80AudioProcessor::getEngineMode() const
{
    return getParamValue(cs80::P_sourcePerformance) >= 0.5f ? EngineMode::circuit : EngineMode::legacy;
}

Cs80AudioProcessor::EngineMode Cs80AudioProcessor::getEffectiveEngineMode() const
{
    return cs80::usesSourceModels(cs80::renderPolicy(getParamValue(cs80::P_parityMode),
                                                     getParamValue(cs80::P_sourcePerformance)))
        ? EngineMode::circuit : EngineMode::legacy;
}

void Cs80AudioProcessor::setPure(bool pure)
{
    setParameterGesture(cs80::P_parityMode, pure ? 1.0f : 0.0f);
}

bool Cs80AudioProcessor::isPure() const
{
    return getParamValue(cs80::P_parityMode) >= 0.5f;
}

void Cs80AudioProcessor::applyFreshInstancePatch()
{
    if (!getFactoryPresets().empty())
        applyValues(getFactoryPresets()[0].values);
    // The original six-function SUB panel and the circuit engine (plan 1.7).
    // Factory bank0 itself remains unchanged/legacy when explicitly selected.
    setParamValue(cs80::P_subOriginalMode, 1.0f);
    setParamValue(cs80::P_sourcePerformance, 1.0f);
}

void Cs80AudioProcessor::loadInitPatch()
{
    const auto& bank = getFactoryPresets();
    int index = 0;
    for (int i = 0; i < (int) bank.size(); ++i)
        if (juce::String(bank[(size_t) i].name).equalsIgnoreCase("Init Patch"))
        {
            index = i;
            break;
        }
    loadFactoryPreset(index);
    // A new preset starts like a fresh instance: the original SUB panel on the
    // circuit engine (plan 1.7). The bank program itself stays legacy.
    setParamValue(cs80::P_subOriginalMode, 1.0f);
    setParamValue(cs80::P_sourcePerformance, 1.0f);
    if (!bank.empty())
    {
        setPresetMetadata(juce::String(bank[(size_t) index].name) + " / Original SUB", index);
        updateHostDisplay();
    }
}

void Cs80AudioProcessor::loadFactoryPreset(int index)
{
    const auto& bank = getFactoryPresets();
    if (bank.empty())
        return;
    index = juce::jlimit(0, (int) bank.size() - 1, index);
    applyValues(bank[(size_t) index].values);
    setPresetMetadata(juce::String(bank[(size_t) index].name), index);
    updateHostDisplay();
}

void Cs80AudioProcessor::handleMidi(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        engine.noteOn(message.getNoteNumber(), message.getFloatVelocity(), message.getChannel());
    }
    else if (message.isNoteOff())
    {
        engine.noteOff(message.getNoteNumber(), message.getChannel());
    }
    else if (message.isAftertouch())
    {
        engine.polyPressure(message.getNoteNumber(),
                            message.getAfterTouchValue() / 127.0f,
                            message.getChannel());
    }
    else if (message.isChannelPressure())
    {
        engine.channelPressure(message.getChannel(), message.getChannelPressureValue() / 127.0f);
    }
    else if (message.isPitchWheel())
    {
        const float bend = (message.getPitchWheelValue() - 8192) / 8192.0f;
        engine.pitchBend(message.getChannel(), bend);
    }
    else if (message.isAllSoundOff())
    {
        // CC120/123 are controllers too: dispatch before the generic branch.
        engine.allNotesOff(false, message.getChannel());
    }
    else if (message.isAllNotesOff())
    {
        engine.allNotesOff(true, message.getChannel());
    }
    else if (message.isController())
    {
        if (message.getControllerNumber() == 11 && !engine.isParityMode())
            expressionValue.store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
        engine.controlChange(message.getControllerNumber(), message.getControllerValue() / 127.0f, message.getChannel());
    }
}

void Cs80AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    if (! midi.isEmpty())
        hostMidiEvents.fetch_add((unsigned) midi.getNumEvents(), std::memory_order_relaxed);   // lock-free UI activity telemetry
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), getTotalNumOutputChannels());
    const bool inputEnabled = getTotalNumInputChannels() == 1;
    externalInputEnabled.store(inputEnabled, std::memory_order_relaxed);
    // The sole mono input aliases channel0 in JUCE's process buffer. Do not
    // clear output before copying each bounded input chunk.
    const float* input = inputEnabled && buffer.getNumChannels() > 0
                           ? buffer.getReadPointer(0) : nullptr;

    if (pendingPanic.exchange(false, std::memory_order_acq_rel))
    {
        // Audio-owned global hard stop, not MIDI CC120/123's channel semantics.
        // Retain the patch, performance controls, noise and existing FX tails.
        engine.allNotesOff(false, 0);
        resetKeyboardHandoff();
        // The message thread clears the keyboard and acknowledges this epoch.
        // Old queued UI strikes/releases cannot resurrect or claim host notes.
        // Host events below are new input after the block-boundary panic.
    }

    if (keyboardEvents.hasOverflow() && !keyboardEvents.needsAcknowledgment())
    {
        // No channel-wide panic: release only notes accepted from the UI.
        // A successful later host allocation revokes the corresponding UI bit;
        // a refused host onset must not strand the earlier accepted UI release.
        for (size_t note = 0; note < uiHeldMasks.size(); ++note)
            for (int channel = 1; channel <= 16; ++channel)
                if ((uiHeldMasks[note] & (1u << (channel - 1))) != 0)
                    engine.noteOff((int) note, channel);
        keyboardEvents.requestReset();
        uiHeldMasks.fill(0);
        keyPressureOwnership.clear();
    }
    cs80::KeyboardEventQueue<>::Batch uiEvents;
    auto uiCount = keyboardEvents.take(uiEvents);

    readParameters();
    const auto previousPolicy = engine.getRenderPolicy();
    const int previousFactor = engine.getOversamplingFactor();
    // Hosts may switch to offline rendering without a new prepareToPlay.
    engine.setNonRealtime(isNonRealtime());
    engine.setControlFrame({ cached, cachedHardwareSources });
    // A policy or oversampling-factor change resets the engine the same way.
    if (engine.getRenderPolicy() != previousPolicy || engine.getOversamplingFactor() != previousFactor)
    {
        engine.setExpression(expressionValue.load(std::memory_order_relaxed));
        // Discard the already-captured pre-transition batch too: otherwise its
        // old onset could establish ownership again after the Engine reset.
        resetKeyboardHandoff();
        uiCount = 0;
    }
    // 63 at every factor today; report any change rather than assume it.
    if (engine.getLatencySamples() != getLatencySamples())
        setLatencySamples(engine.getLatencySamples());
    const bool pressureArpOn = !engine.isParityMode() && cached[cs80::P_arpOn] >= 0.5f;
    if (pressureArpOn || pressureArpOn != pressureArpWasOn) keyPressureOwnership.clear();
    pressureArpWasOn = pressureArpOn;

    const float ribbon = pendingRibbon.exchange(noPendingControl, std::memory_order_relaxed);
    const float pitch = pendingPitch.exchange(noPendingControl, std::memory_order_relaxed);
    const float mod = pendingMod.exchange(noPendingControl, std::memory_order_relaxed);
    const float pedal = pendingExpression.exchange(noPendingControl, std::memory_order_relaxed);
    if (std::isfinite(pedal)) engine.setExpression(juce::jlimit(0.0f, 1.0f, pedal));
    if (std::isfinite(ribbon)) engine.setRibbon(juce::jlimit(-1.0f, 1.0f, ribbon));
    if (std::isfinite(pitch)) engine.pitchBend(1, juce::jlimit(-1.0f, 1.0f, pitch));
    if (std::isfinite(mod)) engine.controlChange(1, juce::jlimit(0.0f, 1.0f, mod), 1);

    // Hosts may supply a block larger than their advertised maximum. Render
    // bounded chunks rather than allocating on the real-time thread.
    auto renderSegment = [&](int start, int length)
    {
        const int capacity = std::min((int) interleaved.size() / 2, (int) externalScratch.size());
        if (capacity == 0)
        {
            for (int ch = 0; ch < numChannels; ++ch) buffer.clear(ch, start, length);
            return;
        }
        for (int offset = 0; offset < length; )
        {
            const int n = std::min(capacity, length - offset);
            if (input != nullptr)
                std::copy_n(input + start + offset, n, externalScratch.data());
            engine.render(interleaved.data(), n, input != nullptr ? externalScratch.data() : nullptr);
            for (int i = 0; i < n; ++i)
            {
                const float left = interleaved[(size_t) i * 2];
                const float right = interleaved[(size_t) i * 2 + 1];
                if (numChannels >= 2)
                {
                    buffer.setSample(0, start + offset + i, left);
                    buffer.setSample(1, start + offset + i, right);
                }
                else if (numChannels == 1)
                    buffer.setSample(0, start + offset + i, 0.5f * (left + right));
            }
            offset += n;
        }
    };

    int done = 0;
    auto advanceTo = [&](int position)
    {
        if (position > done)
        {
            renderSegment(done, position - done);
            done = position;
        }
    };
    size_t uiIndex = 0;
    auto voiceSnapshot = [&]
    {
        std::array<cs80::Engine::VoiceInfo, 8> voices;
        for (int i = 0; i < 8; ++i) voices[(size_t) i] = engine.getVoiceInfo(i);
        return voices;
    };
    auto uiPosition = [&]
    {
        return cs80::KeyboardEventQueue<>::samplePosition(uiEvents, uiCount, uiIndex, numSamples);
    };
    auto dispatchUI = [&]
    {
        advanceTo(uiPosition());
        const auto& event = uiEvents[uiIndex++];
        const unsigned bit = 1u << (event.channel - 1);
        auto& held = uiHeldMasks[(size_t) event.note];
        if (event.pressure)
        {
            if (!pressureArpOn && keyPressureOwnership.allows(event.token, voiceSnapshot()))
                engine.polyPressure(event.note, event.velocity, event.channel);
        }
        else if (event.on)
        {
            const auto before = voiceSnapshot();
            engine.noteOn(event.note, event.velocity, event.channel);
            if (pressureArpOn || keyPressureOwnership.allocated(before, voiceSnapshot(), event.token,
                                                                event.channel, event.note))
                held |= bit;
        }
        else if ((held & bit) != 0)
        {
            engine.noteOff(event.note, event.channel);
            held &= ~bit;
        }
        if (!event.on && !event.pressure) keyPressureOwnership.noteOff(event.channel, event.note);
    };
    for (const auto metadata : midi)
    {
        // Filter BEFORE getMessage(): long/system messages otherwise own heap data.
        // Only channel messages handled below are supported; program changes remain ignored.
        if (metadata.numBytes < 2 || metadata.numBytes > 3) continue;
        const auto status = metadata.data[0] & 0xf0;
        if (status < 0x80 || status >= 0xf0 || status == 0xc0) continue;
        if (metadata.numBytes != (status == 0xd0 ? 2 : 3)) continue;
        if (metadata.data[1] >= 0x80 || (metadata.numBytes == 3 && metadata.data[2] >= 0x80)) continue;
        const int pos = juce::jlimit(0, numSamples, metadata.samplePosition);
        // Existing host events win equal-sample ties, matching JUCE addEvent order.
        while (uiIndex < uiCount && uiPosition() < pos) dispatchUI();
        advanceTo(pos);
        const auto message = metadata.getMessage();
        std::array<cs80::Engine::VoiceInfo, 8> before;
        const bool allocatingHostNote = message.isNoteOn() && !pressureArpOn;
        if (allocatingHostNote) before = voiceSnapshot();
        observeHostMidi(message);
        handleMidi(message);
        if (message.isNoteOn()
            && (pressureArpOn || keyPressureOwnership.allocated(before, voiceSnapshot(), {},
                                                               message.getChannel(), message.getNoteNumber())))
            uiHeldMasks[(size_t) message.getNoteNumber()] &= ~(1u << (message.getChannel() - 1));
    }
    while (uiIndex < uiCount) dispatchUI();

    if (done < numSamples)
        renderSegment(done, numSamples - done);

    unsigned mask = 0;
    for (int i = 0; i < 8; ++i)
        if (engine.getVoiceInfo(i).active) mask |= 1u << i;
    activeVoiceMask.store(mask, std::memory_order_relaxed);
    activeVoiceCount.store(engine.activeVoices(), std::memory_order_relaxed);

    // Inspect the completed host buffer, not pre-decimator/internal stereo
    // values: mono mapping/cancellation and its arithmetic are already applied.
    // Only actual output channels count; neither input nor unused channels do.
    // No output writes, audio allocations, locks or atomic retry loops here.
    if (numSamples > 0 && numChannels > 0)
    {
        cs80::OutputMeterBlock measured;
        for (int channel = 0; channel < numChannels; ++channel)
            measured.addSamples(buffer.getReadPointer(channel), numSamples);
        outputMeter.publish(measured);
    }
}


bool Cs80AudioProcessor::isMemoryControl(int local) noexcept
{
    return local >= 0 && local < cs80::CP_count && local != cs80::CP_feet
        && local != cs80::CP_pan && local != cs80::CP_hpfOn && local != cs80::CP_lpfOn;
}

juce::String Cs80AudioProcessor::hardwareSourceParameterId(int channel)
{
    return "hardware_source_" + juce::String(channel + 1);
}

juce::String Cs80AudioProcessor::memoryParameterId(int bank, int local)
{
    if (bank < 0 || bank >= 4 || !isMemoryControl(local)) return {};
    return "memory_" + juce::String(bank + 1) + "_" + cs80::kChannelDefs[local].id;
}

int Cs80AudioProcessor::getHardwareSource(int channel) const noexcept
{
    if (channel < 0 || channel >= 2) return 13;
    const auto* p = hardwareSources[(size_t) channel];
    const float value = p != nullptr ? p->load(std::memory_order_relaxed) : 13.0f;
    return std::isfinite(value) ? juce::jlimit(0, 13, juce::roundToInt(value)) : 13;
}

void Cs80AudioProcessor::resetHardwareSources() noexcept
{
    for (int channel = 0; channel < 2; ++channel)
        if (auto* p = apvts.getParameter(hardwareSourceParameterId(channel)))
            p->setValueNotifyingHost(p->convertTo0to1(13.0f));
}

void Cs80AudioProcessor::selectHardwareSource(int channel, int source)
{
    if (channel < 0 || channel > 1 || source < 0 || source > 13) return;
    if (auto* p = apvts.getParameter(hardwareSourceParameterId(channel)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1((float) source));
        p->endChangeGesture();
    }
    juce::String label = source < 11 ? juce::String(cs80::hardwareToneName(source, channel))
        : source == 13 ? "Panel" : "Memory " + juce::String(channel + (source == 12 ? 3 : 1));
    setPresetName(juce::String(channel == 0 ? "Tone I: " : "Tone II: ") + label);
}

float Cs80AudioProcessor::getMemoryValue(int bank, int local) const noexcept
{
    if (bank < 0 || bank >= 4 || local < 0 || local >= cs80::CP_count) return 0.0f;
    const auto* p = hardwareMemories[(size_t) bank][(size_t) local];
    return p != nullptr ? p->load(std::memory_order_relaxed) : cs80::kChannelDefs[local].def;
}

void Cs80AudioProcessor::setMemoryValue(int bank, int local, float value) noexcept
{
    if (bank < 0 || bank >= 4 || !isMemoryControl(local) || !std::isfinite(value)) return;
    if (auto* p = apvts.getParameter(memoryParameterId(bank, local)))
    {
        const auto& def = cs80::kChannelDefs[local];
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(juce::jlimit(def.min, def.max, value)));
        p->endChangeGesture();
    }
}

juce::ValueTree Cs80AudioProcessor::normaliseRestoredState(const juce::ValueTree& state) const
{
    // APVTS is the sole authority. Complete a detached state before replacing
    // it: JUCE otherwise retains live parameter values for missing children.
    auto result = state.createCopy();
    auto hardware = state.getChildWithName("HARDWARE");
    // Malformed or missing schema markers are treated as the oldest schema.
    // XML attributes arrive as strings (ValueTree::fromXml), so a complete
    // decimal integer string is a valid marker too; before schema 3 the string
    // form was always read as schema 1, which no earlier rule could observe.
    const auto schemaValue = state.getProperty(stateSchemaKey, 1);
    int schema = 1;
    if (schemaValue.isInt() || schemaValue.isInt64() || schemaValue.isDouble())
        schema = (int) schemaValue;
    else if (schemaValue.isString())
    {
        const auto text = schemaValue.toString().trim();
        if (text.isNotEmpty() && text.containsOnly("0123456789") && text.length() < 9)
            schema = text.getIntValue();
    }
    schema = juce::jlimit(1, stateSchemaVersion, schema);
    const bool legacy = hardware.isValid() && (int) hardware.getProperty("version", 1) == 1;
    // ENGINE that a state of this schema means when its value is missing or
    // malformed: Legacy before the plan 1.7 default flip, Circuit after it.
    const float engineFallback = schema >= 3 ? 1.0f : 0.0f;
    auto ensure = [&](const juce::String& id, float fallback)
    {
        auto* p = apvts.getParameter(id);
        if (p == nullptr) return;
        auto child = result.getChildWithProperty("id", id);
        if (!child.isValid())
        {
            child = juce::ValueTree("PARAM");
            child.setProperty("id", id, nullptr);
            result.addChild(child, -1, nullptr);
        }
        float value = (float) child.getProperty("value", fallback);
        if (!std::isfinite(value)) value = fallback;
        if (id == parameterId(cs80::P_subOriginalMode) && value != 0.0f && value != 1.0f)
            value = 0.0f;
        if (id == parameterId(cs80::P_parityMode)
            || id == parameterId(cs80::P_sourcePerformance))
        {
            // Only complete finite numeric 0/1 values are taken literally.
            // Fractional, out-of-range, partially parsed or missing values
            // restore the writing schema's meaning (the fallback): PURE off,
            // ENGINE Legacy before schema 3 and Circuit from schema 3. A
            // missing value never inherits the live instance's policy.
            const auto encoded = child.getProperty("value", juce::var()).toString().trim();
            const auto* begin = encoded.toRawUTF8();
            char* end = nullptr;
            const auto parsed = std::strtod(begin, &end);
            const bool literal = child.hasProperty("value") && end != begin && *end == '\0'
                                 && (parsed == 0.0 || parsed == 1.0);
            value = literal ? (float) parsed : fallback;
        }
        // Round discrete choices/switches and clamp to the declared range.
        const auto& range = p->getNormalisableRange();
        value = range.snapToLegalValue(juce::jlimit(range.start, range.end, value));
        child.setProperty("value", value, nullptr);
    };
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        // Missing/invalid mode means old session, NOT the fresh-instance default.
        // Schema-1 states always predate any change of the render-policy default.
        const bool oldSessionOff = i == cs80::P_subOriginalMode;
        ensure(parameterId(i), i == cs80::P_sourcePerformance ? engineFallback
                               : oldSessionOff ? 0.0f : cs80::paramDef(i).def);
    }
    // The marker describes serialized data only; savers write the current one.
    result.removeProperty(stateSchemaKey, nullptr);
    for (int channel = 0; channel < 2; ++channel)
    {
        float value = legacy ? (float) hardware.getProperty(channel == 0 ? "sourceI" : "sourceII", 13) : 13.0f;
        if (!std::isfinite(value) || value < 0.0f || value > 13.0f) value = 13.0f;
        ensure(hardwareSourceParameterId(channel), value);
    }
    for (int bank = 0; bank < 4; ++bank)
    {
        auto memory = legacy ? hardware.getChildWithProperty("bank", bank) : juce::ValueTree();
        for (int local = 0; local < cs80::CP_count; ++local)
        {
            if (!isMemoryControl(local)) continue;
            const float def = cs80::kChannelDefs[local].def;
            float value = memory.isValid() ? (float) memory.getProperty("p" + juce::String(local), def) : def;
            if (!std::isfinite(value)) value = def;
            ensure(memoryParameterId(bank, local), value);
        }
    }
    // Do not serialize a second, potentially stale copy of automated controls.
    for (int i = result.getNumChildren(); --i >= 0;)
        if (result.getChild(i).hasType("HARDWARE"))
            result.removeChild(i, nullptr);
    return result;
}

juce::AudioProcessorEditor* Cs80AudioProcessor::createEditor()
{
    return new Cs80AudioProcessorEditor(*this);
}

int Cs80AudioProcessor::getNumPrograms()
{
    return (int) getFactoryPresets().size();
}

void Cs80AudioProcessor::setCurrentProgram(int index)
{
    loadFactoryPreset(index);
}

const juce::String Cs80AudioProcessor::getProgramName(int index)
{
    const auto& bank = getFactoryPresets();
    if (index >= 0 && index < (int) bank.size())
        return juce::String(bank[(size_t) index].name);
    return "Init Patch";
}

void Cs80AudioProcessor::changeProgramName(int, const juce::String&)
{
    // Programs are immutable factory content. User presets are files.
}

void Cs80AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    const auto metadata = getPresetMetadata();
    state.setProperty("presetName", metadata.name, nullptr);
    state.setProperty("factoryIndex", metadata.factoryIndex, nullptr);
    state.setProperty("subOscStateVersion", 1, nullptr);
    state.setProperty(stateSchemaKey, stateSchemaVersion, nullptr);
    if (auto xml = state.createXml())
    {
        auto text = xml->toString();
        destData.replaceAll(text.toRawUTF8(), text.getNumBytesAsUTF8());
    }
}

void Cs80AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;
    const auto text = juce::String::fromUTF8(static_cast<const char*>(data), sizeInBytes);
    if (auto xml = juce::XmlDocument::parse(text))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid() && tree.hasType(stateType))
        {
            apvts.replaceState(normaliseRestoredState(tree));
            setPresetMetadata(tree.getProperty("presetName", "Init Patch").toString(),
                              (int) tree.getProperty("factoryIndex", 0));
            // DSP state is updated by the next audio block, not the state-loading thread.
        }
    }
}

juce::File Cs80AudioProcessor::userPresetDirectory() const
{
    return presetDirectoryOverride == juce::File() ? makeUserPresetDirectory() : presetDirectoryOverride;
}

Cs80AudioProcessor::PresetSaveResult Cs80AudioProcessor::saveUserPreset(const juce::String& name,
                                                                      PresetOverwrite overwrite)
{
    return writeUserPreset(name, overwrite, [](const juce::XmlElement& xml, const juce::File& file)
    {
        juce::FileOutputStream stream(file);
        if (! stream.openedOk())
            return stream.getStatus();
        xml.writeTo(stream);
        stream.flush();
        return stream.getStatus();
    });
}

Cs80AudioProcessor::PresetSaveResult Cs80AudioProcessor::writeUserPreset(
    const juce::String& name, PresetOverwrite overwrite, const PresetWriter& write)
{
    const auto dir = userPresetDirectory();
    const auto metadata = getPresetMetadata();
    const auto savedName = name.isNotEmpty() ? name : metadata.name;
    auto safe = juce::File::createLegalFileName(savedName);
    if (safe.isEmpty())
        safe = "User Preset";
    const auto file = dir.getChildFile(safe + ".cs80xml");
    const auto failure = [&file](const juce::String& message)
    {
        return PresetSaveResult { PresetSaveStatus::error, file, message };
    };
    if (file.isDirectory())
        return failure("The destination is a folder, not a preset file.");
    if (file.exists() && overwrite == PresetOverwrite::no)
        return { PresetSaveStatus::alreadyExists, file, "A preset with this filename already exists." };
    const auto created = dir.createDirectory();
    if (created.failed())
        return failure("Could not create the preset folder: " + created.getErrorMessage());

    auto state = apvts.copyState();
    state.setProperty("presetName", savedName, nullptr);
    state.setProperty("factoryIndex", metadata.factoryIndex, nullptr);
    state.setProperty("subOscStateVersion", 1, nullptr);
    state.setProperty(stateSchemaKey, stateSchemaVersion, nullptr);
    const auto xml = state.createXml();
    if (xml == nullptr)
        return failure("Could not serialize the current patch.");

    // Only this new, uniquely named staging file is cleaned up by the guard.
    // Keep it beside the destination so publication never crosses filesystems.
    // This guarantees atomic visibility, not power-loss/crash durability of
    // the directory entry. Never fall back to truncating/copying the target.
    juce::TemporaryFile temporary(file,
        dir.getChildFile(".cs80-preset-" + juce::Uuid().toString() + ".tmp"));
    const auto written = write(*xml, temporary.getFile());
    if (written.failed())
        return failure("Could not write the preset: " + written.getErrorMessage());

    const auto path = [](const juce::File& f)
    {
       #if JUCE_WINDOWS
        return std::filesystem::path(f.getFullPathName().toWideCharPointer());
       #else
        return std::filesystem::path(f.getFullPathName().toRawUTF8());
       #endif
    };
    std::error_code error;
    if (overwrite == PresetOverwrite::yes)
    {
        // Unlike JUCE File's move/copy fallback, filesystem::rename does not
        // truncate an existing destination if the atomic replacement fails.
        std::filesystem::rename(path(temporary.getFile()), path(file), error);
    }
    else
    {
        // Atomic no-replace publication also handles another instance saving
        // this name after our initial existence check. Never check then rename.
        std::filesystem::create_hard_link(path(temporary.getFile()), path(file), error);
    }
    if (error)
    {
        if (overwrite == PresetOverwrite::no && error == std::errc::file_exists)
            return { PresetSaveStatus::alreadyExists, file, "A preset with this filename already exists." };
        return failure("Could not publish the preset: " + juce::String(error.message()));
    }

    setPresetName(savedName);
    return { PresetSaveStatus::saved, file, {} };
}

juce::Array<juce::File> Cs80AudioProcessor::getUserPresetFiles() const
{
    juce::Array<juce::File> files;
    auto dir = userPresetDirectory();
    for (const auto& entry : juce::RangedDirectoryIterator(dir, false, "*.cs80xml", juce::File::findFiles))
        files.add(entry.getFile());
    return files;
}

// The standalone wrapper and some hosts resolve the plugin through this
// entry point. JUCE's generated VST3 wrapper also uses it.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Cs80AudioProcessor();
}

void Cs80AudioProcessor::loadUserPreset(const juce::File& file)
{
    if (! file.existsAsFile())
        return;
    if (auto xml = juce::XmlDocument::parse(file))
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        if (tree.isValid() && tree.hasType(stateType))
        {
            auto restored = normaliseRestoredState(tree);
            // Session settings, not sounds: a user preset keeps the live values.
            for (int index : { (int) cs80::P_parityMode, (int) cs80::P_oversampling })
                restored.getChildWithProperty("id", parameterId(index))
                    .setProperty("value", getParamValue(index), nullptr);
            apvts.replaceState(restored);
            setPresetMetadata(tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString(),
                              (int) tree.getProperty("factoryIndex", 0));
            // DSP state is updated by the next audio block, not the state-loading thread.
        }
    }
}
