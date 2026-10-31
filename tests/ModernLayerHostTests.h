#pragma once

// ws5-modern host self-test, run by CS80Snapshots --self-test (src/tools/ui_snapshot.cpp):
// - modern layer session data: ValueTree round trip, old/future/malformed states, user presets;
// - host parameters (AU version hint 12, ranges, named values);
// - audio reaches the engine through processBlock, PURE CS-80 stays inert, host tempo sync.

#include "plugin/ModernLayerState.h"
#include "plugin/PluginProcessor.h"
#include "core/FxModels.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <set>

namespace cs80::tests
{
namespace modern_ui_detail
{
using namespace cs80::modern;

inline juce::MouseEvent mouse(juce::Component& c, juce::Point<float> at, juce::Point<float> downAt, int clicks = 1,
                              bool shift = false)
{
    const auto now = juce::Time::getCurrentTime();
    const int mods = juce::ModifierKeys::leftButtonModifier | (shift ? juce::ModifierKeys::shiftModifier : 0);
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys(mods), 1.0f, 0.0f, 0.0f,
                            0.0f, 0.0f, &c, &c, now, downAt, now, clicks, at != downAt);
}

inline void drag(juce::Component& c, juce::Point<float> from, juce::Point<float> to)
{
    c.mouseDown(mouse(c, from, from));
    c.mouseDrag(mouse(c, (from + to) * 0.5f, from));
    c.mouseDrag(mouse(c, to, from));
    c.mouseUp(mouse(c, to, from));
}

inline int distinctColours(const juce::Image& image)
{
    std::set<juce::uint32> colours;
    for (int y = 0; y < image.getHeight(); y += 7)
        for (int x = 0; x < image.getWidth(); x += 7)
            colours.insert(image.getPixelAt(x, y).getARGB());
    return (int) colours.size();
}

inline std::vector<float> renderBlocks(Cs80AudioProcessor& host, int blocks, bool note)
{
    juce::AudioBuffer<float> audio(2, 256);
    juce::MidiBuffer midi;
    std::vector<float> out;
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (note && b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 110), 0);
        audio.clear();
        host.processBlock(audio, midi);
        for (int i = 0; i < audio.getNumSamples(); ++i) out.push_back(audio.getSample(0, i));
    }
    return out;
}

struct FixedPlayHead final : juce::AudioPlayHead
{
    double bpm = 120.0;
    bool reportsTempo = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        if (reportsTempo) info.setBpm(bpm);
        info.setIsPlaying(false);
        return info;
    }
};

inline ModernConfig routedConfig()
{
    ModernConfig c;
    c.slots[0] = { ModSource::velocity, ModSource::none, ModDestination::pitch, ModTarget::both };
    c.slots[3] = { ModSource::mseg2, ModSource::lfo1, ModDestination::lpCutoff, ModTarget::rankII };
    c.msegs[1].trigger = MsegTrigger::free;
    c.msegs[1].sync = true;
    c.msegs[2].points[1].level = 0.3f;
    c.msegs[2].bipolar = true;
    return sanitizeConfig(c);
}
} // namespace modern_ui_detail

template <typename Check>
void testModernLayerHost(Check check)
{
    using namespace modern_ui_detail;
    juce::TemporaryFile root("-cs80-modern");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    auto& layer = host.getModernLayer();

    // Fresh instance: defaults, and no child written until something is edited.
    check(layer.getConfig() == ModernConfig {}, "fresh modern layer holds the default shapes and an empty matrix");
    check(! host.getAPVTS().copyState().getChildWithName(ModernLayerState::treeType).isValid(),
          "a fresh instance does not add modern layer state until edited");

    // Host parameters: AU hint 12, declared ranges and defaults, named discrete values.
    for (int index = P_lfo1Wave; index <= P_fxGainCompensation; ++index)
    {
        auto* p = dynamic_cast<juce::RangedAudioParameter*>(host.getAPVTS().getParameter(juce::String(paramId(index))));
        const auto& d = paramDef(index);
        check(p != nullptr && p->getVersionHint() == 12 && p->getNormalisableRange().start == d.min
                  && p->getNormalisableRange().end == d.max && std::abs(p->convertFrom0to1(p->getDefaultValue()) - d.def) < 1e-5f,
              "modern layer host parameter has AU hint 12 and its declared range and default");
    }
    if (auto* wave = host.getAPVTS().getParameter("lfo1_wave"))
        check(wave->getText(wave->convertTo0to1(4.0f), 32) == "Square", "LFO waveform parameter shows its name");
    if (auto* model = host.getAPVTS().getParameter("fx2_model"))
        check(model->getText(model->convertTo0to1(3.0f), 32) == "Plate reverb", "FX model parameter shows its name");

    // Round trip through the plugin state.
    layer.setConfig(routedConfig());
    auto child = host.getAPVTS().copyState().getChildWithName(ModernLayerState::treeType);
    check(child.isValid() && (int) child.getProperty("version") == ModernLayerState::version,
          "edited modern layer is a versioned child of the plugin state");
    juce::MemoryBlock saved;
    host.getStateInformation(saved);
    Cs80AudioProcessor restored(root.getFile().getChildFile("presets"));
    restored.setStateInformation(saved.getData(), (int) saved.getSize());
    check(restored.getModernLayer().getConfig() == routedConfig(), "MSEG shapes and matrix routing survive a state round trip");

    // A session saved before the layer existed (no child) restores the defaults, even over
    // an edited instance; restoring the edited state brings the routing back.
    auto xml = juce::parseXML(juce::String::fromUTF8((const char*) saved.getData(), (int) saved.getSize()));
    if (xml != nullptr)
    {
        auto tree = juce::ValueTree::fromXml(*xml);
        auto old = tree.createCopy();
        old.removeChild(old.getChildWithName(ModernLayerState::treeType), nullptr);
        const auto oldText = old.createXml()->toString();
        restored.setStateInformation(oldText.toRawUTF8(), (int) oldText.getNumBytesAsUTF8());
        check(restored.getModernLayer().getConfig() == ModernConfig {}, "a state without modern layer data restores the defaults");

        // A later version with unknown fields: known fields still load.
        auto future = tree.createCopy();
        auto node = future.getChildWithName(ModernLayerState::treeType);
        node.setProperty("version", 9, nullptr);
        node.setProperty("somethingNew", "x", nullptr);
        node.addChild(juce::ValueTree("FUTURE_THING"), -1, nullptr);
        const auto futureText = future.createXml()->toString();
        restored.setStateInformation(futureText.toRawUTF8(), (int) futureText.getNumBytesAsUTF8());
        check(restored.getModernLayer().getConfig() == routedConfig(), "a later state version loads every field it understands");

        // Malformed values are sanitised, never trusted.
        auto broken = tree.createCopy();
        auto layerNode = broken.getChildWithName(ModernLayerState::treeType);
        for (auto mseg : layerNode)
            if (mseg.hasType("MSEG"))
            {
                mseg.setProperty("points", "nan 2 9;-4 x;1e9 -3 0", nullptr);
                mseg.setProperty("loopEnd", 99, nullptr);
                mseg.setProperty("loop", "banana", nullptr);
            }
        for (auto slot : layerNode.getChildWithName("MATRIX"))
            slot.setProperty("source", 1000, nullptr);
        const auto brokenText = broken.createXml()->toString();
        restored.setStateInformation(brokenText.toRawUTF8(), (int) brokenText.getNumBytesAsUTF8());
        const auto sane = restored.getModernLayer().getConfig();
        check(sane == sanitizeConfig(sane), "malformed modern layer state restores sanitised");
        bool sourcesCleared = true;
        for (const auto& s : sane.slots) sourcesCleared = sourcesCleared && s.source == ModSource::none;
        check(sourcesCleared, "out-of-range matrix sources are dropped");
    }

    // User presets carry the layer (it lives in the APVTS tree they save).
    {
        host.getModernLayer().setConfig(routedConfig());
        const auto result = host.saveUserPreset("ws5 modern preset");
        check(result.wasSaved(), "user preset with modern layer saves");
        host.getModernLayer().resetToDefaults();
        if (result.wasSaved()) host.loadUserPreset(result.file);
        check(host.getModernLayer().getConfig() == routedConfig(), "user presets carry MSEG shapes and matrix routing");
    }

    // Audio: routing + amount reaches the engine; PURE CS-80 leaves it inert.
    auto make = [&root](bool routed, bool pure)
    {
        auto p = std::make_unique<Cs80AudioProcessor>(root.getFile().getChildFile("presets"));
        ParamValues values;
        values[P_analogNoise] = values[P_drift] = values[P_voiceDispersion] = 0.0f;
        values[P_mod1Amount] = routed ? 0.5f : 0.0f;
        p->applyParameterValues(values);
        p->setParamValue(P_parityMode, pure ? 1.0f : 0.0f);
        if (routed) p->getModernLayer().setConfig(routedConfig());
        p->prepareToPlay(48000.0, 256);
        return p;
    };
    {
        auto plain = make(false, false), routed = make(true, false), pure = make(false, true), pureRouted = make(true, true);
        const auto a = renderBlocks(*plain, 40, true), b = renderBlocks(*routed, 40, true);
        const auto c = renderBlocks(*pure, 40, true), d = renderBlocks(*pureRouted, 40, true);
        check(a != b, "matrix routing in the session state reaches the audio");
        check(c == d, "PURE CS-80 render is unchanged by modern layer routing and amounts");
    }

    // Host tempo drives synced MSEGs; no tempo falls back to 120 BPM.
    for (bool reports : { true, false })
    {
        auto p = make(false, false);
        auto config = ModernConfig {};
        config.slots[0] = { ModSource::mseg1, ModSource::none, ModDestination::pan, ModTarget::both };
        config.msegs[0] = defaultShape(2); // 0 -> 1 over two units, one shot
        config.msegs[0].points[1].time = 8.0f;
        config.msegs[0].sync = true;
        config.msegs[0].trigger = MsegTrigger::free;
        p->getModernLayer().setConfig(config);
        p->setParamValue(P_mod1Amount, 1.0f);
        FixedPlayHead head;
        head.bpm = 90.0;
        head.reportsTempo = reports;
        p->setPlayHead(&head);
        renderBlocks(*p, 188, false);
        // The playhead is published at the start of each block, so it shows 187 blocks.
        const double beats = p->getModernLayer().getDisplayPosition(0);
        const double expected = 187.0 * 256.0 / 48000.0 * (reports ? 90.0 : 120.0) / 60.0;
        check(std::abs(beats - expected) < 0.01, "synced MSEG follows the host tempo (120 BPM fallback)");
        p->setPlayHead(nullptr);
    }
    root.getFile().deleteRecursively();
}

} // namespace cs80::tests
