#pragma once

#include "HostParameterState.h"
#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"

#include <chrono>
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
    explicit RefreshProbe(Cs80AudioProcessor& p) : processor(p), state(cs80::tests::copyHostParameterState(p))
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
        return state.isEquivalentTo(cs80::tests::copyHostParameterState(processor));
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

    // Opening the browser as part of editor construction must never inspect or
    // alter a real user's library. This fresh, uncreated root is fixture-only.
    juce::TemporaryFile presetRoot("-cs80-ui-readiness");
    Cs80AudioProcessor host(presetRoot.getFile().getChildFile("presets"));
    // Plan 1.7 (ws2): fresh instances default to the circuit engine; this fixture
    // exercises the legacy-start contract, so it selects Legacy (compat) explicitly.
    host.setEngineMode(Cs80AudioProcessor::EngineMode::legacy);
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
    check(mode != nullptr && rankI != nullptr && rankII != nullptr && pedal != nullptr
              && sourceToggle != nullptr && isolationToggle != nullptr,
          "requested mode, both rank routes, expression pedal and policy toggles have stable UI IDs");
    if (mode == nullptr || rankI == nullptr || rankII == nullptr || pedal == nullptr
        || sourceToggle == nullptr || isolationToggle == nullptr)
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

    refresh([&] { return panels() && has(mode->getText(), "LEGACY PERFORMANCE"); },
            "fresh editor reports both requested PANEL routes and legacy performance");
    check(host.getHardwareSource(0) == 13 && host.getHardwareSource(1) == 13
              && host.getParamValue(cs80::P_parityMode) == 0.0f
              && host.getParamValue(cs80::P_sourcePerformance) == 0.0f
              && !sourceToggle->getToggleState() && !isolationToggle->getToggleState(),
          "readiness display leaves initial selectors and both opt-in policy defaults intact");
    check(sourceToggle->getButtonText() == "SOURCE PERFORMANCE"
              && isolationToggle->getButtonText() == "PARITY ISOLATION"
              && isolationToggle->getTooltip().contains("not calibrated")
              && isolationToggle->getTooltip().contains("No hardware match"),
          "Advanced About clearly distinguishes source performance from uncalibrated isolation");
    const auto sourceHelp = sourceToggle->getTooltip();
    check(has(sourceHelp, "current source models") && has(sourceHelp, "CC1/11/64/74")
              && has(sourceHelp, "optional MPE") && has(sourceHelp, "software overlays")
              && has(sourceHelp, "effective mode change stops notes")
              && has(sourceHelp, "is masked and does not stop notes")
              && has(sourceHelp, "26 programmer controls")
              && has(sourceToggle->getDescription(), "no hardware match"),
          "source toggle help describes controller policy, masked requests, note resets and source ownership");
    std::vector<float> retainedPanel;
    for (int rank = 0; rank < 2; ++rank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local))
                retainedPanel.push_back(host.getParamValue(cs80::chParam(rank, local)));
    check(retainedPanel.size() == 52, "readiness fixture observes exactly 26 retained programmer controls per rank");

    source(0, 1);
    source(1, 0);
    editor.showAdvancedPanel(true);
    editor.showAdvancedPage(3);
    check(sourceToggle->isShowing() && isolationToggle->isShowing()
              && !sourceToggle->getBounds().intersects(isolationToggle->getBounds()),
          "both Advanced About policy switches are visible and nonoverlapping");
    auto* sourceParameter = host.getAPVTS().getParameter("source_performance");
    check(sourceParameter != nullptr, "source policy toggle has an APVTS host parameter");
    if (sourceParameter != nullptr)
    {
        ToggleGestureProbe probe;
        sourceParameter->addListener(&probe);
        sourceToggle->triggerClick();
        flush(60);
        sourceParameter->removeListener(&probe);
        check(host.getParamValue(cs80::P_sourcePerformance) == 1.0f && probe.valid(),
              "source toggle sends one balanced host gesture and enables its stored request");
    }
    editor.showAdvancedPanel(false);
    refresh([&] { return sourceTones() && has(mode->getText(), "SOURCE PERFORMANCE"); },
            "source-performance request shows complete and inferred source routes before any audio render");
    check(has(mode->getText(), "NOT CALIBRATED") && has(mode->getText(), "BEND + CC1/11/64/74 LIVE")
              && has(mode->getText(), "MPE OPTIONAL") && has(mode->getText(), "MODERN OVERLAYS LIVE")
              && has(mode->getText(), "MODE CHANGE STOPS NOTES") && sourceToggle->getToggleState(),
          "source-performance status explains the live controller and modern-overlay policy without a calibration claim");

    isolation(true);
    refresh([&] { return sourceTones() && has(mode->getText(), "COMPARISON ISOLATION"); },
            "both flags ON selects comparison isolation while preserving complete and inferred source routes");
    const auto isolatedPolicy = mode->getText();
    check(has(isolatedPolicy, "NOT CALIBRATED") && has(isolatedPolicy, "MIDI OFF")
              && has(isolatedPolicy, "BEND") && has(isolatedPolicy, "CC1/11/64/74")
              && has(isolatedPolicy, "MPE") && has(isolatedPolicy, "PRESSURE")
              && has(isolatedPolicy, "RIBBON") && has(isolatedPolicy, "UI PEDAL LIVE")
              && has(isolatedPolicy, "MODE CHANGE STOPS NOTES"),
          "isolation status explains controller masks, live physical controls and note reset");
    check(sourceToggle->getToggleState() && isolationToggle->getToggleState()
              && has(mode->getTooltip(), "Source-performance request is ON but masked by isolation"),
          "both stored requests remain visible while mode help makes isolation precedence explicit");
    sourcePerformance(false);
    refresh([&] { return sourceTones() && mode->getText() == isolatedPolicy && !sourceToggle->getToggleState(); },
            "turning the masked source request OFF does not change the requested isolation policy");
    sourcePerformance(true);
    refresh([&] { return sourceTones() && mode->getText() == isolatedPolicy && sourceToggle->getToggleState(); },
            "host automation restores the masked source request without changing isolation labels");
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

    isolation(false);
    refresh([&] { return sourceTones() && has(mode->getText(), "SOURCE PERFORMANCE"); },
            "disabling isolation reveals the retained source-performance request rather than legacy mode");
    sourcePerformance(false);
    refresh([&]
    {
        return rankShows(rankI, "I REQUEST:", "STRING 3", "LEGACY STUDY")
            && rankShows(rankII, "II REQUEST:", "STRING 2", "LEGACY STUDY")
            && has(mode->getText(), "LEGACY PERFORMANCE") && has(mode->getText(), "SOURCE MODEL OFF");
    }, "both switches OFF reclassifies the same requested tones as legacy studies");
    check(host.getHardwareSource(0) == 1 && host.getHardwareSource(1) == 0
              && has(mode->getText(), "BEND + CC1/11/64/74 LIVE")
              && has(mode->getText(), "MPE OPTIONAL") && !sourceToggle->getToggleState(),
          "legacy performance restores MIDI policy without resetting requested sources");

    isolation(true);
    refresh([&] { return sourceTones() && has(mode->getText(), "COMPARISON ISOLATION"); },
            "switching back to isolation retains both source identities and restores their classifications");
    check(host.getHardwareSource(0) == 1 && host.getHardwareSource(1) == 0,
          "mode round trip preserves both hardware source selections");
    size_t panelIndex = 0;
    bool panelUnchanged = true;
    for (int rank = 0; rank < 2; ++rank)
        for (int local = 0; local < cs80::CP_count; ++local)
            if (Cs80AudioProcessor::isMemoryControl(local))
                panelUnchanged = (host.getParamValue(cs80::chParam(rank, local)) == retainedPanel[panelIndex++])
                    && panelUnchanged;
    check(panelUnchanged && panelIndex == retainedPanel.size(),
          "source-policy changes and requested-route displays preserve all 26 stored panel controls per rank");

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

    sourcePerformance(true);
    host.loadFactoryPreset(0);
    refresh([&] { return panels() && has(mode->getText(), "COMPARISON ISOLATION"); },
            "library program recall returns both requested routes to PANEL while retaining isolation");
    check(host.getHardwareSource(0) == 13 && host.getHardwareSource(1) == 13
              && host.getParamValue(cs80::P_sourcePerformance) == 0.0f && !sourceToggle->getToggleState(),
          "library-to-PANEL UI resets source performance OFF while preserving session isolation");
    isolation(false);
    sourcePerformance(true);
    refresh([&] { return panels() && has(mode->getText(), "SOURCE PERFORMANCE"); },
            "source performance can be requested for PANEL without selecting a hardware factory tab");
    host.loadFactoryPreset(0);
    refresh([&] { return panels() && has(mode->getText(), "LEGACY PERFORMANCE"); },
            "ordinary library recall returns source-performance mode to its legacy default");

    {
        RefreshProbe probe(host);
        const auto description = pedal->getDescription();
        check(has(pedal->getTitle(), "expression") && has(description, "MIDI CC11")
                  && has(description, "isolation OFF") && has(description, "CC11 is ignored")
                  && has(description, "on-screen pedal remains active") && has(description, "EXP-WAH"),
              "pedal accessibility help explains the CC11 isolation exception and live UI pedal");
        check(tooltips != nullptr && tooltips->getTipFor(*pedal) == description,
              "actual editor tooltip provider exposes pedal accessibility help on hover");
        check(has(mode->getTooltip(), "audio applies it on a control frame")
                  && has(mode->getTooltip(), "resets sounding notes")
                  && has(mode->getTooltip(), "does not stop notes")
                  && has(mode->getTooltip(), "Isolation wins when both switches are ON")
                  && has(mode->getTooltip(), "Library recall returns both ranks to PANEL"),
              "mode help distinguishes requested state, note reset and library recall policy");
        check(probe.unchanged(), "querying readiness hover help does not mutate APVTS or notify the host");
    }
}
} // namespace cs80::tests
