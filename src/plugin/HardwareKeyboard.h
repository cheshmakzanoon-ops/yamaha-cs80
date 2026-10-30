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
//    timer runs while some depth is non-zero and repaints only dirty keys. While the keyboard
//    is visible and the editor has wired `hostKeyDown`, the same timer idles at 30 Hz to catch
//    host-played notes, which raise no event on the UI side.
//  - Keybed paints the felt strip, the ruler with its gold line and labels (middle C marked),
//    the base rail and the stepped walnut end blocks once into a cached image, and lays the
//    deck-overhang shadow, the blocks' cap fronts and the ruler glow of pressed keys over the keys.
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

// Warm ivory (about 0xffe6dcc4) with a small deterministic tint per key (hashed from the note number).
inline Colour ivoryFor(int note)
{
    const float a = hash01(note, 1), b = hash01(note, 2), c = hash01(note, 3);
    return Colour::fromHSV(0.118f + (a - 0.5f) * 0.012f, 0.135f + b * 0.026f, 0.882f + c * 0.040f, 1.0f);
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
// seam on each side. Layers, back to front: top face with ivory grain, light reflection, edge falloff,
// rounded lip, vertical nose with a per-key highlight and contact shadow. Pressed, the key is still
// ivory: its front edge drops, the whole body darkens a few percent warm, a dark gap and ambient occlusion
// open at the back under the felt, the neighbours shade its edges, and only the nose glows amber.
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
    const float d = k.depth;
    const float gd = juce::jmin(1.0f, k.glow * d * 1.25f);
    const Colour ivory = ivoryFor(note);
    const Colour amb = glowColour(k);
    const float hLip = hash01(note, 11), hAo = hash01(note, 12), hBand = hash01(note, 13);

    juce::Path body;
    body.addRoundedRectangle(x0, y0, w, yb - y0, 2.2f * u, 2.2f * u, false, false, true, true);
    g.saveState();
    g.reduceClipRegion(body);

    // Top face: soft and light; the deck's own shadow is laid over the keys by the Keybed.
    const auto top = Rectangle<float>(x0, y0, w, topEnd - y0);
    fillV(g, top, { { 0.0f, ivory.darker(0.10f) }, { 0.16f, ivory.darker(0.02f) }, { 0.50f, ivory.brighter(0.035f) },
                    { 0.82f, ivory }, { 1.0f, ivory.darker(0.09f) } });

    // Ivory micro-grain: faint streaks along the key and a few specks, different on every key.
    {
        juce::Random rng(note * 7919 + 11);
        const float topH = topEnd - y0;
        const int streaks = 11 + (int) (hash01(note, 14) * 9.0f);
        for (int i = 0; i < streaks; ++i)
        {
            const float px = x0 + 2.0f * u + rng.nextFloat() * (w - 4.0f * u);
            const float py = y0 + rng.nextFloat() * topH * 0.9f;
            const float len = (8.0f + rng.nextFloat() * 26.0f) * u;
            const bool lit = rng.nextFloat() < 0.4f;
            g.setColour((lit ? juce::Colours::white : Colour(0xff7a6844)).withAlpha(lit ? 0.07f + rng.nextFloat() * 0.05f
                                                                                       : 0.030f + rng.nextFloat() * 0.040f));
            g.fillRect(px, py, 0.7f * u, len);
        }
        const int specks = 6 + (int) (hash01(note, 15) * 8.0f);
        for (int i = 0; i < specks; ++i)
        {
            g.setColour(Colour(0xff6e5a38).withAlpha(0.05f + rng.nextFloat() * 0.07f));
            g.fillRect(x0 + rng.nextFloat() * w, y0 + rng.nextFloat() * topH, 0.8f * u, 0.8f * u);
        }
    }

    // Reflection of the lit deck above: a soft light band, a little higher or lower on each key.
    const float bandA = y0 + h * (0.17f + 0.07f * hBand), bandB = bandA + h * 0.26f;
    fillV(g, Rectangle<float>(x0, bandA, w, bandB - bandA),
          { { 0.0f, white(0.0f) }, { 0.45f, white(0.13f + 0.07f * hLip) }, { 1.0f, white(0.0f) } });

    // Rounded shoulders: both long edges roll away from the light; the left one keeps a lit 1 px edge.
    const float ao = 1.0f + 0.7f * d;
    fillH(g, Rectangle<float>(x0, y0, 4.0f * u, topEnd - y0),
          { { 0.0f, black(0.16f * ao) }, { 0.4f, black(0.06f * ao) }, { 1.0f, black(0.0f) } });
    fillH(g, Rectangle<float>(x1 - 6.5f * u, y0, 6.5f * u, topEnd - y0),
          { { 0.0f, black(0.0f) }, { 0.55f, black(0.09f * ao) }, { 1.0f, black(0.30f * ao) } });

    // Rounded front lip: bright upper edge rolling into the nose.
    const Colour lipHi = ivory.interpolatedWith(juce::Colours::white, 0.52f + 0.30f * hLip);
    fillV(g, Rectangle<float>(x0, topEnd, w, bevelH),
          { { 0.0f, lipHi }, { 0.30f, ivory.brighter(0.08f) }, { 1.0f, ivory.darker(0.16f) } });
    g.setColour(juce::Colours::white.withAlpha(0.80f + 0.15f * hLip));
    g.fillRect(x0 + 0.4f * u, topEnd, w - 0.8f * u, 1.1f * u);

    // Nose: the vertical front, bright where it faces the light, shadowed toward the felt.
    const float noseH = yb - edgeY;
    const auto nose = Rectangle<float>(x0, edgeY, w, noseH + 1.0f);
    fillV(g, nose, { { 0.0f, ivory.brighter(0.04f) }, { 0.36f, ivory.darker(0.12f) }, { 0.76f, ivory.darker(0.34f) },
                     { 1.0f, ivory.darker(0.58f) } });
    fillH(g, nose, { { 0.0f, white(0.30f) }, { 0.20f, white(0.05f) }, { 0.70f, black(0.0f) }, { 1.0f, black(0.28f) } });
    g.setColour(black(0.14f + 0.08f * hAo));
    g.fillRect(x1 - 1.1f * u, edgeY + 1.0f * u, 1.1f * u, noseH); // shaded right corner of the nose
    {
        // The specular highlight differs on every key: its shape, position, size and strength all vary.
        const int style = juce::jmin(2, (int) (hash01(note, 7) * 3.0f));
        const float bx = 0.20f + 0.26f * hash01(note, 5), by = 0.12f + 0.22f * hash01(note, 6);
        const float bs = 0.28f + 0.22f * hash01(note, 8), bi = 0.28f + 0.30f * hash01(note, 9);
        if (style == 0)
            fillBlob(g, x0 + w * bx, edgeY + noseH * by, w * bs, noseH * bs * 0.75f, white(bi), white(bi * 0.34f), nose);
        else if (style == 1)
            fillBlob(g, x0 + w * (0.30f + 0.2f * bx), edgeY + noseH * (0.10f + 0.5f * by), w * 0.62f, noseH * 0.20f,
                     white(bi * 0.9f), white(bi * 0.30f), nose);
        else
        {
            fillBlob(g, x0 + w * bx, edgeY + noseH * by, w * bs * 0.8f, noseH * bs * 0.6f, white(bi), white(bi * 0.34f), nose);
            fillBlob(g, x0 + w * (bx + 0.30f), edgeY + noseH * (by + 0.10f), w * 0.16f, noseH * 0.12f,
                     white(bi * 0.7f), white(bi * 0.2f), nose);
        }
    }
    g.setColour(ivory.darker(0.9f).withAlpha(0.55f));
    g.fillRect(x0, edgeY, w, 1.0f * u); // crease between lip and nose
    // Contact shadow into the felt: a slightly different depth on every key.
    fillV(g, Rectangle<float>(x0, yb - 9.0f * u, w, 9.0f * u + 1.0f),
          { { 0.0f, black(0.0f) }, { 1.0f, black(0.40f + 0.18f * hAo - 0.14f * d) } });

    if (d > 0.0f)
    {
        // The body of a depressed key is a few percent darker and warmer: it sits lower in the deck's shadow.
        g.setColour(Colour(0xff4a2e12).withAlpha(0.10f * d));
        g.fillRect(x0, y0, w, yb - y0);
        // At the back, under the felt: a dark gap, then ambient occlusion falling away along the key.
        g.setColour(black(0.90f * d));
        g.fillRect(x0, y0, w, 1.3f * u);
        fillV(g, Rectangle<float>(x0, y0 + 1.3f * u, w, 32.0f * u),
              { { 0.0f, black(0.62f * d) }, { 0.22f, black(0.30f * d) }, { 1.0f, black(0.0f) } });
        // Its lower neighbours shade the edges (light comes from the left, so the left edge takes more).
        fillH(g, Rectangle<float>(x0, y0, 5.0f * u, yb - y0), { { 0.0f, black(0.34f * d) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(x1 - 3.0f * u, y0, 3.0f * u, yb - y0), { { 0.0f, black(0.0f) }, { 1.0f, black(0.16f * d) } });
        if (gd > 0.01f)
        {
            // Warm bounce on the front of the top face, then the glow proper: the nose, a bright rim and a hot line
            // where it meets the ruler.
            fillV(g, Rectangle<float>(x0, topEnd - 28.0f * u, w, 28.0f * u),
                  { { 0.0f, amb.withAlpha(0.0f) }, { 1.0f, amb.withAlpha(0.20f * gd) } });
            g.setColour(Colour(0xffffe2a6).withAlpha(0.85f * d));
            g.fillRect(x0 + 0.4f * u, topEnd, w - 0.8f * u, 1.5f * u);
            g.setColour(amb.withAlpha(0.45f * gd));
            g.fillRect(x0, topEnd - 1.4f * u, w, 1.4f * u);
            fillV(g, nose, { { 0.0f, amb.withAlpha(0.14f * gd) }, { 0.50f, amb.withAlpha(0.46f * gd) }, { 1.0f, amb.withAlpha(0.94f * gd) } });
            g.setColour(Colour(0xffffe6b4).withAlpha(0.80f * gd));
            g.fillRect(x0 + 0.6f * u, yb - 1.8f * u, w - 1.2f * u, 1.4f * u);
        }
    }

    // Lit left edge of the whole key.
    g.setColour(white(0.50f - 0.20f * d));
    g.fillRect(x0, y0, 1.0f * u, yb - y0);

    if (k.hover && d < 0.05f)
    {
        g.setColour(Colour(0xfff3d691).withAlpha(0.22f));
        g.fillRect(x0, y0, w, yb - y0);
    }
    g.restoreState();
}

// Cast shadow of a black key: a tapered wedge on the white key to its right (light comes from the
// upper left), widest at the key's tip and running out toward the back, in four stacked alpha steps
// that fall off away from the key. `rightWhiteLeft` is the left edge of that white key; nothing is
// cast on the key to the left. Also the warm glow a pressed key throws on the white keys.
inline void drawBlackShadow(Graphics& g, Rectangle<float> r, const KeyLook& k, float u, float rightWhiteLeft)
{
    const float x = r.getX(), w = r.getWidth(), h = r.getHeight();
    const float ease = 1.0f - 0.55f * k.depth; // a pressed key sits closer to the white key
    const float dx = 12.0f * u * ease, dy = 14.0f * u * ease;
    const float tip = r.getY() + h + k.sink * 0.9f;
    const float taper = h * 0.80f;
    const Colour shade(0xff1a1008);

    g.saveState();
    juce::Path clip;
    clip.addRectangle(rightWhiteLeft + 1.0f * u, r.getY(), 4.0f * w + 60.0f * u, h + 60.0f * u);
    g.reduceClipRegion(clip);
    for (const auto& [s, alpha] : { std::pair { 1.00f, 0.060f }, std::pair { 0.76f, 0.075f },
                                    std::pair { 0.52f, 0.095f }, std::pair { 0.30f, 0.125f } })
    {
        juce::Path wedge;
        wedge.startNewSubPath(x + w, tip - taper * s);
        wedge.lineTo(x + w + dx * s, tip + dy * s * 0.12f);
        wedge.lineTo(x + w + dx * s * 0.82f, tip + dy * s);
        wedge.lineTo(x + w * 0.5f + dx * s * 0.25f, tip + dy * s);
        wedge.lineTo(x + w * 0.5f, tip);
        wedge.lineTo(x + w, tip);
        wedge.closeSubPath();
        g.setColour(shade.withAlpha(alpha * (1.0f - 0.35f * k.depth)));
        g.fillPath(wedge.createPathWithRoundedCorners(2.2f * u * s));
    }
    g.restoreState();

    const float gd = juce::jmin(1.0f, k.glow * k.depth * 1.25f);
    if (gd > 0.01f)
    {
        const Colour amb = glowColour(k);
        fillBlob(g, x + w * 0.5f, tip + 1.0f * u, w * 1.0f, 18.0f * u, amb.withAlpha(0.60f * gd), amb.withAlpha(0.24f * gd),
                 Rectangle<float>(x - w * 0.55f, tip - 18.0f * u, w * 2.1f, 38.0f * u));
    }
}

// Glossy ebony key with a domed cap and a faceted front slope: a 1 px lit facet along the top and left of
// the slope, a narrow dark facet at its lower right, a thin specular streak offset to the left of the cap.
// Pressed, it dips, loses its gloss, darkens and carries a warm rim at its tip. Every key differs a little.
inline void drawBlackKey(Graphics& g, int note, Rectangle<float> r, const KeyLook& k, float u)
{
    const float x = r.getX(), w = r.getWidth();
    const float ys = r.getY();
    const float d = k.depth;
    const float lift = k.sink * 0.9f;
    const float tipY = r.getBottom() + lift;
    const float capFront = r.getY() + r.getHeight() * 0.775f + lift * 0.7f;
    const float ins = w * 0.13f;
    const float capL = x + ins, capR = x + w - ins;
    const float gd = juce::jmin(1.0f, k.glow * d * 1.25f);
    const Colour amb = glowColour(k);
    const float hA = 0.85f + 0.30f * hash01(note, 21); // facet and streak strength
    const float hB = hash01(note, 22);                 // streak position
    const float hC = 0.85f + 0.30f * hash01(note, 23); // dome brightness

    juce::Path left, right, front;
    left.addQuadrilateral(x, ys, capL, ys, capL, capFront, x, tipY);
    right.addQuadrilateral(capR, ys, x + w, ys, x + w, tipY, capR, capFront);
    front.addQuadrilateral(capL, capFront - 0.9f * u, capR, capFront - 0.9f * u, x + w, tipY, x, tipY); // tucks under the cap

    // Narrow sloped sides: the left catches the light, the right is nearly black.
    g.setGradientFill(makeGrad(x, 0.0f, capL, 0.0f, { { 0.0f, Colour(0xff565662) }, { 1.0f, Colour(0xff2c2c35) } }));
    g.fillPath(left);
    g.setGradientFill(makeGrad(capR, 0.0f, x + w, 0.0f, { { 0.0f, Colour(0xff111115) }, { 1.0f, Colour(0xff050506) } }));
    g.fillPath(right);

    // Front slope: bright at the top edge, falling off quickly to near black at the tip.
    const float fh = tipY - capFront;
    g.setGradientFill(makeGrad(0.0f, capFront, 0.0f, tipY,
                               { { 0.0f, Colour(0xff64646f) }, { 0.14f, Colour(0xff3c3c46) }, { 0.50f, Colour(0xff1b1b21) },
                                 { 1.0f, Colour(0xff09090b) } }));
    g.fillPath(front);
    g.saveState();
    g.reduceClipRegion(front);
    fillH(g, Rectangle<float>(x, capFront, w, fh + 1.0f),
          { { 0.0f, white(0.12f * hA) }, { 0.35f, white(0.0f) }, { 0.72f, black(0.0f) }, { 1.0f, black(0.36f) } });
    {
        // Left facet: a narrow lit strip along the slanted left edge.
        juce::Path lf;
        lf.addQuadrilateral(capL, capFront, capL + 1.7f * u, capFront, x + 1.5f * u, tipY, x, tipY);
        g.setGradientFill(makeGrad(0.0f, capFront, 0.0f, tipY,
                                   { { 0.0f, Colour(0xffd4d4e0).withAlpha(0.72f * hA) }, { 1.0f, Colour(0xffa0a0b0).withAlpha(0.10f) } }));
        g.fillPath(lf);
        // Right facet: narrow and dark, at the lower right.
        juce::Path rf;
        rf.addQuadrilateral(capR - 3.0f * u, capFront, capR, capFront, x + w, tipY, x + w - 4.4f * u, tipY);
        g.setGradientFill(makeGrad(0.0f, capFront, 0.0f, tipY, { { 0.0f, black(0.22f) }, { 1.0f, black(0.72f) } }));
        g.fillPath(rf);
        // Top facet: a thin light edge with a short chamfer under it.
        fillV(g, Rectangle<float>(capL, capFront, capR - capL, 3.2f * u),
              { { 0.0f, white(0.30f * hA) }, { 0.35f, white(0.10f * hA) }, { 1.0f, white(0.0f) } });
        g.setColour(Colour(0xffb4b4c2).withAlpha(0.55f * hA));
        g.fillRect(capL, capFront - 0.2f * u, capR - capL, 1.0f * u);
    }
    if (gd > 0.01f)
        fillV(g, Rectangle<float>(x, capFront, w, fh + 1.0f),
              { { 0.0f, amb.withAlpha(0.0f) }, { 0.45f, amb.withAlpha(0.26f * gd) }, { 1.0f, amb.withAlpha(0.90f * gd) } });
    g.restoreState();

    // Cap: a dome, lit from the upper left, falling off toward the right edge.
    const auto cap = Rectangle<float>(capL, ys, capR - capL, capFront - ys);
    fillV(g, cap, { { 0.0f, Colour(0xff0e0e12) }, { 0.5f, Colour(0xff1b1b21) }, { 1.0f, Colour(0xff2d2d35) } });
    fillH(g, cap, { { 0.0f, black(0.10f) }, { 0.20f, white(0.13f * hC) }, { 0.45f, white(0.04f) }, { 0.78f, black(0.14f) },
                    { 1.0f, black(0.40f) } });
    g.saveState();
    g.reduceClipRegion(cap.getSmallestIntegerContainer());
    const float capW = cap.getWidth(), capH = cap.getHeight();
    const float gloss = 1.0f - 0.70f * d;
    // Soft gloss at the upper left, and a thin neutral specular streak offset to the left of the cap.
    fillBlob(g, capL + capW * 0.26f, ys + capH * 0.26f, capW * 0.70f, capH * 0.24f,
             white(0.12f * gloss * hC), white(0.04f * gloss), cap);
    {
        const float sx = capL + capW * (0.16f + 0.12f * hB);
        const float sy0 = ys + 5.0f * u, sy1 = capFront - 7.0f * u;
        const float sa = (0.80f + 0.40f * hash01(note, 24)) * gloss;
        const Colour sc(0xfff0eef2);
        for (const auto& [wd, al] : { std::pair { 3.8f, 0.065f }, std::pair { 2.2f, 0.12f }, std::pair { 1.0f, 0.30f } })
            fillV(g, Rectangle<float>(sx - wd * u * 0.5f, sy0, wd * u, sy1 - sy0),
                  { { 0.0f, sc.withAlpha(0.0f) }, { 0.28f, sc.withAlpha(al * sa) }, { 0.62f, sc.withAlpha(al * sa * 0.55f) },
                    { 1.0f, sc.withAlpha(0.0f) } });
    }
    g.restoreState();
    if (d > 0.0f)
    {
        g.setColour(black(0.30f * d));
        g.fillRect(cap);
        fillV(g, Rectangle<float>(capL, ys, cap.getWidth(), 36.0f * u),
              { { 0.0f, black(0.46f * d) }, { 1.0f, black(0.0f) } }); // hinge shadow
        if (gd > 0.01f)
            fillV(g, Rectangle<float>(capL, capFront - 26.0f * u, cap.getWidth(), 26.0f * u),
                  { { 0.0f, amb.withAlpha(0.0f) }, { 1.0f, amb.withAlpha(0.20f * gd) } });
    }

    // Top-left rim light, a glossy rim along the tip, and the dark edges.
    fillV(g, Rectangle<float>(x, ys, 1.1f * u, tipY - ys), { { 0.0f, white(0.55f * hA) }, { 0.6f, white(0.20f) }, { 1.0f, white(0.05f) } });
    g.setColour(black(0.70f));
    g.fillRect(capR, ys, 0.9f * u, capFront - ys);
    g.setColour(black(0.62f));
    g.fillRect(x + w - 0.5f * u, ys, 1.0f * u, tipY - ys);
    g.fillRect(x - 0.5f * u, tipY - 0.5f * u, w + 1.0f * u, 1.2f * u);
    g.setColour(Colour(0xffa4a4b2).withAlpha(0.30f * (1.0f - 0.6f * d)));
    g.fillRect(x + 1.2f * u, tipY - 1.9f * u, w - 2.4f * u, 0.9f * u);
    if (gd > 0.01f)
    {
        g.setColour(Colour(0xffffd48a).withAlpha(0.85f * gd));
        g.fillRect(x + 0.8f * u, tipY - 1.6f * u, w - 1.6f * u, 1.4f * u); // warm rim at the tip
    }
    if (k.hover && d < 0.05f)
    {
        g.setColour(white(0.10f));
        g.fillRect(x, ys, w, tipY - ys);
    }
}

// Warm light that a pressed key throws just in front of it: a bright line in the gap under the nose and a
// faint, shallow halo (about 7 px, 20 % at most) on the ruler, which never washes out the printed legend.
inline void drawFeltGlow(Graphics& g, float cx, float top, float keyW, float u, float strength, float glow, Rectangle<float> clip)
{
    if (strength < 0.01f) return;
    const Colour amb = glowColour(glow);
    const float st = juce::jmin(1.0f, 1.2f * strength);
    const auto area = Rectangle<float>(cx - keyW * 0.95f, top - 1.0f, keyW * 1.9f, 11.0f * u);
    if (!area.intersects(clip)) return;
    const auto gapArea = Rectangle<float>(cx - keyW * 0.55f, top, keyW * 1.1f, 2.0f * u);
    fillBlob(g, cx, top + 1.0f * u, keyW * 0.60f, 3.0f * u, Colour(0xffffd9a0).withAlpha(0.80f * st), amb.withAlpha(0.45f * st), gapArea);
    fillBlob(g, cx, top + 2.0f * u, keyW * 0.85f, 7.0f * u, amb.withAlpha(0.20f * st), amb.withAlpha(0.10f * st),
             area.withTop(top + 2.0f * u));
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

// Felt: a dyed base with fine irregular noise (no regular pattern, so it never hatches at 1:1), a few lighter
// and darker fibres and a faint nap running along the strip.
inline void paintFelt(Graphics& g, Rectangle<float> r, Colour top, Colour bottom, int seed, float u)
{
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    fillV(g, r, { { 0.0f, top }, { 1.0f, bottom } });
    juce::Random rng(seed);
    const int specks = (int) (r.getWidth() * r.getHeight() * 0.85f / (u * u));
    for (int i = 0; i < specks; ++i)
    {
        const float px = r.getX() + rng.nextFloat() * r.getWidth(), py = r.getY() + rng.nextFloat() * r.getHeight();
        const float sz = (0.8f + rng.nextFloat() * 0.8f) * u;
        const bool light = rng.nextFloat() < 0.46f;
        g.setColour((light ? Colour(0xffe6d2c2) : juce::Colours::black).withAlpha(light ? 0.035f + rng.nextFloat() * 0.05f
                                                                                         : 0.09f + rng.nextFloat() * 0.13f));
        g.fillRect(px, py, sz, sz);
    }
    const int fibres = (int) (r.getWidth() * r.getHeight() * 0.03f / (u * u));
    for (int i = 0; i < fibres; ++i)
    {
        const float px = r.getX() + rng.nextFloat() * r.getWidth(), py = r.getY() + rng.nextFloat() * r.getHeight();
        g.setColour(Colour(0xffe8d6c6).withAlpha(0.04f + rng.nextFloat() * 0.06f));
        g.fillRect(px, py, (1.5f + rng.nextFloat() * 4.0f) * u, 0.5f * u);
    }
    g.restoreState();
}

// The flat printed strip under the keys: satin black with a very fine grain, hairline gold borders and no texture to speak of.
inline void paintPrintedStrip(Graphics& g, Rectangle<float> r, int seed, float u)
{
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    fillV(g, r, { { 0.0f, Colour(0xff161310) }, { 1.0f, Colour(0xff1f1b17) } });
    juce::Random rng(seed);
    const int specks = (int) (r.getWidth() * r.getHeight() * 0.18f / (u * u));
    for (int i = 0; i < specks; ++i)
    {
        g.setColour((rng.nextBool() ? white(0.035f) : black(0.10f)).withMultipliedAlpha(0.6f + rng.nextFloat()));
        g.fillRect(r.getX() + rng.nextFloat() * r.getWidth(), r.getY() + rng.nextFloat() * r.getHeight(), 0.9f * u, 0.9f * u);
    }
    g.setColour(black(0.80f));
    g.fillRect(r.getX(), r.getY(), r.getWidth(), 1.0f * u);
    g.setColour(kGold.withAlpha(0.22f));
    g.fillRect(r.getX(), r.getBottom() - 1.8f * u, r.getWidth(), 0.7f * u);
    g.setColour(black(0.85f));
    g.fillRect(r.getX(), r.getBottom() - 1.0f * u, r.getWidth(), 1.0f * u);
    g.restoreState();
}

// Walnut face that continues the chassis cheek: the shared figured-grain helper pulled down to the cheek's deep brown.
inline void paintWalnutFace(Graphics& g, Rectangle<float> r, bool vertical, int seed, float veil)
{
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    g.setColour(Colour(0xff3a2014));
    g.fillRect(r);
    drawWalnutGrain(g, r, vertical, seed, false);
    g.setColour(Colour(0xff1f0e07).withAlpha(veil));
    g.fillRect(r);
    g.restoreState();
}

// Warm varnish highlight for wood (a white one would turn the walnut grey).
inline Colour varnish(float a) { return Colour(0xffe8b080).withAlpha(a); }

// Height of an end block's stepped cap, in design pixels.
constexpr float kLipHeight = 14.0f;
// How far the cap overhangs the first and last key, in design pixels.
constexpr float kLipOverhang = 5.0f;

// End block beside the keys: a walnut slab with vertical grain that continues the cheek, topped by a
// stepped cap in cross grain that casts a shadow on the slab below it. The part of the cap that
// overhangs the keys is drawn by paintLipOverhang, over them. `r` runs the full height of the keybed.
inline void paintEndBlock(Graphics& g, Rectangle<float> r, bool left, int seed, float u)
{
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    const float lipH = kLipHeight * u;
    const float inner = left ? r.getRight() : r.getX();
    const float outer = left ? r.getX() : r.getRight();

    // Slab.
    paintWalnutFace(g, r, true, seed, 0.50f);
    fillV(g, r, { { 0.0f, varnish(0.0f) }, { 0.14f, varnish(0.06f) }, { 0.30f, varnish(0.10f) }, { 0.55f, varnish(0.0f) } }); // varnish sheen
    fillV(g, r, { { 0.0f, black(0.0f) }, { 0.04f, black(0.0f) }, { 1.0f, black(0.56f) } }); // sinks into shade like the cheek
    fillH(g, r, { { left ? 0.0f : 1.0f, white(0.0f) }, { left ? 0.45f : 0.55f, white(0.05f) }, { left ? 1.0f : 0.0f, white(0.0f) } });
    // Chamfer on the face toward the keys, in shade on the left block and lit on the right one.
    const float cw = 3.4f * u;
    const auto cham = Rectangle<float>(left ? inner - cw : inner, r.getY(), cw, r.getHeight());
    if (left)
    {
        fillH(g, cham, { { 0.0f, black(0.0f) }, { 1.0f, black(0.55f) } });
        g.setColour(black(0.88f));
        g.fillRect(inner - 1.0f * u, r.getY(), 1.0f * u, r.getHeight());
    }
    else
    {
        fillH(g, cham, { { 0.0f, varnish(0.30f) }, { 0.6f, varnish(0.08f) }, { 1.0f, black(0.0f) } });
        g.setColour(black(0.88f));
        g.fillRect(inner, r.getY(), 1.0f * u, r.getHeight());
    }
    // Dark joint where the block meets the cheek, so the cheek's grain does not look sliced off.
    {
        const auto joint = Rectangle<float>(left ? outer : outer - 3.6f * u, r.getY(), 3.6f * u, r.getHeight());
        if (left) fillH(g, joint, { { 0.0f, black(0.80f) }, { 0.3f, black(0.55f) }, { 1.0f, black(0.0f) } });
        else      fillH(g, joint, { { 0.0f, black(0.0f) }, { 0.7f, black(0.55f) }, { 1.0f, black(0.80f) } });
    }

    // Cap: cross grain, lit on top, rounded at its inner corner.
    const auto lip = Rectangle<float>(r.getX(), r.getY(), r.getWidth(), lipH);
    // Cast shadow of the cap on the slab below it.
    fillV(g, Rectangle<float>(r.getX(), lip.getBottom(), r.getWidth(), 8.0f * u),
          { { 0.0f, black(0.62f) }, { 0.35f, black(0.30f) }, { 1.0f, black(0.0f) } });
    juce::Path capShape;
    capShape.addRoundedRectangle(lip.getX(), lip.getY(), lip.getWidth(), lip.getHeight(), 3.0f * u, 3.0f * u,
                                 !left, left, false, false);
    g.saveState();
    g.reduceClipRegion(capShape);
    paintWalnutFace(g, lip, false, seed + 7, 0.34f);
    fillV(g, lip, { { 0.0f, varnish(0.36f) }, { 0.22f, varnish(0.12f) }, { 0.60f, varnish(0.0f) }, { 1.0f, black(0.30f) } });
    g.setColour(varnish(0.55f));
    g.fillRect(lip.getX(), lip.getY() + 0.8f * u, lip.getWidth(), 0.9f * u); // lit top edge
    {
        const auto edge = Rectangle<float>(left ? inner - 2.2f * u : inner, lip.getY(), 2.2f * u, lip.getHeight());
        if (left) fillH(g, edge, { { 0.0f, black(0.0f) }, { 1.0f, black(0.45f) } });
        else      fillH(g, edge, { { 0.0f, varnish(0.22f) }, { 1.0f, black(0.0f) } });
    }
    g.restoreState();
    g.setColour(black(0.90f));
    g.fillRect(r.getX(), lip.getBottom() - 0.2f * u, r.getWidth(), 1.1f * u); // underside of the step
    g.setColour(varnish(0.16f));
    g.fillRect(r.getX(), lip.getBottom() - 1.3f * u, r.getWidth(), 0.8f * u); // rolled lower edge
    g.restoreState();
}

// The overhanging front of the cap, laid over the first or last key: a face of end-grain walnut from the
// shared grain helper with a lit top bevel and a soft shadow on the key tops (only the left block casts
// one: light comes from the left). `edgeX` is the block's inner edge and `y0` the top of the cap, both in
// the coordinates of `g`.
inline void paintLipOverhang(Graphics& g, float edgeX, float y0, bool left, int seed, float u)
{
    const float ov = kLipOverhang * u, lipH = kLipHeight * u, yb = y0 + lipH;
    const float x = left ? edgeX : edgeX - ov;
    const auto face = Rectangle<float>(x, y0, ov, lipH);
    if (left)
    {
        // Shadow of the cap on the key tops: a soft oval around its lower outer corner.
        fillBlob(g, edgeX + ov, y0 + lipH * 0.58f, 10.0f * u, lipH * 0.58f + 9.0f * u, black(0.62f), black(0.26f),
                 Rectangle<float>(edgeX, y0, ov + 12.0f * u, lipH + 12.0f * u));
    }
    else
        fillBlob(g, edgeX - ov, yb, 6.0f * u, 6.0f * u, black(0.40f), black(0.16f),
                 Rectangle<float>(edgeX - ov - 7.0f * u, yb - 4.0f * u, ov + 7.0f * u, 11.0f * u));
    paintWalnutFace(g, face, false, seed, left ? 0.40f : 0.24f);
    fillV(g, face, { { 0.0f, varnish(left ? 0.30f : 0.46f) }, { 0.35f, varnish(0.06f) }, { 1.0f, black(left ? 0.42f : 0.30f) } });
    g.setColour(varnish(left ? 0.40f : 0.62f));
    g.fillRect(face.getX(), y0 + 0.8f * u, face.getWidth(), 0.9f * u);
    g.setColour(black(0.90f));
    g.fillRect(face.getX(), yb - 0.2f * u, face.getWidth(), 1.1f * u);
    g.setColour(black(0.85f));
    g.fillRect(left ? face.getRight() - 0.8f * u : face.getX(), y0, 0.8f * u, lipH);
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

        // Bring depths up to date even if the timer has not been started yet (first paint, snapshots)
        // or is only idling between polls.
        const auto now = juce::Time::getMillisecondCounterHiRes();
        const bool starved = tickRateHz == kAnimHz && now - lastAdvanceMs > 100.0; // the timer is not getting its turns
        if (!ticker.isTimerRunning() || tickRateHz == kIdleHz || starved)
        {
            if (tickRateHz == kIdleHz) lastAdvanceMs = now - 500.0; // nothing was moving: a press found here lands at once
            const bool any = advanceDepths(now, nullptr);
            if (any) wake(); else ensurePoll();
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

    // One timer: 60 Hz while some key is not at rest; otherwise, only while the keyboard is on screen and the
    // editor has wired `hostKeyDown`, a light 30 Hz poll for host-played notes (they raise no event here).
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

    static constexpr int kAnimHz = 60, kIdleHz = 30;

    void wake()
    {
        if (!ticker.isTimerRunning())
            lastAdvanceMs = juce::Time::getMillisecondCounterHiRes();
        if (!ticker.isTimerRunning() || tickRateHz != kAnimHz)
        {
            ticker.startTimerHz(kAnimHz);
            tickRateHz = kAnimHz;
        }
    }

    void ensurePoll()
    {
        if (ticker.isTimerRunning() || !hostKeyDown) return;
        lastAdvanceMs = juce::Time::getMillisecondCounterHiRes();
        ticker.startTimerHz(kIdleHz);
        tickRateHz = kIdleHz;
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
        auto now = juce::Time::getMillisecondCounterHiRes();
        if (tickRateHz == kIdleHz) lastAdvanceMs = now - 8.0; // a press found by the poll starts its attack at once
        const bool any = advanceDepths(now, &moved);
        int last = -1;
        for (int n : moved)
            if (n != last) { repaintKeyVisual(n); last = n; }
        if (any)
        {
            if (tickRateHz != kAnimHz) { ticker.startTimerHz(kAnimHz); tickRateHz = kAnimHz; }
        }
        else if (hostKeyDown && isVisible())
        {
            if (tickRateHz != kIdleHz) { ticker.startTimerHz(kIdleHz); tickRateHz = kIdleHz; }
        }
        else
        {
            ticker.stopTimer();
            tickRateHz = 0;
        }
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
        k.sink = k.depth * unit() * (6.8f + 2.8f * s);
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
                if (clusterRect(n).toFloat().intersects(clip))
                {
                    const float rightWhite = n + 1 <= hi ? getRectangleForKey(n + 1).getX() : r.getRight();
                    keys_detail::drawBlackShadow(g, r, look(n, live, hover), u, rightWhite);
                }
            }
        for (int n = lo; n <= hi; ++n)
            if (juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = getRectangleForKey(n);
                if (clusterRect(n).toFloat().intersects(clip)) keys_detail::drawBlackKey(g, n, r, look(n, live, hover), u);
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
    int tickRateHz = 0;

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
    // Design size 1512 x 228: key area 1476 x 188, exactly 41 px per white key; 11 px of felt above the keys,
    // 29 px below them (2 px gap, 15 px ruler, 12 px base rail).
    juce::BorderSize<int> keyInsets { 11, 18, 29, 18 };

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
        keys.feltGlowReach = juce::roundToInt(20.0f * u);
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

    // Everything static behind the keys: back felt, the ruler with its gold line and labels, the base rail
    // and the end blocks. Layout from the top: 11 px felt, keys, 2 px gap, 15 px ruler, 12 px base rail.
    void paintFrame(juce::Graphics& g, float u)
    {
        using namespace keys_detail;
        const float W = (float) getWidth(), H = (float) getHeight();
        const auto kb = keys.getBounds().toFloat();
        g.fillAll(Colour(0xff0a0807));

        // Back rail: woven maroon felt lying in the shade of the deck overhang.
        const auto back = Rectangle<float>(kb.getX(), 0.0f, kb.getWidth(), kb.getY());
        paintFelt(g, back, Colour(0xff55201a), Colour(0xff733028), 11, u);
        fillV(g, back, { { 0.0f, black(0.78f) }, { 0.45f, black(0.40f) }, { 1.0f, black(0.08f) } });
        g.setColour(white(0.11f));
        g.fillRect(kb.getX(), kb.getY() - 2.2f * u, kb.getWidth(), 0.8f * u); // raised lip of the felt
        g.setColour(black(0.92f));
        g.fillRect(kb.getX(), kb.getY() - 1.0f * u, kb.getWidth(), 1.0f * u);

        // Front: the dark gap under the key noses, the flat printed ruler, then the walnut front rail.
        const float slipTop = kb.getBottom();
        const float rulerTop = slipTop + 2.0f * u;
        const float railTop = H - 12.0f * u;
        g.setColour(Colour(0xff040302));
        g.fillRect(kb.getX(), slipTop, kb.getWidth(), 2.0f * u);
        paintPrintedStrip(g, Rectangle<float>(kb.getX(), rulerTop, kb.getWidth(), railTop - rulerTop), 23, u);
        fillV(g, Rectangle<float>(kb.getX(), rulerTop, kb.getWidth(), 6.0f * u),
              { { 0.0f, black(0.55f) }, { 1.0f, black(0.0f) } }); // shadow of the key noses
        {
            const auto rail = Rectangle<float>(kb.getX(), railTop, kb.getWidth(), H - railTop);
            paintWalnutFace(g, rail, false, 411, 0.16f);
            fillH(g, rail, { { 0.0f, varnish(0.0f) }, { 0.22f, varnish(0.12f) }, { 0.50f, varnish(0.0f) }, { 0.78f, varnish(0.08f) },
                             { 1.0f, varnish(0.0f) } });
            // Rolled front edge: a lit chamfer along the top, turning into shade toward the bottom.
            fillV(g, Rectangle<float>(rail.getX(), railTop, rail.getWidth(), 4.0f * u),
                  { { 0.0f, varnish(0.50f) }, { 0.30f, varnish(0.22f) }, { 1.0f, varnish(0.0f) } });
            fillV(g, Rectangle<float>(rail.getX(), railTop + 5.0f * u, rail.getWidth(), rail.getHeight() - 5.0f * u + 1.0f),
                  { { 0.0f, black(0.0f) }, { 1.0f, black(0.52f) } });
            g.setColour(black(0.88f));
            g.fillRect(rail.getX(), railTop - 1.0f * u, rail.getWidth(), 1.0f * u);
            g.setColour(varnish(0.78f));
            g.fillRect(rail.getX(), railTop, rail.getWidth(), 0.8f * u);
        }

        // The end blocks shade the strips beside them.
        fillH(g, Rectangle<float>(kb.getX(), 0.0f, 7.0f * u, H), { { 0.0f, black(0.55f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(kb.getRight() - 5.0f * u, 0.0f, 5.0f * u, H), { { 0.0f, black(0.0f) }, { 1.0f, black(0.30f) } });

        // Gold etched line with a bright centre, small end diamonds, a tick at each C and a pip at middle C.
        const float ly = rulerTop + 3.2f * u;
        const float lx0 = kb.getX() + 12.0f * u, lx1 = kb.getRight() - 12.0f * u;
        g.setColour(black(0.75f));
        g.fillRect(lx0, ly + 0.9f * u, lx1 - lx0, 1.1f * u);
        fillH(g, Rectangle<float>(lx0, ly - 0.2f * u, lx1 - lx0, 1.3f * u),
              { { 0.0f, kGold.withAlpha(0.62f) }, { 0.5f, kGold.withAlpha(1.0f) }, { 1.0f, kGold.withAlpha(0.62f) } });
        for (float dx : { lx0 - 5.0f * u, lx1 + 5.0f * u })
        {
            juce::Path d;
            d.addTriangle(dx - 3.0f * u, ly + 0.45f * u, dx, ly - 2.6f * u, dx + 3.0f * u, ly + 0.45f * u);
            d.addTriangle(dx - 3.0f * u, ly + 0.45f * u, dx, ly + 3.5f * u, dx + 3.0f * u, ly + 0.45f * u);
            g.setColour(kGold.withAlpha(0.85f));
            g.fillPath(d);
        }
        // Labels never go below 8 px, even in a small editor.
        const float fontPx = juce::jmax(8.0f, 9.2f * u);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), fontPx, juce::Font::bold))
                      .withExtraKerningFactor(0.07f));
        for (int n = keys.getRangeStart(); n <= keys.getRangeEnd(); ++n)
        {
            if (n % 12 != 0) continue;
            const bool middleC = (n == 60);
            const auto r = keys.getRectangleForKey(n).translated((float) keys.getX(), 0.0f);
            const float cx = r.getCentreX();
            if (middleC)
            {
                const float py = ly + 0.3f * u;
                fillBlob(g, cx, py, 6.0f * u, 6.0f * u, kAmber.withAlpha(0.50f), kAmber.withAlpha(0.20f), Rectangle<float>(cx - 7.0f * u, py - 7.0f * u, 14.0f * u, 14.0f * u));
                g.setColour(black(0.85f));
                g.fillEllipse(cx - 3.2f * u, py - 3.0f * u, 6.4f * u, 6.4f * u);
                g.setColour(Colour(0xffffb84a));
                g.fillEllipse(cx - 2.5f * u, py - 2.5f * u, 5.0f * u, 5.0f * u);
                g.setColour(white(0.70f));
                g.fillEllipse(cx - 1.4f * u, py - 1.7f * u, 1.7f * u, 1.3f * u);
            }
            else
            {
                g.setColour(kGold.withAlpha(0.85f));
                g.fillRect(cx - 0.5f * u, ly - 2.2f * u, 1.0f * u, 4.6f * u);
            }
            const auto name = middleC ? juce::String("C4 MIDDLE C")
                                      : juce::MidiMessage::getMidiNoteName(n, true, true, keys.getOctaveForMiddleC());
            const auto box = Rectangle<float>(cx - 44.0f * u, ly + 2.4f * u, 88.0f * u, railTop - 0.6f * u - (ly + 2.4f * u));
            g.setColour(white(0.14f));
            g.drawText(name, box.translated(0.7f * u, 0.7f * u), juce::Justification::centred, false);
            g.setColour(black(0.85f));
            g.drawText(name, box.translated(-0.6f * u, -0.6f * u), juce::Justification::centred, false);
            g.setColour(middleC ? Colour(0xffefcf7c) : Colour(0xffc4ad66).withAlpha(0.92f));
            g.drawText(name, box, juce::Justification::centred, false);
        }

        // End blocks run the full height and meet the chassis cheeks at x = 0 and x = width.
        paintEndBlock(g, Rectangle<float>(0.0f, 0.0f, kb.getX(), H), true, 301, u);
        paintEndBlock(g, Rectangle<float>(kb.getRight(), 0.0f, W - kb.getRight(), H), false, 302, u);
        paintLipOverhang(g, kb.getX(), 0.0f, true, 308, u);
        paintLipOverhang(g, kb.getRight(), 0.0f, false, 309, u);
    }

    // Light and shadow that fall across the keys: the deck overhang above them and the end blocks beside
    // them, plus the front of each block's overhanging cap. Drawn over the keys; size equals the key area.
    void paintOverlay(juce::Graphics& g, juce::Rectangle<float> area, float u)
    {
        using namespace keys_detail;
        fillV(g, Rectangle<float>(0.0f, 0.0f, area.getWidth(), 26.0f * u),
              { { 0.0f, black(0.76f) }, { 0.28f, black(0.42f) }, { 0.62f, black(0.15f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(0.0f, 0.0f, 16.0f * u, area.getHeight()), { { 0.0f, black(0.38f) }, { 0.45f, black(0.13f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(area.getWidth() - 7.0f * u, 0.0f, 7.0f * u, area.getHeight()),
              { { 0.0f, black(0.0f) }, { 1.0f, black(0.22f) } });
        const float capTop = -(float) keys.getY(); // the cap starts above the key area
        paintLipOverhang(g, 0.0f, capTop, true, 308, u);
        paintLipOverhang(g, area.getWidth(), capTop, false, 309, u);
    }

    juce::Image frameImage, overlayImage;
    bool frameDirty = true;
    int frameW = 0, frameH = 0;
    float frameScale = 0.0f;
    juce::Rectangle<int> frameKeys;
};

} // namespace cs80::ui
