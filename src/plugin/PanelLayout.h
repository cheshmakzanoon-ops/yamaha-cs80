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

// Control surface y 64-660
// The raised back edge is an 8 px ledge (y64-72). Everything from the voice plate down was designed with a 32 px
// ledge, so it is laid out in those coordinates and moved up by midShift when it is painted and placed; the
// 24 px it frees go to the keybed (see shifted()).
const float faceTop = 64.0f, faceBottom = 660.0f, deckBottom = 772.0f;       // design coordinates of the shifted block
const float lipBottom = 72.0f, midShift = 24.0f, midTop = 96.0f;
const float deckBottomFinal = deckBottom - midShift;                          // 748: where the keybed starts
inline float shifted(float y) { return y >= midTop ? y - midShift : y; }
const Rf plate        { 48, 98, 360, 378 };
const Rf memoryLidKey { 62, 436, 160, 24 };
const Rf voiceKey     { 230, 436, 160, 24 };
const Rf viewKeyI     { 332, 112, 26, 22 };
const Rf viewKeyII    { 360, 112, 30, 22 };
const std::array<float, 2> rowY0 { 104.0f, 296.0f };
const Rf lowerPlate   { 48, 484, 1504, 166 };
const Rf toneBezel    { 660, 520, 537, 102 };
const float toneX0 = 683.0f, tonePitch = 36.4f, toneW = 34.0f;

// Performance deck y 660-772 (outer edges at x48 and x1552 like the lower plate and the key area). The two
// blocks are kept to 342 px so the ribbon can run 804 px between them.
const Rf deckLeft     { 48, 664, 342, 104 };
const Rf deckRight    { 1210, 664, 342, 104 };
const Rf nameplate    { 640, 667, 320, 38 };
const Rf ribbonRect   { 398, 724, 804, 36 };
const Rf keybed       { 48, 748, 1504, 252 };           // final coordinates (not shifted)
const Rf overlay      { 0, 64, 1600, 936 };
// Dimming and the toolbar's shadow stop at the walnut cheeks, which stay lit behind every overlay.
const Rf overlayBody  { 48, 64, 1504, 936 };
// The lid component also carries its hinge band above and its cast shadow below.
const Rf memoryLid    { 48, 66, 1504, 590 };            // final coordinates (not shifted)
const float lidTop = 10.0f, lidHeight = 552.0f;
} // namespace cs80::panel::geo
