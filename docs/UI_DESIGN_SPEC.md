# CS-80 editor: design spec

Goal: the editor must look like the Yamaha CS-80 itself, in our own original design, at the polish level of the best commercial software recreations. The quality bar: a stranger who has seen photos of a CS-80 should recognise it at a glance, and every surface should have depth, material and light.

Never copy another product's artwork, icons, text or layout. Design from the hardware and from this spec. Never use Yamaha or Arturia bitmaps; everything is drawn with JUCE vector calls.

## The instrument (what to evoke)

- Dark walnut/rosewood side cheeks with varnish sheen and visible grain, rounded top-front corners.
- Charcoal-black control surface with a fine crinkle-finish grain, cream and gold silkscreen legends, gold etched line-art grouping the controls.
- Two identical channel rows (I and II) of long vertical faders with cream, green, pink, yellow, black and grey rectangular caps; a left-hand voice-flow diagram plate; a centre block of backlit plastic tone-selector tabs; black rocker switches for the PULSE/SAW selectors.
- A YAMAHA nameplate and "CS-80 Polyphonic Synthesizer" legend; a long ribbon controller strip just above the keys; a 61-key keyboard that spans the whole width.
- Light comes from the upper left. Everything raised has a lit top-left edge and a dark bottom-right edge and a cast shadow; everything recessed has an inner shadow at the top-left.

## Palette (existing constants in `UiComponents.h`; extend, do not break)

Panel 0xff1b1a18 to 0xff2e2b27, cream 0xffe9dfc2, gold 0xffcdb878, amber 0xffd9a640. Caps: cream 0xffdcd8c4, green 0xff2b9a50, pink 0xffe0205f, yellow 0xffe9e03e, black 0xff1a1a1f, grey 0xffbdb7ae. Wood 0xff6b2f17 family.

## Target layout (design space stays 1600 x 1000; min window 1120 x 700, aspect locked as today)

```text
y    0-64    Toolbar: brand, preset display (illuminated window), prev/next, LIBRARY, SAVE, INIT, ADVANCED, voice and MIDI lamps, meter, PANIC
y   64-72    Ledge under the toolbar (status strip above it, vent grille detail)
y   72-632   Control surface: voice-flow plate (left) | channel I row | channel II row, then the centre row (ring, sub, feet, tone selector, global)
y  640-745   Performance deck: performance block | nameplate and 804 px ribbon | brilliance/level/foot block
y  748-1000  Keybed: full-width 61-key keyboard in a walnut frame (keys 41.8 px wide, about 5:1 length to width)
```

Both side cheeks (x 0-48 and x 1552-1600) are one continuous walnut surface drawn in the backdrop; overlays leave them visible.

## Source map

| Area | Files | Scope |
|---|---|---|
| Keys | `src/plugin/HardwareKeyboard.h` | Key rendering, depression, glow, keybed frame, felt, rail, note labels |
| Controls | `src/plugin/UiComponents.h`, `UiComponents.cpp` | LookAndFeel, faders, caps, knobs, tabs, rockers, LED buttons, ribbon, tooltips, shared drawing helpers |
| Chassis | `src/plugin/PluginEditor.cpp`, `PluginEditor.h` | Layout, chassis art, toolbar and display, voice-flow plate, memory programmer, overlay transitions |
| Library | `src/plugin/PresetBrowser.cpp`, `PresetBrowser.h` | Preset library UI |
| Advanced | `src/plugin/AdvancedPanel.cpp`, `AdvancedPanel.h` | Advanced pages |

Rules for UI changes:

- Do not remove or change the signature of an existing declaration in `UiComponents.h`; add new ones. Other files compile against them.
- The accessibility names, titles, parameter bindings, keyboard focus and tab order of every control must survive. Tests in `src/tools/ui_snapshot.cpp` check them.
- No pixel-level assertions exist; the self-test checks bounds, accessibility, focus, at least 40 hardware sliders, snapshot sizes.
- Everything static is drawn once into a buffered image (see `Backdrop`). Per-frame repaints must stay cheap: no blur filters, no per-frame gradients over large areas. The editor repaints at 8 Hz; animation needs its own small timer on its own small component.
- JUCE 8.0.11, C++20, software renderer. Fonts in CI are DejaVu only: build the engraved look from bold weights, `withHorizontalScale`, `withExtraKerningFactor` and drop shadows rather than a custom typeface.

## Checking a change

- Build and render with the `CS80Snapshots` target: `CS80Snapshots --out <dir> --self-test`. On Linux run it through `scripts/run-ui-smoke-linux.sh` (Xvfb plus Openbox); a bare X server aborts with `BadAtom` partway through and silently skips the later checks.
- A passing run ends with `UI SMOKE TEST PASSED` and has no `FAIL` line in the log.
- Look at the PNGs, and zoom into crops of the area you changed, at 1600x1000 and at the 1120x700 minimum.
- Never use Yamaha or Arturia bitmaps; everything is drawn with JUCE vector calls.
