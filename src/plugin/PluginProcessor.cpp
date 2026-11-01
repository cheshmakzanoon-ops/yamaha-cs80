#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "core/VoiceModes.h"
#include "PluginProcessorLibrary.h"

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
      apvts(*this, undoManager, stateType, createParameterLayout())   // [ws8-studio] undo history
{
    keyboardState.addListener(this);
    for (int i = 0; i < cs80::P_numParams; ++i)
        parameterValues[(size_t) i] = apvts.getRawParameterValue(parameterId(i));
    for (int channel = 0; channel < 2; ++channel)
        hardwareSources[(size_t) channel] = apvts.getRawParameterValue(hardwareSourceParameterId(channel));
    initialiseLibrary(); // [ws6-library] program change, A/B, randomise
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
    extensionEngine = std::make_unique<cs80::Engine>();
    extensionEngine->setParams(cached);
    extensionEngine->prepare(48000.0);
    // Every oversampling factor's decimator delays 63 host samples, so report it
    // as soon as the processor exists. prepareToPlay and any later quality or
    // offline change re-report it for hosts that query only after a change.
    setLatencySamples(engine.getLatencySamples());
    presetName = (getFactoryPresets().empty() ? juce::String("Init Patch")
                 : juce::String(getFactoryPresets()[0].name)) + " / Original SUB";
    currentProgram = 0;
    apvts.syncNow();                       // [ws8-studio] the initial sound is not an undo step
    undoManager.clearUndoHistory();
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
    auto make = [](int index, int versionHint = 1) {
        const auto& d = cs80::paramDef(index);
        juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
        juce::AudioParameterFloatAttributes attributes;
        if (d.unit != nullptr && *d.unit != 0)
            attributes = attributes.withLabel(d.unit);
        return std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { parameterId(index), versionHint }, juce::String(d.name), range, d.def, attributes);
    };
    // For later workstreams' marker blocks below: one ungrouped float parameter.
    [[maybe_unused]] auto add = [&layout, &make](int index, int versionHint = 1) { layout.add(make(index, versionHint)); };
    auto bool5 = [](int index) {
        return std::make_unique<CanonicalBoolParameter>(juce::ParameterID { parameterId(index), 5 },
            juce::String(cs80::paramDef(index).name), cs80::paramDef(index).def >= 0.5f);
    };
    // Host parameter groups (plan Phase 6) from ParamDef.group. JUCE flattens a
    // group tree depth-first in insertion order, so only CONTIGUOUS runs of the
    // shipped order are grouped: host indexes, IDs, AU hints and the flat APVTS
    // state are unchanged. A ParamDef group split in the shipped order (MASTER,
    // PERFORMANCE among the globals) becomes two host groups of the same name.
    // The channel blocks are one group each: their VCO/VCA rows interleave in
    // the shipped order, so per-row subgroups would split VCO twice, VCA four times.
    using Group = juce::AudioProcessorParameterGroup;
    auto group = [](const juce::String& id, const juce::String& name) { return std::make_unique<Group>(id, name, " | "); };

    // Host 0..61: globals, one subgroup per contiguous ParamDef.group run.
    {
        auto global = group("global", "Global");
        std::unique_ptr<Group> run;
        juce::String runName;
        int runs = 0;
        for (int i = 0; i < cs80::P_numGlobals; ++i)
        {
            const juce::String name(cs80::paramDef(i).group);
            if (run == nullptr || name != runName)
            {
                if (run != nullptr) global->addChild(std::move(run));
                run = group("global_" + juce::String(++runs), name);
                runName = name;
            }
            run->addChild(make(i));
        }
        global->addChild(std::move(run));
        layout.add(std::move(global));
    }
    // Host 62..121: the two original channel blocks.
    for (int ch = 0; ch < 2; ++ch)
    {
        auto channel = group("channel_" + juce::String(ch + 1), ch == 0 ? "Channel I" : "Channel II");
        for (int p = 0; p < cs80::CP_count; ++p)
            channel->addChild(make(cs80::chParam(ch, p)));
        layout.add(std::move(channel));
    }

    // Append only: the original 122 host parameter indexes/IDs remain stable.
    // Host 122..227: tone selectors, then memories 1..4.
    {
        auto programmer = group("programmer", "Programmer");
        for (int channel = 0; channel < 2; ++channel)
        {
            juce::StringArray choices;
            for (int tone = 0; tone < 11; ++tone)
                choices.add(juce::String(cs80::hardwareToneName(tone, channel)));
            choices.add("Memory " + juce::String(channel + 1));
            choices.add("Memory " + juce::String(channel + 3));
            choices.add("Panel");
            programmer->addChild(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID { hardwareSourceParameterId(channel), 2 },
                channel == 0 ? "Channel I source" : "Channel II source", choices, 13));
        }
        for (int bank = 0; bank < 4; ++bank)
        {
            auto memory = group("memory_" + juce::String(bank + 1), "Memory " + juce::String(bank + 1));
            for (int local = 0; local < cs80::CP_count; ++local)
            {
                if (!isMemoryControl(local)) continue;
                const auto& d = cs80::kChannelDefs[local];
                const auto id = juce::ParameterID { memoryParameterId(bank, local), 2 };
                const auto name = "Memory " + juce::String(bank + 1) + " " + d.group + " " + d.name;
                if (local == cs80::CP_saw || local == cs80::CP_pulse)
                    memory->addChild(std::make_unique<CanonicalBoolParameter>(id, name, d.def >= 0.5f));
                else
                {
                    auto attributes = juce::AudioParameterFloatAttributes().withLabel(d.unit);
                    memory->addChild(std::make_unique<juce::AudioParameterFloat>(id, name,
                        juce::NormalisableRange<float>(d.min, d.max, 0.0f, d.skew), d.def, attributes));
                }
            }
            programmer->addChild(std::move(memory));
        }
        layout.add(std::move(programmer));
    }
    // Core extension index122 is NOT host index122: keep all 228 existing
    // host indexes stable, and use a later AU version hint to preserve AU order.
    {
        auto sub = group("sub_osc", cs80::paramDef(cs80::P_subAtVcf).group);
        sub->addChild(make(cs80::P_subAtVcf, 3)); // shipped host228 / AUhint3 stays fixed
        sub->addChild(std::make_unique<CanonicalBoolParameter>(
            juce::ParameterID { parameterId(cs80::P_subOriginalMode), 4 },
            "Original SUB mode", true));
        sub->addChild(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { parameterId(cs80::P_subOriginalFunction), 4 },
            "Original SUB function",
            juce::StringArray { "Sine", "Saw down", "Saw up", "Square", "Noise", "EXT" }, 0));
        sub->addChild(make(cs80::P_subOriginalSpeed, 4));
        sub->addChild(make(cs80::P_subExtLevel, 4));
        layout.add(std::move(sub));
    }

    // Keyboard contours, footswitch/assigners, EXP-WAH and output switch. AU
    // version hint 5 keeps them after every previously shipped parameter.
    {
        auto keyboard = group("keyboard", cs80::paramDef(cs80::P_kbdBrillianceLow).group);
        for (int index = cs80::P_kbdBrillianceLow; index <= cs80::P_kbdLevelHigh; ++index)
            keyboard->addChild(make(index, 5));
        layout.add(std::move(keyboard));
        auto foot = group("foot", cs80::paramDef(cs80::P_footswitchPlugged).group);
        for (int index : { cs80::P_footswitchPlugged, cs80::P_footswitchPressed,
                           cs80::P_sustainAssign, cs80::P_glideAssign })
            foot->addChild(bool5(index));
        foot->addChild(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { parameterId(cs80::P_footMode), 5 },
            "Foot controller mode", juce::StringArray { "EXP", "EXP-WAH" }, 0));
        layout.add(std::move(foot));
        auto output = group("output_switch", cs80::paramDef(cs80::P_outputLevel).group);
        output->addChild(std::make_unique<CanonicalBoolParameter>(
            juce::ParameterID { parameterId(cs80::P_outputLevel), 5 },
            "Output HIGH (on) / LOW (off)", true));
        layout.add(std::move(output));
    }
    {
        auto engine = group("engine", cs80::paramDef(cs80::P_parityMode).group);
        // PURE CS-80 (plan 1.7): the W0 isolation contract, opt-in, appended after
        // all 243 shipped host IDs. ID, index and AU hint are unchanged.
        engine->addChild(std::make_unique<CanonicalBoolParameter>(
            juce::ParameterID { parameterId(cs80::P_parityMode), 6 }, "PURE CS-80", false));
        // ENGINE (plan 1.7): on = Circuit, off = Legacy (compat); a sound-model
        // choice, not a change to isolation. Host244/AU hint 7 unchanged. Fresh
        // instances use the circuit engine; the CORE default stays Legacy so
        // default-ParamValues renders, the legacy factory bank and old sessions keep
        // their sound (normaliseRestoredState restores missing values by schema).
        engine->addChild(std::make_unique<CanonicalBoolParameter>(
            juce::ParameterID { parameterId(cs80::P_sourcePerformance), 7 }, "Engine", true,
            juce::AudioParameterBoolAttributes()
                .withStringFromValueFunction([](bool circuit, int) { return juce::String(circuit ? "Circuit" : "Legacy (compat)"); })
                .withValueFromStringFunction([](const juce::String& text)
                {
                    const auto t = text.trim();
                    return t.startsWithIgnoreCase("c") || t.equalsIgnoreCase("on") || t == "1";
                })));
        layout.add(std::move(engine));
    }

    // [ws1-voice] append this workstream's host parameters below this line, AU version hint 8.
    {
        auto voice = group("ws1_voice", "Voice variation");
        // Plan 2.5: the circuit engine's drift law; the instrument's condition, not a sound.
        voice->addChild(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { parameterId(cs80::P_driftModel), 8 }, "Drift model",
            juce::StringArray { "Thermal (declared 1/f)", "Worn unit (22 ct sine)" }, 0));
        layout.add(std::move(voice));
    }

    // [ws2-engine] append this workstream's host parameters below this line, AU version hint 9.
    {
        auto io = group("ws2_engine_io", "Engine I/O");
        // Numerical quality, a session setting rather than part of a sound.
        io->addChild(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { parameterId(cs80::P_oversampling), 9 }, "Oversampling quality",
            juce::StringArray { "Eco (2x)", "Standard", "HQ (8x)" },
            (int) cs80::paramDef(cs80::P_oversampling).def));
        // Which physical output is heard (Phase 3); physical setup, like HIGH/LOW.
        io->addChild(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { parameterId(cs80::P_outputJack), 9 }, "Listening output",
            juce::StringArray { "Left/Right jacks", "General (mono) jack", "Headphones" }, 0));
        layout.add(std::move(io));
    }

    // [ws3-keyboard] append this workstream's host parameters below this line, AU version hint 10.
    // Plain floats (switches/choices read with >= 0.5 / rounding in the core),
    // like the original channel switches; all defaults leave renders unchanged.
    for (int index = cs80::P_kasScanTiming; index <= cs80::P_unisonDetune; ++index)
        add(index, 10);

    // [ws4-circuits] append this workstream's host parameters below this line, AU version hint 11.

    // [ws5-modern] append this workstream's host parameters below this line, AU version hint 12.
    // Modern modulation layer and FX (src/core/ModernLayer.h, FxModels.h). Float parameters
    // throughout (as the existing LFO/FX selectors are), with named values for the
    // discrete ones; MSEG shapes and matrix routing are session state, not parameters.
    {
        auto modern = [&layout](int index, std::function<juce::String(float, int)> text = {})
        {
            const auto& d = cs80::paramDef(index);
            auto attributes = juce::AudioParameterFloatAttributes();
            if (d.unit != nullptr && *d.unit != 0)
                attributes = attributes.withLabel(d.unit);
            if (text)
                attributes = attributes.withStringFromValueFunction(std::move(text));
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID { parameterId(index), 12 }, juce::String(d.name),
                juce::NormalisableRange<float>(d.min, d.max, 0.0f, d.skew), d.def, attributes));
        };
        auto named = [](const char* (*nameOf)(int)) {
            return [nameOf](float value, int) { return juce::String(nameOf((int) std::lround(value))); };
        };
        for (int index = cs80::P_lfo1Wave; index <= cs80::P_lfo3Wave; ++index)
            modern(index, named(cs80::lfoWaveName));
        for (int index = cs80::P_mseg1Rate; index <= cs80::P_mseg3Rate; ++index)
            modern(index, [](float octaves, int) { return juce::String(std::exp2(octaves), 3) + "x"; });
        for (int index = cs80::P_mod1Amount; index <= cs80::P_modKeyCurve; ++index)
            modern(index);
        for (int slot = 0; slot < 3; ++slot)
        {
            const int base = cs80::P_fx1Model + slot * (cs80::P_fx2Model - cs80::P_fx1Model);
            modern(base, named(cs80::fx::modelName));
            modern(base + 1);
            modern(base + 2);
            modern(base + 3, [](float value, int) { return juce::String(1 << juce::jlimit(0, 3, (int) std::lround(value))) + "x"; });
        }
        modern(cs80::P_fxGainCompensation, [](float value, int) { return juce::String(value >= 0.5f ? "On" : "Off"); });
    }

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
    extensionEngine->prepare(sampleRate);
    extensionActive = false;
    highResolutionVelocity.reset();
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
    extensionInterleaved.assign(interleaved.size(), 0.0f);
    externalScratch.assign(interleaved.size() / 2, 0.0f);
    externalInputEnabled.store(getTotalNumInputChannels() == 1, std::memory_order_relaxed);
    resetKeyboardHandoff();
}

void Cs80AudioProcessor::releaseResources()
{
    outputMeter.reset();
    engine.reset();
    extensionEngine->reset();
    extensionActive = false;
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
    // ws3: controller curves, the ribbon CC, the KAS scan option and the tuning
    // source describe the player's setup, not the sound, and survive recall.
    if (index == cs80::P_kasScanTiming || index == cs80::P_velocityCurve || index == cs80::P_pressureCurve
        || index == cs80::P_pressureThreshold || index == cs80::P_ribbonCcEnable || index == cs80::P_ribbonCc
        || index == cs80::P_tuningMode)
        return true;
    // The oversampling quality and listening output are session settings, not
    // part of a sound.
    return index == cs80::P_footswitchPlugged || index == cs80::P_footswitchPressed
        || index == cs80::P_footMode || index == cs80::P_outputLevel
        || index == cs80::P_oversampling || index == cs80::P_outputJack
        || index == cs80::P_driftModel;   // plan 2.5: the unit's condition survives program recall
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
    // [ws6-library] A MIDI Program Change publishes only its index (audio thread, no lock).
    const int fromMidi = library != nullptr ? library->midiProgram.load(std::memory_order_acquire) : -1;
    const auto& programs = getFactoryPresets();
    if (fromMidi >= 0 && fromMidi < (int) programs.size())
        return { juce::String(programs[(size_t) fromMidi].name), fromMidi };
    return { presetName, currentProgram.load(std::memory_order_relaxed) };
}

void Cs80AudioProcessor::setPresetMetadata(const juce::String& name, int index)
{
    const juce::ScopedLock lock(presetMetadataLock);
    presetName = name;
    currentProgram.store(index, std::memory_order_relaxed);
    if (library != nullptr) library->midiProgram.store(-1, std::memory_order_release);
}

void Cs80AudioProcessor::setPresetName(const juce::String& name)
{
    {
        const juce::ScopedLock lock(presetMetadataLock);
        // A rename of a MIDI-recalled program keeps its index, not its old name.
        if (library != nullptr) library->midiProgram.store(-1, std::memory_order_release);
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
    // [ws8-studio] A program load is not an undo step and starts a fresh history (as replaceState does).
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating(); mm != nullptr && mm->isThisTheMessageThread())
    {
        apvts.syncNow();
        undoManager.clearUndoHistory();
    }
    updateHostDisplay();
}

void Cs80AudioProcessor::routeNoteOn(int note, float velocity, int channel)
{
    if (extensionActive)
        cs80::SixteenVoiceRouter::noteOnTarget(engine, *extensionEngine).noteOn(note, velocity, channel);
    else
        engine.noteOn(note, velocity, channel);
}

void Cs80AudioProcessor::routeNoteOff(int note, int channel)
{
    forEachEngine([&](cs80::Engine& e) { e.noteOff(note, channel); });
}

void Cs80AudioProcessor::handleMidi(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
    {
        // CC88 high-resolution velocity prefix (MMA CA-031); 7-bit otherwise.
        routeNoteOn(message.getNoteNumber(),
                    highResolutionVelocity.noteOnVelocity(message.getChannel(), message.getVelocity()),
                    message.getChannel());
    }
    else if (message.isNoteOff())
    {
        routeNoteOff(message.getNoteNumber(), message.getChannel());
    }
    else if (message.isAftertouch())
    {
        forEachEngine([&](cs80::Engine& e) {
            e.polyPressure(message.getNoteNumber(), message.getAfterTouchValue() / 127.0f, message.getChannel());
        });
    }
    else if (message.isChannelPressure())
    {
        forEachEngine([&](cs80::Engine& e) {
            e.channelPressure(message.getChannel(), message.getChannelPressureValue() / 127.0f);
        });
    }
    else if (message.isPitchWheel())
    {
        const float bend = (message.getPitchWheelValue() - 8192) / 8192.0f;
        forEachEngine([&](cs80::Engine& e) { e.pitchBend(message.getChannel(), bend); });
    }
    else if (message.isAllSoundOff())
    {
        // CC120/123 are controllers too: dispatch before the generic branch.
        forEachEngine([&](cs80::Engine& e) { e.allNotesOff(false, message.getChannel()); });
    }
    else if (message.isAllNotesOff())
    {
        forEachEngine([&](cs80::Engine& e) { e.allNotesOff(true, message.getChannel()); });
    }
    else if (message.isController())
    {
        if (midiLearnConsumes(message.getControllerNumber())) return;   // [ws8-studio] a learned controller drives its parameter instead
        if (message.getControllerNumber() == 11 && !engine.isParityMode())
            expressionValue.store(message.getControllerValue() / 127.0f, std::memory_order_relaxed);
        if (message.getControllerNumber() == 88)
            highResolutionVelocity.controlChange(message.getChannel(), message.getControllerValue());
        if (message.getControllerNumber() == 0 || message.getControllerNumber() == 32)
            handleBankSelect(message.getControllerNumber(), message.getControllerValue(), message.getChannel());
        forEachEngine([&](cs80::Engine& e) {
            e.controlChange(message.getControllerNumber(), message.getControllerValue() / 127.0f, message.getChannel());
        });
    }
}

juce::String Cs80AudioProcessor::publishScala(const juce::String& scl, const juce::String& kbm)
{
    cs80::tuning::Scale scale;
    if (const auto error = cs80::tuning::parseScale(scl.toStdString(), scale); !error.empty())
        return juce::String(error);
    auto mapping = cs80::tuning::defaultMapping(scale);
    if (kbm.trim().isNotEmpty())
        if (const auto error = cs80::tuning::parseKeyboardMapping(kbm.toStdString(), mapping); !error.empty())
            return juce::String(error);
    cs80::tuning::TuningTable table;
    if (const auto error = cs80::tuning::buildTable(scale, mapping, table); !error.empty())
        return juce::String(error);
    const juce::SpinLock::ScopedLockType lock(scalaLock);
    scalaSlot = table;
    scalaText = scl;
    kbmText = kbm;
    scalaDescription = juce::String(scale.description);
    scalaVersion.fetch_add(1, std::memory_order_release);
    return {};
}

juce::String Cs80AudioProcessor::loadScalaTuning(const juce::String& scl, const juce::String& kbm)
{
    return publishScala(scl, kbm);
}

void Cs80AudioProcessor::clearScalaTuning()
{
    const juce::SpinLock::ScopedLockType lock(scalaLock);
    scalaSlot = {};
    scalaText = kbmText = scalaDescription = {};
    scalaVersion.fetch_add(1, std::memory_order_release);
}

juce::String Cs80AudioProcessor::getScalaDescription() const
{
    const juce::SpinLock::ScopedLockType lock(const_cast<juce::SpinLock&>(scalaLock));
    return scalaDescription;
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
        forEachEngine([](cs80::Engine& e) { e.allNotesOff(false, 0); });
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
                    routeNoteOff((int) note, channel);
        keyboardEvents.requestReset();
        uiHeldMasks.fill(0);
        keyPressureOwnership.clear();
    }
    cs80::KeyboardEventQueue<>::Batch uiEvents;
    auto uiCount = keyboardEvents.take(uiEvents);

    applyMidiLearn(midi);   // [ws8-studio] learned controllers move their parameters before this block reads them
    readParameters();
    const auto previousPolicy = engine.getRenderPolicy();
    const int previousFactor = engine.getOversamplingFactor();
    // Hosts may switch to offline rendering without a new prepareToPlay.
    engine.setNonRealtime(isNonRealtime());
    engine.setControlFrame({ cached, cachedHardwareSources });
    modernLayer.applyToEngine(engine, getPlayHead()); // MSEG shapes, routing, host tempo
    // A policy or oversampling-factor change resets the engine the same way.
    if (engine.getRenderPolicy() != previousPolicy || engine.getOversamplingFactor() != previousFactor)
    {
        engine.setExpression(expressionValue.load(std::memory_order_relaxed));
        // Discard the already-captured pre-transition batch too: otherwise its
        // old onset could establish ownership again after the Engine reset.
        resetKeyboardHandoff();
        uiCount = 0;
    }
    // ws3 Phase 6: sixteen-voice mode drives a second, identical instrument.
    if (engine.getVoiceMode() == 1)
    {
        extensionEngine->setNonRealtime(isNonRealtime());
        extensionEngine->setControlFrame({ cached, cachedHardwareSources });
        cs80::ModernLayerState::mirrorToEngine(engine, *extensionEngine);
        if (!extensionActive)
        {
            extensionEngine->reset();
            extensionEngine->setExpression(expressionValue.load(std::memory_order_relaxed));
            extensionActive = true;
        }
    }
    else if (extensionActive)
    {
        extensionEngine->allNotesOff(false, 0);
        keyPressureOwnership.clearExtension();
        extensionActive = false;
    }
    // Scala table from the message thread (never waits for the writer).
    if (const auto version = scalaVersion.load(std::memory_order_acquire); version != scalaAppliedVersion)
    {
        const juce::SpinLock::ScopedTryLockType lock(scalaLock);
        if (lock.isLocked())
        {
            forEachEngine([this](cs80::Engine& e) { e.setScalaTuning(scalaSlot); });
            if (!extensionActive) extensionEngine->setScalaTuning(scalaSlot);
            scalaAppliedVersion = version;
        }
    }
    // MTS-ESP master (tuning_mode 2), when the client is compiled in.
    const bool mtsMaster = cached[cs80::P_tuningMode] >= 1.5f && mtsEsp.fillTable(mtsEspTable);
    mtsEspConnected.store(mtsMaster, std::memory_order_relaxed);
    if (mtsMaster) forEachEngine([this](cs80::Engine& e) { e.setMtsTuning(mtsEspTable); });

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
    forEachEngine([&](cs80::Engine& e) {
        if (std::isfinite(pedal)) e.setExpression(juce::jlimit(0.0f, 1.0f, pedal));
        if (std::isfinite(ribbon)) e.setRibbon(juce::jlimit(-1.0f, 1.0f, ribbon));
        if (std::isfinite(pitch)) e.pitchBend(1, juce::jlimit(-1.0f, 1.0f, pitch));
        if (std::isfinite(mod)) e.controlChange(1, juce::jlimit(0.0f, 1.0f, mod), 1);
    });

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
            if (extensionActive)
            {
                extensionEngine->render(extensionInterleaved.data(), n, input != nullptr ? externalScratch.data() : nullptr);
                for (int i = 0; i < 2 * n; ++i) interleaved[(size_t) i] += extensionInterleaved[(size_t) i];
            }
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
        // Both engines are queried on the render thread at the same sample
        // boundary. 8-voice/PURE modes leave slots 8..15 inactive; pressure
        // attribution must not mistake an extension-engine onset for a miss.
        std::array<cs80::Engine::VoiceInfo, 16> voices {};
        for (int i = 0; i < 8; ++i) voices[(size_t) i] = engine.getVoiceInfo(i);
        if (extensionActive)
            for (int i = 0; i < 8; ++i) voices[(size_t) (8 + i)] = extensionEngine->getVoiceInfo(i);
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
                forEachEngine([&](cs80::Engine& e) { e.polyPressure(event.note, event.velocity, event.channel); });
        }
        else if (event.on)
        {
            const auto before = voiceSnapshot();
            routeNoteOn(event.note, event.velocity, event.channel);
            if (pressureArpOn || keyPressureOwnership.allocated(before, voiceSnapshot(), event.token,
                                                                event.channel, event.note))
                held |= bit;
        }
        else if ((held & bit) != 0)
        {
            routeNoteOff(event.note, event.channel);
            held &= ~bit;
        }
        if (!event.on && !event.pressure) keyPressureOwnership.noteOff(event.channel, event.note);
    };
    for (const auto metadata : midi)
    {
        // ws3 Phase 6: MIDI Tuning Standard SysEx, parsed from the raw bytes
        // (no owning MidiMessage, no allocation) at its sample position.
        if (metadata.numBytes > 5 && metadata.data[0] == 0xf0)
        {
            const int sysexPos = juce::jlimit(0, numSamples, metadata.samplePosition);
            while (uiIndex < uiCount && uiPosition() < sysexPos) dispatchUI();
            advanceTo(sysexPos);
            mtsEsp.parseMidi(metadata.data, metadata.numBytes);
            forEachEngine([&](cs80::Engine& e) { e.handleSysEx(metadata.data, metadata.numBytes); });
            continue;
        }
        // Filter BEFORE getMessage(): long/system messages otherwise own heap data.
        // Only channel messages handled below are supported.
        if (metadata.numBytes < 2 || metadata.numBytes > 3) continue;
        const auto status = metadata.data[0] & 0xf0;
        if (status < 0x80 || status >= 0xf0) continue;
        if (metadata.numBytes != (status == 0xd0 || status == 0xc0 ? 2 : 3)) continue;
        if (metadata.data[1] >= 0x80 || (metadata.numBytes == 3 && metadata.data[2] >= 0x80)) continue;
        const int pos = juce::jlimit(0, numSamples, metadata.samplePosition);
        // Existing host events win equal-sample ties, matching JUCE addEvent order.
        while (uiIndex < uiCount && uiPosition() < pos) dispatchUI();
        advanceTo(pos);
        if (status == 0xc0) // [ws6-library] MIDI Program Change, sample-accurate
        {
            if (handleProgramChange((metadata.data[0] & 0x0f) + 1, metadata.data[1]))
                uiIndex = uiCount; // a policy transition discards the pre-transition UI batch
            continue;
        }
        const auto message = metadata.getMessage();
        std::array<cs80::Engine::VoiceInfo, 16> before;
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
    if (extensionActive)
        for (int i = 0; i < 8; ++i)
            if (extensionEngine->getVoiceInfo(i).active) mask |= 1u << (8 + i);
    activeVoiceMask.store(mask | (extensionActive ? (1u << 16) : 0u), std::memory_order_relaxed);
    activeVoiceCount.store(engine.activeVoices()
                           + (extensionActive ? extensionEngine->activeVoices() : 0),
                           std::memory_order_relaxed);

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
        if (result.getChild(i).hasType("HARDWARE") || result.getChild(i).hasType("AB_COMPARE"))
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
    const juce::ScopedLock treeLock(apvts.treeLock());   // [ws8-studio] the undo flush and the host never touch the tree at once
    apvts.syncNow();
    auto state = apvts.copyState();
    {
        // ws3 Phase 6: the session keeps its Scala/KBM texts (not presets).
        const juce::SpinLock::ScopedLockType lock(scalaLock);
        if (scalaText.isNotEmpty())
        {
            juce::ValueTree tuning("WS3_TUNING");
            tuning.setProperty("scl", scalaText, nullptr);
            tuning.setProperty("kbm", kbmText, nullptr);
            state.appendChild(tuning, nullptr);
        }
    }
    const auto metadata = getPresetMetadata();
    state.setProperty("presetName", metadata.name, nullptr);
    state.setProperty("factoryIndex", metadata.factoryIndex, nullptr);
    state.setProperty("subOscStateVersion", 1, nullptr);
    state.setProperty(stateSchemaKey, stateSchemaVersion, nullptr);
    writeStudioState(state);   // [ws8-studio]
    writeLibraryState(state); // [ws6-library] A/B compare slot
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
            readStudioState(tree);   // [ws8-studio] also removes those children from the parameter tree
            // [ws3-keyboard] session Scala/KBM texts; not part of the parameter tree either.
            const auto tuning = tree.getChildWithName("WS3_TUNING");
            if (tuning.isValid())
            {
                publishScala(tuning.getProperty("scl").toString(), tuning.getProperty("kbm").toString());
                tree.removeChild(tuning, nullptr);
            }
            else
                clearScalaTuning();
            const juce::ScopedLock treeLock(apvts.treeLock());
            apvts.syncNow(); // Drain pending host/user parameter edits before replacing their ValueTree.
            apvts.replaceState(normaliseRestoredState(tree));
            setPresetMetadata(tree.getProperty("presetName", "Init Patch").toString(),
                              (int) tree.getProperty("factoryIndex", 0));
            readLibraryState(tree); // [ws6-library] A/B compare slot
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

    auto state = [this]
    {
        const juce::ScopedLock treeLock(apvts.treeLock());   // [ws8-studio]
        apvts.syncNow();
        return apvts.copyState();
    }();
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
            for (const char* child : { "MIDI_LEARN", "UI_STATE" })   // [ws8-studio] a preset never replaces session state
                tree.removeChild(tree.getChildWithName(child), nullptr);
            auto restored = normaliseRestoredState(tree);
            // Session settings, not sounds: a user preset keeps the live values.
            for (int index : { (int) cs80::P_parityMode, (int) cs80::P_oversampling, (int) cs80::P_outputJack })
                restored.getChildWithProperty("id", parameterId(index))
                    .setProperty("value", getParamValue(index), nullptr);
            const juce::ScopedLock treeLock(apvts.treeLock());   // [ws8-studio]
            apvts.syncNow(); // Preserve pre-load undo ownership before replacing the tree.
            apvts.replaceState(restored);
            setPresetMetadata(tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString(),
                              (int) tree.getProperty("factoryIndex", 0));
            // DSP state is updated by the next audio block, not the state-loading thread.
        }
    }
}

// ---------------------------------------------------------------------------
// [ws8-studio] MIDI learn and editor state
// ---------------------------------------------------------------------------
int Cs80AudioProcessor::hostParameterIndex(int coreIndex) const
{
    if (coreIndex < 0 || coreIndex >= cs80::P_numParams)
        return -1;
    const auto* p = apvts.getParameter(parameterId(coreIndex));
    return p != nullptr ? p->getParameterIndex() : -1;
}

void Cs80AudioProcessor::applyMidiLearn(const juce::MidiBuffer& midi) noexcept
{
    // Lock-free on this side: the map is atomics only. setValueNotifyingHost is the path JUCE's own
    // wrappers use for host automation on the audio thread. PURE CS-80 has no MIDI learn.
    const auto* parity = parameterValues[(size_t) cs80::P_parityMode];
    midiLearnLive = parity == nullptr || parity->load(std::memory_order_relaxed) < 0.5f;
    if (! midiLearnLive || midi.isEmpty())
        return;
    const auto& parameters = getParameters();
    for (const auto metadata : midi)
    {
        if (metadata.numBytes != 3 || (metadata.data[0] & 0xf0) != 0xb0 || metadata.data[1] >= 0x80 || metadata.data[2] >= 0x80)
            continue;
        const int target = midiLearn.handleController(metadata.data[1]);
        if (target >= 0 && target < parameters.size())
            parameters.getUnchecked(target)->setValueNotifyingHost((float) metadata.data[2] / 127.0f);
    }
}

void Cs80AudioProcessor::writeStudioState(juce::ValueTree& state) const
{
    juce::ValueTree learn("MIDI_LEARN");
    learn.setProperty("version", cs80::MidiLearnMap::kStateVersion, nullptr);
    const auto& parameters = getParameters();
    for (int cc = 0; cc < cs80::MidiLearnMap::kNumControllers; ++cc)
    {
        const int index = midiLearn.parameterFor(cc);
        if (index < 0 || index >= parameters.size())
            continue;
        if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters.getUnchecked(index)))
        {
            juce::ValueTree map("MAP");
            map.setProperty("cc", cc, nullptr);
            map.setProperty("param", p->paramID, nullptr);
            learn.appendChild(map, nullptr);
        }
    }
    state.appendChild(learn, nullptr);

    juce::ValueTree ui("UI_STATE");
    ui.setProperty("version", editorStateVersion, nullptr);
    ui.setProperty("tourSeen", editorState.tourSeen.load() ? 1 : 0, nullptr);
    ui.setProperty("qwerty", editorState.qwertyEnabled.load() ? 1 : 0, nullptr);
    ui.setProperty("circuitNotes", editorState.circuitNotes.load() ? 1 : 0, nullptr);
    ui.setProperty("controllers", editorState.controllersShown.load() ? 1 : 0, nullptr);
    state.appendChild(ui, nullptr);
}

void Cs80AudioProcessor::readStudioState(juce::ValueTree& state)
{
    // A restored session owns its map: a state without one (or from before this feature) has no mappings.
    midiLearn.clearAll();
    const auto learn = state.getChildWithName("MIDI_LEARN");
    if (learn.isValid())
    {
        const auto& parameters = getParameters();
        for (const auto& map : learn)
        {
            if (! map.hasType("MAP"))
                continue;
            const int cc = (int) map.getProperty("cc", -1);
            const auto id = map.getProperty("param").toString();
            for (int index = 0; index < parameters.size(); ++index)
                if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters.getUnchecked(index)); p != nullptr && p->paramID == id)
                {
                    midiLearn.assign(cc, index);   // refuses unlearnable controllers
                    break;
                }
        }
    }
    const auto ui = state.getChildWithName("UI_STATE");
    editorState.tourSeen = ui.isValid() && (bool) ui.getProperty("tourSeen", false);
    editorState.qwertyEnabled = ! ui.isValid() || (bool) ui.getProperty("qwerty", true);
    editorState.circuitNotes = ui.isValid() && (bool) ui.getProperty("circuitNotes", false);
    editorState.controllersShown = ui.isValid() && (bool) ui.getProperty("controllers", false);
    state.removeChild(learn, nullptr);
    state.removeChild(ui, nullptr);
}

// ---------------------------------------------------------------------------
// [ws8-studio] Undo history
// ---------------------------------------------------------------------------
bool Cs80AudioProcessor::undo()
{
    apvts.syncNow();
    return undoManager.undo();
}

bool Cs80AudioProcessor::redo()
{
    apvts.syncNow();
    return undoManager.redo();
}
