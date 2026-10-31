#pragma once

#include "UiComponents.h"

#include <array>
#include <functional>
#include <memory>
#include <vector>

class Cs80AudioProcessor;

namespace cs80::ui
{

class AdvancedPage;
class AdvChassis;
class AdvPageTab;
class AdvCloseButton;
class AdvTitleBlock;
class AdvFooterStatus;
class AdvInfoLayer;
class StudioSubTab;
class QwertyKeyboard;

// The tabs of the Studio view, in display order.
enum class StudioTab : int
{
    modulation = 0, // LFOs, wheels, sub oscillator, ring modulator (MSEG and matrix pages mount here)
    keyboard,       // computer keyboard, MIDI learn, pitch bend, MPE and tuning
    fx,             // insert effects, macros, output
    arp,            // arpeggiator, glide and key release
    settings,       // engine mode (render policy) and the evidence register
    count
};

// The modern control layer, drawn as a second instrument panel behind its own overlay, so the
// primary surface stays a CS-80 rather than a generic plugin with 200 knobs. It replaces the
// former Advanced panel (src/plugin/AdvancedPanel.h keeps that name as an alias).
//
// The view occupies design rect (0, 64, 1600, 936) of the editor; everything inside is laid
// out in that design space and scaled to the real bounds. Each tab holds one or more
// sub-pages; a tab with more than one shows sub-page keys in the header.
//
// Page registration (for pages built elsewhere, for example the modulation, FX and tuning
// pages): a registered component is given the content area of its tab (below the header
// rail, above the footer, inside the cheeks) and is shown and hidden with its sub-page. It
// paints itself over the crinkled chassis. setPage() puts the component first in its tab and
// keeps the built-in page as a later sub-page, so no existing control disappears until the
// integrator calls removeBuiltInPage().
class StudioView final : public juce::Component, private juce::Timer
{
public:
    explicit StudioView(Cs80AudioProcessor&);
    ~StudioView() override;

    // Mount `page` as the first sub-page of `tab`. `name` labels its sub-page key (a short
    // upper-case phrase); an empty name uses the component's title, else the tab's name.
    void setPage(StudioTab tab, std::unique_ptr<juce::Component> page, const juce::String& name = {});
    // Mount `page` as the last sub-page of `tab`.
    void addPage(StudioTab tab, std::unique_ptr<juce::Component> page, const juce::String& name);
    // Remove the built-in sub-page(s) of `tab` once a registered page replaces their controls.
    // A tab always keeps at least one page: the call is ignored when nothing else is mounted.
    void removeBuiltInPage(StudioTab tab);
    // The design-space rectangle (in this component's 1600 x 936 design units) that a registered
    // page occupies, and the same in current pixels.
    static juce::Rectangle<float> contentDesignArea() noexcept;
    juce::Rectangle<int> getContentBounds() const;

    void showTab(StudioTab tab, int subPage = 0);
    StudioTab getActiveTab() const noexcept { return activeTab; }
    int getActiveSubPage() const noexcept { return activeSub[(size_t) activeTab]; }
    int getNumSubPages(StudioTab tab) const noexcept { return (int) pagesOf(tab).size(); }
    juce::Component* getSubPage(StudioTab tab, int index) const noexcept;
    static juce::String tabName(StudioTab tab);

    // The computer-keyboard player the KEYBOARD tab shows and switches (owned by the editor's mount).
    void attachQwerty(QwertyKeyboard* keyboard) noexcept { qwerty = keyboard; }

    // Former Advanced page numbers, kept for hosts of the old API and its tests:
    // 0 LFOs and wheels, 1 sub oscillator / ring / chorus / analog voice, 2 FX and macros,
    // 3 the render policy (now in SETTINGS).
    void showPage(int legacyIndex);
    int getActivePage() const noexcept;

    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void()> onClose;

private:
    struct SubPage
    {
        juce::String name;
        juce::Component* component = nullptr;
        std::unique_ptr<juce::Component> owned;
        AdvancedPage* builtIn = nullptr;   // the built-in page behind this entry, if it is one
    };

    void timerCallback() override;
    void stepTab(int direction);
    void stepSubPage(int direction);
    void mount(StudioTab tab, std::unique_ptr<juce::Component> page, const juce::String& name, bool first);
    void refreshSubTabs();
    void layoutPages();
    std::vector<SubPage>& pagesOf(StudioTab tab) noexcept { return tabPages[(size_t) tab]; }
    const std::vector<SubPage>& pagesOf(StudioTab tab) const noexcept { return tabPages[(size_t) tab]; }

    static constexpr int kTabs = (int) StudioTab::count;
    static constexpr int kMaxSubTabs = 3;

    Cs80AudioProcessor& processor;
    std::unique_ptr<AdvChassis> chassis;
    std::array<std::unique_ptr<AdvPageTab>, kTabs> tabs;
    std::array<std::vector<SubPage>, kTabs> tabPages;
    std::array<int, kTabs> activeSub {};
    std::array<std::unique_ptr<StudioSubTab>, kMaxSubTabs> subTabs;
    std::unique_ptr<AdvTitleBlock> titleBlock;
    std::unique_ptr<AdvFooterStatus> footer;
    std::unique_ptr<AdvCloseButton> closeButton;
    std::unique_ptr<AdvInfoLayer> info;
    std::array<AdvancedPage*, 4> legacyPages {};   // former Advanced pages 1-4, for showPage()
    QwertyKeyboard* qwerty = nullptr;
    StudioTab activeTab = StudioTab::modulation;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StudioView)
};

} // namespace cs80::ui
