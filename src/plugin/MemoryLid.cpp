#include "MemoryLid.h"

#include "MemoryEditing.h"

#include "core/FactoryPresets.h"

namespace cs80::panel
{
juce::Colour memoryCapColour(int local)
{
    switch (local)
    {
        case cs80::CP_pwmSpeed: return ui::capCream;
        case cs80::CP_pwmDepth: case cs80::CP_pw: case cs80::CP_noise:
        case cs80::CP_hpfCutoff: case cs80::CP_lpfCutoff:
        case cs80::CP_initBrill: case cs80::CP_afterBrill: return ui::capGreen;
        case cs80::CP_hpfRes: case cs80::CP_lpfRes: return ui::capPink;
        case cs80::CP_fIl: case cs80::CP_fAl: case cs80::CP_fA: case cs80::CP_fD:
        case cs80::CP_vA: case cs80::CP_vD: case cs80::CP_vS: return ui::capBlack;
        case cs80::CP_fR: case cs80::CP_vR: return ui::capYellow;
        default: return ui::capGrey;
    }
}

// ---- LidReadout -----------------------------------------------------------------------------
LidReadout::LidReadout() { setInterceptsMouseClicks(false, false); setOpaque(false); setTitle("Memory readout"); }

void LidReadout::setLines(const juce::String& main, const juce::String& hint)
{
    if (main != mainLine || hint != hintLine) { mainLine = main; hintLine = hint; repaint(); }
}

void LidReadout::paint(juce::Graphics& g)
{
    // Uniform scale from the 44-unit height; the window is as wide as its bounds allow.
    const float sx = (float) getHeight() / 44.0f;
    FloorScope floor(sx);
    g.addTransform(juce::AffineTransform::scale(sx));
    const float width = (float) getWidth() / sx, textW = width - 28.0f;
    const juce::Rectangle<float> w(0.0f, 0.0f, width, 44.0f);
    art::recessTray(g, w.reduced(1.0f), 4.0f, juce::Colour(0xff0e0d0b), juce::Colour(0xff050504), 7.0f, 0.9f);
    g.setGradientFill(art::vgrad(art::white(0.06f), juce::Colours::transparentWhite, w.withHeight(22.0f)));
    g.fillRoundedRectangle(w.reduced(2.0f).withHeight(21.0f), 3.0f);
    auto font = spacedFont(14.5f, 0.06f);
    font = ui::fitFontToWidth(font, mainLine, textW, 0.8f, art::kLegendFloorPx / sx);
    g.setFont(font);
    g.setColour(juce::Colour(0xffffc060));
    g.drawText(mainLine, juce::Rectangle<float>(14.0f, 3.0f, textW, 22.0f), juce::Justification::centredLeft, false);
    auto hint = spacedFont(11.0f, 0.04f);
    hint = ui::fitFontToWidth(hint, hintLine, textW, 0.8f, art::kLegendFloorPx / sx);
    g.setFont(hint);
    g.setColour(juce::Colour(0xffb6aa86));
    g.drawText(hintLine, juce::Rectangle<float>(14.0f, 26.0f, textW, 15.0f), juce::Justification::centredLeft, false);
}

// ---- MemoryLid ------------------------------------------------------------------------------
MemoryLid::MemoryLid(Cs80AudioProcessor& p) : processor(p)
{
    setOpaque(false);
    addAndMakeVisible(readout);
    addAndMakeVisible(close);
    setTitle("Memory programmer");
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    close.setButtonText("CLOSE LID");
    close.setTitle("Close memory programmer");
    close.setWantsKeyboardFocus(true);
    close.onClick = [this] { if (onClose) onClose(); };
    struct EditKey { KeyButton* key; const char* title; const char* tip; };
    const EditKey editKeys[] = {
        { &storeKey, "Store a panel row into a memory",
          "Store a rank's main-panel row into one of its memories, or load a memory into the panel row." },
        { &copyKey, "Copy one memory to another", "Copy all 26 controls of one memory into another memory." },
        { &rankKey, "Copy one rank's panel row to the other", "Copy the 26 main-panel controls of rank I to rank II, or of rank II to rank I. FEET stays as it is." },
    };
    for (const auto& edit : editKeys)
    {
        edit.key->setTitle(edit.title);
        edit.key->setTooltip(edit.tip);
        edit.key->setWantsKeyboardFocus(true);
        addAndMakeVisible(*edit.key);
    }
    storeKey.onClick = [this] { showStoreMenu(); };
    copyKey.onClick = [this] { showCopyMenu(); };
    rankKey.onClick = [this] { showRankMenu(); };
    for (int bank = 0; bank < 4; ++bank)
    {
        auto button = std::make_unique<KeyButton>("MEMORY " + juce::String(bank + 1), KeyButton::Style::selector);
        button->setClickingTogglesState(false);
        button->setWantsKeyboardFocus(true);
        button->setTitle("Select memory " + juce::String(bank + 1));
        // A lit memory key is a toggle: pressing it again hands its channel back to the PANEL.
        button->onClick = [this, bank]
        {
            const int channel = bank % 2, source = bank < 2 ? 11 : 12;
            processor.selectHardwareSource(channel, processor.getHardwareSource(channel) == source ? 13 : source);
            refresh();
        };
        button->setTooltip("Listen to this independent memory on channel " + juce::String(bank % 2 + 1)
                           + ". Press it again while lit to return the channel to PANEL.");
        addAndMakeVisible(*button);
        bankButtons[(size_t) bank] = std::move(button);
        for (int col = 0; col < 26; ++col)
        {
            const int local = order[(size_t) col];
            const auto& def = cs80::kChannelDefs[local];
            Item item;
            item.bank = bank; item.local = local;
            const bool discrete = local == cs80::CP_saw || local == cs80::CP_pulse;
            if (discrete)
            {
                item.toggle = std::make_unique<ui::LedButton>(local == cs80::CP_saw ? "SAW" : "PULSE");
                auto* toggle = item.toggle.get();
                toggle->addMouseListener(this, false);
                toggle->hardwareTab = true;
                toggle->accent = ui::capBlack;
                toggle->setTitle("Memory " + juce::String(bank + 1) + " " + def.name);
                toggle->setComponentID(Cs80AudioProcessor::memoryParameterId(bank, local));
                item.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
                    processor.getAPVTS(), Cs80AudioProcessor::memoryParameterId(bank, local), *toggle);
                addAndMakeVisible(*toggle);
            }
            else
            {
                item.slider = std::make_unique<ui::FocusSlider>();
                auto* slider = item.slider.get();
                slider->addMouseListener(this, false);
                slider->setSliderStyle(juce::Slider::LinearVertical);
                slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
                slider->setRange(def.min, def.max);
                slider->setSkewFactor(def.skew);
                slider->setColour(juce::Slider::thumbColourId, memoryCapColour(local));
                slider->setDoubleClickReturnValue(true, def.def);
                slider->setTitle("Memory " + juce::String(bank + 1) + " " + def.name);
                slider->setTooltip(slider->getTitle());
                slider->setDescription("Independent analog memory control. Use the arrow keys to adjust; select this memory to hear it.");
                slider->setComponentID(Cs80AudioProcessor::memoryParameterId(bank, local));
                item.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                    processor.getAPVTS(), Cs80AudioProcessor::memoryParameterId(bank, local), *slider);
                slider->onValueChange = [bank, local, slider] {
                    slider->setTooltip(slider->getTitle() + ": "
                        + ui::formatParamValue(cs80::chParam(bank % 2, local), (float) slider->getValue()));
                };
                addAndMakeVisible(*slider);
            }
            items.push_back(std::move(item));
        }
    }
    refresh();
}

MemoryLid::~MemoryLid() = default;

void MemoryLid::refresh()
{
    for (auto& item : items)
    {
        if (item.slider && ! item.slider->isMouseButtonDown())
        {
            item.slider->setTooltip(item.slider->getTitle() + ": "
                + ui::formatParamValue(cs80::chParam(item.bank % 2, item.local), (float) item.slider->getValue()));
        }
        if (item.toggle)
        {
            item.toggle->setTooltip(item.toggle->getTitle() + (item.toggle->getToggleState() ? ": ON" : ": OFF"));
        }
    }
    bool changed = false;
    for (int bank = 0; bank < 4; ++bank)
    {
        const bool active = processor.getHardwareSource(bank % 2) == (bank < 2 ? 11 : 12);
        bankButtons[(size_t) bank]->setToggleState(active, juce::dontSendNotification);
        if (active != rowActive[(size_t) bank]) { rowActive[(size_t) bank] = active; changed = true; }
    }
    if (changed)
    {
        // Rows that are not the sounding memory sit back; the one that is heard stands out.
        for (auto& item : items)
        {
            const float alpha = rowActive[(size_t) item.bank] ? 1.0f : 0.62f;
            if (item.slider) item.slider->setAlpha(alpha);
            if (item.toggle) item.toggle->setAlpha(alpha);
        }
        repaint();
    }
    if (juce::Time::getMillisecondCounterHiRes() < messageUntilMs) return;
    if (hovered != nullptr) describe(*hovered, false);
    else if (lastTouched != nullptr) describe(*lastTouched, true);
    else showIdle();
}

namespace
{
juce::String memoryName(int bank) { return "MEMORY " + juce::String(bank + 1); }
juce::String rowName(int channel) { return channel == 0 ? "PANEL I" : "PANEL II"; }
} // namespace

bool MemoryLid::runEditCommand(int command)
{
    namespace edit = cs80::memory_editing;
    const int group = command / 100, a = (command / 10) % 10, b = command % 10;
    bool done = false;
    juce::String report;
    switch (group)
    {
        case 1: done = edit::storePanelToMemory(processor, a, b); report = "STORED " + rowName(a) + " IN " + memoryName(b); break;
        case 2: done = edit::recallMemoryToPanel(processor, a, b); report = "LOADED " + memoryName(a) + " INTO " + rowName(b); break;
        case 3: done = edit::copyMemory(processor, a, b); report = "COPIED " + memoryName(a) + " TO " + memoryName(b); break;
        case 4: done = edit::copyRank(processor, a, b); report = "COPIED " + rowName(a) + " TO " + rowName(b); break;
        default: break;
    }
    if (! done)
        return false;
    readout.setLines(report, "26 CONTROLS WRITTEN AS HOST AUTOMATION  /  YOUR HOST'S UNDO REVERTS IT");
    messageUntilMs = juce::Time::getMillisecondCounterHiRes() + 2500.0;
    return true;
}

void MemoryLid::showMenu(juce::PopupMenu menu, juce::Component& target)
{
    juce::Component::SafePointer<MemoryLid> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target),
                       [safe](int chosen) { if (safe != nullptr && chosen > 0) safe->runEditCommand(chosen); });
}

void MemoryLid::showStoreMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("STORE PANEL ROW INTO MEMORY");
    for (int bank = 0; bank < 4; ++bank)
        menu.addItem(storeCommand(cs80::memory_editing::rankOfBank(bank), bank), rowName(cs80::memory_editing::rankOfBank(bank)) + "  >  " + memoryName(bank));
    menu.addSectionHeader("LOAD MEMORY INTO PANEL ROW");
    for (int bank = 0; bank < 4; ++bank)
        menu.addItem(recallCommand(bank, cs80::memory_editing::rankOfBank(bank)), memoryName(bank) + "  >  " + rowName(cs80::memory_editing::rankOfBank(bank)));
    showMenu(menu, storeKey);
}

void MemoryLid::showCopyMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("COPY MEMORY");
    for (int from = 0; from < 4; ++from)
    {
        juce::PopupMenu targets;
        for (int to = 0; to < 4; ++to)
            if (to != from)
                targets.addItem(copyMemoryCommand(from, to), "TO " + memoryName(to));
        menu.addSubMenu("FROM " + memoryName(from), targets);
    }
    showMenu(menu, copyKey);
}

void MemoryLid::showRankMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("COPY PANEL ROW");
    menu.addItem(copyRankCommand(0, 1), "PANEL I  >  PANEL II");
    menu.addItem(copyRankCommand(1, 0), "PANEL II  >  PANEL I");
    showMenu(menu, rankKey);
}

// ---- live readout ---------------------------------------------------------------------------
void MemoryLid::mouseDown(const juce::MouseEvent& e)
{
    if (auto* item = findItem(e.eventComponent)) { lastTouched = item; hovered = item; describe(*item, false); }
}

void MemoryLid::mouseExit(const juce::MouseEvent& e)
{
    if (findItem(e.eventComponent) != nullptr)
    {
        hovered = nullptr;
        if (lastTouched != nullptr) describe(*lastTouched, true); else showIdle();
    }
}

unsigned MemoryLid::activeRowMask() const noexcept
{
    unsigned mask = 0;
    for (int bank = 0; bank < 4; ++bank)
        if (rowActive[(size_t) bank]) mask |= 1u << bank;
    return mask;
}

void MemoryLid::paint(juce::Graphics& g)
{
    const float pixelScale = juce::jmax(1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const auto rows = activeRowMask();
    if (artCache.isNull() || artBounds != getLocalBounds() || artPixelScale != pixelScale || artRows != rows)
    {
        artBounds = getLocalBounds();
        artPixelScale = pixelScale;
        artRows = rows;
        artCache = juce::Image(juce::Image::ARGB, juce::jmax(1, juce::roundToInt((float) getWidth() * pixelScale)),
                               juce::jmax(1, juce::roundToInt((float) getHeight() * pixelScale)), true);
        juce::Graphics ig(artCache);
        ig.addTransform(juce::AffineTransform::scale((float) artCache.getWidth() / (float) juce::jmax(1, getWidth()),
                                                     (float) artCache.getHeight() / (float) juce::jmax(1, getHeight())));
        paintArt(ig);
    }
    g.drawImage(artCache, getLocalBounds().toFloat());
}

void MemoryLid::paintArt(juce::Graphics& g)
{
    const float scale = (float) getWidth() / kW;
    FloorScope floor(scale);
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(scale, (float) getHeight() / kH));
    const juce::Rectangle<float> lid(0.0f, geo::lidTop, kW, geo::lidHeight);

    // Shadow of the raised lid on the deck below it, kept short so it does not smear over the keys.
    ui::softShadow(g, lid, 9.0f, 0.0f, 9.0f, 15.0f, 0.85f);
    g.setGradientFill(art::vgrad(art::black(0.32f), juce::Colours::transparentBlack, { 0.0f, lid.getBottom(), kW, kH - lid.getBottom() }));
    g.fillRect(8.0f, lid.getBottom(), kW - 16.0f, kH - lid.getBottom());

    // Hinge: a brushed barrel strip along the back edge with three knuckles.
    {
        const juce::Rectangle<float> strip(48.0f, 0.0f, kW - 96.0f, geo::lidTop + 6.0f);
        art::brushedMetal(g, strip, 2.0f, 0.0f, 41);
        g.setColour(art::black(0.8f));
        g.drawRoundedRectangle(strip, 2.0f, 1.0f);
        for (int i = 0; i < 4; ++i)
        {
            const float kx = strip.getX() + 120.0f + (float) i * (strip.getWidth() - 240.0f) / 3.0f;
            const juce::Rectangle<float> knuckle(kx - 34.0f, strip.getY() - 1.0f, 68.0f, strip.getHeight() + 2.0f);
            art::brushedMetal(g, knuckle, 3.0f, 0.3f, 50 + i);
            g.setColour(art::black(0.85f));
            g.drawRoundedRectangle(knuckle, 3.0f, 1.0f);
            art::slotScrew(g, { kx - 20.0f, knuckle.getCentreY() }, 2.4f, kx);
            art::slotScrew(g, { kx + 20.0f, knuckle.getCentreY() }, 2.4f, kx + 1.0f);
        }
    }

    // Machined rim, then the plate set into it: the same crinkle finish as the instrument panel.
    g.setGradientFill(art::vgrad(juce::Colour(0xff55514a), juce::Colour(0xff1c1a16), lid));
    g.fillRoundedRectangle(lid, 9.0f);
    g.setColour(art::white(0.30f));
    g.drawLine(10.0f, lid.getY() + 1.5f, kW - 10.0f, lid.getY() + 1.5f, 1.0f);
    g.setColour(art::black(0.85f));
    g.drawRoundedRectangle(lid, 9.0f, 1.0f);
    const auto inner = lid.reduced(9.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(inner, 5.0f);
        g.reduceClipRegion(clip);
        art::fillTile(g, art::panelTile(), inner, g.getInternalContext().getPhysicalPixelScaleFactor());
        g.setGradientFill(juce::ColourGradient(art::white(0.045f), inner.getX() + 160.0f, inner.getY(), juce::Colours::transparentWhite,
                                               inner.getX() + 900.0f, inner.getBottom(), true));
        g.fillRect(inner);
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, inner.getX() + 500.0f, inner.getY() + 100.0f,
                                               art::black(0.12f), inner.getRight(), inner.getBottom(), false));
        g.fillRect(inner);
    }
    art::insetShadow(g, inner, 5.0f, 12.0f, 0.5f);
    art::etchRect(g, inner.reduced(7.0f), 3.0f, 0.55f);
    for (float sx : { lid.getX() + 5.0f, lid.getRight() - 5.0f })
        for (float sy : { lid.getY() + 5.0f, lid.getBottom() - 5.0f })
            art::slotScrew(g, { sx, sy }, 3.0f, sx + sy);
    // Front rail: two latch catches that hold the lid down.
    for (float lx : { 330.0f, kW - 330.0f })
    {
        const juce::Rectangle<float> catchPlate(lx - 42.0f, lid.getBottom() - 9.0f, 84.0f, 13.0f);
        ui::softShadow(g, catchPlate, 3.0f, 0.0f, 2.0f, 4.0f, 0.7f);
        art::brushedMetal(g, catchPlate, 3.0f, 0.3f, (int) lx);
        g.setColour(art::black(0.85f));
        g.drawRoundedRectangle(catchPlate, 3.0f, 1.0f);
        art::slotScrew(g, { catchPlate.getX() + 11.0f, catchPlate.getCentreY() }, 2.5f, lx);
        art::slotScrew(g, { catchPlate.getRight() - 11.0f, catchPlate.getCentreY() }, 2.5f, lx + 3.0f);
        g.setColour(art::black(0.7f));
        g.fillRoundedRectangle(catchPlate.getCentreX() - 14.0f, catchPlate.getY() + 3.5f, 28.0f, 6.0f, 2.5f);
    }

    // Everything below is laid out in the programmer's own 1472 x 554 frame.
    g.addTransform(juce::AffineTransform::translation(kOffsetX, geo::lidTop));
    art::legend(g, "MEMORY PROGRAMMER", { 34, 14, 520, 26 }, spacedFont(19.0f, 0.16f),
                juce::Justification::centredLeft, juce::Colour(0xffe9d695));
    art::legend(g, "FOUR INDEPENDENT MINIATURE PANELS", { 35, 41, 520, 14 },
                spacedFont(10.5f, 0.12f), juce::Justification::centredLeft, juce::Colour(0xffcfc4a2));

    // One header strip for all four memories: group brackets and a caption over every column, so each
    // row carries only its sliders. The columns line up exactly with the rows below.
    {
        art::etchRect(g, { 24.0f, 62.0f, 1424.0f, 50.0f }, 4.0f, 0.6f);
        const struct { const char* title; int first, last; } groups[] = {
            { "VCO", 0, 5 }, { "VCF", 6, 14 }, { "VCA", 15, 21 }, { "INITIAL", 22, 23 }, { "AFTER", 24, 25 } };
        for (const auto& group : groups)
        {
            const float x0 = colX(group.first) + 1.0f, x1 = colX(group.last) + 47.0f;
            art::bracket(g, group.title, x0, 75.0f, x1 - x0, 4.0f, 11.0f);
        }
        for (int col = 0; col < 26; ++col)
            art::legend(g, labels[(size_t) col], { colX(col), 82.0f, 48.0f, 27.0f }, displayFont(11.5f, juce::Font::bold),
                        juce::Justification::centred, juce::Colour(0xffeee4c4), 0.7f);
    }
    for (int bank = 0; bank < 4; ++bank)
    {
        const float y = rowY(bank);
        const bool active = rowActive[(size_t) bank];
        const juce::Rectangle<float> row(24.0f, y, 1424.0f, 98.0f);
        if (active)
        {
            // The heard row: a lit ring, a soft glow and a mark at its left end.
            g.setColour(juce::Colour(0xffffc060).withAlpha(0.10f));
            g.fillRoundedRectangle(row.reduced(1.0f), 4.0f);
            g.setColour(juce::Colour(0xffffc060).withAlpha(0.28f));
            g.drawRoundedRectangle(row.expanded(2.0f), 6.0f, 3.0f);
            art::etchRect(g, row, 4.0f, 1.0f);
            g.setColour(juce::Colour(0xffffc060));
            g.fillRoundedRectangle(row.getX() + 3.0f, row.getY() + 12.0f, 3.0f, row.getHeight() - 24.0f, 1.5f);
        }
        else
            art::etchRect(g, row, 4.0f, 0.34f);
        art::legend(g, bank % 2 == 0 ? "CHANNEL I" : "CHANNEL II", { 33.0f, y + 10.0f, 100.0f, 16.0f },
                    spacedFont(11.0f, 0.12f), juce::Justification::centred, juce::Colour(active ? 0xfffff0cc : 0xffb8ae8e));
        art::legend(g, active ? "HEARD" : "NOT ACTIVE", { 33.0f, y + 72.0f, 100.0f, 14.0f },
                    spacedFont(10.0f, 0.10f), juce::Justification::centred, juce::Colour(active ? 0xffffc060 : 0xffb3aa8a), 0.6f);   // NOT ACTIVE: 5.4:1
    }
    g.restoreState();
}

void MemoryLid::resized()
{
    const float sx = (float) getWidth() / kW, sy = (float) getHeight() / kH;
    auto r = [sx, sy](float x, float y, float w, float h) {
        return juce::Rectangle<int>(juce::roundToInt((x + kOffsetX) * sx), juce::roundToInt((y + geo::lidTop) * sy),
                                    juce::roundToInt(w * sx), juce::roundToInt(h * sy));
    };
    close.setBounds(r(1318, 20, 126, 30));
    rankKey.setBounds(r(1228, 20, 84, 30));
    copyKey.setBounds(r(1130, 20, 92, 30));
    storeKey.setBounds(r(1032, 20, 92, 30));
    readout.setBounds(r(560, 12, 464, 44));
    for (int bank = 0; bank < 4; ++bank)
    {
        const float y = rowY(bank);
        bankButtons[(size_t) bank]->setBounds(r(34, y + 36, 98, 32));
        for (int col = 0; col < 26; ++col)
        {
            auto& item = items[(size_t) (bank * 26 + col)];
            if (item.slider) item.slider->setBounds(r(colX(col), y + 6, 48, 86));
            if (item.toggle) item.toggle->setBounds(r(colX(col) + 8.0f, y + 20, 32, 58));
        }
    }
}

MemoryLid::Item* MemoryLid::findItem(const juce::Component* c)
{
    if (c == nullptr) return nullptr;
    for (auto& item : items)
        if (c == item.slider.get() || c == item.toggle.get()) return &item;
    return nullptr;
}

void MemoryLid::track(const juce::MouseEvent& e)
{
    if (auto* item = findItem(e.eventComponent)) { hovered = item; describe(*item, false); }
}

juce::String MemoryLid::sourceName(int source, int channel)
{
    return source < 11 ? juce::String(cs80::hardwareToneName(source, channel)).toUpperCase()
         : source == 13 ? juce::String("PANEL") : "MEMORY " + juce::String(channel + (source == 12 ? 3 : 1));
}

void MemoryLid::showIdle()
{
    const bool anyActive = rowActive[0] || rowActive[1] || rowActive[2] || rowActive[3];
    readout.setLines("CH I: " + sourceName(processor.getHardwareSource(0), 0) + "      CH II: " + sourceName(processor.getHardwareSource(1), 1),
                     anyActive ? "HOVER OR DRAG A CONTROL TO READ ITS VALUE" : "BOTH CHANNELS ON PANEL  /  PRESS A MEMORY KEY TO HEAR ITS ROW");
}

void MemoryLid::describe(const Item& item, bool lastTouch)
{
    const juce::String value = item.slider != nullptr
        ? ui::formatParamValue(cs80::chParam(item.bank % 2, item.local), (float) item.slider->getValue())
        : (item.toggle->getToggleState() ? juce::String("ON") : juce::String("OFF"));
    const bool active = rowActive[(size_t) item.bank];
    const juce::String hint = ! active ? "NOT ACTIVE - PRESS MEMORY " + juce::String(item.bank + 1) + " TO HEAR THIS ROW"
                            : lastTouch ? juce::String("LAST TOUCHED  /  DRAG TO CHANGE  /  DOUBLE-CLICK FOR DEFAULT")
                                        : juce::String("DRAG TO CHANGE  /  DOUBLE-CLICK FOR DEFAULT");
    readout.setLines("MEMORY " + juce::String(item.bank + 1) + "  /  CH " + (item.bank % 2 == 0 ? "I" : "II") + "  /  "
                         + panelName(item.local) + "   " + value, hint);
}

juce::String MemoryLid::panelName(int local)
{
    if (local == cs80::CP_vcfLevel) return "VCF LV";
    return juce::String(cs80::kChannelDefs[local].name).toUpperCase();
}
} // namespace cs80::panel
