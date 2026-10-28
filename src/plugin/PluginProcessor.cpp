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
    cached = currentValues();
    engine.setParams(cached);
    engine.prepare(48000.0);
    presetName = getFactoryPresets().empty() ? "Init Patch" : juce::String(getFactoryPresets()[0].name);
    currentProgram = 0;
}

juce::AudioProcessorValueTreeState::ParameterLayout Cs80AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout](int index) {
        const auto& d = cs80::paramDef(index);
        juce::NormalisableRange<float> range(d.min, d.max, 0.0f, d.skew);
        juce::AudioParameterFloatAttributes attributes;
        if (d.unit != nullptr && *d.unit != 0)
            attributes = attributes.withLabel(d.unit);
        const auto id = parameterId(index);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { id, 1 }, juce::String(d.name), range, d.def, attributes));
    };

    for (int i = 0; i < cs80::P_numGlobals; ++i)
        add(i);
    for (int ch = 0; ch < 2; ++ch)
        for (int p = 0; p < cs80::CP_count; ++p)
            add(cs80::chParam(ch, p));

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
        const auto id = parameterId(i);
        if (auto* p = apvts.getRawParameterValue(id))
            cached.v[(size_t) i] = p->load();
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
        const float normalised = cs80::normaliseParam(index, physicalValue);
        p->setValueNotifyingHost(normalised);
        cached.v[(size_t) index] = physicalValue;
    }
}

void Cs80AudioProcessor::applyParameterValues(const cs80::ParamValues& values)
{
    applyValues(values);
}

void Cs80AudioProcessor::applyValues(const cs80::ParamValues& values)
{
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        if (auto* p = apvts.getParameter(parameterId(i)))
            p->setValueNotifyingHost(cs80::normaliseParam(i, values[i]));
    }
    cached = values;
    engine.setParams(cached);
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
        engine.polyPressure(message.getNoteNumber(), message.getAfterTouchValue() / 127.0f);
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
    else if (message.isController())
    {
        engine.controlChange(message.getControllerNumber(), message.getControllerValue() / 127.0f, message.getChannel());
    }
    else if (message.isAllNotesOff() || message.isAllSoundOff())
    {
        engine.allNotesOff(message.isAllSoundOff());
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

    if (interleaved.size() < (size_t) numSamples * 2)
        interleaved.resize((size_t) numSamples * 2, 0.0f);

    int done = 0;
    for (const auto metadata : midi)
    {
        const int pos = juce::jlimit(0, numSamples, metadata.samplePosition);
        if (pos > done)
        {
            const int n = pos - done;
            engine.render(interleaved.data(), n);
            // The interleaved buffer has L/R pairs, so copy separately.
            if (numChannels >= 2)
            {
                for (int i = 0; i < n; ++i)
                {
                    buffer.setSample(0, done + i, interleaved[(size_t) i * 2]);
                    buffer.setSample(1, done + i, interleaved[(size_t) i * 2 + 1]);
                }
            }
            else if (numChannels == 1)
            {
                for (int i = 0; i < n; ++i)
                    buffer.setSample(0, done + i,
                                     (interleaved[(size_t) i * 2]
                                      + interleaved[(size_t) i * 2 + 1]) * 0.5f);
            }
            done = pos;
        }
        handleMidi(metadata.getMessage());
    }

    if (done < numSamples)
    {
        const int n = numSamples - done;
        engine.render(interleaved.data(), n);
        if (numChannels >= 2)
        {
            for (int i = 0; i < n; ++i)
            {
                buffer.setSample(0, done + i, interleaved[(size_t) i * 2]);
                buffer.setSample(1, done + i, interleaved[(size_t) i * 2 + 1]);
            }
        }
        else if (numChannels == 1)
        {
            for (int i = 0; i < n; ++i)
                buffer.setSample(0, done + i, (interleaved[(size_t) i * 2] + interleaved[(size_t) i * 2 + 1]) * 0.5f);
        }
    }
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
            apvts.replaceState(tree);
            presetName = tree.getProperty("presetName", "Init Patch").toString();
            currentProgram = (int) tree.getProperty("factoryIndex", 0);
            readParameters();
            engine.setParams(cached);
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
            apvts.replaceState(tree);
            presetName = tree.getProperty("presetName", file.getFileNameWithoutExtension()).toString();
            currentProgram = (int) tree.getProperty("factoryIndex", 0);
            readParameters();
            engine.setParams(cached);
            if (onPresetChanged)
                onPresetChanged();
        }
    }
}
