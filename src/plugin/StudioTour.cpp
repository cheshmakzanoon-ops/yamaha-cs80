#include "StudioTour.h"

#include "ArtCommon.h"

namespace cs80::ui
{

namespace
{
juce::Font tourFont(float height, bool bold)
{
    return art::displayFont(height, bold ? juce::Font::bold : juce::Font::plain);
}

class TourKey final : public juce::Button
{
public:
    TourKey(const juce::String& text, const juce::String& title, bool primary) : juce::Button(text), accent(primary)
    {
        setTitle(title);
        setWantsKeyboardFocus(true);
        setMouseClickGrabsKeyboardFocus(false);
    }
    void paintButton(juce::Graphics& g, bool hovered, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced(1.0f);
        if (accent)
            drawAccentPlate(g, r, down, hovered);
        else
            drawSatinPlate(g, r, false, down, hovered, false);
        g.setFont(tourFont(juce::jlimit(11.0f, 15.0f, r.getHeight() * 0.42f), true).withExtraKerningFactor(0.08f));
        g.setColour(accent ? juce::Colour(0xff2b1c06) : ivory);
        g.drawFittedText(getButtonText(), r.toNearestInt(), juce::Justification::centred, 1, 0.85f);
        if (hasKeyboardFocus(true))
            drawFocusRing(g, r);
    }

private:
    bool accent;
};
} // namespace

TourOverlay::TourOverlay()
{
    setVisible(false);
    setTitle("Guided tour");
    setWantsKeyboardFocus(true);
    setFocusContainerType(juce::Component::FocusContainerType::keyboardFocusContainer);
    setAlwaysOnTop(true);
    backKey = std::make_unique<TourKey>("BACK", "Previous tour step", false);
    nextKey = std::make_unique<TourKey>("NEXT", "Next tour step", true);
    closeKey = std::make_unique<TourKey>("CLOSE TOUR", "Close the tour", false);
    backKey->setComponentID("tour_back");
    nextKey->setComponentID("tour_next");
    closeKey->setComponentID("tour_close");
    int order = 0;
    for (auto* k : { nextKey.get(), backKey.get(), closeKey.get() })
    {
        k->setExplicitFocusOrder(++order);
        addAndMakeVisible(*k);
    }
    backKey->onClick = [this] { back(); };
    nextKey->onClick = [this] { next(); };
    closeKey->onClick = [this] { dismiss(); };
}

TourOverlay::~TourOverlay() = default;

void TourOverlay::setSteps(std::vector<Step> newSteps)
{
    steps = std::move(newSteps);
    current = juce::jlimit(0, juce::jmax(0, (int) steps.size() - 1), current);
}

void TourOverlay::start(int step)
{
    if (steps.empty())
        return;
    current = juce::jlimit(0, (int) steps.size() - 1, step);
    setVisible(true);
    toFront(true);
    layoutCard();
    repaint();
    if (isShowing())
        nextKey->grabKeyboardFocus();
}

void TourOverlay::dismiss()
{
    if (! isVisible())
        return;
    setVisible(false);
    if (onDismissed)
        onDismissed();
}

void TourOverlay::next()
{
    if (current + 1 >= (int) steps.size())
    {
        dismiss();
        return;
    }
    ++current;
    layoutCard();
    repaint();
}

void TourOverlay::back()
{
    current = juce::jmax(0, current - 1);
    layoutCard();
    repaint();
}

bool TourOverlay::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey) { dismiss(); return true; }
    if (key == juce::KeyPress::rightKey) { next(); return true; }
    if (key == juce::KeyPress::leftKey) { back(); return true; }
    return false;   // Tab, Space and Return go to the focused key
}

juce::Rectangle<int> TourOverlay::cardBounds() const
{
    const float s = juce::jlimit(0.7f, 1.4f, (float) getWidth() / 1600.0f);
    const int w = juce::roundToInt(470.0f * s), h = juce::roundToInt(236.0f * s);
    const auto area = getLocalBounds().reduced(juce::roundToInt(12.0f * s));
    juce::Rectangle<int> target;
    if (! steps.empty() && steps[(size_t) current].target)
        target = steps[(size_t) current].target();
    if (target.isEmpty())
        return juce::Rectangle<int>(w, h).withCentre(getLocalBounds().getCentre());
    // Beside the highlight: below it when there is room, else above, else centred.
    const int gap = juce::roundToInt(16.0f * s);
    juce::Rectangle<int> card(w, h);
    if (target.getBottom() + gap + h <= area.getBottom())
        card.setPosition(target.getCentreX() - w / 2, target.getBottom() + gap);
    else if (target.getY() - gap - h >= area.getY())
        card.setPosition(target.getCentreX() - w / 2, target.getY() - gap - h);
    else
        card = card.withCentre(area.getCentre());
    return card.constrainedWithin(area);
}

void TourOverlay::layoutCard()
{
    const auto card = cardBounds();
    const float s = (float) card.getWidth() / 470.0f;
    const int keyH = juce::jmax(26, juce::roundToInt(36.0f * s)), pad = juce::roundToInt(18.0f * s);
    auto row = card.reduced(pad).removeFromBottom(keyH);
    closeKey->setBounds(row.removeFromLeft(juce::roundToInt(130.0f * s)));
    nextKey->setBounds(row.removeFromRight(juce::roundToInt(110.0f * s)));
    row.removeFromRight(juce::roundToInt(8.0f * s));
    backKey->setBounds(row.removeFromRight(juce::roundToInt(100.0f * s)));
    backKey->setEnabled(current > 0);
    const bool last = current + 1 >= (int) steps.size();
    nextKey->setButtonText(last ? "DONE" : "NEXT");
    nextKey->setTitle(last ? "Finish the tour" : "Next tour step");
    if (! steps.empty())
        setDescription("Tour step " + juce::String(current + 1) + " of " + juce::String((int) steps.size()) + ": "
                       + steps[(size_t) current].title + ". " + steps[(size_t) current].body);
}

void TourOverlay::resized()
{
    layoutCard();
}

void TourOverlay::paint(juce::Graphics& g)
{
    if (steps.empty())
        return;
    const auto& step = steps[(size_t) current];
    const auto target = step.target ? step.target() : juce::Rectangle<int>();
    // Dim the editor except the highlighted area.
    juce::Path dim;
    dim.addRectangle(getLocalBounds().toFloat());
    if (! target.isEmpty())
    {
        dim.addRoundedRectangle(target.toFloat().expanded(4.0f), 6.0f);
        dim.setUsingNonZeroWinding(false);
    }
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.fillPath(dim);
    if (! target.isEmpty())
    {
        g.setColour(amberBright.withAlpha(0.9f));
        g.drawRoundedRectangle(target.toFloat().expanded(4.0f), 6.0f, 2.0f);
    }
    // The card.
    const auto card = cardBounds().toFloat();
    const float s = card.getWidth() / 470.0f;
    softShadow(g, card, 8.0f, 0.0f, 6.0f, 14.0f, 0.8f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2d2a25), card.getX(), card.getY(), juce::Colour(0xff161411), card.getX(),
                                           card.getBottom(), false));
    g.fillRoundedRectangle(card, 8.0f);
    g.setColour(gold.withAlpha(0.75f));
    g.drawRoundedRectangle(card.reduced(0.5f), 8.0f, 1.4f);
    auto inner = card.reduced(18.0f * s, 14.0f * s);
    g.setFont(tourFont(11.0f * s, true).withExtraKerningFactor(0.16f));
    g.setColour(gold);
    g.drawText("TOUR  " + juce::String(current + 1) + " / " + juce::String((int) steps.size()), inner.removeFromTop(16.0f * s),
               juce::Justification::centredLeft, false);
    g.setFont(tourFont(19.0f * s, true).withExtraKerningFactor(0.04f));
    g.setColour(ivory);
    g.drawFittedText(step.title, inner.removeFromTop(30.0f * s).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
    inner.removeFromTop(4.0f * s);
    juce::AttributedString text;
    text.setWordWrap(juce::AttributedString::byWord);
    text.setLineSpacing(2.0f * s);
    text.append(step.body, tourFont(13.5f * s, false), ivory.withAlpha(0.9f));
    juce::TextLayout layout;
    layout.createLayout(text, inner.getWidth());
    layout.draw(g, inner.withTrimmedBottom(48.0f * s));
}

juce::File TourMemory::file()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("CS80Reference")
        .getChildFile("ui-settings.xml");
}

bool TourMemory::seen()
{
    if (auto xml = juce::XmlDocument::parse(file()))
        return xml->getBoolAttribute("tourSeen", false);
    return false;
}

void TourMemory::markSeen()
{
    auto target = file();
    std::unique_ptr<juce::XmlElement> xml = juce::XmlDocument::parse(target);
    if (xml == nullptr || ! xml->hasTagName("CS80_UI_SETTINGS"))
        xml = std::make_unique<juce::XmlElement>("CS80_UI_SETTINGS");
    xml->setAttribute("version", 1);
    xml->setAttribute("tourSeen", true);
    target.getParentDirectory().createDirectory();
    xml->writeTo(target);
}

} // namespace cs80::ui
