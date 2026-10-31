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
//  - Keybed paints the felt strip, the walnut front rail with its printed note names (middle C marked)
//    once into a cached image (the walnut cheeks beside it are the chassis's), and lays the felt's shadow and
//    the cheeks' shading over the keys; pressed keys dip 3 px over the rail and light it, drawn live over the keys.
//  - Key edges are snapped to whole device pixels, so the 1.2 px seams stay crisp at any scale.
// Everything is lit from the upper left.
namespace cs80::ui
{
namespace keys_detail
{
using juce::Colour;
using juce::Graphics;
using juce::Rectangle;
using Stop = std::pair<float, Colour>;

// Design-space size of the keybed assembly and its frame (1504 x 228 at a 1600 wide editor).
// The assembly sits flush between the walnut cheeks, so all 1504 px are keys: 36 white keys of 41.78 px
// spanning the same 48..1552 as the control rows above. Key edges are snapped to whole device pixels when
// drawn, so the seams stay crisp.
constexpr float kDesignWidth = 1504.0f;
constexpr float kDesignHeight = 228.0f;
constexpr float kDesignKeyWidth = 1504.0f / 36.0f;

// Width of the dark seam between two white keys: about 1.2 design px, but always a whole number of device pixels.
inline float seamWidth(float ps) { return juce::jmax(1.0f, std::round(1.2f * ps)) / ps; }

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
// seam on its right (`seam`, a whole number of device pixels). Layers, back to front: top face with ivory
// grain, light reflection, edge falloff, rounded lip, vertical nose with a per-key highlight and contact
// shadow. Pressed, the key is still ivory: it darkens a few percent and takes a light amber wash that grows
// toward the front, its lip and nose drop, a dark gap and a 4 px inner shadow open at the back under the felt,
// the neighbours shade its edges, and the nose glows amber. The Keybed lets the tip dip 3 px over the rail.
inline void drawWhiteKey(Graphics& g, int note, Rectangle<float> r, const KeyLook& k, float u, float seam)
{
    const float x0 = r.getX(), x1 = r.getRight() - seam;
    const float w = x1 - x0;
    const float y0 = r.getY(), yb = r.getBottom(), h = r.getHeight();
    // Nose and lip keep their absolute size on a taller key: only the long face grows.
    const float noseRest = juce::jmin(h * 0.150f, 30.0f * u);
    const float bevelH = juce::jmax(3.0f, juce::jmin(h * 0.034f, 6.8f * u));
    const float d = k.depth;
    const float edgeY = yb - noseRest + k.sink + 3.0f * u * d; // bottom of the lip, top of the nose
    const float topEnd = edgeY - bevelH;
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
        // A depressed key sits lower than its neighbours. The ivory darkens about 8 percent and a faint amber wash
        // (at most 20 percent, stronger toward the front) keeps it obvious at a glance; the sinking itself is
        // drawn with depth: the back edge drops 3 px under the felt leaving a dark gap, the neighbours' side walls
        // show as wedges that grow toward the front (the left one in shade, the right one lit), and the lip and nose
        // fall into shadow.
        const float gl = juce::jlimit(0.0f, 1.0f, (k.glow - 0.5f) * 2.0f);
        const Colour wash(0xffee7c12);
        const float drop = 3.0f * u * d;
        g.setColour(black(0.08f * d));
        g.fillRect(x0, y0, w, yb - y0);
        fillV(g, Rectangle<float>(x0, y0, w, yb - y0),
              { { 0.0f, wash.withAlpha(d * (0.06f + 0.02f * gl)) }, { 0.55f, wash.withAlpha(d * (0.12f + 0.02f * gl)) },
                { 1.0f, wash.withAlpha(d * (0.18f + 0.02f * gl)) } });
        // Back to front the face falls toward the felt: shaded under the deck, brighter once it clears it.
        fillV(g, Rectangle<float>(x0, y0, w, h * 0.62f), { { 0.0f, black(0.24f * d) }, { 1.0f, black(0.0f) } });
        // At the back, under the felt: the key's top edge drops 3 px, a 4 px inner shadow follows, then occlusion.
        fillV(g, Rectangle<float>(x0, y0 + drop, w, 4.5f * u), { { 0.0f, black(0.72f * d) }, { 1.0f, black(0.0f) } });
        fillV(g, Rectangle<float>(x0, y0 + drop, w, 30.0f * u),
              { { 0.0f, black(0.34f * d) }, { 0.22f, black(0.18f * d) }, { 1.0f, black(0.0f) } });
        // The neighbours' side walls: wedges from about 1 px at the back to 4 px at the front, light from the upper left.
        {
            const float wBack = 1.0f * u, wFront = 1.6f * u + 0.26f * k.sink;
            const float yTop = y0 + drop;
            juce::Path leftWall, rightWall;
            leftWall.addQuadrilateral(x0, yTop, x0 + wBack, yTop, x0 + wFront, yb, x0, yb);
            rightWall.addQuadrilateral(x1 - wBack, yTop, x1, yTop, x1, yb, x1 - wFront, yb);
            g.saveState();
            g.reduceClipRegion(leftWall);
            fillH(g, Rectangle<float>(x0, yTop, wFront, yb - yTop),
                  { { 0.0f, Colour(0xff0e0804).withAlpha(0.94f * d) }, { 1.0f, Colour(0xff4a3622).withAlpha(0.84f * d) } });
            g.restoreState();
            g.saveState();
            g.reduceClipRegion(rightWall);
            fillH(g, Rectangle<float>(x1 - wFront, yTop, wFront, yb - yTop),
                  { { 0.0f, ivory.darker(0.50f).withAlpha(0.86f * d) }, { 1.0f, ivory.darker(0.20f).withAlpha(0.90f * d) } });
            g.restoreState();
            // Thin dark gaps down both sides where the key sinks below its neighbours.
            g.setColour(black(0.90f * d));
            g.fillRect(x0, yTop, 0.9f * u, yb - yTop);
            g.fillRect(x1 - 0.9f * u, yTop, 0.9f * u, yb - yTop);
            // A little occlusion on the face beside each wall.
            fillH(g, Rectangle<float>(x0, yTop, 6.0f * u, yb - yTop), { { 0.0f, black(0.20f * d) }, { 1.0f, black(0.0f) } });
            fillH(g, Rectangle<float>(x1 - 4.0f * u, yTop, 4.0f * u, yb - yTop), { { 0.0f, black(0.0f) }, { 1.0f, black(0.16f * d) } });
        }
        // Front lip and nose fall into shadow: the neighbours' higher lips shade the lip, the nose is darker still.
        fillV(g, Rectangle<float>(x0, topEnd - 7.0f * u, w, 7.0f * u), { { 0.0f, black(0.0f) }, { 1.0f, black(0.16f * d) } });
        fillV(g, Rectangle<float>(x0, topEnd, w, yb - topEnd),
              { { 0.0f, black(0.14f * d) }, { 0.30f, black(0.28f * d) }, { 1.0f, black(0.30f * d) } });
        fillH(g, Rectangle<float>(x0, topEnd, 9.0f * u, yb - topEnd), { { 0.0f, black(0.30f * d) }, { 1.0f, black(0.0f) } });
        // The key's top edge itself: the dark gap that opens between its back end and the felt.
        g.setColour(Colour(0xff050302).withAlpha(0.97f * d));
        g.fillRect(x0, y0, w, drop + 0.7f * u * d);
        if (gd > 0.01f)
        {
            // A modest glow at the front: the nose, a warm rim along the lip and a bright line where it meets the rail.
            fillV(g, Rectangle<float>(x0, topEnd - 22.0f * u, w, 22.0f * u),
                  { { 0.0f, amb.withAlpha(0.0f) }, { 1.0f, amb.withAlpha(0.06f * gd) } });
            g.setColour(Colour(0xffffe2a6).withAlpha(0.42f * d));
            g.fillRect(x0 + 0.4f * u, topEnd, w - 0.8f * u, 1.4f * u);
            g.setColour(amb.withAlpha(0.20f * gd));
            g.fillRect(x0, topEnd - 1.2f * u, w, 1.2f * u);
            fillV(g, nose, { { 0.0f, amb.withAlpha(0.06f * gd) }, { 0.50f, amb.withAlpha(0.18f * gd) }, { 1.0f, amb.withAlpha(0.42f * gd) } });
        }
    }

    // Lit left edge of the whole key.
    g.setColour(white(0.50f - 0.44f * d));
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
        fillBlob(g, x + w * 0.5f, tip + 1.0f * u, w * 0.9f, 14.0f * u, amb.withAlpha(0.36f * gd), amb.withAlpha(0.14f * gd),
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
    const float capFront = r.getY() + r.getHeight() * 0.80f + lift * 0.7f;
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
              { { 0.0f, amb.withAlpha(0.06f * gd) }, { 0.40f, amb.withAlpha(0.34f * gd) }, { 1.0f, amb.withAlpha(0.92f * gd) } });
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
        g.setColour(Colour(0xffb07034).withAlpha(0.16f * d)); // the same warm tint as the white keys
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
// barely visible halo (5 px, 8 % at most) on the rail, so the printed legend is never washed out.
inline void drawFeltGlow(Graphics& g, float cx, float top, float keyW, float u, float strength, float glow, Rectangle<float> clip)
{
    if (strength < 0.01f) return;
    const Colour amb = glowColour(glow);
    const float st = juce::jmin(1.0f, 1.2f * strength);
    const auto area = Rectangle<float>(cx - keyW * 0.8f, top - 1.0f, keyW * 1.6f, 8.0f * u);
    if (!area.intersects(clip)) return;
    const auto gapArea = Rectangle<float>(cx - keyW * 0.5f, top, keyW * 1.0f, 2.0f * u);
    fillBlob(g, cx, top + 1.0f * u, keyW * 0.55f, 3.0f * u, Colour(0xffffd9a0).withAlpha(0.55f * st), amb.withAlpha(0.28f * st), gapArea);
    fillBlob(g, cx, top + 2.0f * u, keyW * 0.70f, 5.0f * u, amb.withAlpha(0.08f * st), amb.withAlpha(0.04f * st),
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

// Walnut face in the chassis cheeks' palette: a deep mid-brown (0x4a2412) laid with long grain that runs from the
// late-wood dark (0x3e1c0d) to the early-wood tone (0x6a3418), fine dark lines and short pores. `vertical` runs
// the grain along y (side walls); otherwise along x (the front rail).
inline void paintWalnutFace(Graphics& g, Rectangle<float> r, bool vertical, int seed)
{
    const Colour late(0xff3e1c0d), early(0xff6a3418);
    g.saveState();
    g.reduceClipRegion(r.getSmallestIntegerContainer());
    g.setColour(Colour(0xff4a2412));
    g.fillRect(r);
    juce::Random rng(seed);
    const float along = vertical ? r.getHeight() : r.getWidth();
    const float across = vertical ? r.getWidth() : r.getHeight();
    const float a0 = vertical ? r.getY() : r.getX(), c0 = vertical ? r.getX() : r.getY();
    const int count = (int) (along * across / 55.0f) + 14;
    auto stroke = [&](float start, float len, float pos, float amp, float phase, float freq, Colour colour, float width)
    {
        juce::Path line;
        for (float t = 0.0f; t <= len; t += 5.0f)
        {
            const float aa = a0 + start + t, cc = c0 + pos + std::sin(phase + t * freq) * amp;
            const float x = vertical ? cc : aa, y = vertical ? aa : cc;
            if (t == 0.0f) line.startNewSubPath(x, y); else line.lineTo(x, y);
        }
        g.setColour(colour);
        g.strokePath(line, juce::PathStrokeType(width));
    };
    for (int i = 0; i < count; ++i)       // broad grain lines between the late and early tones
    {
        const float tone = rng.nextFloat();
        stroke(rng.nextFloat() * (along + 80.0f) - 80.0f, 40.0f + rng.nextFloat() * 260.0f, rng.nextFloat() * across,
               0.3f + rng.nextFloat() * 0.9f, rng.nextFloat() * 6.28f, 0.01f + rng.nextFloat() * 0.02f,
               late.interpolatedWith(early, tone).withAlpha(0.40f + rng.nextFloat() * 0.40f), 0.8f + rng.nextFloat() * 1.8f);
    }
    for (int i = 0; i < count / 2; ++i)   // fine dark lines
        stroke(rng.nextFloat() * (along + 60.0f) - 60.0f, 30.0f + rng.nextFloat() * 200.0f, rng.nextFloat() * across,
               0.2f + rng.nextFloat() * 0.6f, rng.nextFloat() * 6.28f, 0.015f + rng.nextFloat() * 0.03f,
               Colour(0xff1c0c06).withAlpha(0.14f + rng.nextFloat() * 0.20f), 0.7f);
    for (int i = 0; i < count; ++i)       // pores: short dark dashes along the grain
    {
        const float pa = a0 + rng.nextFloat() * along, pc = c0 + rng.nextFloat() * across, len = 1.5f + rng.nextFloat() * 5.0f;
        g.setColour(Colour(0xff1c0c06).withAlpha(0.14f + rng.nextFloat() * 0.18f));
        if (vertical) g.fillRect(pc, pa, 0.7f, len); else g.fillRect(pa, pc, len, 0.7f);
    }
    g.restoreState();
}

// Warm varnish highlight for wood (a white one would turn the walnut grey).
inline Colour varnish(float a) { return Colour(0xffe8b080).withAlpha(a); }

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

    // A key's slot (what JUCE hit-tests) with its left and right edges moved onto whole device pixels at
    // pixel density `ps`, so neighbouring keys tile exactly and the seams between them stay crisp.
    juce::Rectangle<float> slotRect(int note, float ps) const
    {
        const auto r = getRectangleForKey(note);
        const float l = std::round(r.getX() * ps) / ps, rt = std::round(r.getRight() * ps) / ps;
        return { l, r.getY(), juce::jmax(1.0f / ps, rt - l), r.getHeight() };
    }

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

        const auto clip = g.getClipBounds().toFloat();
        for (int n = getRangeStart(); n <= getRangeEnd(); ++n)
        {
            if (depth[(size_t) n] <= 0.0f && n != hover) continue;
            // A white key redraws exactly its own slot (edges on device pixels), so its neighbours' pixels are never touched.
            auto cluster = (juce::MidiMessage::isMidiNoteBlack(n) ? clusterRect(n).toFloat() : slotRect(n, ps)).getIntersection(clip);
            if (cluster.isEmpty()) continue;
            {
                // Edges on device pixels, so the clip never blends the redraw with the cached image at its border.
                const float l = std::round(cluster.getX() * ps) / ps, t = std::round(cluster.getY() * ps) / ps;
                cluster = { l, t, std::round(cluster.getRight() * ps) / ps - l, std::round(cluster.getBottom() * ps) / ps - t };
                if (cluster.isEmpty()) continue;
            }
            juce::Path region;
            region.addRectangle(cluster);
            g.saveState();
            g.reduceClipRegion(region);
            drawKeys(g, cluster, true, hover, ps);
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
    void drawKeys(juce::Graphics& g, juce::Rectangle<float> clip, bool live, int hover, float ps)
    {
        const float u = unit();
        const float seam = keys_detail::seamWidth(ps);
        g.setColour(keys_detail::kSeam);
        g.fillRect(clip);
        const int lo = juce::jmax(0, getRangeStart()), hi = juce::jmin(127, getRangeEnd());
        for (int n = lo; n <= hi; ++n)
            if (!juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = slotRect(n, ps);
                if (r.intersects(clip)) keys_detail::drawWhiteKey(g, n, r, look(n, live, hover), u, seam);
            }
        for (int n = lo; n <= hi; ++n)
            if (juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = slotRect(n, ps);
                if (clusterRect(n).toFloat().intersects(clip))
                {
                    const float rightWhite = n + 1 <= hi ? slotRect(n + 1, ps).getX() : r.getRight();
                    keys_detail::drawBlackShadow(g, r, look(n, live, hover), u, rightWhite);
                }
            }
        for (int n = lo; n <= hi; ++n)
            if (juce::MidiMessage::isMidiNoteBlack(n))
            {
                const auto r = slotRect(n, ps);
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
            drawKeys(ig, getLocalBounds().toFloat(), false, -1, ps);
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

// The keybed assembly (frame, key slip) around the keys. The editor
// places this one component; `keys` is the playable MIDI keyboard inside it.
class Keybed final : public juce::Component
{
public:
    explicit Keybed(Cs80AudioProcessor& p) : keys(p, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setOpaque(true);
        keys.setBlackNoteWidthProportion(0.60f);
        keys.setBlackNoteLengthProportion(0.64f);
        addAndMakeVisible(keys);
    }

    // Insets of the key area inside the assembly, in design pixels at the 1504 x 228 design size
    // (top: back felt rail under the panel lip, sides: none, the cheeks are the walls, bottom: dark gap and
    // front rail). They scale with the component width, so any size of Keybed keeps its proportions.
    // Design size 1504 x 228: key area 1504 x 193 (36 white keys of 41.78 px, spanning x 48..1552 of the
    // editor); 12 px of felt above the keys, 23 px below them (2 px dark gap, 21 px walnut rail with the
    // printed legend). Black keys are 64 percent of the white key length. Everything scales with the height it is given:
    // the keys take whatever is left between the felt and the rail.
    juce::BorderSize<int> keyInsets { 12, 0, 23, 0 };

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
        keys.feltGlowReach = juce::roundToInt(12.0f * u);
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

        // A pressed key's tip dips about 3 px over the dark gap and the top of the rail, and throws a warm
        // light on the rail in front of it.
        const float u = unit();
        const float ps = physicalScale(g);
        const float seam = keys_detail::seamWidth(ps);
        const auto clip = g.getClipBounds().toFloat();
        const float kw = keys.getKeyWidth();
        for (int n = keys.getRangeStart(); n <= keys.getRangeEnd(); ++n)
        {
            if (juce::MidiMessage::isMidiNoteBlack(n)) continue;
            const float d = keys.getKeyDepth(n);
            if (d < 0.02f) continue;
            const auto slot = keys.slotRect(n, ps);
            const float x0 = (float) kb.getX() + slot.getX(), x1 = (float) kb.getX() + slot.getRight() - seam;
            const float top = (float) kb.getBottom(), ext = 3.0f * u * d;
            const auto tip = juce::Rectangle<float>(x0, top - 0.6f * u, x1 - x0, ext + 0.6f * u);
            if (tip.intersects(clip))
            {
                juce::Path nose;
                nose.addRoundedRectangle(tip.getX(), tip.getY(), tip.getWidth(), tip.getHeight(), 1.5f * u, 1.5f * u, false, false, true, true);
                g.saveState();
                g.reduceClipRegion(nose);
                keys_detail::fillV(g, tip, { { 0.0f, keys_detail::Colour(0xff8a6038) }, { 1.0f, keys_detail::Colour(0xff4a301a) } });
                g.setColour(keys_detail::white(0.30f));
                g.fillRect(x0, tip.getY(), 1.0f * u, tip.getHeight()); // lit left corner
                g.setColour(keys_detail::black(0.34f));
                g.fillRect(x1 - 1.0f * u, tip.getY(), 1.0f * u, tip.getHeight());
                g.restoreState();
            }
            const float glow = keys.getKeyGlow(n);
            keys_detail::drawFeltGlow(g, (float) kb.getX() + slot.getCentreX(), top + ext, kw, u, d * glow, glow, clip);
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

    // Everything static behind the keys: the back felt, the walnut front rail with its printed legend, and the
    // side walls. Layout from the top: 12 px felt, keys, a 2 px dark gap under the noses, then a 21 px rail.
    void paintFrame(juce::Graphics& g, float u)
    {
        using namespace keys_detail;
        const float W = (float) getWidth(), H = (float) getHeight();
        const auto kb = keys.getBounds().toFloat();
        g.fillAll(Colour(0xff0a0807));

        // Back rail: felt lying in the shade of the deck overhang, 12 px deep, with a raised lip at its front edge.
        const auto back = Rectangle<float>(kb.getX(), 0.0f, kb.getWidth(), kb.getY());
        paintFelt(g, back, Colour(0xff55201a), Colour(0xff733028), 11, u);
        fillV(g, back, { { 0.0f, black(0.78f) }, { 0.45f, black(0.36f) }, { 1.0f, black(0.06f) } });
        g.setColour(white(0.13f));
        g.fillRect(kb.getX(), kb.getY() - 2.6f * u, kb.getWidth(), 0.9f * u); // raised lip of the felt
        g.setColour(black(0.94f));
        g.fillRect(kb.getX(), kb.getY() - 1.2f * u, kb.getWidth(), 1.2f * u);

        // Front: the dark gap under the key noses, then one walnut rail carrying the printed note names.
        const float slipTop = kb.getBottom();
        const float railTop = slipTop + 2.0f * u;
        const auto rail = Rectangle<float>(kb.getX(), railTop, kb.getWidth(), H - railTop);
        g.setColour(Colour(0xff040302));
        g.fillRect(kb.getX(), slipTop, kb.getWidth(), 2.0f * u);
        paintWalnutFace(g, rail, false, 411);
        fillH(g, rail, { { 0.0f, varnish(0.0f) }, { 0.20f, varnish(0.05f) }, { 0.50f, varnish(0.0f) }, { 0.78f, varnish(0.04f) },
                         { 1.0f, varnish(0.0f) } });
        // Lacquer: a thin reflection across the face, a lit chamfer along the top, and a rounded front edge below:
        // a lit band where the face rolls over, then a dark roll into the shadow under the rail.
        fillV(g, Rectangle<float>(rail.getX(), railTop + rail.getHeight() * 0.36f, rail.getWidth(), 2.4f * u),
              { { 0.0f, varnish(0.0f) }, { 0.5f, varnish(0.07f) }, { 1.0f, varnish(0.0f) } });
        fillV(g, Rectangle<float>(rail.getX(), railTop, rail.getWidth(), 3.6f * u),
              { { 0.0f, varnish(0.42f) }, { 0.35f, varnish(0.18f) }, { 1.0f, varnish(0.0f) } });
        fillV(g, Rectangle<float>(rail.getX(), railTop + 8.0f * u, rail.getWidth(), rail.getHeight() - 8.0f * u + 1.0f),
              { { 0.0f, black(0.0f) }, { 1.0f, black(0.34f) } });
        {
            const float bevelTop = H - 5.6f * u;
            fillV(g, Rectangle<float>(rail.getX(), bevelTop, rail.getWidth(), 2.4f * u),
                  { { 0.0f, varnish(0.0f) }, { 0.45f, varnish(0.20f) }, { 1.0f, varnish(0.06f) } });
            fillV(g, Rectangle<float>(rail.getX(), bevelTop + 2.4f * u, rail.getWidth(), 3.2f * u + 1.0f),
                  { { 0.0f, black(0.10f) }, { 1.0f, black(0.66f) } });
        }
        g.setColour(varnish(0.50f));
        g.fillRect(rail.getX(), railTop, rail.getWidth(), 0.9f * u);
        fillV(g, Rectangle<float>(rail.getX(), railTop + 0.9f * u, rail.getWidth(), 4.0f * u),
              { { 0.0f, black(0.20f) }, { 1.0f, black(0.0f) } }); // shadow of the key noses on the rail

        // The cheeks stand against both ends: a dark joint and a little occlusion on the felt and the rail beside them.
        fillH(g, Rectangle<float>(0.0f, 0.0f, 7.0f * u, H), { { 0.0f, black(0.55f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(W - 5.0f * u, 0.0f, 5.0f * u, H), { { 0.0f, black(0.0f) }, { 1.0f, black(0.40f) } });

        // Note names printed in cream ink on the rail, with a small tick at each C and a pip at middle C.
        // Labels never go below 8 px, even in a small editor.
        const float fontPx = juce::jmax(8.0f, 9.4f * u);
        g.setFont(juce::Font(juce::FontOptions(juce::Font::getDefaultSansSerifFontName(), fontPx, juce::Font::bold))
                      .withExtraKerningFactor(0.07f));
        const float textTop = railTop + 4.4f * u, textBottom = textTop + 12.0f * u;
        const float capTop = textTop + (textBottom - textTop - 0.72f * fontPx) * 0.5f; // top of the capital letters
        const float tickBottom = juce::jmin(railTop + 5.8f * u, capTop - 1.0f);
        const float tickTop = juce::jmax(railTop + 1.0f, tickBottom - 3.2f * u);
        const Colour ink(0xffeee2c4);
        for (int n = keys.getRangeStart(); n <= keys.getRangeEnd(); ++n)
        {
            if (n % 12 != 0) continue;
            const bool middleC = (n == 60);
            const auto r = keys.getRectangleForKey(n).translated((float) keys.getX(), 0.0f);
            const float cx = r.getCentreX();
            if (middleC)
            {
                const float py = juce::jmin(railTop + 3.8f * u, capTop - 1.0f - 1.8f * u);
                g.setColour(black(0.55f));
                g.fillEllipse(cx - 2.5f * u, py - 2.3f * u, 5.0f * u, 5.0f * u);
                g.setColour(Colour(0xffffb84a));
                g.fillEllipse(cx - 1.8f * u, py - 1.8f * u, 3.6f * u, 3.6f * u);
                g.setColour(white(0.65f));
                g.fillEllipse(cx - 1.0f * u, py - 1.3f * u, 1.3f * u, 0.9f * u);
            }
            else
            {
                g.setColour(black(0.40f));
                g.fillRect(cx - 0.1f * u, tickTop + 0.4f * u, 1.0f * u, tickBottom - tickTop);
                g.setColour(ink.withAlpha(0.88f));
                g.fillRect(cx - 0.6f * u, tickTop, 1.0f * u, tickBottom - tickTop);
            }
            const auto name = middleC ? juce::String("C4 MIDDLE C")
                                      : juce::MidiMessage::getMidiNoteName(n, true, true, keys.getOctaveForMiddleC());
            const auto box = Rectangle<float>(cx - 44.0f * u, textTop, 88.0f * u, textBottom - textTop);
            g.setColour(black(0.42f));
            g.drawText(name, box.translated(0.6f * u, 0.7f * u), juce::Justification::centred, false);
            g.setColour(middleC ? Colour(0xfff4dc9a) : ink.withAlpha(0.94f));
            g.drawText(name, box, juce::Justification::centred, false);
        }

    }

    // Light and shadow that fall across the keys: the raised felt above them and the walls beside them.
    // Drawn over the keys; size equals the key area.
    void paintOverlay(juce::Graphics& g, juce::Rectangle<float> area, float u)
    {
        using namespace keys_detail;
        // The felt stands proud of the key tops: a dense 6 px shadow right under it, then a long soft fall-off.
        fillV(g, Rectangle<float>(0.0f, 0.0f, area.getWidth(), 30.0f * u),
              { { 0.0f, black(0.74f) }, { 0.20f, black(0.38f) }, { 0.55f, black(0.12f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(0.0f, 0.0f, 12.0f * u, area.getHeight()), { { 0.0f, black(0.30f) }, { 0.45f, black(0.10f) }, { 1.0f, black(0.0f) } });
        fillH(g, Rectangle<float>(area.getWidth() - 6.0f * u, 0.0f, 6.0f * u, area.getHeight()),
              { { 0.0f, black(0.0f) }, { 1.0f, black(0.20f) } });
    }

    juce::Image frameImage, overlayImage;
    bool frameDirty = true;
    int frameW = 0, frameH = 0;
    float frameScale = 0.0f;
    juce::Rectangle<int> frameKeys;
};

// [ws8-studio] Computer-keyboard note play. Attach it as a KeyListener to the editor: letter keys play notes through
// the same MidiKeyboardState the on-screen keys use (so they reach the processor's keyboard event queue and light the
// keys), on MIDI channel 1.
//
//   A W S E D F T G Y H U J K O L P ;   C C# D D# E F F# G G# A A# B C C# D D# E
//   Z / X                               octave down / up (the A key spans C1 to C7)
//   C / V                               velocity down / up
//
// It never takes keys while a text field has focus, nor keys pressed with Ctrl, Cmd or Alt (menu shortcuts). A held
// note is released when its key comes up, when the editor stops seeing the key (for example after losing focus; a
// 10 Hz check), on disable and on destruction, so no note can hang.
class QwertyKeyboard final : public juce::KeyListener, private juce::Timer
{
public:
    static constexpr int kChannel = 1;
    static constexpr const char* kNoteKeys = "awsedftgyhujkolp;";

    explicit QwertyKeyboard(juce::MidiKeyboardState& s) : state(s) {}
    ~QwertyKeyboard() override { releaseAll(); }

    void setEnabled(bool shouldPlay)
    {
        enabled = shouldPlay;
        if (! enabled)
            releaseAll();
    }
    bool isEnabled() const noexcept { return enabled; }
    // MIDI note of the A key: 60 (C4) by default, moved in octaves by Z and X.
    int getBaseNote() const noexcept { return baseNote; }
    void setBaseNote(int note) { baseNote = juce::jlimit(12, 108, note - note % 12); }
    float getVelocity() const noexcept { return velocity; }
    int heldCount() const noexcept { return (int) held.size(); }
    // Overrides the physical key test (tests drive key-up without a real keyboard).
    std::function<bool(int keyCode)> isKeyDown = [](int keyCode) { return juce::KeyPress::isKeyCurrentlyDown(keyCode); };
    // Whether a text field has the keyboard (tests override it: a headless editor cannot take focus).
    std::function<bool()> textEntryFocused = [] { return focusIsTextEntry(); };
    std::function<void()> onChange;

    static bool focusIsTextEntry()
    {
        auto* focused = juce::Component::getCurrentlyFocusedComponent();
        return focused != nullptr && (dynamic_cast<juce::TextEditor*>(focused) != nullptr
                                      || dynamic_cast<juce::TextEditor*>(focused->getParentComponent()) != nullptr);
    }

    bool keyPressed(const juce::KeyPress& key, juce::Component*) override
    {
        if (! enabled || (textEntryFocused && textEntryFocused()))
            return false;
        const auto mods = key.getModifiers();
        if (mods.isCommandDown() || mods.isCtrlDown() || mods.isAltDown())
            return false;
        const auto c = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
        if (c == 'z' || c == 'x')
        {
            setBaseNote(baseNote + (c == 'z' ? -12 : 12));
            if (onChange) onChange();
            return true;
        }
        if (c == 'c' || c == 'v')
        {
            velocity = juce::jlimit(0.1f, 1.0f, velocity + (c == 'c' ? -0.1f : 0.1f));
            if (onChange) onChange();
            return true;
        }
        const int offset = noteOffset(c);
        if (offset < 0)
            return false;
        const int keyCode = key.getKeyCode();
        for (const auto& h : held)
            if (h.keyCode == keyCode)
                return true;   // auto-repeat of a held key
        const int note = baseNote + offset;
        if (note < 0 || note > 127)
            return true;
        state.noteOn(kChannel, note, velocity);
        held.push_back({ keyCode, note });
        if (! isTimerRunning())
            startTimerHz(10);
        return true;
    }

    bool keyStateChanged(bool, juce::Component*) override
    {
        const bool any = ! held.empty();
        releaseLifted();
        return any;
    }

    void releaseAll()
    {
        for (const auto& h : held)
            state.noteOff(kChannel, h.note, 0.0f);
        held.clear();
        stopTimer();
    }

    static int noteOffset(juce::juce_wchar c)
    {
        for (int i = 0; kNoteKeys[i] != 0; ++i)
            if ((juce::juce_wchar) kNoteKeys[i] == c)
                return i;
        return -1;
    }

private:
    struct Held { int keyCode; int note; };

    void releaseLifted()
    {
        for (auto it = held.begin(); it != held.end();)
        {
            if (! isKeyDown(it->keyCode))
            {
                state.noteOff(kChannel, it->note, 0.0f);
                it = held.erase(it);
            }
            else
                ++it;
        }
        if (held.empty())
            stopTimer();
    }

    void timerCallback() override { releaseLifted(); }

    juce::MidiKeyboardState& state;
    std::vector<Held> held;
    bool enabled = true;
    int baseNote = 60;
    float velocity = 0.8f;
};

} // namespace cs80::ui
