#include "plugin/PluginProcessor.h"
#include "plugin/StudioView.h"
#include "plugin/ArtCommon.h"

#include <cmath>
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
    host.setParamValue(cs80::P_sourcePerformance, 0);
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
    const juce::Rectangle<int> amountText(528, 24, 90, 22);
    const auto legacyHalf = render();
    const int before = probe->invalidations;
    host.setParamValue(cs80::P_sourcePerformance, 1);
    tickTimers();
    check(probe->invalidations > before,
          "engine-mode-only change invalidates the AT SPEED diagnostic");
    const auto circuitHalf = render();
    // Independent documented 200/60 anchor. At half amount and full touch the
    // Original circuit rate factor is sqrt(10/3), shown rounded as x1.83.
    const float linearEquivalent = static_cast<float>((std::sqrt(10.0 / 3.0) - 1.0) * 0.5);
    host.setParamValue(cs80::P_sourcePerformance, 0);
    host.setParamValue(cs80::P_subAtSpeed, linearEquivalent);
    tickTimers();
    const auto legacyEquivalent = render();
    check(cropHash(circuitHalf, amountText) == cropHash(legacyEquivalent, amountText),
          "Circuit Original half-depth displays x1.83 rather than the legacy x2.00");
    check(cropHash(circuitHalf, amountText) != cropHash(legacyHalf, amountText),
          "valid engine-mode switch changes the AT SPEED numeric text");

    const juce::Rectangle<int> maximumText(624, 24, 74, 22);
    struct Case { float amount; const char* linear; const char* exponential; };
    // Independent powers of 10/3 rounded to two decimals; zero retains the
    // established 'no added modulation' display rather than changing UI policy.
    constexpr Case cases[] = {
        {0.0f, "0", "0"}, {0.25f, "x1.50", "x1.35"},
        {0.5f, "x2.00", "x1.83"}, {1.0f, "x3.00", "x3.33"}
    };
    for (bool isolation : {false, true})
        for (bool source : {false, true})
            for (bool original : {false, true})
                for (const auto& test : cases)
                {
                    host.setParamValue(cs80::P_parityMode, isolation ? 1.0f : 0.0f);
                    host.setParamValue(cs80::P_sourcePerformance, source ? 1.0f : 0.0f);
                    host.setParamValue(cs80::P_subOriginalMode, original ? 1.0f : 0.0f);
                    host.setParamValue(cs80::P_subAtSpeed, test.amount);
                    const auto image = render();
                    const bool exponential = isolation || (source && original);
                    const bool lit = test.amount > 0 && !isolation;
                    check(cropHash(image, amountText) == expectedText(
                              exponential ? test.exponential : test.linear, amountText, lit,
                              lit ? cs80::ui::amberBright : cs80::ui::ivoryDim),
                          "actual readout uses the effective Original/circuit full-touch law");
                    check(cropHash(image, maximumText) == expectedText(
                              exponential ? "max x3.33" : "max x3.00", maximumText, false,
                              cs80::ui::ivoryDim.withAlpha(0.92f), 0.0f),
                          "actual maximum readout follows the effective mode");
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

    std::vector<float> values;
    for (auto* parameter : host.getParameters()) values.push_back(parameter->getValue());
    (void)render();
    tickTimers();
    bool unchanged = true;
    for (int i = 0; i < host.getParameters().size(); ++i)
        unchanged &= values[static_cast<std::size_t>(i)] == host.getParameters()[i]->getValue();
    check(unchanged, "painting and polling this diagnostic never edits host parameter values");
    if (argc > 1)
    {
        const juce::File output(argv[1]);
        check(output.createDirectory().wasOk(), "optional offline capture directory is available");
        for (bool circuit : {false, true})
            for (const auto size : {juce::Point<int>(712, 100), juce::Point<int>(498, 70)})
            {
                host.setParamValue(cs80::P_sourcePerformance, circuit ? 1.0f : 0.0f);
                widget->setSize(size.x, size.y);
                const auto image = render();
                const auto file = output.getChildFile(juce::String(circuit ? "circuit" : "legacy")
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
