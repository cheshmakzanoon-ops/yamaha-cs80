#pragma once

// Shared panel art for the editor and its controls: the panel typeface, the design-space type floor,
// the wrinkle-paint finish and the printed/etched/machined painters every panel section uses.
// Message thread only (painting). One copy of each helper; do not duplicate them per file.

#include <juce_gui_basics/juce_gui_basics.h>

namespace cs80::art
{
using Rf = juce::Rectangle<float>;
using juce::Colour;

// ---- Type -----------------------------------------------------------------------------------
// The smallest printed legend, in screen pixels, at every editor size including the 1120 x 700 minimum.
inline constexpr float kLegendFloorPx = 10.5f;
// Painters that draw in the 1600 x 1000 design space (chassis, plates, lid, meter, lamps) run under a
// FloorScope, so printed type never falls under kLegendFloorPx screen pixels when the editor is scaled down.
// Code that sizes fonts in screen pixels applies kLegendFloorPx itself.
struct FloorScope
{
    explicit FloorScope(float screenScale);
    ~FloorScope();
    FloorScope(const FloorScope&) = delete;
    FloorScope& operator=(const FloorScope&) = delete;
    float previous;
    bool previousSmall;
};
// The current floor in design units (0 outside a FloorScope).
float designFloor() noexcept;
// True while painting for an editor narrower than 85 percent of the design width.
bool isSmallEditor() noexcept;

// The embedded panel typeface (Barlow Condensed, OFL; bold = SemiBold), or null if it could not be loaded.
const juce::Typeface::Ptr& panelTypeface(bool bold);
// Release shared strong references while the final editor's JUCE graphics lifetime is live.
// Fonts/images held by a still-live component retain their own reference-counted ownership.
void releasePanelResources();
// The panel typeface at `height`, never below the current design floor.
juce::Font displayFont(float height, int style = juce::Font::plain);
juce::Font spacedFont(float height, float kerning, float hscale = 1.0f, int style = juce::Font::bold);
float textWidth(const juce::Font& font, const juce::String& text);

// ---- Colour and gradients ---------------------------------------------------------------------
inline Colour black(float a) { return juce::Colours::black.withAlpha(a); }
inline Colour white(float a) { return juce::Colours::white.withAlpha(a); }
// Matte instrument plate colour (voice plate, memory lid, status strip): also the halo under its type.
inline Colour plateColour() { return Colour(0xff262421); }
// The smoked glass of the voice-flow window (also the halo under its type).
inline Colour glassColour() { return Colour(0xff1c1a17); }
inline juce::ColourGradient vgrad(Colour top, Colour bottom, Rf r)
{
    return juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
}
inline juce::ColourGradient hgrad(Colour left, Colour right, Rf r)
{
    return juce::ColourGradient(left, r.getX(), r.getY(), right, r.getRight(), r.getY(), false);
}

// ---- Wrinkle-paint finish ---------------------------------------------------------------------
// Near-black enamel broken into crisp cells of 2 to 3 px (toroidal Worley cells, so the tile wraps).
juce::Image makeWrinkleTile(int n, int seed, Colour albedo, float contrast);
// The one finish used by the toolbar, panel, deck and memory lid (built once, cached).
const juce::Image& panelTile();
// Tile an image at one texel per device pixel inside a design-space context.
void fillRelief(juce::Graphics& g, const juce::Image& tile, Rf area, float deviceScale,
                float opacity, float shiftX = 0.0f, float shiftY = 0.0f);
void fillTile(juce::Graphics& g, const juce::Image& tile, Rf area, float deviceScale,
              float shiftX = 0.0f, float shiftY = 0.0f);

// ---- Depth: recesses, raised plates, metal ----------------------------------------------------
void insetShadow(juce::Graphics& g, Rf r, float radius, float depth, float alpha);
// The edge of a recess: a dark line outside the top/left, a lit lip outside the bottom/right.
void recessEdge(juce::Graphics& g, Rf r, float radius, float strength = 1.0f);
void recessTray(juce::Graphics& g, Rf r, float radius, Colour top, Colour bottom, float depth = 10.0f, float shade = 0.65f);
void raisedPlate(juce::Graphics& g, Rf r, float radius, Colour top, Colour bottom, float lift = 1.0f);
void brushedMetal(juce::Graphics& g, Rf r, float radius, float brightness, int seed);
void slotScrew(juce::Graphics& g, juce::Point<float> c, float r, float turn);
// Soft contact shadow all around an object that stands on the panel (no offset).
void ambient(juce::Graphics& g, Rf r, float radius, float spread = 6.0f, float alpha = 0.40f);

// ---- Print ------------------------------------------------------------------------------------
// Printed legend with a drop shadow, in the cream silkscreen colour.
void legend(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
            juce::Justification just = juce::Justification::centred,
            Colour ink = Colour(0xffe6dcbc), float shadow = 0.6f);
// Printed text with a plate-coloured halo so lines and dots never cut through it.
void haloText(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
              juce::Justification just, Colour ink, Colour halo);
// Engraved lettering on metal: the cut is darker, lit on its lower right lip.
void engraved(juce::Graphics& g, const juce::String& text, Rf area, const juce::Font& font,
              juce::Justification just = juce::Justification::centred);
// Gold line art of the panel (frames, grouping rules), etched into the enamel.
float etchAlpha(float a);
void etchLine(juce::Graphics& g, float x1, float y1, float x2, float y2, float alpha);
void etchRect(juce::Graphics& g, Rf r, float radius, float alpha);
// Gold etched grouping bracket: a rule with its title in a gap and short end ticks. A title that does not
// fit falls back to shortTitle, then to no text.
void bracket(juce::Graphics& g, const juce::String& title, float x, float ruleY, float w,
             float tick = 6.0f, float fontSize = 9.5f, const juce::String& shortTitle = {});
// A small toothed wheel (the ADVANCED key).
void gearGlyph(juce::Graphics& g, juce::Point<float> c, float radius, Colour ink);
} // namespace cs80::art
