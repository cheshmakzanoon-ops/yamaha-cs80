#pragma once

// Host boundary regressions for the optional instrument bodies. These cover
// published automation identity and actual persistence/recall, not acoustic
// likeness. Run after JUCE initialization; files stay in temporary fixtures.
#include "plugin/PluginProcessor.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>

namespace cs80::tests
{
namespace instrument_body_host_detail
{
using Values = std::array<float, 8>;
inline constexpr Values a { 1.0f, 0.83f, 0.31f, 2.17f, 2.0f, 0.47f, 0.72f, 0.63f };
inline constexpr Values b { 15.0f, 0.21f, 0.81f, 0.44f, 10.0f, 0.91f, 0.19f, 3.31f };
inline constexpr Values defaults { 0.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f };
inline constexpr const char* ids[] {
    "body1_model", "body1_mix", "body1_brightness", "body1_decay",
    "body2_model", "body2_mix", "body2_brightness", "body2_decay"
};

inline void hashId(std::uint64_t& hash, const juce::String& id)
{
    // FNV-1a over UTF-8 IDs, each followed by '\n'. Baselines below were
    // calculated from the published 73dc17b8 layout, not this test's enums.
    for (auto p = id.toRawUTF8(); *p != 0; ++p)
    {
        hash ^= static_cast<unsigned char>(*p);
        hash *= UINT64_C(1099511628211);
    }
    hash ^= 10;
    hash *= UINT64_C(1099511628211);
}

inline void set(Cs80AudioProcessor& host, const Values& values)
{
    for (int i = 0; i < 8; ++i) host.setParamValue(P_body1Model + i, values[(size_t) i]);
}

inline Values valuesOf(const ParamValues& values)
{
    Values result {};
    for (int i = 0; i < 8; ++i) result[(size_t) i] = values[P_body1Model + i];
    return result;
}

inline bool matches(const Cs80AudioProcessor& host, const Values& expected)
{
    for (int i = 0; i < 8; ++i)
    {
        const float actual = host.getParamValue(P_body1Model + i);
        if (!std::isfinite(actual) || std::abs(actual - expected[(size_t) i]) > 1.0e-5f)
            return false;
    }
    return true;
}

inline bool engineMatches(Cs80AudioProcessor& host, const Values& expected)
{
    const auto actual = valuesOf(host.getEngine().getParams());
    for (size_t i = 0; i < actual.size(); ++i)
        if (!std::isfinite(actual[i]) || std::abs(actual[i] - expected[i]) > 1.0e-5f) return false;
    return true;
}

inline void restore(Cs80AudioProcessor& host, const juce::ValueTree& tree)
{
    const auto text = tree.createXml()->toString();
    host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
}

inline void process(Cs80AudioProcessor& host, juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> audio(2, 64);
    audio.clear();
    host.processBlock(audio, midi);
}
} // namespace instrument_body_host_detail

template <typename Check>
void testInstrumentBodyHost(Check&& check)
{
    using namespace instrument_body_host_detail;
    static_assert(P_body1Model == 190 && P_numParams == 198,
                  "instrument bodies append eight core controls");
    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        const auto& parameters = host->getParameters();
        check(parameters.size() == 304 && Cs80AudioProcessor::hostParameterCount == 304,
              "instrument bodies append eight parameters after the 296 published host slots");
        std::uint64_t coreHash = UINT64_C(14695981039346656037);
        for (int index = 0; index < 190; ++index) hashId(coreHash, Cs80AudioProcessor::parameterId(index));
        check(coreHash == UINT64_C(0x3a8332a1fb546bd8),
              "all first 190 core IDs retain their published order and spelling");
        std::uint64_t hostHash = UINT64_C(14695981039346656037);
        bool oldHostIds = parameters.size() >= 296;
        for (int index = 0; oldHostIds && index < 296; ++index)
            if (auto* parameter = dynamic_cast<juce::HostedAudioProcessorParameter*>(parameters[index]))
                hashId(hostHash, parameter->getParameterID());
            else oldHostIds = false;
        check(oldHostIds && hostHash == UINT64_C(0x00acbd99c8c30a07),
              "all first 296 host indexes retain their published parameter IDs");

        constexpr float minimum[] { 0.0f, 0.0f, 0.0f, 0.25f };
        constexpr float maximum[] { 19.0f, 1.0f, 1.0f, 4.0f };
        const juce::String modelNames = "Off|Piano strings|Tine electric piano|Reed electric piano|Clavichord|Harpsichord|Nylon strings|Steel strings|Plucked bass|Bowed strings|Flute|Clarinet|Oboe|Bassoon|Brass|Organ|Bell|Vibraphone|Marimba|Celesta";
        for (int i = 0; i < 8; ++i)
        {
            const int index = P_body1Model + i;
            auto* parameter = host->getAPVTS().getParameter(ids[i]);
            check(parameter != nullptr && Cs80AudioProcessor::parameterId(index) == ids[i],
                  "both body ranks expose their eight stable parameter IDs");
            if (parameter == nullptr) continue;
            const auto& range = parameter->getNormalisableRange();
            check(parameters.indexOf(parameter) == 296 + i && host->hostParameterIndex(index) == 296 + i
                      && parameter->getVersionHint() == 16 && parameter->isAutomatable(),
                  "body automation appends in core order with AU version hint 16");
            check(range.start == minimum[i % 4] && range.end == maximum[i % 4]
                      && std::abs(parameter->convertFrom0to1(parameter->getDefaultValue()) - defaults[(size_t) i]) < 1.0e-5f
                      && !Cs80AudioProcessor::isPhysicalSetupParameter(index),
                  "body ranges and off defaults are sound controls owned by preset recall");
            if (i % 4 == 0)
            {
                auto* choice = dynamic_cast<juce::AudioParameterChoice*>(parameter);
                check(choice != nullptr && parameter->getNumSteps() == 20
                          && choice->choices.joinIntoString("|") == modelNames,
                      "body models are twenty named discrete choices in published model order");
                for (int model = 0; model < 20; ++model)
                {
                    host->setParamValue(index, (float) model);
                    check(host->getParamValue(index) == (float) model,
                          "every body model survives host normalization without a shifted choice index");
                }
            }
            else
                check(dynamic_cast<juce::AudioParameterFloat*>(parameter) != nullptr
                          && range.interval == 0.0f,
                      "body mix, brightness and decay remain continuous host controls");
        }
    }

    juce::TemporaryFile fixture("-cs80-instrument-body-host");
    const auto directory = fixture.getFile();
    check(directory.createDirectory().wasOk(), "instrument-body host fixture directory is available");
    {
        auto writer = std::make_unique<Cs80AudioProcessor>(directory.getChildFile("presets"));
        auto reader = std::make_unique<Cs80AudioProcessor>();
        set(*writer, a);
        juce::MemoryBlock saved;
        writer->getStateInformation(saved);
        set(*reader, b);
        reader->setStateInformation(saved.getData(), (int) saved.getSize());
        check(matches(*reader, a), "session round trip replaces all eight live body controls");
        reader->prepareToPlay(48000.0, 64);
        process(*reader);
        check(engineMatches(*reader, a), "restored body controls reach the engine on the next audio block");
        const auto file = writer->saveUserPreset("Distinct instrument bodies", Cs80AudioProcessor::PresetOverwrite::yes);
        check(file.wasSaved(), "user preset saves both instrument bodies");
        if (file.wasSaved())
        {
            set(*reader, b);
            check(reader->loadUserPreset(file.file).wasOk() && matches(*reader, a),
                  "user preset loading restores all eight body controls over different live values");
        }

        // An old file must not inherit the receiving instance's audible body.
        // Exercise every prior schema and an absent marker through both APIs.
        for (const int schema : { 0, 1, 2, 3, 4 })
        {
            auto old = writer->getAPVTS().copyState();
            if (schema != 0) old.setProperty(Cs80AudioProcessor::stateSchemaKey, schema, nullptr);
            for (const char* id : ids) old.removeChild(old.getChildWithProperty("id", id), nullptr);
            set(*reader, b);
            restore(*reader, old);
            check(matches(*reader, defaults), "pre-body sessions reset both bodies off instead of retaining the live instrument");
            process(*reader);
            check(engineMatches(*reader, defaults), "pre-body session defaults replace the previous audio-thread body controls");
            const auto oldFile = directory.getChildFile("old-body-" + juce::String(schema) + ".cs80xml");
            const bool wrote = old.createXml()->writeTo(oldFile);
            check(wrote, "pre-body user-preset fixture writes");
            if (wrote)
            {
                set(*reader, a);
                check(reader->loadUserPreset(oldFile).wasOk() && matches(*reader, defaults),
                      "pre-body user presets reset missing body models, mix and shaping controls");
            }
        }

        auto malformed = writer->getAPVTS().copyState();
        const Values outside { 3.7f, -0.25f, 1.25f, 0.01f, 77.0f, 1.5f, -0.5f, 8.0f };
        const Values repaired { 4.0f, 0.0f, 1.0f, 0.25f, 19.0f, 1.0f, 0.0f, 4.0f };
        for (int i = 0; i < 8; ++i)
            malformed.getChildWithProperty("id", ids[i]).setProperty("value", outside[(size_t) i], nullptr);
        restore(*reader, malformed);
        check(matches(*reader, repaired), "state admission rounds body choices and bounds every continuous body control");
    }

    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        auto reopened = std::make_unique<Cs80AudioProcessor>();
        set(*host, a);
        host->abToggle();
        check(matches(*host, a), "first A/B toggle copies the complete instrument bodies");
        set(*host, b);
        host->abToggle();
        check(matches(*host, a), "A/B recall restores body models and all six continuous controls in A");
        host->abToggle();
        check(matches(*host, b), "A/B recall restores the distinct instrument bodies in B");
        juce::MemoryBlock saved;
        host->getStateInformation(saved);
        reopened->setStateInformation(saved.getData(), (int) saved.getSize());
        check(matches(*reopened, b), "session save preserves the active B instrument bodies");
        reopened->abToggle();
        check(matches(*reopened, a), "session save preserves the inactive A instrument bodies");
        const auto xml = juce::parseXML(saved.toString());
        check(xml != nullptr, "A/B body session is readable XML");
        if (xml != nullptr)
        {
            auto old = juce::ValueTree::fromXml(*xml);
            auto slot = old.getChildWithName("AB_COMPARE").getChildWithName("SLOT");
            check(slot.isValid(), "body A/B fixture includes an inactive slot");
            for (const char* id : ids) slot.removeProperty(id, nullptr);
            restore(*reopened, old);
            reopened->abToggle();
            check(matches(*reopened, defaults), "older inactive A/B slots cannot inherit the active slot's body");
        }
    }

    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        set(*host, a);
        host->getUndoableState().syncNow();
        host->getUndoManager().clearUndoHistory();
        for (int i = 0; i < 8; ++i)
        {
            auto* parameter = host->getAPVTS().getParameter(ids[i]);
            if (parameter == nullptr) { check(false, "body gesture parameter exists"); continue; }
            Values edited = a;
            edited[(size_t) i] = b[(size_t) i];
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost(parameter->convertTo0to1(edited[(size_t) i]));
            parameter->endChangeGesture();
            check(matches(*host, edited), "a body parameter gesture changes only the intended model or continuous control");
            check(host->undo() && matches(*host, a), "undo restores a body gesture without changing the other rank");
            check(host->redo() && matches(*host, edited), "redo restores the body gesture's exact physical value");
            check(host->undo() && matches(*host, a), "body gesture history can return to the baseline for the next field");
        }
    }

    {
        auto host = std::make_unique<Cs80AudioProcessor>();
        host->prepareToPlay(48000.0, 64);
        const auto& programs = host->getFactoryPresets();
        bool sawBody = false, sawOff = false;
        for (int index = 0; index < (int) programs.size(); ++index)
        {
            const auto expected = valuesOf(programs[(size_t) index].values);
            sawBody = sawBody || expected[0] != 0.0f || expected[4] != 0.0f;
            sawOff = sawOff || (expected[0] == 0.0f && expected[4] == 0.0f);
            set(*host, b);
            host->loadFactoryPreset(index);
            const auto factoryLabel = "factory program " + juce::String(index) + " replaces all eight body controls, including off values";
            check(matches(*host, expected), factoryLabel.toRawUTF8());
            set(*host, a);
            const bool original = index < legacyProgramCount;
            const int inBank = original ? index : index - legacyProgramCount;
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, original ? 1 : 0), 0);
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, inBank / 128), 1);
            midi.addEvent(juce::MidiMessage::programChange(1, inBank % 128), 2);
            process(*host, midi);
            const auto midiLabel = "MIDI program " + juce::String(index) + " publishes all eight body values to the engine at recall";
            check(host->getCurrentProgram() == index && matches(*host, expected) && engineMatches(*host, expected),
                  midiLabel.toRawUTF8());
        }
        check(sawBody && sawOff, "program recall covers authored bodies and programs that clear a preceding body");
    }

    {
        // A Program Change followed by a note in the same block must sound
        // like the same recall done before that block on the message thread.
        // Keep earlier voices alive, so stale note/body caches cannot hide
        // behind reset(), and include a return through a body-off program.
        auto midiHost = std::make_unique<Cs80AudioProcessor>();
        auto directHost = std::make_unique<Cs80AudioProcessor>();
        const auto& programs = midiHost->getFactoryPresets();
        int firstBody = -1, differentBody = -1, offProgram = -1;
        for (int index = 0; index < (int) programs.size(); ++index)
        {
            const auto current = valuesOf(programs[(size_t) index].values);
            if (current[0] == 0.0f && current[4] == 0.0f)
            {
                if (offProgram < 0) offProgram = index;
                continue;
            }
            if (firstBody < 0) firstBody = index;
            const auto first = valuesOf(programs[(size_t) firstBody].values);
            if (differentBody < 0 && (current[0] != first[0] || current[4] != first[4]))
                differentBody = index;
        }
        check(firstBody >= 0 && differentBody >= 0 && offProgram >= 0,
              "same-block body recall fixture contains two different instruments and an off program");
        if (firstBody >= 0 && differentBody >= 0 && offProgram >= 0)
        {
            for (auto* host : { midiHost.get(), directHost.get() })
            {
                host->loadFactoryPreset(firstBody);
                host->prepareToPlay(48000.0, 256);
                juce::MidiBuffer start;
                start.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
                process(*host, start);
            }
            int nextNote = 64;
            for (int target : { differentBody, offProgram, firstBody })
            {
                directHost->loadFactoryPreset(target);
                const bool original = target < legacyProgramCount;
                const int inBank = original ? target : target - legacyProgramCount;
                juce::MidiBuffer fromMidi, direct;
                fromMidi.addEvent(juce::MidiMessage::controllerEvent(1, 0, original ? 1 : 0), 0);
                fromMidi.addEvent(juce::MidiMessage::controllerEvent(1, 32, inBank / 128), 0);
                fromMidi.addEvent(juce::MidiMessage::programChange(1, inBank % 128), 0);
                const auto note = juce::MidiMessage::noteOn(1, nextNote++, (juce::uint8) 109);
                fromMidi.addEvent(note, 1);
                direct.addEvent(note, 1);
                juce::AudioBuffer<float> midiAudio(2, 256), directAudio(2, 256);
                midiAudio.clear(); directAudio.clear();
                midiHost->processBlock(midiAudio, fromMidi);
                directHost->processBlock(directAudio, direct);
                bool identical = true;
                for (int channel = 0; channel < 2; ++channel)
                    for (int frame = 0; frame < 256; ++frame)
                        identical = identical && midiAudio.getSample(channel, frame) == directAudio.getSample(channel, frame);
                check(identical && midiHost->getEngine().activeVoices() > 0
                          && engineMatches(*midiHost, valuesOf(programs[(size_t) target].values)),
                      "same-block Program Change and next note render the recalled body, including off and return transitions");
            }
        }
    }

    // Body settings belong to PANEL voicing. Original hardware tabs and all
    // four memory banks must render exactly the same with these controls on
    // or off, while PANEL must have an audible difference. Compare actual
    // processBlock output, not the eligibility predicate in Engine.cpp.
    for (int source = 0; source <= 13; ++source)
    {
        auto enabled = std::make_unique<Cs80AudioProcessor>();
        auto disabled = std::make_unique<Cs80AudioProcessor>();
        ParamValues sound;
        sound[P_sourcePerformance] = 1.0f;
        sound[P_analogNoise] = sound[P_drift] = sound[P_voiceDispersion] = sound[P_chorusOn] = 0.0f;
        sound[P_fx1Mix] = sound[P_fx2Mix] = sound[P_fx3Mix] = 0.0f;
        sound[P_body1Model] = (float) body::piano;
        sound[P_body2Model] = (float) body::tine;
        sound[P_body1Mix] = sound[P_body2Mix] = 1.0f;
        enabled->applyParameterValues(sound);
        sound[P_body1Model] = sound[P_body2Model] = 0.0f;
        sound[P_body1Mix] = sound[P_body2Mix] = 0.0f;
        disabled->applyParameterValues(sound);
        for (auto* host : { enabled.get(), disabled.get() })
        {
            host->selectHardwareSource(0, source);
            host->selectHardwareSource(1, source);
            host->prepareToPlay(48000.0, 256);
        }
        double delta = 0.0, energy = 0.0;
        bool finite = true;
        juce::AudioBuffer<float> on(2, 256), off(2, 256);
        for (int block = 0; block < 16; ++block)
        {
            juce::MidiBuffer events;
            if (block == 0) events.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
            auto otherEvents = events;
            on.clear(); off.clear();
            enabled->processBlock(on, events);
            disabled->processBlock(off, otherEvents);
            for (int channel = 0; channel < 2; ++channel)
                for (int frame = 0; frame < 256; ++frame)
                {
                    const double x = on.getSample(channel, frame), y = off.getSample(channel, frame);
                    finite = finite && std::isfinite(x) && std::isfinite(y);
                    delta += (x - y) * (x - y);
                    energy += x * x + y * y;
                }
        }
        check(finite && energy > 1.0e-8, "hardware source isolation fixture renders finite audible notes");
        check(source == 13 ? delta > 1.0e-7 : delta == 0.0,
              source == 13 ? "PANEL body controls change the actual rendered instrument"
                           : "hardware tone tabs and memory sources exclude the software bodies exactly");
    }
}
} // namespace cs80::tests
