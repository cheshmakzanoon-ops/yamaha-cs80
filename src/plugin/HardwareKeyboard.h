#pragma once

#include "PluginProcessor.h"
#include "UiComponents.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

// On-screen keyboard. Owner of everything about how the keys look and animate.
// Behaviour (gesture capture, pressure, host key mirroring) must stay as written:
// tests in src/tools/ui_snapshot.cpp and tests/Keyboard*Tests exercise it.
//
// Drawing model (all vector, software renderer):
//  - HardwareKeyboard::paint() blits one cached image of the whole key surface at rest
//    (rebuilt only when size, key width or pixel density change) and then redraws,
//    over it, only the narrow key columns that are pressed, moving or hovered.
//  - Key depth is a smoothed value per key (12 ms attack, 90 ms release). A 60 Hz
//    timer exists only while some depth is non-zero and repaints only dirty keys.
//  - Keybed paints the walnut end blocks, the felt rails, the gold etched line and the
//    "C" labels once into a cached image, and lays the panel-lip shadow and the felt
//    glow of pressed keys over the keys.
// Everything is lit from the upper left.
namespace cs80::ui
{
namespace keys_detail
{
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using Stop = std::pair<float, Colour>;

// Design-space size of the keybed assembly and its frame (1512 x 228 at a 1600 wide editor).
// 18 px side blocks leave 1476 px for 36 white keys: exactly 41 px per key.
constexpr float kDesignWidth = 1512.0f;
constexpr float kDesignHeight = 228.0f;
constexpr float kDesignKeyWidth = 41.0f;

inline const Colour kSeam { 0xff090706 };
inline const Colour kAmber { 0xffffa63a };
inline const Colour kGold { 0xffcdb878 };

inline Colour white(float a) { return juce::Colours::white.withAlpha(a); }
inline Colour black(float a) { return juce::Colours::black.withAlpha(a); }
inline Colour amber(float a) { return kAmber.withAlpha(a); }

inline float hash01(int a, int b) noexcept
{
    std::uint32_t h = (std::uint32_t) a * 0x9E3779B1u ^ (((std::uint32_t) b + 0x7F4A7C15u) * 0x85EBCA6Bu);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return (float) (h & 0xffffffu) / 16777216.0f;
}

inline juce::ColourGradient makeGrad(float x0, float y0, float x1, float y1, std::initializer_list<Stop> stops)
{
    const auto* first = stops.begin();
    const auto* last = stops.end() - 1;
    juce::ColourGradient grad(first->second, x0, y0, last->second, x1, y1, false);
    for (const auto* it = first + 1; it != last; ++it)
        grad.addColour((double) it->first, it->second);
    return grad;
}

// Fill `area` with a vertical (top to bottom of `span`) or horizontal gradient.
inline void fillV(Graphics& g, Rectangle<float> area, Rectangle<float> span, std::initializer_list<Stop> stops)
{
    if (area.isEmpty() || span.getHeight() <= 0.0f) return;
    g.setGradientFill(makeGrad(span.getX(), span.getY(), span.getX(), span.getBottom(), stops));
    g.fillRect(area);
}
inline void fillV(Graphics& g, Rectangle<float> area, std::initializer_list<Stop> stops) { fillV(g, area, area, stops); }
inline void fillH(Graphics& g, Rectangle<float> area, Rectangle<float> span, std::initializer_list<Stop> stops)
{
    if (area.isEmpty() || span.getWidth() <= 0.0f) return;
    g.setGradientFill(makeGrad(span.getX(), span.getY(), span.getRight(), span.getY(), stops));
    g.fillRect(area);
}
inline void fillH(Graphics& g, Rectangle<float> area, std::initializer_list<Stop> stops) { fillH(g, area, area, stops); }

// Soft elliptical blob of light or shadow: colour `inner` at the centre, `mid` at 45 %, clear at the rim.
inline void fillBlob(Graphics& g, float cx, float cy, float rx, float ry, Colour inner, Colour mid, Rectangle<float> area)
{
    if (rx <= 0.0f || ry <= 0.0f) return;
    juce::ColourGradient grad(inner, cx, cy, mid.withAlpha(0.0f), cx + rx, cy, true);
    grad.addColour(0.45, mid);
    juce::FillType fill(grad);
    fill.transform = juce::AffineTransform::scale(1.0f, ry / rx, cx, cy);
    g.setFillType(fill);
    g.fillRect(area);
}

// Warm ivory with a small tonal variation per key (hashed from the note number).
inline Colour ivoryFor(int note)
{
    const float a = hash01(note, 1), b = hash01(note, 2), c = hash01(note, 3);
    return Colour::fromHSV(0.114f + (a - 0.5f) * 0.018f, 0.100f + b * 0.050f, 0.935f + c * 0.050f, 1.0f);
}

struct KeyLook
{
    float depth = 0.0f; // 0 at rest, 1 fully down
    float sink = 0.0f;  // pixels the front edge has dropped
    float glow = 0.0f;  // 0.5..1 strength of the amber glow when down (strike velocity and pointer pressure)
    bool hover = false;
};

// Harder strikes glow hotter: the amber drifts toward a pale gold.
inline Colour glowColour(float glow) { return kAmber.interpolatedWith(Colour(0xffffd890), juce::jlimit(0.0f, 1.0f, (glow - 0.55f) * 1.5f)); }
inline Colour glowColour(const KeyLook& k) { return glowColour(k.glow); }

// White key. `r` is the full key slot from JUCE (full height, no gap): the key is inset by half a
// seam on each side. Layers, back to front: top face, light reflection, edge bevels and falloff,
// rounded lip, vertical nose with its highlight and contact shadow, then the amber glow.
inline void drawWhiteKey(Graphics& g, int note, Rectangle<float> r, const KeyLook& k, float u)
{
    const float gap = 2.3f * u;
    const float x0 = r.getX() + gap * 0.5f, x1 = r.getRight() - gap * 0.5f;
    const float w = x1 - x0;
    const float y0 = r.getY(), yb = r.getBottom(), h = r.getHeight();
    const float noseRest = h * 0.150f;
    const float bevelH = juce::jmax(3.0f, h * 0.034f);
    const float edgeY = yb - noseRest + k.sink; // bottom of the lip, top of the nose
    const float topEnd = edgeY - bevelH;
    const float gd = k.glow * k.depth;
    const float d = k.depth;
    const Colour amb = glowColour(k);
    const Colour ivory = ivoryFor(note);

    juce::Path body;
    body.addRoundedRectangle(x0, y0, w, (yb - y0), 2.2f * u, 2.2f * u, false, false, true, true);
    g.saveState();
    g.reduceClipRegion(body);

    // Top face: shaded at the back where the panel overhangs, bright mid-section, gentle roll-off to the lip.
    const auto top = Rectangle<float>(x0, y0, w, topEnd - y0);
    fillV(g, top, { { 0.0f, ivory.darker(0.42f) }, { 0.12f, ivory.darker(0.20f) }, { 0.40f, ivory.darker(0.02f) },
                    { 0.70f, ivory.brighter(0.02f) }, { 1.0f, ivory.darker(0.13f) } });
    // Lengthwise tonal drift so neighbouring keys do not look cloned.
    const float drift = (hash01(note, 7) - 0.5f) * 0.10f;
    g.setColour((drift > 0.0f ? white(drift) : black(-drift)).withMultipliedAlpha(0.7f));
    g.fillRect(top);

    // Reflection of the lit panel above: a soft light band at the same height on every key.
    const float bandA = y0 + h * 0.20f, bandB = y0 + h * 0.46f;
    fillV(g, Rectangle<float>(x0, bandA, w, bandB - bandA),
          { { 0.0f, white(0.0f) }, { 0.45f, white(0.20f) }, { 1.0f, white(0.0f) } });

    // Rounded shoulders: both long edges roll away from the light; the left one still catches it.
    const float ao = 1.0f + 1.2f * d;
    fillH(g, Rectangle<float>(x0, y0, 5.0f * u, topEnd - y0),
          { { 0.0f, black(0.26f * ao) }, { 0.35f, black(0.09f * ao) }, { 1.0f, black(0.0f) } });
    fillH(g, Rectangle<float>(x1 - 7.0f * u, y0, 7.0f * u, topEnd - y0),
          { { 0.0f, black(0.0f) }, { 0.55f, black(0.10f * ao) }, { 1.0f, black(0.40f * ao) } });
    g.setColour(white(0.62f * (1.0f - 0.5f * d)));
    g.fillRect(x0 + 1.5f * u, y0, 0.9f * u, topEnd - y0);

    // Pressed: the top darkens and takes a warm cast from the glow beneath.
    if (d > 0.0f)
    {
        g.setColour(black(0.09f * d));
        g.fillRect(top);
        if (gd > 0.01f)
            fillV(g, Rectangle<float>(x0, topEnd - 58.0f * u, w, 58.0f * u),
                  { { 0.0f, amb.withAlpha(0.0f) }, { 0.55f, amb.withAlpha(0.07f * gd) }, { 1.0f, amb.withAlpha(0.30f * gd) } });
    }

    // Rounded front lip: bright upper edge rolling into the nose.
    const Colour lipHi = ivory.interpolatedWith(juce::Colours::white, 0.75f);
    fillV(g, Rectangle<float>(x0, topEnd, w, bevelH),
          { { 0.0f, lipHi }, { 0.30f, ivory.brighter(0.08f) }, { 1.0f, ivory.darker(0.16f) } });
    g.setColour(white(0.92f - 0.5f * d));
    g.fillRect(x0 + 0.4f * u, topEnd, w - 0.8f * u, 1.1f * u);

    // Nose: the vertical front, bright where it faces the light, shadowed toward the felt.
    const float noseH = yb - edgeY;
    const auto nose = Rectangle<float>(x0, edgeY, w, noseH + 1.0f);
    fillV(g, nose, { { 0.0f, ivory.brighter(0.04f) }, { 0.36f, ivory.darker(0.12f) }, { 0.76f, ivory.darker(0.36f) },
                     { 1.0f, ivory.darker(0.62f) } });
    fillH(g, nose, { { 0.0f, white(0.34f) }, { 0.20f, white(0.06f) }, { 0.70f, black(0.0f) }, { 1.0f, black(0.30f) } });
    g.setColour(black(0.20f));
    g.fillRect(x1 - 1.1f * u, edgeY + 1.0f * u, 1.1f * u, noseH); // shaded right corner of the nose
    fillBlob(g, x0 + w * 0.32f, edgeY + noseH * 0.22f, w * 0.44f, noseH * 0.30f,
             white(0.60f), white(0.22f), nose);
    g.setColour(ivory.darker(0.9f).withAlpha(0.55f));
    g.fillRect(x0, edgeY, w, 1.0f * u); // crease between lip and nose
    g.setColour(white(0.42f));
    g.fillRect(x0, edgeY + 1.0f * u, 0.9f * u, noseH - 1.0f * u); // lit left corner of the nose
    fillV(g, Rectangle<float>(x0, yb - 9.0f * u, w, 9.0f * u + 1.0f),
          { { 0.0f, black(0.0f) }, { 1.0f, black(0.50f + 0.25f * d) } }); // contact shadow into the felt
    if (d > 0.0f)
    {
        g.setColour(black(0.10f * d));
        g.fillRect(nose);
        // the lip shades as the key dips toward the rail
        fillV(g, Rectangle<float>(x0, topEnd - 3.0f * u, w, bevelH + 3.0f * u),
              { { 0.0f, black(0.0f) }, { 1.0f, black(0.16f * d) } });
        if (gd > 0.01f)
            fillV(g, nose, { { 0.0f, amb.withAlpha(0.0f) }, { 0.40f, amb.withAlpha(0.28f * gd) }, { 1.0f, amb.withAlpha(0.85f * gd) } });
    }
    if (k.hover && d < 0.05f)
    {
        g.setColour(Colour(0xfff3d691).withAlpha(0.18f));
        g.fillRect(x0, y0, w, yb - y0);
    }
    g.restoreState();
}

// Cast shadow of a black key, plus the glow it throws on the white keys when pressed.
// Light comes from the upper left, so the shadow falls to the right and in front of the key.
inline void drawBlackShadow(Graphics& g, Rectangle<float> r, const KeyLook& k, float u)
{
    const float x = r.getX(), y = r.getY(), w = r.getWidth(), h = r.getHeight();
    const float ease = 1.0f - 0.55f * k.depth; // a pressed key sits closer to the white key
    const float dx = w * 0.50f * ease, dy = h * 0.075f * ease;
    const float tip = y + h + k.sink * 0.65f;
    juce::Path p;
    p.startNewSubPath(x + w * 0.5f, y);
    p.lineTo(x + w + dx, y);
    p.lineTo(x + w + dx, tip + dy);
    p.lineTo(x + dx * 0.75f, tip + dy);
    p.lineTo(x, tip);
    p.lineTo(x + w * 0.5f, tip);
    p.closeSubPath();
    const Colour shade(0xff1c120a);
    constexpr int passes = 8;
    for (int i = 0; i < passes; ++i)
    {
        const float t = (float) i / (float) (passes - 1);
        g.setColour(shade.withAlpha(0.060f * (1.0f - 0.35f * t)));
        g.strokePath(p, juce::PathStrokeType((0.5f + 9.5f * t) * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour(shade.withAlpha(0.13f));
    g.fillPath(p);
    // Tight contact shadow hugging the right wall and the tip.
    fillH(g, Rectangle<float>(x + w - 1.0f, y, 3.6f * u, tip - y + dy * 0.4f),
          { { 0.0f, black(0.40f) }, { 1.0f, black(0.0f) } });
    fillV(g, Rectangle<float>(x, tip - 0.5f, w + 1.0f, 3.4f * u),
          { { 0.0f, black(0.40f) }, { 1.0f, black(0.0f) } });

    const float gd = k.glow * k.depth;
    const Colour amb = glowColour(k);
    if (gd > 0.01f)
        fillBlob(g, x + w * 0.5f, tip + 1.0f * u, w * 1.05f, 22.0f * u, amb.withAlpha(0.62f * gd), amb.withAlpha(0.26f * gd),
                 Rectangle<float>(x - w * 0.55f, tip - 22.0f * u, w * 2.1f, 46.0f * u));
}

// Glossy ebony key: a cap, two sloped side faces and a sloped front face. `r` is the JUCE key slot.
inline void drawBlackKey(Graphics& g, Rectangle<float> r, const KeyLook& k, float u)
{
    const float x = r.getX(), w = r.getWidth();
    const float ys = r.getY();
    const float lift = k.sink * 0.65f;
    const float tipY = r.getBottom() + lift;
    const float capFront = r.getY() + r.getHeight() * 0.765f + lift * 0.85f;
    const float ins = w * 0.20f;
    const float capL = x + ins, capR = x + w - ins;
    const float gd = k.glow * k.depth;
    const Colour amb = glowColour(k);

    // Sloped side faces: left is lit, right is in shade.
    juce::Path left, right, front;
    left.addQuadrilateral(x, ys, capL, ys, capL, capFront, x, tipY);
    right.addQuadrilateral(capR, ys, x + w, ys, x + w, tipY, capR, capFront);
    front.addQuadrilateral(capL, capFront, capR, capFront, x + w, tipY, x, tipY);

    g.setGradientFill(makeGrad(x, 0.0f, capL, 0.0f, { { 0.0f, Colour(0xff62626f) }, { 1.0f, Colour(0xff30303a) } }));
    g.fillPath(left);
    g.setGradientFill(makeGrad(capR, 0.0f, x + w, 0.0f, { { 0.0f, Colour(0xff14141a) }, { 1.0f, Colour(0xff060608) } }));
    g.fillPath(right);

    // Sloped front face: the lit upper rim falls off to near black at the tip.
    g.setGradientFill(makeGrad(0.0f, capFront, 0.0f, tipY,
                               { { 0.0f, Colour(0xff55555f) }, { 0.30f, Colour(0xff2a2a31) }, { 1.0f, Colour(0xff08080a) } }));
    g.fillPath(front);
    g.saveState();
    g.reduceClipRegion(front);
    fillH(g, Rectangle<float>(x, capFront, w, tipY - capFront + 1.0f),
          { { 0.0f, white(0.20f) }, { 0.30f, white(0.0f) }, { 0.7f, black(0.0f) }, { 1.0f, black(0.32f) } });
    if (gd > 0.01f)
        fillV(g, Rectangle<float>(x, capFront, w, tipY - capFront + 1.0f),
              { { 0.0f, amb.withAlpha(0.0f) }, { 0.45f, amb.withAlpha(0.26f * gd) }, { 1.0f, amb.withAlpha(0.90f * gd) } });
    g.restoreState();

    // Cap: glossy moulded top. Dark at the back, rising to the front rim.
    const auto cap = Rectangle<float>(capL, ys, capR - capL, capFront - ys);
    fillV(g, cap, { { 0.0f, Colour(0xff0a0a0d) }, { 0.45f, Colour(0xff18181d) }, { 1.0f, Colour(0xff2f2f37) } });
    fillH(g, cap, { { 0.0f, white(0.13f) }, { 0.38f, white(0.0f) }, { 0.65f, black(0.0f) }, { 1.0f, black(0.28f) } });
    // Panel reflection band and the long specular streak of moulded plastic.
    const float capH = capFront - r.getY();
    fillV(g, Rectangle<float>(capL, r.getY() + capH * 0.22f, cap.getWidth(), capH * 0.26f),
          { { 0.0f, white(0.0f) }, { 0.5f, white(0.10f) }, { 1.0f, white(0.0f) } });
    {
        const float sx = capL + cap.getWidth() * 0.26f;
        const float sw = juce::jmax(2.0f, 2.6f * u);
        const float sy0 = r.getY() + capH * 0.12f, sy1 = capFront - capH * 0.08f;
        g.setGradientFill(makeGrad(0.0f, sy0, 0.0f, sy1,
                                   { { 0.0f, Colour(0x00dfe4ff) }, { 0.25f, Colour(0x90dfe4ff) },
                                     { 0.70f, Colour(0x48dfe4ff) }, { 1.0f, Colour(0x00dfe4ff) } }));
        g.fillRoundedRectangle(sx, sy0, sw, sy1 - sy0, sw * 0.5f);
        g.setGradientFill(makeGrad(0.0f, sy0, 0.0f, sy1,
                                   { { 0.0f, Colour(0x00b9c2ff) }, { 0.35f, Colour(0x22b9c2ff) }, { 1.0f, Colour(0x00b9c2ff) } }));
        g.fillRoundedRectangle(sx - 2.2f * u, sy0 + 3.0f * u, sw + 6.4f * u, sy1 - sy0 - 6.0f * u, 2.5f * u);
    }
    if (k.depth > 0.0f)
    {
        g.setColour(black(0.18f * k.depth));
        g.fillRect(cap);
        if (gd > 0.01f)
            fillV(g, Rectangle<float>(capL, capFront - 26.0f * u, cap.getWidth(), 26.0f * u),
                  { { 0.0f, amb.withAlpha(0.0f) }, { 1.0f, amb.withAlpha(0.20f * gd) } });
    }

    // Edge lines on the slanted corners of the front face.
    g.setColour(white(0.22f));
    g.drawLine(capL, capFront, x, tipY, 0.9f * u);
    g.setColour(black(0.65f));
    g.drawLine(capR, capFront, x + w, tipY, 0.9f * u);
    // Rim lights and creases: the bevel between cap and front face, and the cap's side chamfers.
    g.setColour(Colour(0xffb4b4c2).withAlpha(0.85f - 0.30f * k.depth));
    g.fillRect(capL, capFront - 0.5f * u, cap.getWidth(), 1.1f * u);
    g.setColour(black(0.80f));
    g.fillRect(capL, capFront + 0.6f * u, cap.getWidth(), 1.0f * u);
    g.setColour(white(0.34f));
    g.fillRect(capL, ys, 0.9f * u, capFront - ys);
    g.setColour(black(0.75f));
    g.fillRect(capR - 0.9f * u, ys, 0.9f * u, capFront - ys);
    // Silhouette: dark outline around the whole key and a glossy rim along its tip.
    g.setColour(black(0.82f));
    g.fillRect(x - 0.5f * u, ys, 1.0f * u, tipY - ys);
    g.fillRect(x + w - 0.5f * u, ys, 1.0f * u, tipY - ys);
    g.fillRect(x - 0.5f * u, tipY - 0.5f * u, w + 1.0f * u, 1.2f * u);
    g.setColour(Colour(0xffa4a4b2).withAlpha(0.40f * (1.0f - 0.5f * k.depth)));
    g.fillRect(x + 1.2f * u, tipY - 1.9f * u, w - 2.4f * u, 0.9f * u);
    if (k.hover && k.depth < 0.05f)
    {
        g.setColour(white(0.10f));
        g.fillRect(x, ys, w, tipY - ys);
    }
}

// Warm light that a pressed key throws on the felt in front of it.
inline void drawFeltGlow(Graphics& g, float cx, float top, float keyW, float u, float strength, float glow, Rectangle<float> clip)
{
    if (strength < 0.01f) return;
    const Colour amb = glowColour(glow);
    const auto area = Rectangle<float>(cx - keyW * 1.3f, top - 2.0f, keyW * 2.6f, 34.0f * u);
    if (!area.intersects(clip)) return;
    fillBlob(g, cx, top + 2.0f * u, keyW * 1.25f, 26.0f * u, amb.withAlpha(0.85f * strength), amb.withAlpha(0.38f * strength), area);
    // Hot core right where the nose meets the felt.
    fillBlob(g, cx, top + 1.0f * u, keyW * 0.52f, 8.0f * u, Colour(0xffffdc9a).withAlpha(0.70f * strength),
             amb.withAlpha(0.30f * strength), area);
}

// Blit a cache that was rendered at pixel density `ps` back at exactly 1:1 device pixels, so its
// features line up with the vector drawing that is laid over it.
inline void drawCache(Graphics& g, const juce::Image& image, float ps, juce::Point<int> at = {})
{
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(1.0f / ps));
    g.setImageResamplingQuality(juce::Graphics::lowResamplingQuality);
    g.drawImageAt(image, juce::roundToInt((float) at.x * ps), juce::roundToInt((float) at.y * ps));
    g.restoreState();
}

// Walnut/rosewood end block with grain, varnish sheen and bevels. `innerOnRight`: the face toward the keys.
inline void paintWoodBlock(Graphics& g, Rectangle<float> r, bool innerOnRight, int seed, float u)
{
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    fillV(g, r, { { 0.0f, Colour(0xff6e3720) }, { 0.30f, Colour(0xff5d2d19) }, { 0.70f, Colour(0xff4b2213) }, { 1.0f, Colour(0xff301409) } });
    // Broad figure, then fine grain lines that wander slightly.
    juce::Random rng(seed);
    for (int i = 0; i < 7; ++i)
    {
        const float gx = r.getX() + rng.nextFloat() * r.getWidth();
        g.setColour(Colour(rng.nextBool() ? 0xff2a0e05 : 0xffa06034).withAlpha(0.05f + rng.nextFloat() * 0.05f));
        g.fillRect(gx, r.getY(), 1.5f * u + rng.nextFloat() * 3.5f * u, r.getHeight());
    }
    for (int i = 0; i < 26; ++i)
    {
        const float gx = r.getX() + rng.nextFloat() * r.getWidth();
        const float wob = (rng.nextFloat() - 0.5f) * 2.6f * u;
        juce::Path p;
        p.startNewSubPath(gx, r.getY());
        p.quadraticTo(gx + wob, r.getY() + r.getHeight() * (0.3f + rng.nextFloat() * 0.4f), gx + wob * 0.4f, r.getBottom());
        const bool dark = rng.nextFloat() < 0.72f;
        g.setColour((dark ? Colour(0xff2b0f05) : Colour(0xffb27442)).withAlpha(dark ? 0.12f + rng.nextFloat() * 0.22f
                                                                                    : 0.06f + rng.nextFloat() * 0.10f));
        g.strokePath(p, juce::PathStrokeType((0.35f + rng.nextFloat() * 0.9f) * u));
    }
    // Varnish: a glossy wash from the top and a narrow vertical streak.
    fillV(g, r, { { 0.0f, white(0.17f) }, { 0.20f, white(0.05f) }, { 0.50f, white(0.0f) }, { 1.0f, black(0.20f) } });
    fillH(g, Rectangle<float>(r.getX() + r.getWidth() * 0.22f, r.getY(), r.getWidth() * 0.22f, r.getHeight()),
          { { 0.0f, white(0.0f) }, { 0.5f, white(0.075f) }, { 1.0f, white(0.0f) } });
    // The face toward the keys is a chamfer: lit on the right-hand block, in shade on the left-hand one.
    const float cw = 4.0f * u;
    if (innerOnRight)
    {
        fillH(g, Rectangle<float>(r.getRight() - cw, r.getY(), cw, r.getHeight()),
              { { 0.0f, black(0.0f) }, { 1.0f, black(0.55f) } });
        g.setColour(black(0.85f));
        g.fillRect(r.getRight() - 1.0f * u, r.getY(), 1.0f * u, r.getHeight());
        g.setColour(white(0.18f));
        g.fillRect(r.getX() + 1.0f * u, r.getY(), 0.9f * u, r.getHeight());
        g.setColour(black(0.55f));
        g.fillRect(r.getX(), r.getY(), 1.0f * u, r.getHeight());
    }
    else
    {
        fillH(g, Rectangle<float>(r.getX(), r.getY(), cw, r.getHeight()),
              { { 0.0f, white(0.30f) }, { 0.6f, white(0.07f) }, { 1.0f, white(0.0f) } });
        g.setColour(black(0.85f));
        g.fillRect(r.getX(), r.getY(), 1.0f * u, r.getHeight());
        g.setColour(white(0.18f));
        g.fillRect(r.getRight() - 1.9f * u, r.getY(), 0.9f * u, r.getHeight());
        g.setColour(black(0.55f));
        g.fillRect(r.getRight() - 1.0f * u, r.getY(), 1.0f * u, r.getHeight());
    }
    g.setColour(white(0.22f));
    g.fillRect(r.getX(), r.getY() + 1.0f * u, r.getWidth(), 0.9f * u);
    g.restoreState();
}

// Fine felt: a dark base with scattered lighter and darker fibres.
inline void paintFelt(Graphics& g, Rectangle<float> r, Colour top, Colour bottom, int seed, float u)
{
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    fillV(g, r, { { 0.0f, top }, { 1.0f, bottom } });
    juce::Random rng(seed);
    const int specks = (int) (r.getWidth() * r.getHeight() * 0.55f / (u * u));
    for (int i = 0; i < specks; ++i)
    {
        const float px = r.getX() + rng.nextFloat() * r.getWidth();
        const float py = r.getY() + rng.nextFloat() * r.getHeight();
        const bool light = rng.nextFloat() < 0.5f;
        g.setColour((light ? Colour(0xffd8c8b8) : juce::Colours::black).withAlpha(light ? 0.035f + rng.nextFloat() * 0.04f
                                                                                        : 0.10f + rng.nextFloat() * 0.12f));
        g.fillRect(px, py, 0.9f * u, 0.9f * u);
    }
    g.restoreState();
}
} // namespace keys_detail

// Software-rendered key surfaces keep snapshots identical to the interactive
// editor. The key depth, moulded front lip and black-key bevel are drawn here.
class HardwareKeyboard final : public juce::MidiKeyboardComponent, public juce::SettableTooltipClient
{
public:
    HardwareKeyboard(Cs80AudioProcessor& p, Orientation orientation)
        : MidiKeyboardComponent(p.getKeyboardState(), orientation), processor(p),
          watcher(*this, p.getKeyboardState()), ticker(*this)
    {
        setOpaque(true);
        for (auto& v : noteVelocity) v.store(0.72f, std::memory_order_relaxed);
        for (auto& v : notePressure) v.store(0.0f, std::memory_order_relaxed);
    }

    ~HardwareKeyboard() override { ticker.stopTimer(); }

    // Pointer gesture standing in for the original per-key pressure sensor.
    std::function<bool(int)> hostKeyDown;

    // Pixels below the keys that a pressed key's glow reaches (set by the Keybed that owns the felt).
    int feltGlowReach = 0;

    // Animation state for the Keybed's felt glow: 0..1 depth and 0..1 glow strength of a key.
    float getKeyDepth(int note) const noexcept { return isIndex(note) ? depth[(size_t) note] : 0.0f; }
    float getKeyGlow(int note) const noexcept { return isIndex(note) ? 0.5f + 0.5f * strength[(size_t) note] : 0.0f; }

    void mouseDown(const juce::MouseEvent& e) override
    {
        token = processor.captureKeyboardGesture(getMidiChannel(), getNoteAndVelocityAtPosition(e.position).note, [&]
        { MidiKeyboardComponent::mouseDown(e); });
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const int note = getNoteAndVelocityAtPosition(e.position).note;
        const bool newStrike = note >= 0 && note != gesture.getNote();
        const auto accepted = processor.captureKeyboardGesture(getMidiChannel(), note, [&]
        { MidiKeyboardComponent::mouseDrag(e); });
        // A same-key drag retains its original token even after reset+ack.
        // Cross-key slides must bind the actual new onset or become invalid.
        // A genuine accepted onset can also follow JUCE's enter/release path
        // at the same pitch. Do not discard its proven new identity.
        if (accepted && !newStrike) gesture.begin(note, e.position.y);
        if (accepted || newStrike) token = accepted;
    }

    bool mouseDownOnKey(int note, const juce::MouseEvent& e) override
    {
        gesture.begin(note, e.position.y);
        return true;
    }
    bool mouseDraggedToKey(int note, const juce::MouseEvent& e) override
    {
        const float pressure = gesture.drag(note, e.position.y, (float) getHeight());
        if (pressure >= 0.0f)
        {
            processor.setKeyPressureFromUI(token, pressure);
            // Visual only: harder pressure sinks the key a little further and brightens its glow.
            if (note >= 0 && note < 128)
            {
                notePressure[(size_t) note].store(pressure, std::memory_order_relaxed);
                wake();
            }
        }
        return true;
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        MidiKeyboardComponent::mouseUp(e);
        gesture.end();
        token = {};
    }
    void mouseExit(const juce::MouseEvent& e) override
    {
        // JUCE mouseExit releases its held pointer note. Re-entry onto even
        // the same pitch is therefore a fresh strike, not the old gesture.
        MidiKeyboardComponent::mouseExit(e);
        gesture.end();
        token = {};
    }

    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;
        const float ps = juce::jmax(0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
        ensureRestImage(ps);

        // Bring depths up to date even if the timer has not been started yet (first paint, snapshots).
        if (!ticker.isTimerRunning())
        {
            const bool any = advanceDepths(juce::Time::getMillisecondCounterHiRes(), nullptr);
            if (any) wake();
        }

        keys_detail::drawCache(g, restImage, ps);

        int hover = -1;
        if (isMouseOver(false))
            hover = getNoteAndVelocityAtPosition(getMouseXYRelative().toFloat()).note;

        const auto clip = g.getClipBounds();
        for (int n = getRangeStart(); n <= getRangeEnd(); ++n)
        {
            if (depth[(size_t) n] <= 0.0f && n != hover) continue;
            const auto cluster = clusterRect(n).getIntersection(clip);
            if (cluster.isEmpty()) continue;
            g.saveState();
            g.reduceClipRegion(cluster);
            drawKeys(g, cluster.toFloat(), true, hover);
            g.restoreState();
        }
    }

    void resized() override
    {
        MidiKeyboardComponent::resized();
        restDirty = true;
    }

private:
    // Watches the UI keyboard state so a click wakes the animation timer at once and the
    // strike velocity can colour the glow. Callbacks arrive on the producing thread: atomics only.
    class StateWatcher final : private juce::MidiKeyboardState::Listener, private juce::AsyncUpdater
    {
    public:
        StateWatcher(HardwareKeyboard& o, juce::MidiKeyboardState& s) : owner(o), state(s) { state.addListener(this); }
        ~StateWatcher() override { state.removeListener(this); cancelPendingUpdate(); }
    private:
        void handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity) override
        {
            if (note >= 0 && note < 128)
            {
                owner.noteVelocity[(size_t) note].store(juce::jlimit(0.0f, 1.0f, velocity), std::memory_order_relaxed);
                owner.notePressure[(size_t) note].store(0.0f, std::memory_order_relaxed);
            }
            triggerAsyncUpdate();
        }
        void handleNoteOff(juce::MidiKeyboardState*, int, int, float) override { triggerAsyncUpdate(); }
        void handleAsyncUpdate() override { owner.wake(); }
        HardwareKeyboard& owner;
        juce::MidiKeyboardState& state;
    };

    // 60 Hz animation, alive only while some key is not at rest.
    class Ticker final : public juce::Timer
    {
    public:
        explicit Ticker(HardwareKeyboard& o) : owner(o) {}
        void timerCallback() override { owner.tick(); }
    private:
        HardwareKeyboard& owner;
    };

    static bool isIndex(int note) noexcept { return note >= 0 && note < 128; }

    float unit() const noexcept { return juce::jmax(0.3f, getKeyWidth() / keys_detail::kDesignKeyWidth); }

    bool isPressed(int note) const
    {
        return processor.getKeyboardState().isNoteOnForChannels(getMidiChannelsToDisplay(), note)
            || (hostKeyDown && hostKeyDown(note));
    }

    void wake()
    {
        if (!ticker.isTimerRunning())
        {
            lastAdvanceMs = juce::Time::getMillisecondCounterHiRes();
            ticker.startTimerHz(60);
        }
    }

    // Smooth every key toward its pressed state. Returns true while any key has depth.
    bool advanceDepths(double nowMs, std::vector<int>* moved)
    {
        const bool first = lastAdvanceMs <= 0.0;
        const float dt = first ? 10.0f : (float) juce::jlimit(0.0, 0.5, (nowMs - lastAdvanceMs) * 0.001);
        lastAdvanceMs = nowMs;
        bool any = false;
        for (int n = juce::jmax(0, getRangeStart()); n <= juce::jmin(127, getRangeEnd()); ++n)
        {
            const bool down = isPressed(n);
            if (down)
            {
                // Strike velocity (on-screen keys) and pointer pressure modulate depth and glow;
                // host-played keys carry no per-key data here and use a uniform strength.
                const bool fromUi = processor.getKeyboardState().isNoteOnForChannels(getMidiChannelsToDisplay(), n);
                const float v = fromUi ? noteVelocity[(size_t) n].load(std::memory_order_relaxed) : 0.72f;
                const float p = fromUi ? notePressure[(size_t) n].load(std::memory_order_relaxed) : 0.0f;
                const float s = juce::jlimit(0.0f, 1.0f, 0.6f * v + 0.4f * p);
                if (std::abs(s - strength[(size_t) n]) > 0.002f) { strength[(size_t) n] = s; if (moved) moved->push_back(n); }
            }
            const float target = down ? 1.0f : 0.0f;
            float d = depth[(size_t) n];
            if (d != target)
            {
                const float tau = target > d ? 0.012f : 0.090f;
                d += (target - d) * (1.0f - std::exp(-dt / tau));
                if (std::abs(target - d) < 0.006f) d = target;
                depth[(size_t) n] = d;
                if (moved) moved->push_back(n);
            }
            if (d > 0.0f) any = true;
        }
        return any;
    }

    void tick()
    {
        std::vector<int>& moved = movedScratch;
        moved.clear();
        const bool any = advanceDepths(juce::Time::getMillisecondCounterHiRes(), &moved);
        int last = -1;
        for (int n : moved)
            if (n != last) { repaintKeyVisual(n); last = n; }
        if (!any) ticker.stopTimer();
    }

    // The area one key's look touches, in this component's coordinates. White keys: their own column.
    // Black keys: the key plus the shadow it casts on the white keys beside and below it.
    juce::Rectangle<int> clusterRect(int note) const
    {
        const auto r = getRectangleForKey(note);
        if (!juce::MidiMessage::isMidiNoteBlack(note))
            return r.getSmallestIntegerContainer();
        const float u = unit();
        return r.withLeft(r.getX() - 0.6f * r.getWidth()).withTop(-2.0f)
                .withRight(r.getRight() + 0.6f * r.getWidth())
                .withBottom(r.getBottom() + 28.0f * u)
                .getSmallestIntegerContainer().getIntersection(getLocalBounds());
    }

    void repaintKeyVisual(int note)
    {
        auto keysArea = clusterRect(note);
        juce::Rectangle<int> felt;
        if (!juce::MidiMessage::isMidiNoteBlack(note) && feltGlowReach > 0)
        {
            const int span = juce::roundToInt(getKeyWidth() * 1.1f);
            felt = { keysArea.getX() - span, getHeight(), keysArea.getWidth() + 2 * span, feltGlowReach };
        }
        if (auto* parent = getParentComponent())
        {
            parent->repaint(keysArea + getPosition());
            if (!felt.isEmpty()) parent->repaint(felt + getPosition());
        }
        else
            repaint(keysArea);
    }

    keys_detail::KeyLook look(int note, bool live, int hover) const
    {
        keys_detail::KeyLook k;
        if (!live) return k;
        k.depth = depth[(size_t) note];
        const float s = strength[(size_t) note];
        k.sink = k.depth * unit() * (3.2f + 1.8f * s);
        k.glow = 0.5f + 0.5f * s;
        k.hover = (note == hover);
        return k;
    }

    // Draw every key that intersects `clip`. `live` = true uses the animated state (and hover); false
    // renders the whole keyboard at rest for the cache. Fills opaque, so it replaces whatever was there.
    void drawKeys(juce::Graphics& g, juce::Rectangle<float> clip, bool live, int hover)
    {
        const float u = unit();
        g.setColour(keys_detail::kSeam);
        g.fillRect(clip);
        const int lo = juce::jmax(0, getRangeStart()), hi = juce::jmin(127, getRangeEnd());
        for (int n = lo; n <= hi; ++n)
            if (!juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = getRectangleForKey(n);
                if (r.intersects(clip)) keys_detail::drawWhiteKey(g, n, r, look(n, live, hover), u);
            }
        for (int n = lo; n <= hi; ++n)
            if (juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = getRectangleForKey(n);
                if (clusterRect(n).toFloat().intersects(clip)) keys_detail::drawBlackShadow(g, r, look(n, live, hover), u);
            }
        for (int n = lo; n <= hi; ++n)
            if (juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = getRectangleForKey(n);
                if (clusterRect(n).toFloat().intersects(clip)) keys_detail::drawBlackKey(g, r, look(n, live, hover), u);
            }
    }

    void ensureRestImage(float ps)
    {
        const int w = getWidth(), h = getHeight();
        const float kw = getKeyWidth();
        if (!restDirty && restImage.isValid() && w == restW && h == restH && std::abs(ps - restScale) < 0.01f
            && std::abs(kw - restKeyWidth) < 0.001f && getRangeStart() == restLo && getRangeEnd() == restHi)
            return;
        const int pw = juce::jmax(1, (int) std::ceil((float) w * ps));
        const int ph = juce::jmax(1, (int) std::ceil((float) h * ps));
        restImage = juce::Image(juce::Image::RGB, pw, ph, false, juce::SoftwareImageType());
        {
            juce::Graphics ig(restImage);
            ig.addTransform(juce::AffineTransform::scale(ps));
            drawKeys(ig, getLocalBounds().toFloat(), false, -1);
        }
        restDirty = false;
        restW = w; restH = h; restScale = ps; restKeyWidth = kw; restLo = getRangeStart(); restHi = getRangeEnd();
    }

    Cs80AudioProcessor& processor;
    Cs80AudioProcessor::KeyPressureToken token {};
    cs80::ui::KeyPressureGesture gesture;

    StateWatcher watcher;
    Ticker ticker;
    std::array<float, 128> depth {};
    std::array<float, 128> strength {};
    std::array<std::atomic<float>, 128> noteVelocity {};
    std::array<std::atomic<float>, 128> notePressure {};
    std::vector<int> movedScratch;
    double lastAdvanceMs = 0.0;

    juce::Image restImage;
    bool restDirty = true;
    int restW = 0, restH = 0, restLo = -1, restHi = -1;
    float restScale = 0.0f, restKeyWidth = 0.0f;
};

// The keybed assembly (frame, key slip, end blocks) around the keys. The editor
// places this one component; `keys` is the playable MIDI keyboard inside it.
class Keybed final : public juce::Component
{
public:
    explicit Keybed(Cs80AudioProcessor& p) : keys(p, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setOpaque(true);
        keys.setBlackNoteWidthProportion(0.60f);
        keys.setBlackNoteLengthProportion(0.62f);
        addAndMakeVisible(keys);
    }

    // Insets of the key area inside the assembly, in design pixels at the 1512 x 228 design size
    // (top: back felt rail under the panel lip, sides: wood end blocks, bottom: front felt slip and
    // rail). They scale with the component width, so any size of Keybed keeps its proportions.
    // Design size 1512 x 228: key area 1476 x 192, exactly 41 px per white key.
    juce::BorderSize<int> keyInsets { 8, 18, 28, 18 };

    void resized() override
    {
        const float u = unit();
        const juce::BorderSize<int> scaled(juce::roundToInt((float) keyInsets.getTop() * u),
                                           juce::roundToInt((float) keyInsets.getLeft() * u),
                                           juce::roundToInt((float) keyInsets.getBottom() * u),
                                           juce::roundToInt((float) keyInsets.getRight() * u));
        keys.setBounds(scaled.subtractedFrom(getLocalBounds()));
        if (keys.getWidth() > 0)
            keys.setKeyWidth((float) keys.getWidth() / 36.0f);
        keys.feltGlowReach = juce::roundToInt(26.0f * u);
        frameDirty = true;
    }

    void paint(juce::Graphics& g) override
    {
        ensureImages(physicalScale(g));
        keys_detail::drawCache(g, frameImage, physicalScale(g));
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        ensureImages(physicalScale(g));
        const auto kb = keys.getBounds();
        keys_detail::drawCache(g, overlayImage, physicalScale(g), kb.getPosition());

        // Warm light of the pressed keys on the felt in front of them.
        const float u = unit();
        const auto clip = g.getClipBounds().toFloat();
        const float kw = keys.getKeyWidth();
        for (int n = keys.getRangeStart(); n <= keys.getRangeEnd(); ++n)
        {
            if (juce::MidiMessage::isMidiNoteBlack(n)) continue;
            const float d = keys.getKeyDepth(n);
            if (d < 0.02f) continue;
            const auto r = keys.getRectangleForKey(n);
            keys_detail::drawFeltGlow(g, (float) kb.getX() + r.getCentreX(), (float) kb.getBottom(), kw, u,
                                      d * keys.getKeyGlow(n), keys.getKeyGlow(n), clip);
        }
    }

    HardwareKeyboard keys;

private:
    float unit() const noexcept { return juce::jmax(0.3f, (float) getWidth() / keys_detail::kDesignWidth); }

    static float physicalScale(const juce::Graphics& g)
    {
        return juce::jmax(0.5f, g.getInternalContext().getPhysicalPixelScaleFactor());
    }

    void ensureImages(float ps)
    {
        const int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        if (!frameDirty && frameImage.isValid() && w == frameW && h == frameH && std::abs(ps - frameScale) < 0.01f
            && keys.getBounds() == frameKeys)
            return;
        const float u = unit();
        const int pw = juce::jmax(1, (int) std::ceil((float) w * ps));
        const int ph = juce::jmax(1, (int) std::ceil((float) h * ps));
        frameImage = juce::Image(juce::Image::RGB, pw, ph, false, juce::SoftwareImageType());
        {
            juce::Graphics g(frameImage);
            g.addTransform(juce::AffineTransform::scale(ps));
            paintFrame(g, u);
        }
        const auto kb = keys.getBounds();
        const int opw = juce::jmax(1, (int) std::ceil((float) kb.getWidth() * ps));
        const int oph = juce::jmax(1, (int) std::ceil((float) kb.getHeight() * ps));
        overlayImage = juce::Image(juce::Image::ARGB, opw, oph, true, juce::SoftwareImageType());
        {
            juce::Graphics g(overlayImage);
            g.addTransform(juce::AffineTransform::scale(ps));
            paintOverlay(g, kb.toFloat().withZeroOrigin(), u);
        }
        frameDirty = false;
        frameW = w; frameH = h; frameScale = ps; frameKeys = kb;
    }

    // Everything static behind the keys: end blocks, felt rails, the gold etched line and its labels.
    void paintFrame(juce::Graphics& g, float u)
    {
        using namespace keys_detail;
        const float W = (float) getWidth(), H = (float) getHeight();
        const auto kb = keys.getBounds().toFloat();
        g.fillAll(Colour(0xff0a0807));

        // Back rail: red-black felt in the deep shade of the panel lip.
        paintFelt(g, Rectangle<float>(kb.getX(), 0.0f, kb.getWidth(), kb.getY()), Colour(0xff150908), Colour(0xff32140f), 11, u);
        g.setColour(black(0.85f));
        g.fillRect(kb.getX(), kb.getY() - 1.0f * u, kb.getWidth(), 1.0f * u);

        // Front: the dark gap under the key noses, the felt key slip, then the satin rail.
        const float slipTop = kb.getBottom();
        const float feltTop = slipTop + 2.0f * u;
        const float railTop = juce::jmax(feltTop + 4.0f * u, H - 9.0f * u);
        g.setColour(Colour(0xff040302));
        g.fillRect(kb.getX(), slipTop, kb.getWidth(), 2.0f * u);
        paintFelt(g, Rectangle<float>(kb.getX(), feltTop, kb.getWidth(), railTop - feltTop),
                  Colour(0xff0f0d0c), Colour(0xff25201c), 23, u);
        fillV(g, Rectangle<float>(kb.getX(), railTop, kb.getWidth(), H - railTop),
              { { 0.0f, Colour(0xff3a342e) }, { 0.15f, Colour(0xff25211d) }, { 1.0f, Colour(0xff0d0c0b) } });
        g.setColour(white(0.30f));
        g.fillRect(kb.getX(), railTop, kb.getWidth(), 1.0f * u);
        g.setColour(black(0.8f));
        g.fillRect(kb.getX(), railTop - 1.0f * u, kb.getWidth(), 1.0f * u);
        // Shadow of the key noses falling on the felt.
        fillV(g, Rectangle<float>(kb.getX(), feltTop, kb.getWidth(), 6.0f * u),
              { { 0.0f, black(0.55f) }, { 1.0f, black(0.0f) } });

        // Gold etched line with a bright centre, small end diamonds and tick marks at each C.
        const float ly = feltTop + 4.6f * u;
        const float lx0 = kb.getX() + 12.0f * u, lx1 = kb.getRight() - 12.0f * u;
        g.setColour(black(0.75f));
        g.fillRect(lx0, ly + 0.9f * u, lx1 - lx0, 1.1f * u);
        fillH(g, Rectangle<float>(lx0, ly - 0.1f * u, lx1 - lx0, 1.1f * u),
              { { 0.0f, kGold.withAlpha(0.45f) }, { 0.5f, kGold.withAlpha(0.95f) }, { 1.0f, kGold.withAlpha(0.45f) } });
        for (float dx : { lx0 - 5.0f * u, lx1 + 5.0f * u })
        {
            juce::Path d;
            d.addTriangle(dx - 3.0f * u, ly + 0.45f * u, dx, ly - 2.6f * u, dx + 3.0f * u, ly + 0.45f * u);
            d.addTriangle(dx - 3.0f * u, ly + 0.45f * u, dx, ly + 3.5f * u, dx + 3.0f * u, ly + 0.45f * u);
            g.setColour(kGold.withAlpha(0.85f));
            g.fillPath(d);
        }
        const auto labelFont = juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), 8.4f * u, juce::Font::bold))
                                   .withExtraKerningFactor(0.07f);
        g.setFont(labelFont);
        for (int n = keys.getRangeStart(); n <= keys.getRangeEnd(); ++n)
        {
            if (n % 12 != 0) continue;
            const auto r = keys.getRectangleForKey(n).translated((float) keys.getX(), 0.0f);
            const float cx = r.getCentreX();
            g.setColour(kGold.withAlpha(0.85f));
            g.fillRect(cx - 0.5f * u, ly - 2.4f * u, 1.0f * u, 5.4f * u);
            const auto name = juce::MidiMessage::getMidiNoteName(n, true, true, keys.getOctaveForMiddleC());
            const auto box = Rectangle<float>(cx - 18.0f * u, ly + 3.2f * u, 36.0f * u, 10.0f * u);
            g.setColour(white(0.14f));
            g.drawText(name, box.translated(0.7f * u, 0.7f * u), juce::Justification::centred, false);
            g.setColour(black(0.85f));
            g.drawText(name, box.translated(-0.6f * u, -0.6f * u), juce::Justification::centred, false);
            g.setColour(Colour(0xffc4ad66).withAlpha(0.92f));
            g.drawText(name, box, juce::Justification::centred, false);
        }

        // Wood end blocks run the full height and meet the chassis cheeks at x = 0 and x = width.
        paintWoodBlock(g, Rectangle<float>(0.0f, 0.0f, kb.getX(), H), true, 301, u);
        paintWoodBlock(g, Rectangle<float>(kb.getRight(), 0.0f, W - kb.getRight(), H), false, 302, u);
    }

    // Light and shadow that fall across the keys: the panel lip above them and the end blocks beside them.
    // Drawn over the keys; size equals the key area.
    void paintOverlay(juce::Graphics& g, juce::Rectangle<float> area, float u)
    {
        using namespace keys_detail;
        fillV(g, Rectangle<float>(0.0f, 0.0f, area.getWidth(), 78.0f * u),
              { { 0.0f, black(0.96f) }, { 0.06f, black(0.84f) }, { 0.16f, black(0.60f) }, { 0.34f, black(0.30f) },
                { 0.62f, black(0.10f) }, { 1.0f, black(0.0f) } });
        // A faint reflection of the lit panel edge along the top of the keys.
        g.setColour(white(0.07f));
        g.fillRect(0.0f, 4.0f * u, area.getWidth(), 0.9f * u);
        fillH(g, Rectangle<float>(0.0f, 0.0f, 16.0f * u, area.getHeight()), { { 0.0f, black(0.46f) }, { 0.45f, black(0.16f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(area.getWidth() - 7.0f * u, 0.0f, 7.0f * u, area.getHeight()),
              { { 0.0f, black(0.0f) }, { 1.0f, black(0.22f) } });
        g.setColour(black(0.55f));
        g.fillRect(0.0f, 0.0f, 1.0f * u, area.getHeight());
    }

    juce::Image frameImage, overlayImage;
    bool frameDirty = true;
    int frameW = 0, frameH = 0;
    float frameScale = 0.0f;
    juce::Rectangle<int> frameKeys;
};

} // namespace cs80::ui
