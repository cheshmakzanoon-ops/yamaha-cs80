#include "PluginProcessor.h"

#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
juce::File makeUserPresetDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("CS80Reference")
        .getChildFile("Presets");
}
} // namespace

Cs80AudioProcessor::Cs80AudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "CS80_PARAMS", createParameterLayout())
{
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
    if (!getFactoryPresets().empty())
        applyValues(getFactoryPresets()[0].values);
    cached = currentValues();
    engine.setParams(cached);
    engine.prepare(48000.0);
    // The decimator delay is fixed by the 2x architecture, so report it as
    // soon as the processor exists. prepareToPlay repeats this for hosts that
    // query latency only after a sample-rate change.
    setLatencySamples(engine.getLatencySamples());
    presetName = getFactoryPresets().empty() ? "Init Patch" : juce::String(getFactoryPresets()[0].name);
    currentProgram = 0;
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
                layout.add(std::make_unique<juce::AudioParameterBool>(id, name, d.def >= 0.5f));
            else
            {
                auto attributes = juce::AudioParameterFloatAttributes().withLabel(d.unit);
                layout.add(std::make_unique<juce::AudioParameterFloat>(id, name,
                    juce::NormalisableRange<float>(d.min, d.max, 0.0f, d.skew), d.def, attributes));
            }
        }
    // Core extension index122 is NOT host index122: keep all 228 existing
    // host indexes stable, and use a later AU version hint to preserve AU order.
    for (int i = cs80::P_legacyNumParams; i < cs80::P_numParams; ++i)
        add(i, 3);
    return layout;
}

void Cs80AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate);
    readParameters();
    engine.setParams(cached);
    setLatencySamples(engine.getLatencySamples());
    interleaved.assign((size_t) juce::jmax(4096, samplesPerBlock) * 2, 0.0f);
    keyboardState.reset();
}

void Cs80AudioProcessor::releaseResources()
{
    engine.reset();
    keyboardState.reset();
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

void Cs80AudioProcessor::applyValues(const cs80::ParamValues& values)
{
    resetHardwareSources();
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        if (auto* p = apvts.getParameter(parameterId(i)))
            p->setValueNotifyingHost(p->convertTo0to1(values[i]));
    }
    // The next audio block snapshots APVTS atomics; never mutate DSP from the GUI.
}

void Cs80AudioProcessor::setPresetName(const juce::String& name)
{
    presetName = name;
    updateHostDisplay();
    if (onPresetChanged)
        onPresetChanged();
}

void Cs80AudioProcessor::loadFactoryPreset(int index)
{
    const auto& bank = getFactoryPresets();
    if (bank.empty())
        return;
    index = juce::jlimit(0, (int) bank.size() - 1, index);
    applyValues(bank[(size_t) index].values);
    currentProgram = index;
    presetName = juce::String(bank[(size_t) index].name);
    updateHostDisplay();
    if (onPresetChanged)
        onPresetChanged();
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
        engine.controlChange(message.getControllerNumber(), message.getControllerValue() / 127.0f, message.getChannel());
    }
}

void Cs80AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    for (int ch = 0; ch < numChannels; ++ch)
        buffer.clear(ch, 0, numSamples);

    keyboardState.processNextMidiBuffer(midi, 0, numSamples, true);

    readParameters();
    engine.setParams(cached);

    const float ribbon = pendingRibbon.exchange(noPendingControl, std::memory_order_relaxed);
    const float pitch = pendingPitch.exchange(noPendingControl, std::memory_order_relaxed);
    const float mod = pendingMod.exchange(noPendingControl, std::memory_order_relaxed);
    if (std::isfinite(ribbon)) engine.setRibbon(juce::jlimit(-1.0f, 1.0f, ribbon));
    if (std::isfinite(pitch)) engine.pitchBend(1, juce::jlimit(-1.0f, 1.0f, pitch));
    if (std::isfinite(mod)) engine.controlChange(1, juce::jlimit(0.0f, 1.0f, mod), 1);

    // Hosts may supply a block larger than their advertised maximum. Render
    // bounded chunks rather than allocating on the real-time thread.
    auto renderSegment = [&](int start, int length)
    {
        const int capacity = (int) interleaved.size() / 2;
        if (capacity == 0) return;
        for (int offset = 0; offset < length; )
        {
            const int n = std::min(capacity, length - offset);
            engine.render(interleaved.data(), n);
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
    for (const auto metadata : midi)
    {
        const int pos = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (pos > done)
        {
            renderSegment(done, pos - done);
            done = pos;
        }
        handleMidi(metadata.getMessage());
    }

    if (done < numSamples)
        renderSegment(done, numSamples - done);

    unsigned mask = 0;
    for (int i = 0; i < 8; ++i)
        if (engine.getVoiceInfo(i).active) mask |= 1u << i;
    activeVoiceMask.store(mask, std::memory_order_relaxed);
    activeVoiceCount.store(engine.activeVoices(), std::memory_order_relaxed);
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
    const bool legacy = hardware.isValid() && (int) hardware.getProperty("version", 1) == 1;
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
        // Round discrete choices/switches and clamp to the declared range.
        const auto& range = p->getNormalisableRange();
        value = range.snapToLegalValue(juce::jlimit(range.start, range.end, value));
        child.setProperty("value", value, nullptr);
    };
    for (int i = 0; i < cs80::P_numParams; ++i)
        ensure(parameterId(i), cs80::paramDef(i).def);
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
    state.setProperty("presetName", presetName, nullptr);
    state.setProperty("factoryIndex", currentProgram, nullptr);
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
        if (tree.isValid() && tree.hasType(apvts.state.getType()))
        {
            apvts.replaceState(normaliseRestoredState(tree));
            presetName = tree.getProperty("presetName", "Init Patch").toString();
            currentProgram = (int) tree.getProperty("factoryIndex", 0);
            // DSP state is updated by the next audio block, not the state-loading thread.
            if (onPresetChanged)
                onPresetChanged();
        }
    }
}

juce::File Cs80AudioProcessor::userPresetDirectory() const
{
    auto dir = makeUserPresetDirectory();
    dir.createDirectory();
    return dir;
}

void Cs80AudioProcessor::saveUserPreset(const juce::String& name)
{
    const auto dir = userPresetDirectory();
    auto safe = juce::File::createLegalFileName(name.isNotEmpty() ? name : presetName);
    if (safe.isEmpty())
        safe = "User Preset";
    auto file = dir.getChildFile(safe + ".cs80xml");
    auto state = apvts.copyState();
    state.setProperty("presetName", name.isNotEmpty() ? name : presetName, nullptr);
    state.setProperty("factoryIndex", currentProgram, nullptr);
    if (auto xml = state.createXml())
        xml->writeTo(file);
    presetName = name.isNotEmpty() ? name : presetName;
    updateHostDisplay();
    if (onPresetChanged)
        onPresetChanged();
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
        if (tree.isValid() && tree.hasType(apvts.state.getType()))
        {
            apvts.replaceState(normaliseRestoredState(tree));
            presetName = tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString();
            currentProgram = (int) tree.getProperty("factoryIndex", 0);
            // DSP state is updated by the next audio block, not the state-loading thread.
            if (onPresetChanged)
                onPresetChanged();
        }
    }
}
