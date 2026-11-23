#include "plugin/PluginProcessor.h"
#include "plugin/StudioView.h"
#include "plugin/ArtCommon.h"

#include <cstdint>
#include <iostream>
#include <vector>

namespace
{
int checks = 0, failures = 0;
void check(bool ok, const char* label)
{
    ++checks;
    if (!ok) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
}

juce::Component* routingWidget(juce::Component& page)
{
    // The existing SUB/RING page places exactly one 712 x 100 live routing
    // widget here. No editor, native peer, or test replacement component.
    for (auto* child : page.getChildren())
        if (child->getBounds() == juce::Rectangle<int>(88, 440, 712, 100))
            return child;
    return nullptr;
}

std::uint64_t cropHash(const juce::Image& image, juce::Rectangle<int> region)
{
    std::uint64_t hash = 14695981039346656037ull;
    for (int y = region.getY(); y < region.getBottom(); ++y)
        for (int x = region.getX(); x < region.getRight(); ++x)
        { hash ^= image.getPixelAt(x, y).getARGB(); hash *= 1099511628211ull; }
    return hash;
}

std::uint64_t expectedText(const char* text, juce::Rectangle<int> region, bool bold,
                           juce::Colour colour, float kerning = 0.02f)
{
    // Match only the established typography, never a production multiplier or
    // formatter. The expected diagnostic strings below are literal test data.
    juce::Image image(juce::Image::ARGB, 712, 100, true);
    juce::Graphics g(image);
    g.setFont(cs80::art::displayFont(12.5f, bold ? juce::Font::bold : juce::Font::plain)
                  .withExtraKerningFactor(kerning));
    g.setColour(colour);
    g.drawText(text, region.toFloat(), juce::Justification::centredRight, false);
    return cropHash(image, region);
}

struct RepaintProbe final : juce::CachedComponentImage
{
    explicit RepaintProbe(juce::Component& c) : component(c) {}
    void paint(juce::Graphics& g) override { component.paint(g); }
    bool invalidateAll() override { ++invalidations; return true; }
    bool invalidate(const juce::Rectangle<int>&) override { ++invalidations; return true; }
    void releaseResources() override {}
    juce::Component& component;
    int invalidations = 0;
};

void tickTimers()
{
    juce::Thread::sleep(100);
    juce::Timer::callPendingTimersSynchronously();
}
}

int main(int argc, char** argv)
{
    // JUCE message-thread infrastructure only. Never addToDesktop, create an
    // editor/native peer, run a display server, or open an audio device.
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-cs80-sub-routing-display");
    Cs80AudioProcessor host(fixture.getFile());
    host.setParamValue(cs80::P_parityMode, 0);
    host.setParamValue(cs80::P_sourcePerformance, 1);
    host.setParamValue(cs80::P_subOriginalMode, 1);
    host.setParamValue(cs80::P_subAtSpeed, 0.5f);
    cs80::ui::StudioView studio(host);
    studio.setSize(1600, 936);
    studio.showPage(1);
    studio.setVisible(true);
    auto* page = studio.getSubPage(studio.getActiveTab(), studio.getActiveSubPage());
    check(page != nullptr, "actual Studio SUB/RING page is available");
    if (page == nullptr) return 1;
    // The built-in page's coordinate system is 1600 x 936.
    page->setSize(1600, 936);
    auto* widget = routingWidget(*page);
    check(widget != nullptr, "actual SUB routing widget is found without a native peer");
    if (widget == nullptr) return 1;
    auto* probe = new RepaintProbe(*widget);
    widget->setCachedComponentImage(probe);
    tickTimers();
    const auto render = [&] { return widget->createComponentSnapshot(widget->getLocalBounds()); };
    const auto parameterValues = [&]
    {
        std::vector<float> values;
        for (auto* parameter : host.getParameters()) values.push_back(parameter->getValue());
        return values;
    };
    const juce::Rectangle<int> amountText(528, 24, 90, 22);
    const juce::Rectangle<int> maximumText(624, 24, 74, 22);
    const auto circuitHalf = render();
    check(cropHash(circuitHalf, amountText) == expectedText(
              "x1.83", amountText, true, cs80::ui::amberBright),
          "Circuit Original half-depth displays the independent x1.83 anchor");
    check(cropHash(circuitHalf, maximumText) == expectedText(
              "max x3.33", maximumText, false, cs80::ui::ivoryDim.withAlpha(0.92f), 0.0f),
          "Circuit Original maximum displays the independent x3.33 anchor");

    // Old host slots remain addressable, but neither requested value selects
    // another engine, dims the routing display, or creates a repaint change.
    const auto fixedValues = parameterValues();
    for (const int index : {cs80::P_sourcePerformance, cs80::P_parityMode})
        for (const float request : {0.0f, 1.0f})
        {
            const int before = probe->invalidations;
            host.setParamValue(index, request);
            tickTimers();
            check(host.getParamValue(cs80::P_sourcePerformance) == 1.0f
                      && host.getParamValue(cs80::P_parityMode) == 0.0f,
                  "retired engine writes retain the fixed Circuit readback");
            check(parameterValues() == fixedValues,
                  "retired engine writes leave every host parameter unchanged");
            check(probe->invalidations == before,
                  "retired engine writes do not invalidate the unchanged routing diagnostic");
            check(cropHash(render(), widget->getLocalBounds())
                      == cropHash(circuitHalf, widget->getLocalBounds()),
                  "retired engine writes neither alter nor dim the routing display");
        }

    // Original and Extended are still meaningful SUB waveform sets within the
    // same Circuit engine. Their full-touch rate laws and repaint must differ.
    const int beforeExtended = probe->invalidations;
    host.setParamValue(cs80::P_subOriginalMode, 0);
    tickTimers();
    check(probe->invalidations > beforeExtended,
          "switching to Extended SUB invalidates the changed AT SPEED diagnostic");
    const auto extendedHalf = render();
    check(cropHash(extendedHalf, amountText) == expectedText(
              "x2.00", amountText, true, cs80::ui::amberBright),
          "Circuit Extended half-depth displays the independent x2.00 anchor");
    check(cropHash(extendedHalf, maximumText) == expectedText(
              "max x3.00", maximumText, false, cs80::ui::ivoryDim.withAlpha(0.92f), 0.0f),
          "Circuit Extended maximum displays the independent x3.00 anchor");
    check(cropHash(circuitHalf, amountText) != cropHash(extendedHalf, amountText),
          "the real SUB waveform-set switch changes the AT SPEED numeric text");

    struct Case { float amount; const char* extended; const char* original; };
    // Independent powers of 10/3 rounded to two decimals; zero retains the
    // established 'no added modulation' display rather than changing UI policy.
    constexpr Case cases[] = {
        {0.0f, "0", "0"}, {0.25f, "x1.50", "x1.35"},
        {0.5f, "x2.00", "x1.83"}, {1.0f, "x3.00", "x3.33"}
    };
    for (bool retiredIsolationRequest : {false, true})
        for (bool retiredSourceRequest : {false, true})
        {
            host.setParamValue(cs80::P_parityMode, retiredIsolationRequest ? 1.0f : 0.0f);
            host.setParamValue(cs80::P_sourcePerformance, retiredSourceRequest ? 1.0f : 0.0f);
            check(host.getParamValue(cs80::P_parityMode) == 0.0f
                      && host.getParamValue(cs80::P_sourcePerformance) == 1.0f,
                  "every retired request pair keeps the same Circuit routing policy");
            for (bool original : {false, true})
                for (const auto& test : cases)
                {
                    host.setParamValue(cs80::P_subOriginalMode, original ? 1.0f : 0.0f);
                    host.setParamValue(cs80::P_subAtSpeed, test.amount);
                    const auto image = render();
                    const bool lit = test.amount > 0;
                    check(cropHash(image, amountText) == expectedText(
                              original ? test.original : test.extended, amountText, lit,
                              lit ? cs80::ui::amberBright : cs80::ui::ivoryDim),
                          "actual readout follows the chosen Original/Extended full-touch law");
                    check(cropHash(image, maximumText) == expectedText(
                              original ? "max x3.33" : "max x3.00", maximumText, false,
                              cs80::ui::ivoryDim.withAlpha(0.92f), 0.0f),
                          "actual maximum readout follows the chosen SUB waveform set");
                }
        }

    host.setParamValue(cs80::P_parityMode, 0);
    host.setParamValue(cs80::P_sourcePerformance, 1);
    host.setParamValue(cs80::P_subOriginalMode, 0);
    host.setParamValue(cs80::P_subAtSpeed, 0.5f);
    tickTimers();
    const auto beforeOriginal = probe->invalidations;
    host.setParamValue(cs80::P_subOriginalMode, 1);
    tickTimers();
    check(probe->invalidations > beforeOriginal,
          "waveform-set-only change invalidates the AT SPEED diagnostic");
    check(cropHash(render(), amountText) == cropHash(circuitHalf, amountText),
          "switching back to Original restores the Circuit half-depth readout");

    const auto values = parameterValues();
    (void)render();
    tickTimers();
    check(parameterValues() == values, "painting and polling this diagnostic never edits host parameter values");
    if (argc > 1)
    {
        const juce::File output(argv[1]);
        check(output.createDirectory().wasOk(), "optional offline capture directory is available");
        for (bool original : {false, true})
            for (const auto size : {juce::Point<int>(712, 100), juce::Point<int>(498, 70)})
            {
                host.setParamValue(cs80::P_subOriginalMode, original ? 1.0f : 0.0f);
                widget->setSize(size.x, size.y);
                const auto image = render();
                const auto file = output.getChildFile(juce::String(original ? "circuit-original" : "circuit-extended")
                    + "-" + juce::String(size.x) + ".png");
                auto stream = file.createOutputStream();
                check(stream != nullptr && stream->setPosition(0) && stream->truncate().wasOk()
                          && juce::PNGImageFormat{}.writeImageToStream(image, *stream),
                      "optional real-widget offline capture writes successfully");
            }
    }
    std::cout << "SUB routing display: " << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
