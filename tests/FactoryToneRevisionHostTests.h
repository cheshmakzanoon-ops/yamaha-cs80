#pragma once

#include "HostParameterState.h"
#include "plugin/PluginProcessor.h"

#include <atomic>
#include <bit>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

namespace cs80::tests
{
struct FactoryToneRevisionAccess
{
    // Test-only access to real transaction/publication methods. No DSP test mode.
    static void snapshot(Cs80AudioProcessor& host) { host.readParameters(); }
    static void publishAutomation(Cs80AudioProcessor& host, int position)
    { host.applyHostAutomationAt(position, true); }
};

namespace factory_tone_host_detail
{
constexpr const char* revisionId = "factory_tone_revision";
inline float choice(Cs80AudioProcessor& host)
{
    const auto* p = host.getAPVTS().getRawParameterValue(revisionId);
    return p != nullptr ? p->load(std::memory_order_relaxed) : -1.0f;
}
inline void setChoice(Cs80AudioProcessor& host, float value)
{
    if (auto* p = host.getAPVTS().getParameter(revisionId))
        p->setValueNotifyingHost(p->convertTo0to1(value));
}
inline void block(Cs80AudioProcessor& host, int frames = 0, juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> audio(2, frames);
    audio.clear();
    host.processBlock(audio, midi);
}
inline void configure(Cs80AudioProcessor& host, bool extended)
{
    auto p = defaultValues();
    p[P_parityMode] = 0; p[P_sourcePerformance] = 1;
    p[P_voiceMode] = extended ? 1.0f : 0.0f;
    p[P_oversampling] = p[P_drift] = p[P_voiceDispersion] = p[P_analogNoise] = p[P_chorusOn] = 0;
    host.applyParameterValues(p);
    host.selectHardwareSource(0, 0);
    host.selectHardwareSource(1, 1);
}
inline void restore(Cs80AudioProcessor& host, const juce::ValueTree& tree)
{
    const auto text = tree.createXml()->toString();
    host.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
}
inline juce::ValueTree session(Cs80AudioProcessor& host)
{
    juce::MemoryBlock saved;
    host.getStateInformation(saved);
    const auto xml = juce::parseXML(saved.toString());
    return xml != nullptr ? juce::ValueTree::fromXml(*xml) : juce::ValueTree();
}
template <class Check>
void engineMatches(const Engine& engine, bool amended, Check check, const char* context)
{
    for (int rank = 0; rank < 2; ++rank)
    {
        source_factory::ToneControls expected;
        check(amended ? source_factory::compileTonePublicEstimate(rank, rank, expected)
                      : source_factory::compileTone(rank, rank, expected),
              "registered expected source compiles");
        const auto actual = engine.getSourceToneInfo(rank);
        check(actual.selector == rank
                  && actual.provenance.hypothesisVersion == expected.provenance.hypothesisVersion
                  && actual.provenance.hypothesisHash == expected.provenance.hypothesisHash
                  && actual.provenance.hypothesisOverrideMask == expected.provenance.hypothesisOverrideMask,
              context);
        const auto buses = engine.getSourceBusReadout(rank);
        for (size_t i = 0; i < buses.size(); ++i)
            check(buses[i] == expected.busVolts[i], context);
        check(!actual.provenance.hardwareMeasured, "host selection never claims measured hardware");
    }
}
inline bool readPoint(void*, int index, int& sample, double& value) noexcept
{
    if (index != 0) return false;
    sample = 0; value = 0.123456789;
    return true;
}
} // namespace factory_tone_host_detail

template <class Check>
void testFactoryToneRevisionHost(Check check)
{
    using namespace factory_tone_host_detail;
    juce::TemporaryFile root("-cs80-factory-tone-revision");
    auto owned = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
    auto& host = *owned;

    // Fixed independent pre-addition inventory. Do not derive expected rows
    // from the new layout, which would make an accidental reorder invisible.
    const auto path = std::filesystem::path(__FILE__).parent_path() / "FactoryToneParameterPrefix.tsv";
    std::ifstream inventory(path);
    check(inventory.good(), "frozen 297-parameter inventory is present");
    int rows = 0, highestHint = 0;
    std::string line;
    while (std::getline(inventory, line))
    {
        std::istringstream row(line);
        int index = -1, hint = -1, automatable = -1;
        std::string id, bits;
        row >> index >> id >> hint >> bits >> automatable;
        check(!row.fail() && index == rows, "inventory row is complete and contiguous");
        auto* p = index >= 0 && index < host.getParameters().size()
                    ? dynamic_cast<juce::RangedAudioParameter*>(host.getParameters()[index]) : nullptr;
        check(p != nullptr && p->paramID == juce::String(id), "old parameter index and ID unchanged");
        check(p != nullptr && p->getVersionHint() == hint, "old AU version hint unchanged");
        check(p != nullptr && p->isAutomatable() == (automatable == 1), "old automation flag unchanged");
        check(p != nullptr && std::bit_cast<std::uint32_t>(p->getDefaultValue()) == std::stoul(bits, nullptr, 16),
              "old normalized default float bits unchanged");
        highestHint = std::max(highestHint, hint);
        ++rows;
    }
    check(rows == 297 && highestHint == 16, "entire original parameter prefix and maximum hint locked");
    check(host.getParameters().size() == 298, "exactly one append-only revision parameter");

    // This witness is independent of the new choice's existence. It exposes
    // the pre-existing same-block extension publication gap before a fix.
    configure(host, true);
    host.prepareToPlay(48000, 32);
    block(host);
    check(host.getVoiceCapacity() == 16, "Program Change witness has an active extension");
    juce::MidiBuffer program;
    program.addEvent(juce::MidiMessage::programChange(1, 3), 10);
    block(host, 32, program);
    const auto mainInfo = host.getEngine().getSourceToneInfo(0);
    const auto extensionInfo = host.getExtensionEngine().getSourceToneInfo(0);
    std::printf("Program Change witness: main selector=%d extension selector=%d main voice mode=%g extension voice mode=%g\n",
                mainInfo.selector, extensionInfo.selector, host.getEngine().getParams()[P_voiceMode],
                host.getExtensionEngine().getParams()[P_voiceMode]);
    check(mainInfo.selector == 13 && extensionInfo.selector == 13
              && host.getEngine().getParams().v == host.getExtensionEngine().getParams().v,
          "MIDI Program Change publishes the same aggregate frame to both active engines");

    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(host.getAPVTS().getParameter(revisionId));
    check(parameter != nullptr, "explicit versioned factory-tone choice exists");
    if (parameter == nullptr)
    {
        std::puts("Remaining revision-dependent checks NOT RUN: parameter absent (baseline RED).");
        return;
    }
    check(parameter->getParameterIndex() == 297 && parameter->getVersionHint() == 17,
          "revision choice appends index297 with AU hint17");
    check(!parameter->isAutomatable() && static_cast<juce::AudioProcessorParameter*>(parameter)->getDefaultValue() == 0.0f && choice(host) == 0,
          "factory revision is non-automatable and defaults to frozen V1");
    check(parameter->choices == juce::StringArray{"Frozen V1", "Public-source estimates V2"},
          "choice labels make the opt-in estimate and frozen revision explicit");
    check(Cs80AudioProcessor::stateSchemaVersion == 3, "existing ENGINE migration schema unchanged");

    configure(host, true);
    host.prepareToPlay(48000, 32);
    engineMatches(host.getEngine(), false, check, "prepare retains default V1");
    block(host);
    engineMatches(host.getExtensionEngine(), false, check, "extension defaults to V1");
    setChoice(host, 1);
    // Parameter/UI writes must not mutate either engine before an audio transaction.
    engineMatches(host.getEngine(), false, check, "non-audio choice edit does not mutate main engine");
    block(host);
    engineMatches(host.getEngine(), true, check, "main adopts explicit V2 at transaction boundary");
    engineMatches(host.getExtensionEngine(), true, check, "active extension adopts the same V2");
    const auto mainCount = host.getEngine().getSourceCompileCount();
    const auto extensionCount = host.getExtensionEngine().getSourceCompileCount();
    block(host);
    check(host.getEngine().getSourceCompileCount() == mainCount
              && host.getExtensionEngine().getSourceCompileCount() == extensionCount,
          "repeated unchanged revision does not recompile either engine");
    host.setParamValue(P_voiceMode, 0);
    block(host);
    setChoice(host, 0);
    block(host);
    setChoice(host, 1);
    host.setParamValue(P_voiceMode, 1);
    block(host);
    engineMatches(host.getExtensionEngine(), true, check, "late extension activation receives current V2");
    host.prepareToPlay(48000, 32);
    engineMatches(host.getEngine(), true, check, "prepare preserves explicit saved V2");
    block(host);

    // One deterministic snapshot, then a changed live atomic, then the actual
    // automation publisher. Reading the choice inside a frame helper is wrong.
    auto* volume = host.getAPVTS().getParameter(juce::String(paramId(P_volume)));
    check(volume != nullptr, "transaction witness has a real sample-accurate volume parameter");
    if (volume != nullptr)
    {
        host.beginHostAutomationBlock(32);
        check(host.attachHostAutomationQueue(*volume, {nullptr, 1, readPoint, nullptr, nullptr}),
              "transaction witness attaches actual host curve");
        FactoryToneRevisionAccess::snapshot(host);
        setChoice(host, 0);
        FactoryToneRevisionAccess::publishAutomation(host, 0);
        engineMatches(host.getEngine(), true, check, "automation main uses captured V2, not new live V1");
        engineMatches(host.getExtensionEngine(), true, check, "automation extension uses the same captured V2");
        host.endHostAutomationBlock();
        block(host);
        engineMatches(host.getEngine(), false, check, "next transaction adopts new V1");
    }

    // The concurrent producer writes only the actual lock-free APVTS atomic,
    // not ValueTree or engine state. Deterministic witness above is the primary
    // removed-guard oracle; this bounded stress is supplemental race coverage.
    auto* raw = host.getAPVTS().getRawParameterValue(revisionId);
    if (raw != nullptr)
    {
        std::atomic<bool> start{false}, done{false};
        std::thread writer([&] {
            while (!start.load(std::memory_order_acquire)) std::this_thread::yield();
            while (!done.load(std::memory_order_acquire))
            {
                raw->store(0, std::memory_order_relaxed);
                raw->store(1, std::memory_order_relaxed);
            }
        });
        start.store(true, std::memory_order_release);
        bool coherent = true;
        for (int i = 0; i < 128; ++i)
        {
            block(host);
            for (int rank = 0; rank < 2; ++rank)
                coherent = coherent && host.getEngine().getSourceBusReadout(rank)
                                      == host.getExtensionEngine().getSourceBusReadout(rank)
                    && host.getEngine().getSourceToneInfo(rank).provenance.hypothesisVersion
                       == host.getExtensionEngine().getSourceToneInfo(rank).provenance.hypothesisVersion;
        }
        done.store(true, std::memory_order_release);
        writer.join();
        check(coherent, "128 concurrent-write transactions keep main/extension revision coherent");
        // Restore the parameter object and its raw cache to the same value.
        setChoice(host, 0); setChoice(host, 1);
    }

    // Session and preset round-trip; revision is sound state, not live setup.
    configure(host, true);
    setChoice(host, 1);
    const auto saved = session(host);
    check(saved.isValid() && saved.getProperty(Cs80AudioProcessor::stateSchemaKey).toString() == "3",
          "session retains the existing schema marker");
    setChoice(host, 0);
    restore(host, saved);
    check(choice(host) == 1, "session round-trips explicit V2");
    block(host);
    engineMatches(host.getEngine(), true, check, "restored V2 reaches actual main engine");
    engineMatches(host.getExtensionEngine(), true, check, "restored V2 reaches actual extension");
    const auto preset = host.saveUserPreset("Revision V2");
    check(preset.wasSaved(), "isolated V2 user preset saved");
    setChoice(host, 0);
    host.loadUserPreset(preset.file);
    check(choice(host) == 1, "user preset recalls V2 rather than retaining live V1");

    for (int schema : {1, 2, 3})
    {
        auto missing = saved.createCopy();
        missing.setProperty(Cs80AudioProcessor::stateSchemaKey, schema, nullptr);
        missing.removeChild(missing.getChildWithProperty("id", revisionId), nullptr);
        setChoice(host, 1);
        restore(host, missing);
        check(choice(host) == 0, "missing revision always restores frozen V1, never live V2");
    }
    for (const char* encoded : {"", "nan", "inf", "-inf", "-1", "2", "0.5", "garbage", "1oops", "1 0"})
    {
        auto malformed = saved.createCopy();
        malformed.getChildWithProperty("id", revisionId).setProperty("value", encoded, nullptr);
        setChoice(host, 1);
        restore(host, malformed);
        check(choice(host) == 0, "malformed revision restores V1 without clamping into opt-in");
    }
    for (const char* encoded : {"1", "1.0", "1e0", " 1 "})
    {
        auto literal = saved.createCopy();
        literal.getChildWithProperty("id", revisionId).setProperty("value", encoded, nullptr);
        restore(host, literal);
        check(choice(host) == 1, "complete numeric one restores V2 literally");
    }
    auto missingValue = saved.createCopy();
    missingValue.getChildWithProperty("id", revisionId).removeProperty("value", nullptr);
    restore(host, missingValue);
    check(choice(host) == 0, "missing revision value is V1 even when its PARAM child exists");
    auto oldPreset = saved.createCopy();
    oldPreset.removeChild(oldPreset.getChildWithProperty("id", revisionId), nullptr);
    juce::TemporaryFile oldPresetFile(".cs80xml");
    check(oldPreset.createXml()->writeTo(oldPresetFile.getFile()), "old preset fixture written");
    setChoice(host, 1);
    host.loadUserPreset(oldPresetFile.getFile());
    check(choice(host) == 0, "old user preset does not inherit live V2");

    setChoice(host, 1);
    host.selectHardwareSource(0, 2);
    check(choice(host) == 1, "hardware tab selection preserves explicit revision");
    host.randomiseWithinOriginalRanges(0x802026);
    check(choice(host) == 1, "randomise preserves interpretation revision");
    for (int index : {22, legacyProgramCount + 3})
    {
        setChoice(host, 1);
        host.loadFactoryPreset(index);
        check(choice(host) == 0, "shipped factory recall resets to frozen V1");
    }
    setChoice(host, 1);
    host.loadInitPatch();
    check(choice(host) == 0, "INIT preserves the frozen default contract");
    configure(host, true);
    setChoice(host, 1);
    block(host);
    block(host, 32, program);
    check(choice(host) == 0, "accepted MIDI Program Change resets revision to frozen V1");
    setChoice(host, 1);
    host.setParamValue(P_parityMode, 1);
    block(host);
    block(host, 32, program);
    check(choice(host) == 1, "ignored PURE Program Change does not reset revision");
    host.setParamValue(P_parityMode, 0);

    // Inactive A/B slot is independent sound state, including revision.
    setChoice(host, 1);
    host.abCopyCurrentTo(1);
    setChoice(host, 0);
    host.abToggle();
    check(choice(host) == 1, "A/B recalls the copied V2 sound");
    host.abToggle();
    check(choice(host) == 0, "A/B returns to the frozen V1 sound");
    const auto abSaved = session(host);
    restore(host, abSaved);
    host.abToggle();
    check(choice(host) == 1, "inactive A/B revision survives a session round-trip");
    for (const char* encoded : {"missing", "", "0.5", "2", "nan", "1oops"})
    {
        auto abOld = abSaved.createCopy();
        auto slot = abOld.getChildWithName("AB_COMPARE").getChildWithName("SLOT");
        check(slot.isValid(), "inactive-slot migration fixture exists");
        if (juce::String(encoded) == "missing") slot.removeProperty(revisionId, nullptr);
        else slot.setProperty(revisionId, encoded, nullptr);
        restore(host, abOld);
        host.abToggle();
        check(choice(host) == 0, "old or malformed inactive A/B revision restores V1");
    }
}
} // namespace cs80::tests
