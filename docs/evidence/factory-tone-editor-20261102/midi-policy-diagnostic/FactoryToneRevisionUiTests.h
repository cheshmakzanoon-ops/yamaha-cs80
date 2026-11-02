#pragma once

#include "FactoryToneRevisionHostTests.h"
#include "plugin/PluginEditor.h"
#include "plugin/StudioView.h"
#include "plugin/UiComponents.h"
#include <array>
#include <chrono>
#include <vector>

namespace cs80::tests
{
namespace factory_tone_ui_detail
{
template <class T>
T* findType(juce::Component& parent)
{
    if (auto* found = dynamic_cast<T*>(&parent)) return found;
    for (auto* child : parent.getChildren())
        if (auto* found = findType<T>(*child)) return found;
    return nullptr;
}
inline void collectId(juce::Component& parent, const juce::String& id,
                      std::vector<juce::Component*>& found)
{
    if (parent.getComponentID() == id) found.push_back(&parent);
    for (auto* child : parent.getChildren()) collectId(*child, id, found);
}
template <class Flush, class Ready>
bool waitFor(Flush flush, Ready ready)
{
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ready() && std::chrono::steady_clock::now() < until) flush(20);
    return ready();
}
inline std::vector<float> numericState(Cs80AudioProcessor& host)
{
    std::vector<float> values;
    for (auto* p : host.getParameters()) values.push_back(p->getValue());
    return values;
}
inline std::array<std::uint64_t, 4> engineTags(Cs80AudioProcessor& host)
{
    return { host.getEngine().getSourceToneInfo(0).provenance.hypothesisHash,
             host.getEngine().getSourceToneInfo(1).provenance.hypothesisHash,
             host.getExtensionEngine().getSourceToneInfo(0).provenance.hypothesisHash,
             host.getExtensionEngine().getSourceToneInfo(1).provenance.hypothesisHash };
}
inline bool engineRevisionMatches(const cs80::Engine& engine, bool amended)
{
    for (int rank = 0; rank < 2; ++rank)
    {
        cs80::source_factory::ToneControls expected;
        if (!(amended ? cs80::source_factory::compileTonePublicEstimate(rank, rank, expected)
                      : cs80::source_factory::compileTone(rank, rank, expected))) return false;
        const auto actual = engine.getSourceToneInfo(rank);
        if (actual.selector != rank
            || actual.provenance.hypothesisHash != expected.provenance.hypothesisHash
            || actual.provenance.hypothesisVersion != expected.provenance.hypothesisVersion
            || actual.provenance.hardwareMeasured) return false;
    }
    return true;
}
struct Gesture final : juce::AudioProcessorParameter::Listener
{
    int begins=0, ends=0, changes=0, depth=0, outside=0;
    void parameterValueChanged(int, float) override { ++changes; if(depth!=1)++outside; }
    void parameterGestureChanged(int, bool begin) override
    { if(begin){++begins;++depth;}else{++ends;--depth;} }
    bool balanced() const { return begins==1 && ends==1 && changes>0 && depth==0 && outside==0; }
};
inline std::vector<juce::Rectangle<int>> physicalBounds(juce::Component& editor)
{
    std::vector<juce::Rectangle<int>> bounds;
    const auto visit = [&](auto&& self, juce::Component& parent) -> void
    {
        if (auto* knob = dynamic_cast<cs80::ui::ParamKnob*>(&parent))
            bounds.push_back(editor.getLocalArea(knob, knob->getLocalBounds()));
        for (auto* child : parent.getChildren()) self(self, *child);
    };
    visit(visit, editor);
    return bounds;
}
} // namespace factory_tone_ui_detail

template <class Check, class Flush>
void testFactoryToneRevisionEditor(Check check, Flush flush)
{
    using namespace factory_tone_host_detail;
    using namespace factory_tone_ui_detail;
    juce::TemporaryFile root("-cs80-tone-editor");
    Cs80AudioProcessor host(root.getFile().getChildFile("presets"));
    configure(host, true);
    host.prepareToPlay(48000, 32);
    block(host);
    auto* parameter = dynamic_cast<juce::AudioParameterChoice*>(host.getAPVTS().getParameter(revisionId));
    check(parameter != nullptr && !parameter->isAutomatable() && choice(host)==0
              && parameter->choices==juce::StringArray{"Frozen V1","Public-source estimates V2"},
          "factory editor retains explicit non-automatable V1 host contract");
    if (parameter==nullptr) return;
    const auto numericBefore=numericState(host);
    const auto enginesBefore=engineTags(host);
    Cs80AudioProcessorEditor editor(host);
    editor.setSize(1600,1000);
    editor.setVisible(true);
    editor.showAdvancedPanel(true);
    auto* studio=findType<cs80::ui::StudioView>(editor);
    if(studio!=nullptr)studio->showTab(cs80::ui::StudioTab::settings);
    check(studio!=nullptr && waitFor(flush,[&]{return studio->getActiveTab()==cs80::ui::StudioTab::settings
                    && studio->getSubPage(cs80::ui::StudioTab::settings,0)!=nullptr;}),
          "factory revision editor fixture opens actual Settings page");
    check(numericState(host)==numericBefore,"opening factory revision settings changes no numeric host parameter");
    check(engineTags(host)==enginesBefore,"opening factory revision settings does not publish engine revision");
    std::vector<juce::Component*> matches;
    collectId(editor,revisionId,matches);
    auto* selector=matches.size()==1 ? dynamic_cast<juce::ComboBox*>(matches.front()) : nullptr;
    check(selector!=nullptr,"custom editor exposes exactly one factory revision choice");
    if(selector==nullptr || studio==nullptr)
    {
        std::puts("Remaining factory-revision editor binding checks NOT RUN: selector absent (baseline RED).");
        return;
    }
    check(selector->getNumItems()==2 && selector->getItemText(0)=="Frozen V1"
              && selector->getItemText(1)=="Public-source estimates V2",
          "factory editor exposes both explicit version labels");
    check(selector->getWantsKeyboardFocus() && selector->getTitle()=="Factory tone revision"
              && selector->getDescription().isNotEmpty(),
          "factory revision selector is focusable with accessible title and description");
    check(dynamic_cast<cs80::ui::ContextComboBox*>(selector)==nullptr
              && !selector->getProperties().contains("parameterContextId"),
          "non-automatable revision selector has no MIDI-learn parameter context");
    const auto help=selector->getTooltip()+" "+selector->getDescription();
    check(help.containsIgnoreCase("factory") && help.containsIgnoreCase("source")
              && help.containsIgnoreCase("PURE") && help.containsIgnoreCase("LEGACY")
              && help.containsIgnoreCase("PANEL") && help.containsIgnoreCase("MEMORY")
              && help.containsIgnoreCase("non-automatable"),
          "revision help states source-factory applicability including PURE and unchanged Legacy Panel Memory");
    const auto expectChoice=[&](int expected,const char* label)
    {
        check(waitFor(flush,[&]{return choice(host)==(float)expected && selector->getSelectedId()==expected+1;}),label);
    };
    expectChoice(0,"new editor displays frozen V1 by default");
    for(const auto size : {juce::Point<int>(1600,1000),juce::Point<int>(1120,700)})
    {
        editor.setSize(size.x,size.y);
        editor.showAdvancedPanel(false);
        flush(30);
        const auto boundsBefore=physicalBounds(editor);
        editor.showAdvancedPanel(true);
        studio->showTab(cs80::ui::StudioTab::settings);
        flush(30);
        const auto bounds=editor.getLocalArea(selector,selector->getLocalBounds());
        check(selector->isVisible() && !bounds.isEmpty() && editor.getLocalBounds().contains(bounds),
              "revision selector has visible in-editor bounds at both registered sizes");
        check(bounds.getHeight()>=24 && bounds.getWidth()>=200,
              "revision selector retains usable hit dimensions at smallest editor size");
        std::vector<juce::Component*> legacy;
        collectId(editor,"render_policy_legacy",legacy);
        check(legacy.size()==1 && !bounds.intersects(editor.getLocalArea(legacy.front(),legacy.front()->getLocalBounds())),
              "revision selector does not overlap existing policy controls");
        check(physicalBounds(editor)==boundsBefore,
              "opening revision settings does not alter physical panel control geometry");
    }
    for(int expected : {1,0})
    {
        const auto previous=engineTags(host);
        Gesture gesture;
        parameter->addListener(&gesture);
        selector->setSelectedId(expected+1,juce::sendNotificationSync);
        expectChoice(expected,"manual revision selection reaches the actual host parameter");
        parameter->removeListener(&gesture);
        check(gesture.balanced(),"manual revision selection is one balanced host gesture");
        check(engineTags(host)==previous,"manual revision selection waits for the audio transaction");
        block(host);
        check(engineRevisionMatches(host.getEngine(),expected!=0)
                  && engineRevisionMatches(host.getExtensionEngine(),expected!=0),
              "selected revision reaches main and active extension at the next audio transaction");
    }
    for(int pure : {0,1})for(int source : {0,1})
    {
        host.setParamValue(cs80::P_parityMode,(float)pure);
        host.setParamValue(cs80::P_sourcePerformance,(float)source);
        flush(30);
        check(selector->isEnabled(),"revision opt-in remains selectable including PURE overriding stored Legacy");
    }
    // Also test constructor-time enablement: an existing editor alone cannot expose a stale
    // one-time decision based only on the stored ENGINE request when PURE overrides it.
    for(int pure : {0,1})for(int source : {0,1})
    {
        Cs80AudioProcessor fresh(root.getFile().getChildFile(juce::String("fresh-")+juce::String(pure)+juce::String(source)));
        configure(fresh,true);
        fresh.setParamValue(cs80::P_parityMode,(float)pure);
        fresh.setParamValue(cs80::P_sourcePerformance,(float)source);
        setChoice(fresh,1);
        fresh.prepareToPlay(48000,32);
        block(fresh);
        Cs80AudioProcessorEditor reopened(fresh);
        reopened.setSize(1600,1000);
        reopened.setVisible(true);
        reopened.showAdvancedPanel(true);
        auto* page=findType<cs80::ui::StudioView>(reopened);
        if(page!=nullptr)page->showTab(cs80::ui::StudioTab::settings);
        std::vector<juce::Component*> copies;
        collectId(reopened,revisionId,copies);
        auto* control=copies.size()==1 ? dynamic_cast<juce::ComboBox*>(copies.front()) : nullptr;
        check(page!=nullptr && control!=nullptr && waitFor(flush,[&]{return control->isEnabled()
                    && control->getSelectedId()==2 && choice(fresh)==1;}),
              "fresh ENGINE/PURE combinations keep saved revision opt-in selectable");
    }
    setChoice(host,1);
    expectChoice(1,"external parameter change synchronizes the editor asynchronously");
    const auto saved=session(host);
    setChoice(host,0);expectChoice(0,"external frozen revision synchronizes the editor");
    restore(host,saved);expectChoice(1,"session restoration synchronizes explicit V2");
    const auto preset=host.saveUserPreset("Editor revision V2");
    check(preset.wasSaved(),"isolated editor V2 preset fixture saves");
    setChoice(host,0);host.loadUserPreset(preset.file);expectChoice(1,"user preset recall synchronizes V2 rather than live V1");
    for(int schema : {1,2,3})
    {
        auto old=saved.createCopy();old.setProperty(Cs80AudioProcessor::stateSchemaKey,schema,nullptr);
        old.removeChild(old.getChildWithProperty("id",revisionId),nullptr);
        setChoice(host,1);restore(host,old);expectChoice(0,"old session without revision displays frozen V1");
    }
    for(int index : {22,cs80::legacyProgramCount+3})
    {
        setChoice(host,1);expectChoice(1,"factory-recall fixture starts opted in");
        host.loadFactoryPreset(index);expectChoice(0,"shipped factory recall resets displayed revision to V1");
    }
    setChoice(host,1);host.loadInitPatch();expectChoice(0,"INIT resets displayed revision to V1");
    configure(host,true);setChoice(host,1);block(host);
    std::printf("MIDI fixture before event: pure=%.0f source=%.0f policy=%d revision=%.0f selected=%d\n",
        (double)host.getParamValue(cs80::P_parityMode),(double)host.getParamValue(cs80::P_sourcePerformance),
        (int)host.getEngine().getRenderPolicy(),(double)choice(host),selector->getSelectedId());
    juce::MidiBuffer program;program.addEvent(juce::MidiMessage::programChange(1,7),0);
    block(host,32,program);expectChoice(0,"accepted MIDI Program Change synchronizes frozen V1");
    setChoice(host,1);host.setParamValue(cs80::P_parityMode,1);block(host);
    block(host,32,program);expectChoice(1,"PURE-ignored Program Change preserves displayed V2");
    setChoice(host,1);host.abCopyCurrentTo(1);setChoice(host,0);host.abToggle();
    expectChoice(1,"A/B recalls the copied V2 in the editor");
    host.abToggle();expectChoice(0,"A/B returns the editor to V1");
    setChoice(host,1);host.selectHardwareSource(0,2);expectChoice(1,"hardware factory tabs preserve displayed revision");
    host.randomiseWithinOriginalRanges(0x802026);expectChoice(1,"randomise preserves displayed interpretation revision");
    editor.showAdvancedPanel(false);setChoice(host,0);
    editor.showAdvancedPanel(true);studio->showTab(cs80::ui::StudioTab::settings);
    expectChoice(0,"hidden-page host changes synchronize when Settings reopens");
}
} // namespace cs80::tests
