#pragma once

#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <chrono>
#include <cstdio>
#include <vector>

namespace cs80::tests
{
namespace ui_readiness_detail
{
template <typename ComponentType>
ComponentType* findComponent(juce::Component& parent, const juce::String& id = {})
{
    for (auto* child : parent.getChildren())
    {
        if (auto* candidate = dynamic_cast<ComponentType*>(child))
            if (id.isEmpty() || candidate->getComponentID() == id)
                return candidate;
        if (auto* found = findComponent<ComponentType>(*child, id))
            return found;
    }
    return nullptr;
}

// Observe all host parameters, not just the displayed selectors: a refresh
// must neither write values nor emit redundant host value/gesture callbacks.
class RefreshProbe final : private juce::AudioProcessorParameter::Listener
{
public:
    explicit RefreshProbe(Cs80AudioProcessor& p) : processor(p), state(p.getAPVTS().copyState())
    {
        for (auto* parameter : processor.getParameters())
        {
            values.push_back(parameter->getValue());
            parameter->addListener(this);
        }
    }

    ~RefreshProbe() override
    {
        for (auto* parameter : processor.getParameters())
            parameter->removeListener(this);
    }

    bool unchanged()
    {
        const auto& parameters = processor.getParameters();
        if (valueChanges != 0 || gestures != 0 || (size_t) parameters.size() != values.size())
            return false;
        for (int i = 0; i < parameters.size(); ++i)
            if (parameters[i]->getValue() != values[(size_t) i])
                return false;
        return state.isEquivalentTo(processor.getAPVTS().copyState());
    }

private:
    void parameterValueChanged(int, float) override { ++valueChanges; }
    void parameterGestureChanged(int, bool) override { ++gestures; }

    Cs80AudioProcessor& processor;
    juce::ValueTree state;
    std::vector<float> values;
    int valueChanges = 0, gestures = 0;
};

class ToggleGestureProbe final : public juce::AudioProcessorParameter::Listener
{
public:
    void parameterValueChanged(int, float) override
    {
        ++changes;
        if (depth != 1) balanced = false;
    }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting) { ++begins; if (depth++ != 0) balanced = false; }
        else { ++ends; if (--depth != 0) balanced = false; }
    }
    bool valid() const { return balanced && changes > 0 && begins == 1 && ends == 1 && depth == 0; }
    void reportIfInvalid(float request) const
    {
        if (request != 1.0f || !valid())
            std::fprintf(stderr,
                         "READINESS CLICK: request=%g changes=%d begins=%d ends=%d depth=%d balanced=%d\n",
                         static_cast<double>(request), changes, begins, ends, depth, balanced ? 1 : 0);
    }
private:
    int changes = 0, begins = 0, ends = 0, depth = 0;
    bool balanced = true;
};
} // namespace ui_readiness_detail

// Called by the existing UI smoke harness on its JUCE message thread. Flush
// must dispatch that thread's normal event loop; no extra GUI/audio thread or
// direct timer callback is used. In particular, never prepare/render this host:
// status must show requested APVTS state before the Engine has accepted it.
template <typename Check, typename Flush>
void runUiReadinessTests(Check check, Flush flush)
{
    using namespace ui_readiness_detail;
    check(juce::MessageManager::getInstance()->isThisTheMessageThread(),
          "readiness UI tests use the existing JUCE message thread");
    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
        return;

    // Negative controls for the one-balanced-gesture observer. They must
    // reject absent callbacks, an incomplete transaction, and duplicate
    // transactions before it is used to qualify the real SUB waveform switch.
    {
        ToggleGestureProbe missing;
        check(!missing.valid(), "readiness gesture gate rejects an omitted host transaction");
        ToggleGestureProbe incomplete;
        incomplete.parameterGestureChanged(0, true);
        incomplete.parameterValueChanged(0, 1.0f);
        check(!incomplete.valid(), "readiness gesture gate rejects an unbalanced host transaction");
        ToggleGestureProbe duplicate;
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            duplicate.parameterGestureChanged(0, true);
            duplicate.parameterValueChanged(0, 1.0f);
            duplicate.parameterGestureChanged(0, false);
        }
        check(!duplicate.valid(), "readiness gesture gate rejects two complete host transactions");
    }

    // Opening the browser as part of editor construction must never inspect or
    // alter a real user's library. This fresh, uncreated root is fixture-only.
    juce::TemporaryFile presetRoot("-cs80-ui-readiness");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    // Old API requests are accepted as compatibility input, but cannot
    // create another engine or suppress the circuit instrument's controls.
    host.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);
    host.setPure(true);
    RefreshProbe construction(host);
    Cs80AudioProcessorEditor editor(host);
    // isShowing() requires a real native peer even if every ancestor's visible
    // flag is true. Keep the visibility assertion meaningful: this temporary
    // fixture owns its window, removed automatically with the editor below.
    editor.addToDesktop(0);
    editor.setVisible(true);
    check(editor.getPeer() != nullptr && editor.isShowing(),
          "readiness visibility fixture has a showing native editor peer");
    check(construction.unchanged(), "opening readiness UI preserves all APVTS values and metadata");

    auto* mode = findComponent<juce::Label>(editor, "performance_mode_status");
    auto* rankI = findComponent<juce::Label>(editor, "source_route_status_1");
    auto* rankII = findComponent<juce::Label>(editor, "source_route_status_2");
    auto* pedal = findComponent<juce::Component>(editor, "expression_pedal");
    auto* tooltips = findComponent<juce::TooltipWindow>(editor);
    auto* sourceToggle = findComponent<juce::Button>(editor, "source_performance");
    auto* isolationToggle = findComponent<juce::Button>(editor, "parity_mode");
    auto* originalMode = findComponent<juce::Button>(editor, "sub_original_mode");
    check(mode != nullptr && rankI != nullptr && rankII != nullptr && pedal != nullptr && originalMode != nullptr,
          "circuit status, both rank routes, expression pedal and independent SUB control have stable UI IDs");
    check(sourceToggle == nullptr && isolationToggle == nullptr
              && findComponent<juce::Button>(editor, "render_policy_legacy") == nullptr,
          "readiness UI contains no selectable engine or isolation policies");
    if (mode == nullptr || rankI == nullptr || rankII == nullptr || pedal == nullptr || originalMode == nullptr)
        return;

    const auto has = [](const juce::String& text, const char* fragment)
    {
        return text.containsIgnoreCase(fragment);
    };
    const auto rankShows = [&](juce::Label* label, const char* rank,
                               const char* identity, const char* route)
    {
        return label->getText().startsWith(rank) && has(label->getText(), identity)
            && has(label->getText(), route);
    };
    const auto panels = [&]
    {
        return rankShows(rankI, "I REQUEST:", "MAIN PANEL", "/ PANEL")
            && rankShows(rankII, "II REQUEST:", "MAIN PANEL", "/ PANEL");
    };
    const auto sourceTones = [&]
    {
        return rankShows(rankI, "I REQUEST:", "STRING 3", "SOURCE-COMPLETE")
            && rankShows(rankII, "II REQUEST:", "STRING 2", "SOURCE-INFERRED");
    };
    const auto refresh = [&](auto ready, const char* assertion)
    {
        RefreshProbe probe(host);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (! ready() && std::chrono::steady_clock::now() < deadline)
            flush(20);
        // Also exercise an ordinary refresh when the constructor already
        // supplied the expected text; the editor timer runs at 8 Hz.
        flush(160);
        check(ready(), assertion);
        check(probe.unchanged(), "readiness status refresh does not mutate APVTS or notify the host");
    };
    const auto automate = [&](const juce::String& id, float physicalValue)
    {
        auto* parameter = host.getAPVTS().getParameter(id);
        check(parameter != nullptr, "readiness automation uses an existing host parameter");
        if (parameter != nullptr)
            parameter->setValueNotifyingHost(parameter->convertTo0to1(physicalValue));
    };
    const auto isolation = [&](bool enabled)
    {
        automate(Cs80AudioProcessor::parameterId(cs80::P_parityMode), enabled ? 1.0f : 0.0f);
    };
    const auto sourcePerformance = [&](bool enabled)
    {
        automate(Cs80AudioProcessor::parameterId(cs80::P_sourcePerformance), enabled ? 1.0f : 0.0f);
    };
    const auto source = [&](int rank, int selection)
    {
        automate(Cs80AudioProcessor::hardwareSourceParameterId(rank), (float) selection);
    };

    const auto circuit = [&]
    {
        return mode->getText() == "Circuit engine"
            && host.getParamValue(cs80::P_sourcePerformance) == 1.0f
            && host.getParamValue(cs80::P_parityMode) == 0.0f;
    };
    refresh([&] { return panels() && circuit(); },
            "fresh editor reports both PANEL routes and the fixed circuit engine before audio renders");
    check(host.getHardwareSource(0) == 13 && host.getHardwareSource(1) == 13,
          "readiness display leaves both initial PANEL selections intact");
    check(has(mode->getTooltip(), "Every program uses the circuit engine")
              && has(mode->getTooltip(), "optional effects, modulation and performance controls")
              && !has(mode->getTooltip(), "isolation") && !has(mode->getTooltip(), "legacy"),
          "circuit status explains the instrument without obsolete mode choices");
    std::vector<float> retainedPanel;
    for (int rank = 0; rank < 2; ++rank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local))
                retainedPanel.push_back(host.getParamValue(cs80::chParam(rank, local)));
    check(retainedPanel.size() == 52, "readiness fixture observes exactly 26 retained programmer controls per rank");

    source(0, 1);
    source(1, 0);
    refresh([&] { return sourceTones() && circuit(); },
            "requested source identities show complete and inferred classifications before any audio render");

    // A real sound control must still issue one balanced host gesture. The
    // removed policy switches cannot provide this positive control; use the
    // independently editable original/extended SUB waveform selection.
    automate(Cs80AudioProcessor::parameterId(cs80::P_subOriginalMode), 0.0f);
    editor.showAdvancedPanel(true);
    editor.showAdvancedPage(1);
    refresh([&] { return originalMode->isShowing() && !originalMode->getToggleState(); },
            "the SUB waveform switch remains visible and follows a host edit");
    auto* subParameter = host.getAPVTS().getParameter("sub_original_mode");
    check(subParameter != nullptr, "SUB waveform switch retains its real host parameter");
    if (subParameter != nullptr)
    {
        ToggleGestureProbe probe;
        subParameter->addListener(&probe);
        originalMode->triggerClick();
        // Observe the asynchronous result, not an arbitrary fixed sleep.
        const auto clickDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while ((host.getParamValue(cs80::P_subOriginalMode) != 1.0f || !probe.valid())
               && std::chrono::steady_clock::now() < clickDeadline)
            flush(20);
        flush(160);
        probe.reportIfInvalid(host.getParamValue(cs80::P_subOriginalMode));
        subParameter->removeListener(&probe);
        check(host.getParamValue(cs80::P_subOriginalMode) == 1.0f && probe.valid(),
              "SUB waveform switch sends exactly one balanced host gesture");
    }
    editor.showAdvancedPage(3);
    check(findComponent<juce::Button>(editor, "source_performance") == nullptr
              && findComponent<juce::Button>(editor, "parity_mode") == nullptr
              && findComponent<juce::Button>(editor, "render_policy_legacy") == nullptr,
          "opening Settings cannot recreate retired engine selectors");
    editor.showAdvancedPanel(false);

    // All four combinations of obsolete host values must retain the same
    // classifications and controls, with no change to any live sound value.
    for (bool requestSource : { false, true })
        for (bool requestIsolation : { false, true })
        {
            const auto before = host.currentValues();
            sourcePerformance(requestSource);
            isolation(requestIsolation);
            refresh([&] { return sourceTones() && circuit(); },
                    "obsolete host policy writes cannot change source classifications or circuit status");
            check(host.currentValues().v == before.v
                      && host.getHardwareSource(0) == 1 && host.getHardwareSource(1) == 0,
                  "obsolete policy writes preserve the entire sound and both requested source selections");
        }
    for (auto* label : { rankI, rankII })
    {
        const auto tip = label->getTooltip();
        check(has(tip, "Requested APVTS selection") && has(tip, "not an audio-thread acknowledgment")
                  && has(tip, "source-matrix") && has(tip, "not that the instrument is calibrated")
                  && has(tip, "device laws remain inferred") && has(tip, "unresolved cells"),
              "rank help distinguishes requested source completeness from calibrated device fidelity");
        check(has(tip, "26 programmer controls") && has(tip, "retained for PANEL")
                  && has(tip, "Footage and global controls remain live")
                  && has(tip, "Library programs reset both selectors to PANEL"),
              "rank help explains retained panel controls and library-to-PANEL recall");
    }

    size_t panelIndex = 0;
    bool panelUnchanged = true;
    for (int rank = 0; rank < 2; ++rank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local))
                panelUnchanged = (host.getParamValue(cs80::chParam(rank, local)) == retainedPanel[panelIndex++])
                    && panelUnchanged;
    check(panelUnchanged && panelIndex == retainedPanel.size(),
          "source displays and obsolete policy writes preserve all 26 stored panel controls per rank");

    source(0, 11);
    source(1, 12);
    refresh([&]
    {
        return rankShows(rankI, "I REQUEST:", "BANK 1", "/ MEMORY")
            && rankShows(rankII, "II REQUEST:", "BANK 4", "/ MEMORY");
    }, "memory routing identifies rank-I bank 1 and rank-II bank 4");
    source(0, 12);
    source(1, 11);
    refresh([&]
    {
        return rankShows(rankI, "I REQUEST:", "BANK 3", "/ MEMORY")
            && rankShows(rankII, "II REQUEST:", "BANK 2", "/ MEMORY");
    }, "alternate memory routing identifies rank-I bank 3 and rank-II bank 2");

    for (int program : { 0, cs80::programIndexForCurated(0) })
    {
        source(0, 1);
        source(1, 0);
        sourcePerformance(false);
        isolation(true);
        host.loadFactoryPreset(program);
        refresh([&] { return panels() && circuit(); },
                "either factory bank returns both ranks to PANEL within the same circuit instrument");
        check(host.getHardwareSource(0) == 13 && host.getHardwareSource(1) == 13,
              "factory recall exposes its authored panel voicing without selecting a hardware tone");
    }

    {
        RefreshProbe probe(host);
        const auto description = pedal->getDescription();
        check(has(pedal->getTitle(), "expression") && has(description, "MIDI CC11")
                  && has(description, "circuit engine") && has(description, "EXP-WAH")
                  && !has(description, "isolation") && !has(description, "legacy"),
              "pedal accessibility help describes live CC11 and the on-screen circuit foot controller");
        check(tooltips != nullptr && tooltips->getTipFor(*pedal) == description,
              "actual editor tooltip provider exposes pedal accessibility help on hover");
        check(has(mode->getTooltip(), "Every program uses the circuit engine")
                  && !has(mode->getTooltip(), "resets sounding notes")
                  && !has(mode->getTooltip(), "switches"),
              "circuit status cannot advertise a removed engine transition");
        check(probe.unchanged(), "querying readiness hover help does not mutate APVTS or notify the host");
    }
}
} // namespace cs80::tests
