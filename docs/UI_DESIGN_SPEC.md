# CS-80 editor: design spec

Goal: the editor must look like the Yamaha CS-80 itself, in our own original design, at the polish level of the best commercial software recreations. The quality bar: a stranger who has seen photos of a CS-80 should recognise it at a glance, and every surface should have depth, material and light.

Never copy another product's artwork, icons, text or layout. Design from the hardware and from this spec. Never use Yamaha or Arturia bitmaps; everything is drawn with JUCE vector calls.

## The instrument (what to evoke)

- Dark walnut/rosewood side cheeks with varnish sheen and visible grain, rounded top-front corners.
- Charcoal-black control surface with a fine crinkle-finish grain, cream and gold silkscreen legends, gold etched line-art grouping the controls.
- Two identical channel rows (I and II) of long vertical faders with cream, green, pink, yellow, black and grey rectangular caps; at their left the hinged memory lid, printed with the voice block diagram; a centre block of backlit plastic tone-selector tabs with the full tone names; black tabs for the PULSE/SAW selectors.
- A YAMAHA nameplate and "CS-80 Polyphonic Synthesizer" legend; a long ribbon controller strip just above the keys; a 61-key keyboard that spans the whole width.
- Light comes from the upper left. Everything raised has a lit top-left edge and a dark bottom-right edge and a cast shadow; everything recessed has an inner shadow at the top-left.

## Palette (existing constants in `UiComponents.h`; extend, do not break)

Panel 0xff1b1a18 to 0xff2e2b27, cream 0xffe9dfc2, gold 0xffcdb878, amber 0xffd9a640. Caps: cream 0xffdcd8c4, green 0xff2b9a50, pink 0xffe0205f, yellow 0xffe9e03e, black 0xff1a1a1f, grey 0xffbdb7ae. Wood 0xff6b2f17 family.

## Target layout (design space stays 1600 x 1000; min window 1120 x 700, aspect locked as today)

The arrangement follows the conformance audit, [PANEL_CONFORMANCE.md](PANEL_CONFORMANCE.md). Every rectangle comes
from one table, `src/plugin/PanelLayout.h`, in final design coordinates (no shifted blocks).

```text
y    0-64    Toolbar (software): brand, preset display, prev/next, LIBRARY, SAVE, INIT, ADVANCED, lamps, meter, PANIC
y   64-72    Ledge under the toolbar (status strip above it, vent slots)
y   74-452   Memory lid (x 52-340, closed, printed block diagram, OPEN MEMORY LID and VOICE SETTINGS keys)
             | channel I row (y 74-260) and channel II row (y 266-452), x 348-1552
y  460-626   Front strip (PN2): PITCH, DETUNE, ring, sub, feet | tone selector | MIX/BRILL/RESO, keyboard
             brilliance and level, touch response, VOLUME
y  640-744   Deck: left-hand panel (sustain, portamento, foot assign, tremolo/chorus ON/OFF + TREMOLO + CHORUS,
             foot controller) | nameplate and 860 px ribbon | rear jack panel (footswitch jack, contact, output)
y  748-1000  Keybed: full-width 61-key keyboard in a walnut frame (keys 41.8 px wide, about 5:1 length to width)
```

Both side cheeks (x 0-48 and x 1552-1600) are one continuous walnut surface drawn in the backdrop; overlays leave them visible.

## Source map

| Area | Files | Scope |
|---|---|---|
| Keys | `src/plugin/HardwareKeyboard.h` | Key rendering, depression, glow, keybed frame, felt, rail, note labels |
| Controls | `src/plugin/UiComponents.h`, `UiComponents.cpp` | LookAndFeel, faders, caps, knobs, tabs, rockers, LED buttons, ribbon, tooltips, shared drawing helpers |
| Chassis | `src/plugin/PluginEditor.cpp`, `PluginEditor.h` | Chassis art, closed lid, toolbar and display, overlay transitions |
| Panel sections | `src/plugin/PanelLayout.h`, `PanelWidgets.*`, `ChannelRow.*`, `CentreRow.*`, `PerformanceDeck.*`, `MemoryLid.*`, `ArtCommon.*` | Layout table, panel keys, the section components, the memory programmer, shared art |
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
