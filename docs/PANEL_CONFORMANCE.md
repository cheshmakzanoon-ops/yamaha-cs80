# Panel conformance audit (Phase 5.1)

Audit date 2026-10-31, workstream ws7-panel. This is a control-by-control comparison of the editor's main view
against the original CS-80 panel: the owner's-manual control index 1-46 and the service-manual S2 panel
drawing. It records what the evidence says, what the editor does, and every place where the two disagree or
the sources disagree with each other. It is the input to the main-view re-layout (Phase 5.2) and the source of
the layout table in `src/plugin/PanelLayout.h`.

## What was reachable

| Source | Reachable from this session? | How it is used here |
| --- | --- | --- |
| Owner's manual scans ([CS80E_1](https://www.therogoffs.com/cs80/CS80E_1.pdf), part 2, part 3) | **No.** The session proxy refuses therogoffs.com, synthfool.com and yamaha.com (HTTP 403, policy). | Only through the page citations already recorded in [HARDWARE_COVERAGE.md](HARDWARE_COVERAGE.md) (index 1-46, printed pages) and [HARDWARE_REFERENCES.md](HARDWARE_REFERENCES.md) (cap colours pp8-23, tone order p37). |
| Service manual S2, [02 - Panel Layout](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/02%20-%20Panel%20Layout.jpg) | **No.** Same proxy refusal. `scripts/inspect_circuit_sources.py` has no S2 entry, and running `.github/workflows/circuit-inspection.yml` needs CI, which this session cannot trigger. | Only the layout summary in HARDWARE_REFERENCES ("diagram/memory lid upper left; two repeated synth-control rows upper right; global performance controls and two tone-selector rows below; long ribbon above keyboard; sustain/portamento/tremolo controls left of keyboard"). |
| Panel circuit drawings PN1 p62/63, PN2 p64, Panel 3 p66, Panel 5 p67 | Not fetched again; earlier native CI crops are recorded in the evidence docs below. | Board membership: which control sits on which physical panel. |
| Adjustment procedure, printed pp21-22 | Not fetched again; read in [BA617_SOURCE_REVIEW.md](BA617_SOURCE_REVIEW.md). | Tab names of the tremolo/chorus group. |

Nothing below was measured or newly read from a Yamaha page in this session. Exact positions, spacing and
proportions on S2 remain **to be verified**; where the editor needs a number, it is a declared inference.

## Evidence classes

- **D** documented: stated by a Yamaha page, as cited by the named repository evidence document.
- **I** declared inference: a reasoned choice, with its alternatives in the notes.
- **U** unknown: no reachable evidence; the editor's choice is a placeholder.
- **C** conflict: the sources, or the editor and the sources, disagree. Listed again under "Source conflicts".

Abbreviations: O = owner's manual printed page; HC = HARDWARE_COVERAGE.md; HR = HARDWARE_REFERENCES.md;
FCE = FOOT_CONTROL_EVIDENCE.md; KCE = KEYBOARD_CONTOUR_EVIDENCE.md; OLR = OUTPUT_LEVEL_SOURCE_REVIEW.md;
BA617 = BA617_SOURCE_REVIEW.md.

## Physical areas

| Area | Contents (by evidence) | Evidence |
| --- | --- | --- |
| Upper-left lid | Hinged lid printed with the block diagram; under it the four memory panels (index 46) | D: HR (S2 summary, Yamaha museum), PN1 MKB p63 in PRESET_COMPLETENESS_AUDIT.md |
| Upper panel (PN1) | The two programmable rows, index 20-45; rank I above rank II | D: HR (S2 summary), HC rows 20-45 |
| Front strip (PN2) | Tone selectors (TS1-14), stereo VOLUME (LI/RI wipers), master RES (VR14), level VR11, keyboard contour VR19-22, sub-oscillator switch routing | D: PRESET_BA_CUTOFF_BRANCHES.md (PN2-TS), OLR (VOLUME wipers), RESONANCE_CONTROL_REFERENCE_DESIGN.md (VR14), KCE (VR19-22), HC row 11 (PN2 switch routing) |
| Left of the keyboard (Panel 3) | Sustain, portamento/glissando, tremolo/chorus; footswitch assigners SW1/SW2; EXP / EXP-WAH selector SW10 | D: HR (S2 summary: sustain/portamento/tremolo left of keyboard); D: FCE (Panel3 p66 SW1/SW2, EXP/EXP-WAH). I: that Panel 3 *is* the left-hand panel (it carries the two assigners of the sustain and portamento controls; the pre-ws7 editor comment says the same) |
| Jack panel (Panel 5) | Footswitch jack J5 (normalled contact), expression-pedal jack, output HIGH/LOW SW1 (three linked poles to LEFT, GENERAL, RIGHT) | D: FCE (EJ/EXP/PFS p67 J5), OLR (PN5 p67 SW1). I: rear of the instrument |
| Above the keys | Long ribbon controller | D: HR, O14 |

## Control table

Columns follow the plan: position, group, legend text, cap colour, lever direction, switch type; then what the
editor does after the ws7 re-layout. "Toward player" means the lever increases as it is pushed toward the
keyboard; on the editor's top-down view that is downward.

### Global controls (index 1-19)

| # | Control | Position | Group legend | Legend | Cap | Lever direction | Switch type | Editor (after ws7) | Evidence / notes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | AC power | U (O6) | — | POWER | — | — | switch | none: host lifecycle | D: HC row 1. No editor control by design |
| 2 | Main volume | PN2 (stereo pot) | — | VOLUME | U | U | U (dual-gang pot) | PN2 right end, rotary | D: board (OLR). Type and position along PN2: U |
| 3 | Tone selectors, rank I / rank II | PN2, two rows, rank I upper | TONE SELECTOR | full tone names (table below) | tab colours U | — | interlocked tabs, one active per row | PN2 centre, two rows of 14 backlit tabs, full names | D: order and one-per-row (HR, S1, O37). Tab plastic colours are the pre-ws7 choice (U) |
| 4 | MIX I-II | PN2 | U | MIX | U | U | lever | PN2, after the tone selector | D: function (O7,37). Cap red is the pre-ws7 choice (U) |
| 5 | FEET I, FEET II | PN2 | FEET | FEET I / FEET II | U | U | six-position | PN2, left of the tone selector | D: function (O8,37, S1). Cap grey (U) |
| 6 | DETUNE (CH II) | PN2 | U | DETUNE | white | U | lever | PN2, after PITCH | D: cap (HR, O8-14) |
| 7 | Overall brilliance | PN2 | U | BRILLIANCE | green | toward player (C) | lever | PN2 global group | D: cap (HR); D: "most global amounts increase toward the player" (HR). Editor increases upward: C |
| 8 | Overall resonance | PN2 (VR14) | U | RESONANCE | red | toward player (D, "downwards physical increase") | lever | PN2 global group | D: HC row 8, RESONANCE_CONTROL_REFERENCE_DESIGN.md. Editor increases upward: C |
| 9 | Keyboard brilliance LOW / HIGH | PN2 (VR19, VR20) | KEYBOARD CONTROL | BRILLIANCE LOW / HIGH | green | centre-neutral bipolar; physical polarity U | lever | **moved** from the deck to PN2, after RESONANCE | D: board and pot names (KCE, Panel2 p64); D: caps (HR); I: placement beside BRILLIANCE/RESONANCE (same board, owner index 7-10 consecutive). Pre-ws7 it sat in the deck's right block: C |
| 10 | Keyboard level LOW / HIGH | PN2 (VR21, VR22) | KEYBOARD CONTROL | LEVEL LOW / HIGH | grey | centre-neutral bipolar; polarity U | lever | **moved** with #9 | As #9 |
| 11 | Sub oscillator: function, speed, VCO, VCF, VCA | PN2 (switch routing) | SUB OSCILLATOR | FUNCTION, SPEED, VCO, VCF, VCA | function black; speed, VCO white; VCF green; VCA grey | U | function: six-position; others levers | PN2, unchanged | D: caps (HR); D: board (HC row 11) |
| 12 | Touch response: initial pitch bend; after-touch speed, VCO, VCF | U (likely PN2) | TOUCH RESPONSE | PITCH BEND / SPEED / VCO / VCF | bend, speed, VCO white; VCF green | U | levers | PN2 global group, unchanged | D: caps (HR); position U |
| 13 | Sustain amount; sustain I/II; footswitch assigner | left of keyboard (Panel 3) | SUSTAIN | SUSTAIN, I / II, assign | amount yellow | U | amount lever; I/II two-position; assigner two-position (SW1) | deck left block: lever, I/II tab, assign rocker beside it | D: HR, FCE. **Assigner moved** from the deck's right block to the left block |
| 14 | Portamento/glissando amount; P/G selection; assigner | left of keyboard (Panel 3) | PORTAMENTO / GLISSANDO | PORTAMENTO, P / G, assign | amount white | U | amount lever; selection U; assigner two-position (SW2) | deck left block: lever, three-state GLIDE lever, assign rocker | D: HR, FCE. **Assigner moved** to the left block. Selection type U (the editor's OFF/PORTA/GLISS is a software three-state) |
| 15 | Tremolo/chorus: on/off, tremolo vs chorus, speed, depth | left of keyboard | TREMOLO / CHORUS | ON / OFF, TREMOLO, CHORUS, SPEED, DEPTH | U | U | tabs (C) | deck left block: **separate ON/OFF, TREMOLO and CHORUS tabs** (replacing the OFF/CHORUS/TREMOLO 3-way selector) and SPEED, DEPTH | D: owner lists on/off and tremolo-versus-chorus (HC row 15, O13,39); D: adjustment p21 names TREMOLO, CHORUS and ON/OFF tabs (BA617). C: see conflicts. Speed/depth as knobs is the pre-ws7 choice (U) |
| 16 | Ring modulator: speed, depth, attack, decay, modulation | U (pre-ws7: PN2 left) | RING MODULATOR | SPEED, DEPTH, ATTACK, DECAY, MODULATION | attack, decay black; depth, modulation grey; speed white | U | levers | PN2 left, unchanged | D: caps (HR); position U |
| 17 | Foot pedal selector EXP / EXP-WAH | Panel 3 (SW10) | FOOT CONTROLLER | EXP / EXP-WAH | — | — | two-position | **moved** to the deck left block, with the on-screen pedal | D: FCE (Panel3 p66; WAH-only drawing crossed out, no third mode) |
| 18 | Pitch tuning | U | — | PITCH | — | — | rotary (I: "knob circuitry", HC row 18) | PN2 left end, rotary | Position U |
| 19 | Ribbon | above the keys | RIBBON CONTROLLER | — | — | first contact = zero | strip | deck centre, lengthened | D: HR, O14 |
| — | Footswitch jack / contact | Panel 5 (J5) and the pedal | — | — | — | — | jack; momentary pedal | deck right block, labelled as the rear jack panel: PLUG (jack), PRESS (latching stand-in for the momentary pedal) | D: FCE. I: rear. Pre-ws7 these sat in the front-panel group: C |
| — | Output HIGH / LOW | Panel 5 (SW1) | — | HIGH / LOW | — | — | three-pole changeover | deck right block, rear jack panel | D: OLR. I: rear |

### Programmable rows (index 20-45, each rank)

Both ranks carry the same controls in the same order; the editor draws rank I above rank II (D: HR).

| # | Control | Group | Legend | Cap | Switch type | Editor | Evidence / notes |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 20 | PWM speed | VCO | SPEED | white | lever | lever, white | D: cap (HR, O15-23) |
| 21 | PWM depth | VCO | PWM | green | lever | lever, green | D |
| 22 | PW duty | VCO | PW | U (editor green) | lever | lever, green | Cap U |
| 23 | Square on/off | VCO | PULSE (square) | — | on/off switch; type U | black-lens tab | D: function (O14,32). Legend wording and switch form U |
| 24 | Saw on/off | VCO | SAW | — | on/off switch; type U | black-lens tab | As #23 |
| 25 | Noise level | VCO | NOISE | green | lever | lever, green | D |
| 26 | HPF cutoff | VCF | HPF | green | lever | lever, green | D |
| 27 | HPF resonance | VCF | RES | red | lever | lever, red | D |
| 28 | LPF cutoff | VCF | LPF | U (editor green) | lever | lever, green | Cap U (only "noise/HPF green" is documented) |
| 29 | LPF resonance | VCF | RES | red | lever | lever, red | D |
| 30-34 | Filter IL, AL, A, D, R | VCF | IL, AL, A, D, R | U (editor black ×4, yellow R) | levers | unchanged | Caps U: the documented black/yellow set is the amplitude envelope; the filter set follows it by analogy (I) |
| 35 | VCF level | VCA (mix) | LEVEL (editor "VCF LV") | grey | lever | lever, grey | D: "VCF/sine mixing gray" |
| 36 | Sine level | VCA (mix) | SINE | grey | lever | lever, grey | D |
| 37-39 | VCA attack, decay, sustain | VCA | A, D, S | black | levers | black | D |
| 40 | VCA release | VCA | R | yellow | lever | yellow | D |
| 41 | VCA level | VCA | LEVEL | U (editor grey) | lever | grey | Cap U |
| 42-43 | Initial-touch brilliance, level | INITIAL | BRILLIANCE, LEVEL | U (editor green, grey) | levers | unchanged | Caps U (by analogy with #7 and #10: I) |
| 44-45 | After-touch brilliance, level | AFTER | BRILLIANCE, LEVEL | U | levers | unchanged | As #42-43 |
| 46 | Four memory panels | under the upper-left lid | MEMORY | — | miniature levers and switches, 4 × 26 | **closed lid at the upper left**, printed with a block diagram; opening it shows the magnified programmer | D: HR (Yamaha museum, S2 summary), HC row 46. The pre-ws7 upper-left plate was a live software signal-flow display with the lid behind a key: C |

Row lever direction: U for every row control (no reachable statement); the editor increases upward.

### Tone-selector legends (index 3)

Printed wording, left to right (D: S1, O37 via HR). The editor now prints these words on the tabs; the core
keeps its own short names ("E. Piano") for status text.

| Column | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Rank I | STRING 1 | STRING 3 | BRASS 1 | FLUTE | ELECTRIC PIANO | CLAVICHORD 1 | HARPSICHORD 1 | ORGAN 1 | GUITAR 1 | FUNKY 1 | FUNKY 3 | MEMORY 1 | MEMORY 3 | PANEL |
| Rank II | STRING 2 | STRING 4 | BRASS 2 | BRASS 3 | BASS | CLAVICHORD 2 | HARPSICHORD 2 | ORGAN 2 | GUITAR 2 | FUNKY 2 | FUNKY 4 | MEMORY 2 | MEMORY 4 | PANEL |

## Source conflicts

1. **Tremolo/chorus switches (index 15).** The owner's manual lists an ON/OFF control and a
   tremolo-versus-chorus selection (HC row 15, O13,39). The adjustment procedure (printed p21, BA617) names three
   tabs, TREMOLO, CHORUS and ON/OFF, and calls for "TREMOLO and CHORUS tabs off, ON/OFF tab ON", a state in
   which neither effect tab is engaged; it also measures OE1 "with TREMOLO on". The two readings agree on an
   ON/OFF tab and separate TREMOLO and CHORUS tabs; they disagree on whether TREMOLO and CHORUS are an exclusive
   pair. The host parameters (`chorus_on`, `chorus_mode` = CHORUS/TREMOLO) can only represent the owner's
   reading. The editor shows the three tabs; TREMOLO and CHORUS act as the exclusive pair stored in
   `chorus_mode` (pressing the lit one keeps it). The "both off with ON/OFF on" and "both on" states are not
   representable without a new engine parameter (ws4 owns OE1/OE2).
2. **Upper-left area (index 46).** Evidence: the upper-left element is the hinged block-diagram lid over the
   memories. Pre-ws7 editor: a live software signal-flow display there, and the lid behind a separate key. The
   improvement plan also says the channel rows should "span the panel"; on the hardware they span the panel to
   the right of the lid, which is what the editor now does with the lid restored to the upper left.
3. **Keyboard contour (index 9-10).** Evidence: Panel 2 (the front strip with BRILLIANCE, RESONANCE, VOLUME and the
   tone selectors). Pre-ws7 editor: the deck's right block. Now on PN2, after RESONANCE.
4. **Foot group.** Evidence: the assigners and EXP / EXP-WAH are on Panel 3 with the sustain and portamento
   controls (left of the keyboard); the jack and the output switch are on Panel 5. Pre-ws7 editor: all six in one
   front-panel block at the right. Now split by panel: assigners and EXP / EXP-WAH left, jack and output right
   under a rear-panel label.
5. **Global lever direction (index 7, 8 and "most" global amounts).** Evidence: increase toward the player
   (HR; HC row 8 "downwards physical increase"). Editor: every lever increases upward. **Not changed in this
   pass:** which global levers invert is not itemised in any reachable source ("most"), and inverting a lever
   changes its drag behaviour, which belongs to the ws8 interaction work. Recorded as an open item.
6. **Tone name "E. Piano" vs "ELECTRIC PIANO".** The core table abbreviates; S1/O37 print ELECTRIC PIANO. The
   editor prints the hardware wording; core status text is unchanged.
7. **GENERAL LOW shunt and RIGHT HIGH series values** (OLR) concern the output network, not the panel; listed
   there.

## Layout table

The main view is laid out from one table, `src/plugin/PanelLayout.h`, in the 1600 x 1000 design space. Its
blocks follow the areas above:

| Block | Design rectangle (x, y, w, h) | Contents | Basis |
| --- | --- | --- | --- |
| Toolbar | 48, 0, 1504, 64 | software toolbar (ws8 replaces it) | software |
| Memory lid (closed) | `geo::lidFace` = 52, 74, 288, 378 | index 46, block-diagram lid | D placement; I proportions |
| Channel rows I, II | `geo::rowFrame(channel)` = 348, 74 / 266, 1204, 186 | index 20-45 | D order; I spacing |
| Front strip (PN2) | `geo::frontStrip` = 48, 460, 1504, 166 | index 2-12, 16, 18 | D board membership; U group order (pre-ws7 order kept) |
| Deck left (left-hand panel) | `geo::deckLeft` = 48, 640, 492, 104 | index 13-15, 17 and the on-screen pedal | D |
| Deck centre | `geo::ribbonRect` = 556, 700, 860, 36; `geo::nameplate` = 826, 643, 320, 38 | index 19 | D |
| Deck right (rear jack panel) | `geo::deckRight` = 1428, 640, 124, 104 | footswitch jack, pedal contact, output HIGH/LOW | D board; I rear |
| Keybed | 48, 748, 1504, 252 | 61 keys | software proportions |

Exact positions, spacing and proportions are declared inferences sized to fit 1600 x 1000 and 1120 x 700 with
the legend floor; replace them with S2 measurements when the drawing can be read.

## Open items

- Read S2 and the owner's pp6-23 directly (add S2 to `scripts/inspect_circuit_sources.py` and run
  `circuit-inspection.yml`) to settle: group order along PN2, positions of #2, #12, #16 and #18, lever
  directions, unlisted cap colours, wave-switch form, tone-tab plastic colours, lid proportions.
- Tremolo/chorus: decide between the owner and adjustment readings; a new parameter would be needed for the
  adjustment reading.
- Global lever inversion (conflict 5).
