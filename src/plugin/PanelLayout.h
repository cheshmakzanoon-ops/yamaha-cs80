#pragma once

// Panel geometry in the 1600 x 1000 design space: the one layout table every panel section, the chassis
// art and the editor's resized() read. Coordinates are design units; the editor scales them uniformly.

#include <juce_graphics/juce_graphics.h>

#include <array>

namespace cs80::panel::geo
{
using Rf = juce::Rectangle<float>;

// The instrument body (toolbar, panel, deck) sits between the wooden cheeks.
const float bodyLeft = 48.0f, bodyRight = 1552.0f;

// Toolbar y 0-64
const Rf brandBadge   { 54, 8, 34, 34 };
const Rf brandText    { 96, 10, 150, 30 };
const Rf prevBtn      { 252, 9, 34, 34 };
const Rf presetWell   { 292, 6, 414, 40 };
const Rf presetNumber { 302, 10, 52, 30 };
const Rf presetTag    { 372, 9, 228, 12 };
const Rf presetIndex  { 600, 9, 72, 12 };
const Rf presetName   { 372, 21, 300, 24 };
const Rf presetChevron{ 678, 18, 18, 12 };
const Rf nextBtn      { 712, 9, 34, 34 };
const Rf cluster      { 762, 6, 350, 40 };
const Rf libraryBtn   { 768, 10, 90, 32 };
const Rf saveBtn      { 862, 10, 66, 32 };
const Rf initBtn      { 944, 13, 58, 26 };
const Rf advancedBtn  { 1014, 10, 92, 32 };
const Rf lampWell     { 1122, 6, 128, 40 };
const Rf meterWell    { 1258, 6, 196, 40 };
const Rf meterText    { 1262, 17, 52, 20 };
const Rf meterBar     { 1318, 11, 128, 14 };
const Rf panicBtn     { 1466, 10, 74, 32 };   // >= 50 x 20 px at the 1120 x 700 minimum (PluginPanicTests)
// The status strip is a recessed window under the toolbar whose length follows what it prints (see layoutStrip).
const float stripX = 52.0f, stripY = 46.0f, stripH = 16.0f, stripPad = 12.0f, stripGap = 24.0f;
const Rf ticker       { 54, 47, 1492, 14 };
const Rf rankI        { 64, 47, 150, 14 };
const Rf rankII       { 238, 47, 150, 14 };
const Rf voiceStatus  { 1300, 47, 100, 14 };      // (blank: the lamps in the toolbar already show voices and MIDI)
const Rf midiStatus   { 1410, 47, 100, 14 };
const Rf subStatus    { 412, 47, 200, 14 };
const Rf modeStatus   { 636, 47, 300, 14 };
const std::array<float, 5> tickerRules { 283.0f, 509.0f, 673.0f, 797.0f, 1001.0f };

// ---- Control surface, y 64-636 -----------------------------------------------------------------
// One table in final design coordinates, derived from docs/PANEL_CONFORMANCE.md. Areas follow the original
// panels: the block-diagram lid over the memories at the upper left, the two programmable rows to its right
// (PN1), the front strip (PN2), then the deck: the left-hand panel (Panel 3), the ribbon, and the rear jack
// panel (Panel 5). Placement is documented; spacing and proportions are declared inferences sized for
// 1600 x 1000 and the 1120 x 700 minimum (S2 could not be read; see the conformance doc).
const float faceTop = 64.0f, lipBottom = 72.0f;
const float faceBottom = 636.0f, deckBottom = 748.0f;           // the deck ends where the keybed starts

// Upper left: the closed memory lid, printed with the voice block diagram (owner index 46).
const Rf lidFace      { 52, 74, 288, 378 };
const Rf lidDiagram   { 64, 100, 264, 260 };                      // the printed block diagram on the lid
const Rf lidOptions   { 60, 152, 272, 238 };                     // software voice settings, shown over the diagram
const Rf memoryLidKey { 62, 412, 130, 26 };
const Rf voiceKey     { 200, 412, 130, 26 };

// The two programmable rows (owner index 20-45), rank I above rank II.
const std::array<float, 2> rowY0 { 80.0f, 272.0f };
const float rowLeft = 348.0f, rowTop = -6.0f, rowHeight = 186.0f;   // frame: x rowLeft..bodyRight, y rowY0 + rowTop
const float rowTagX = 356.0f, rowTagY = 66.0f;                  // engraved channel numeral, relative to rowY0
const float rowFirstX = 392.0f, rowPitch = 43.0f, rowFaderW = 40.0f, rowGroupGap = 10.0f;
const float rowFaderY = 24.0f, rowFaderH = 148.0f;              // relative to rowY0
inline Rf rowFrame(int channel) { return { rowLeft, rowY0[(size_t) channel] + rowTop, bodyRight - rowLeft, rowHeight }; }

// Front strip, PN2 (owner index 2-12, 16, 18): levers, the two tone-selector rows and the two rotaries.
const Rf frontStrip   { 48, 460, 1504, 166 };
const float stripLeverY = 486.0f, stripLeverH = 126.0f, stripRuleY = 475.0f;
const float stripFirstX = 52.0f, stripKnobW = 46.0f, stripGroupGap = 5.0f;
const float stripLeverPitch = 29.25f, stripLeverW = 27.5f;      // 24 levers share what the tone selector leaves
// Tone selector: 14 tabs per row with the full hardware names printed on them.
const float tonePitch = 44.0f, toneW = 42.0f, toneTabH = 41.0f, toneRowGap = 4.0f;
const float toneMarkerW = 23.0f, toneMarginRight = 7.0f, toneTop = 503.0f;
const float toneBezelW = toneMarkerW + 13.0f * tonePitch + toneW + toneMarginRight;

// ---- Deck, y 636-748 -----------------------------------------------------------------------------
const float deckRuleY = 650.0f, deckControlTop = 656.0f;
// The left-hand panel (Panel 3): sustain, portamento/glissando, the footswitch assigners, tremolo/chorus and
// the foot-controller selector with the on-screen pedal.
const Rf deckLeft     { 48, 640, 492, 104 };
// The rear jack panel (Panel 5): footswitch jack and contact, output HIGH/LOW. Not on the original front panel.
const Rf deckRight    { 1428, 640, 124, 104 };
const Rf ribbonRect   { 556, 700, 860, 36 };
const Rf nameplate    { 826, 643, 320, 38 };
const Rf keybed       { 48, 748, 1504, 252 };
const Rf overlay      { 0, 64, 1600, 936 };
// Dimming and the toolbar's shadow stop at the walnut cheeks, which stay lit behind every overlay.
const Rf overlayBody  { 48, 64, 1504, 936 };
// The opened lid: the magnified programmer, with its hinge band above and its cast shadow below.
const Rf memoryLid    { 48, 66, 1504, 590 };
const float lidTop = 10.0f, lidHeight = 552.0f;
} // namespace cs80::panel::geo
