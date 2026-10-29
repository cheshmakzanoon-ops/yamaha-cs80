# CS-80 factory preset completeness audit

Audit baseline: `f452586383af06db0b0d8c2284050a0cafdd3f18`, 2026-10-28.
Scope: original 22 factory tone circuits and their control-voltage interfaces.
This is an inventory of **known versus unresolved source data**, not a new
transcription or a claim of authentic preset sound. No physical CS-80 was measured.

## Implementation increment after this baseline

The independently reviewed source-domain table now contains547 classified cells
and25 explicit revision/readability exclusions across all22 tones ×26 outputs.
Board-local shunts are distinct: T51/T53 analog rows have10k, switch rows have none,
and T52/T54 have no local shunts. Companion-board and receiver loading are not
invented. This is transcription coverage, not electrical calibration or audible
preset equivalence. See PRESET_FULL_MATRIX_AUDIT.md for exclusions and evidence.

## What the original baseline implementation covered

`src/core/HardwareControl.h` and `tests/HardwareControlTests.cpp` contain
six sampled columns times eleven output rows = **66 classified cells**.
The existing ledger is their source snapshot. This audit checked agreement
between the ledger, implementation and test data; it has **not independently
re-read the original component marks**. Existing verified values are preserved.

The eleven sampled outputs are 7–15, 18 and 19. The sampled columns are
T51/A, T51/C, T51/D, T53/A, T53/C and T53/D.
The tests establish transcription consistency and conditional divider arithmetic;
their duplicated expected table is not an independent hardware measurement.

The full inventory is 22 tone columns × 26 named control outputs = **572 cells**.
Of these, 66 are classified and **506 are unresolved** in this implementation.
A cell is an inventory address, **not necessarily a physical resistor or even an
analog divider**. Switch rows, absent branches and alternate sources require
separate topology classification. Thus 66/572 is not a sonic-fidelity score.

## Exact column coverage

Tone names/column correspondence below are inherited from
[the existing ledger](PRESET_CIRCUIT_LEDGER.md), which attributes them to the
[original foldout](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg).

| Synth rank | Board | Columns with eleven classified outputs | Columns with no classified outputs | Classified / unresolved cells |
|---|---|---|---|---|
| I | T51 | A String 1; C Brass 1; D Flute | B String 3; E Electric Piano; F Clavichord 1 | 33 / 123 |
| I | T52 | none | A Harpsichord 1; B Organ 1; C Guitar 1; D Funky 1; E Funky 3 | 0 / 130 |
| II | T53 | A String 2; C Brass 2; D Brass 3 | B String 4; E Bass; F Clavichord 2 | 33 / 123 |
| II | T54 | none | A Harpsichord 2; B Organ 2; C Guitar 2; D Funky 2; E Funky 4 | 0 / 130 |

T52/F and T54/F are listed unused in the ledger; they are **not extra factory
tones** and are excluded from 572. The two memory choices and PANEL in each
rank are alternative control sources, also excluded from the factory-tone count.
They still must be traced to solve loading and switch-state interactions.

For all sixteen wholly untranscribed columns, outputs 1–26 remain unresolved.
For each of the six sampled columns, these fifteen outputs remain unresolved:

| Output | Control |
|---|---|
| 1 | Speed |
| 2 | PWM |
| 3 | PW |
| 4 | Square switch |
| 5 | Saw switch |
| 6 | Noise |
| 16 | VCF level |
| 17 | Sine |
| 20 | VCA sustain |
| 21 | VCA release |
| 22 | VCA level |
| 23 | Initial brilliance |
| 24 | Initial level |
| 25 | After brilliance |
| 26 | After level |

The remaining-cell arithmetic is 16 × 26 + 6 × 15 = 506.
No unresolved value may be replaced with `open`, zero or a neighboring tone's
value. In particular, all six currently sampled tones still lack waveform and
amplitude-release information: none is a complete circuit-derived voice.

## Exact source retrieval queue

The [original service-manual index](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/index.html)
was checked for these page identities. Original-image pixels were not exposed by
the browser fetch in this audit; native image inspection remains necessary.

| Page/source | Required evidence |
|---|---|
| [31 T51](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/31%20-%20T51%20Circuit%20Diagram.jpg) | All six columns × 26 outputs; diode direction, shared shunts and primed terminals |
| [33 T52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/33%20-%20T52%20Circuit%20Diagram.jpg) | Five active columns; companion-bus connections and unused-column status |
| [35 T53](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/35%20-%20T53%20Circuit%20Diagram.jpg) | Independent rank-II transcription, not copied rank-I data |
| [37 T54](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/37%20-%20T54%20Circuit%20Diagram.jpg) | Five active rank-II columns and shared buses |
| [27 BA](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/27%20-%20BA%20Circuit%20Diagram.jpg) | Selected-column driver, rail, transistor operating state and source impedance |
| [62 Panel 1](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/62%20-%20Panel%201%20Circuit%20Diagram.jpg) | Output-name mapping; potentiometer endpoints, wipers and alternate paths |
| [63 Panel 1 MKB](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/63%20-%20Panel%201%20%28MKB%29%20Circuit%20Diagram.jpg) | Memory wiring and isolation; trace, do not infer, primed-terminal destinations |
| [65 Tone Selector](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/65%20-%20Panel%202%20%28Tone%20Selector%29%20Circuit%20Diagram.jpg) | Factory/memory/panel selection and rank-column correspondence |
| [39 R51–52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/39%20-%20R51-52%20Circuit%20Diagram.jpg) | Connected control summing, source impedances, references and gain |
| [41 M](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) | Actual receiving pins, parallel loads and voice/rank fan-out |
| [42 M ICs](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg), [52 M ICs](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg) | Electrical control meaning and documented transfer anchors |

Board-wiring pages 28, 32, 34, 36, 38 and 40 should settle cross-board continuity
where schematic labels alone are ambiguous; retrieve links from the original
index rather than guessing a connector destination.

Each new transcription should retain source URL, source-byte SHA256, crop box
and crop SHA256, printed units, branch kind, diode orientation and independent
review status. Use board/column/output locators: the matrix does not provide an
individual resistor designator for every entry. Record unreadable marks as
unknown and request a native-resolution crop rather than trusting OCR.

## Why a complete resistance table still will not produce calibrated patches

The present solver permits one ideal selected-column voltage, a caller-supplied
constant diode drop, one 10 kΩ shunt and an optional linear load to ground.
It expressly excludes the actual BA source impedance, nonlinear diode I–V,
alternate sources and receiving circuits.

A loaded per-bus solve must establish:

1. **Driver:** actual BA selected output under the simultaneous loads of the
   selected tone. The printed +10.6 V rail is not proof of a 10.6 V column.
   A common driver can couple otherwise separate branch loads.
2. **Topology:** which factory, PANEL and memory paths conduct in each selection
   state, including primed terminals. Account for the one shared shunt, without
   inventing a second parallel 10 kΩ on the companion board.
3. **Receivers:** for every output, connector destination, number of connected
   voices, input impedance/current, reference potential, gain/polarity,
   and any control capacitor. Do not silently treat eight receiving inputs as
   one infinite-impedance input.
4. **Electrical domain:** carry volts through the actual intervening network to
   the relevant IC pin. A matrix bus is not automatically the effective cutoff
   voltage, envelope timing input, or normalized knob position.
5. **Transfer/calibration:** use the correct IC law and service setup. Separate
   synthesis rank I/II from the filter-IC component grading also called A/B/C
   “rank.” Preserve trim settings and documented tolerances.
6. **Dynamics:** validate note, velocity, aftertouch, release, global controls
   and selector changes, not just an idle DC patch.

Specific boundaries already recorded in [the circuit specification](CIRCUIT_MODEL_SPEC.md):
HP and LP have different summing paths; filter cutoff controls are loaded at
M-board nodes; Q endpoints do not establish the intermediate curve; timing
resistors are not envelope time constants; amplitude control distinguishes
linear from exponential inputs. These require receiver-specific solutions,
not one universal voltage-to-slider conversion.

A useful general formulation is nodal KCL with signed receiver currents:

`I_branch(Vcolumn, Vbus, Rs, diode) + I_alternate = Vbus/Rshunt + Σ I_receiver(Vbus, state)`

This is a bookkeeping equation, not a validated device model. A receiver tied to
a nonzero reference cannot be represented merely by a ground-load resistance.
Even a direct diode branch is not an ideal stiff source once driver impedance
and diode differential resistance are restored.

## Next bounded deliverables and acceptance gate

1. Inspect native T51/T53 pages first to independently recheck the existing 66
   entries and fill complete sampled columns, including waveform rows.
2. Independently transcribe T52/T54 and remaining columns. Expand the source
   matrix only for verified cells; preserve unknown/open/direct distinctions.
3. Produce a 26-output receiver table with node names, references, fan-out,
   passive elements and unresolved IC loads; trace selection separately.
4. Add voltage-domain loaded-network regression fixtures with explicit
   assumptions and source locators. Keep them offline until conversion is
   justified.
5. Derive all 22 preset electrical states; then compare service test points and
   authorized hardware recordings under documented settings before replacing
   provisional production voicings.

No production preset parameter is changed by this audit. Correct names and
passing software tests are not proof of a matching factory tone.

### Separate documentation correction

The keyboard-control subsection of the baseline preset ledger contains the old
LOW-reference 56 kΩ feedback / gain −1 claim. Subsequent native-source review
in the parallel contour work identified 15 kΩ feedback on both inputs.
This audit does **not** use that subsection to derive any preset quantity.
The ledger has now been corrected to the independently inspected 15 kΩ feedback
on both inputs, with the reference offset retained. See KEYBOARD_CONTOUR_EVIDENCE.md.
Do not propagate the obsolete claim into receiver calculations.
