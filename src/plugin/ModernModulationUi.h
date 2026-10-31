#pragma once
#include "ParameterContext.h"

#include "core/ModernLayer.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>
#include <vector>

class Cs80AudioProcessor;

// Self-contained pages of the modern modulation and FX layer (ws5-modern), for the Studio
// view's Modulation, Keyboard and FX tabs (ws8-studio mounts them). Each page takes the
// processor and edits either APVTS parameters (through standard JUCE attachments) or the
// MSEG/matrix session data (through Cs80AudioProcessor::getModernLayer()). None of them is
// part of the original panel; PURE CS-80 makes everything they control inert.
namespace cs80::ui::modern_layer
{

// Drawable MSEG editor for one envelope.
//   drag a point          move it (the first point stays at time 0); Shift = fine
//   drag a segment        bend its curve (up = faster start)
//   double-click empty    add a point;   double-click a point: delete it
//   drag the S / E flags  move the loop start / end (they snap to points)
//   Delete / Backspace    delete the selected point
class MsegEditor final : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    explicit MsegEditor(Cs80AudioProcessor& processor);
    ~MsegEditor() override;

    void setMsegIndex(int index);
    int getMsegIndex() const noexcept { return msegIndex; }
    const cs80::modern::MsegShape& getShape() const noexcept { return shape; }

    // Geometry, public for tests and hosts of the page.
    juce::Rectangle<float> plotArea() const;
    juce::Point<float> pointPosition(int point) const;
    juce::Point<float> toScreen(double time, double level) const;
    double timeAt(float x) const;
    double levelAt(float y) const;
    float viewLength() const;
    juce::Rectangle<float> loopMarkerArea(bool end) const;

    void paint(juce::Graphics&) override;
    void resized() override {}
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    enum class Drag { none, point, curve, loopStart, loopEnd };
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void reload();
    void commit();
    int hitPoint(juce::Point<float>) const;
    int segmentAt(float x) const;
    int nearestPoint(float x) const;

    Cs80AudioProcessor& processor;
    int msegIndex = 0;
    cs80::modern::MsegShape shape;
    Drag drag = Drag::none;
    int dragIndex = -1, selected = -1, hovered = -1;
    float dragStartCurve = 0.0f;
    juce::Point<float> dragStart;
    float frozenViewLength = 0.0f; // the time axis does not rescale under a dragging pointer
    double playhead = -1.0;
};

// MSEG page: envelope selector, editor, loop/trigger/sync/bipolar, rate and shape presets.
class MsegPage final : public juce::Component, private juce::ChangeListener
{
public:
    explicit MsegPage(Cs80AudioProcessor& processor);
    ~MsegPage() override;
    void selectMseg(int index);
    MsegEditor& getEditor() noexcept { return editor; }
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refreshControls();
    void writeShape(const std::function<void(cs80::modern::MsegShape&)>& edit);

    Cs80AudioProcessor& processor;
    std::array<juce::TextButton, cs80::modern::kMsegCount> selectors;
    MsegEditor editor;
    juce::ComboBox loopBox, triggerBox, presetBox;
    juce::ToggleButton syncButton { "Sync" }, bipolarButton { "Bipolar" };
    juce::Slider rate { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label rateLabel;
    std::unique_ptr<juce::SliderParameterAttachment> rateAttachment;
    bool refreshing = false;
};

// The three modern LFOs: waveform (ws5) plus the existing rate, depth and destination.
class LfoPanel final : public juce::Component
{
public:
    explicit LfoPanel(Cs80AudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Row
    {
        juce::Label title;
        ContextComboBox wave, destination;
        juce::Slider rate { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Slider depth { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::ComboBoxParameterAttachment> waveAttachment, destinationAttachment;
        std::unique_ptr<juce::SliderParameterAttachment> rateAttachment, depthAttachment;
    };
    std::array<Row, 3> rows;
};

// 16-slot modulation matrix: source, via, destination, rank target, amount.
class ModMatrixPage final : public juce::Component, private juce::ChangeListener
{
public:
    explicit ModMatrixPage(Cs80AudioProcessor& processor);
    ~ModMatrixPage() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::ComboBox& sourceBox(int slot) { return rows[(size_t) slot].source; }
    juce::ComboBox& viaBox(int slot) { return rows[(size_t) slot].via; }
    juce::ComboBox& destinationBox(int slot) { return rows[(size_t) slot].destination; }
    juce::ComboBox& targetBox(int slot) { return rows[(size_t) slot].target; }
    juce::Slider& amountSlider(int slot) { return rows[(size_t) slot].amount; }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void refresh();
    void write(int slot);

    struct Row
    {
        juce::Label index;
        juce::ComboBox source, via, destination, target;
        juce::Slider amount { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::SliderParameterAttachment> amountAttachment;
    };
    Cs80AudioProcessor& processor;
    std::array<Row, cs80::modern::kModSlots> rows;
    bool refreshing = false;
};

// Keyboard modulation page: response curves of the controller-derived matrix sources.
class KeyboardModulationPage final : public juce::Component
{
public:
    explicit KeyboardModulationPage(Cs80AudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::Slider& curveSlider(int index) { return curves[(size_t) index].slider; }

private:
    struct Curve
    {
        juce::Label title;
        juce::Slider slider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
        juce::Rectangle<int> plot;
        bool bipolar = false;
    };
    std::array<Curve, 4> curves;
    juce::Label note;
};

// Modern FX page: three serial slots (model, classic type, mix, two controls, drive
// oversampling) and the loudness-match switch.
class ModernFxPage final : public juce::Component, private juce::Timer
{
public:
    explicit ModernFxPage(Cs80AudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
    juce::ComboBox& modelBox(int slot) { return slots[(size_t) slot].model; }
    juce::ToggleButton& loudnessMatchButton() { return gainComp; }

private:
    void timerCallback() override;
    void refreshLabels();
    struct Slot
    {
        juce::Label title, mixLabel, aLabel, bLabel;
        ContextComboBox model, type, oversampling;
        juce::Slider mix { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Slider a { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        juce::Slider b { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
        std::unique_ptr<juce::ComboBoxParameterAttachment> modelAttachment, typeAttachment, oversamplingAttachment;
        std::unique_ptr<juce::SliderParameterAttachment> mixAttachment, aAttachment, bAttachment;
        int shownModel = -1;
    };
    Cs80AudioProcessor& processor;
    std::array<Slot, 3> slots;
    ContextToggleButton gainComp { "Loudness match (all slots)" };
    std::unique_ptr<juce::ButtonParameterAttachment> gainCompAttachment;
};

// Studio "Modulation" tab content: MSEG page above, LFOs and the matrix below.
class ModernModulationPage final : public juce::Component
{
public:
    explicit ModernModulationPage(Cs80AudioProcessor& processor);
    void resized() override;
    void paint(juce::Graphics&) override;
    MsegPage& getMsegPage() noexcept { return msegPage; }
    LfoPanel& getLfoPanel() noexcept { return lfoPanel; }
    ModMatrixPage& getMatrixPage() noexcept { return matrixPage; }

private:
    MsegPage msegPage;
    LfoPanel lfoPanel;
    ModMatrixPage matrixPage;
    juce::Viewport matrixViewport;
};

} // namespace cs80::ui::modern_layer
