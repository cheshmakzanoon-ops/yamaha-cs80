# CS-80 editor: design spec (round 2 redesign)

Goal from the owner: the editor must look like the Yamaha CS-80 itself, in our own original design, at the polish level of the best commercial software recreations (for example Arturia's CS-80 V). Round 1 only recoloured the old layout; round 2 changes layout, controls, menus and keys. The quality bar: a stranger who has seen photos of a CS-80 should recognise it at a glance, and every surface should have depth, material and light.

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
y    0-64    Toolbar: brand, preset display (illuminated window), prev/next, LIBRARY, SAVE, INIT, ADVANCED, PANIC, meter
y   64-660   Control surface: left voice-flow plate | channel I row | channel II row | centre/ring/sub/tone/global row
y  660-772   Performance deck: performance block | ribbon + YAMAHA nameplate | keyboard/foot block
y  772-1000  Keybed: full-width 61-key keyboard in a wood-and-felt frame (keys about 40 px wide, 190+ px tall)
```

The old arrangement put performance and foot controls beside a short keyboard. The new one gives the keyboard the full width and moves those controls to the deck.

## Ownership (each agent edits only its own files)

| Owner | Files | Scope |
|---|---|---|
| KEYS | `src/plugin/HardwareKeyboard.h` | Key rendering, depression animation, glow, keybed frame (`Keybed`), key slip, felt, end blocks, note labels |
| CONTROLS | `src/plugin/UiComponents.h`, `UiComponents.cpp` | LookAndFeel, faders, caps, knobs, tabs, rockers, LED buttons, wheels, ribbon, tooltips, drag value bubble, shared drawing helpers |
| CHASSIS | `src/plugin/PluginEditor.cpp`, `PluginEditor.h` | New layout, chassis art, toolbar and display, left plate, memory programmer, overlay transitions |
| BROWSER | `src/plugin/PresetBrowser.cpp`, `PresetBrowser.h` | Library UI |
| ADVANCED | `src/plugin/AdvancedPanel.cpp`, `AdvancedPanel.h` | Advanced pages |

Shared rules:

- Do not remove or change the signature of an existing declaration in `UiComponents.h`; add new ones. Others compile against them.
- The accessibility names, titles, parameter bindings, keyboard focus and tab order of every control must survive. Tests in `src/tools/ui_snapshot.cpp` check them.
- No pixel-level assertions exist; the self-test checks bounds, accessibility, focus, at least 40 hardware sliders, snapshot sizes.
- Everything static is drawn once into a buffered image (see `Backdrop`). Per-frame repaints must stay cheap: no blur filters, no per-frame gradients over large areas. The editor repaints at 8 Hz; animation needs its own small timer on its own small component.
- JUCE 8.0.11, C++20, software renderer. Fonts in CI are DejaVu only: build the engraved look from bold weights, `withHorizontalScale`, `withExtraKerningFactor` and drop shadows rather than a custom typeface.

## Process

- One shared source tree and one build tree. Build and render with `/home/claude/work/render.sh <outdir>`; it serialises builds across agents. If it reports an error in a file you do not own, another agent is mid-edit: wait 60 s and rerun. Never edit another agent's file.
- Look at your PNGs with the Read tool, critique them against this spec, and iterate at least four times before reporting. Zoom by cropping with PIL (`Image.crop().resize()`) and read the crops.
- Commit only your own files: `git add <your files> && git commit -m "..." -- <your files>`. Never `git add -A`, `git stash`, `git reset`, `git checkout` or `git push`. If `index.lock` exists, wait and retry.
- A passing run has no `FAIL` line in `<outdir>/run.log` (the process may exit 1 with only an `X Error BadAtom` line; that is the missing window manager and is fine).
- Commit message style: plain prose, imperative, no emoji, end with the two trailer lines
  `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` and `Claude-Session: https://claude.ai/code/session_01MxucX1TjDGxNX6pJvnvHnf`.
