# ws6-library — tones, memories, the curated library, Program Change, A/B and randomise

Workstream report, 2026-10-31. Branch `worktree-agent-ac28cc3ac9758547d`, based on
integration commit `6719ce4`. Plan steps: Phase 4.1–4.4 and the Phase 6 editing items
(A/B compare, randomise within original ranges) plus the library UX items of 5.4.
No hardware was measured anywhere in this workstream; every law below is either
traced to a service-manual page or labelled a declared inference.

| Step | State | Where |
| --- | --- | --- |
| 1. 22 tone tabs: 25 unknown cells, BA driver and diode boundaries | done (all 25 cells explicitly hypotheses, none newly transcribed; refinement is an opt-in versioned path) | `FactoryToneHypotheses.h`, `SourceFactoryTones.h` |
| 2. Memory panels: loading and switch states for M1–M4 and PANEL | done as a model + tests; not wired into audio | `core/MemoryPanels.h`, `reference/MemoryPanelNetwork.h` |
| 3. Curated factory library (~300) and Legacy Library | done (301 programs) | `core/CuratedLibrary.inc`, `CuratedLibraryLevels.inc`, `FactoryPresets.*` |
| 4. MIDI Program Change (+ bank select) | done | `PluginProcessor.cpp`, `PluginProcessorLibrary.*` |
| 5. Library UX: audition, prev/next through the filter, Legacy section, category/tag filters | done | `PresetBrowser.*` |
| 6. A/B compare, randomise within original ranges (processor API + tests) | done | `PluginProcessorLibrary.*`, `core/Randomise.h` |

---

## Step 1 — the 22 tone tabs

### State: done. Every one of the 25 cells is an explicit, labelled hypothesis with its source pages; none could be newly transcribed.

### Native crops: what happened

- Direct download of the public service-manual pages (`www.therogoffs.com`,
  `therogoffs.com`, `web.archive.org`, `www.goltsynth.com`, `dragonflyalley.com`)
  through the session proxy failed: `CONNECT tunnel failed, response 403` for every host.
  `scripts/inspect_circuit_sources.py` therefore cannot run here.
- The GitHub Actions API is reachable. Earlier `circuit-inspection.yml` runs that hold
  the native T51–T54 schematic and board crops exist (schematics: run `40790769942`,
  jobs `123175135239/…934/…110/…114`; boards: run `40801679102`, jobs
  `123210997016/…022/…002/…996778`). Their logs are served from
  `productionresultssa30.blob.core.windows.net`, which the proxy also refused (403),
  and the API's text path cannot carry the multi-megabyte base64 crops into a file.
- Dispatching the workflow for new pages would need a workflow-file change on a pushed
  branch (its matrix currently lists only `oe55, oe56`); pushing is outside this
  workstream's permissions, and `gh` has no valid token in this session.
- Result: the re-examination rests on the recorded native inspections (hashes and crop
  boxes in `PRESET_NATIVE_MATRIX_REVIEW.md`, `PRESET_FULL_MATRIX_AUDIT.md`,
  `PRESET_PCB_UPDATE_REVIEW.md`, `RANK_II_PRESET_TRANSCRIPTION.md`).

### Re-examination of the 25 cells

Recorded as `factory_hypotheses::review` (id `ws6-cell-review-2026-10-31`) in
`src/core/FactoryToneHypotheses.h`. The frozen v1 manifest (median hypothesis,
fingerprint `0xe5f877a025fc52c6`) is unchanged, so audio and V1/V2 numerics are
unchanged. Each cell carries its schematic page, board page, outcome and a
*candidate* reading used only by the new "visible candidates" sensitivity scenario
(`source_factory::visibleCandidateScenario`). The candidate is the first visible
alternative already listed for the cell — a convention, not a preference.

| Cell | Pages (schematic / board) | Reason held | Outcome | Candidate |
| --- | --- | --- | --- | --- |
| T51/A/20 | S31 / S32 | 1.2K beside a revision triangle | held (revision) | 1.2K |
| T51/A/25 | S31 / S32 | overdrawn diode | held | diode |
| T51/C/25 | S31 / S32 | marked 10K / bypass topology | held | 10K |
| T51/E/22 | S31 / S32 | 12K revision applicability | held | 12K |
| T51/E/26 | S31 / S32 | marked/overdrawn diode | held | diode |
| T51/F/22 | S31 / S32 | overwritten 3.6K/12K | held | 3.6K |
| T51/F/25 | S31 / S32 | overdrawn 5.6K/diode | held | 5.6K |
| T51/F/26 | S31 / S32 | marked diode | held | diode |
| T52/A/26 | S33 / S34 | marked/overdrawn diode | held | diode |
| T52/B/9 | S33 / S34 | crossed 12K/10K | held | 12K |
| T52/C/22 | S33 / S34 | overwritten former value/12K | held | 12K |
| T52/E/20 | S33 / S34 | 1.2K revision applicability | held | 1.2K |
| T53/A/20 | S35 / S36 | 1.2K revision applicability | held | 1.2K |
| T53/A/25 | S35 / S36 | overdrawn diode | held | diode |
| **T53/B/14** | S35 / S36 | 25K-looking glyph (readability) | **board candidate** | **75K** |
| T53/B/20 | S35 / S36 | 1.2K revision applicability | held | 1.2K |
| T53/C/20 | S35 / S36 | 1.2K revision applicability | held | 1.2K |
| T53/E/23 | S35 / S36 | crossed 16K/22K | held | 16K |
| T53/E/26 | S35 / S36 | crossed diode | held | diode |
| T53/F/22 | S35 / S36 | 12K revision applicability | held | 12K |
| T53/F/26 | S35 / S36 | crossed diode | held | diode |
| T54/A/26 | S37 / S38 | crossed diode | held | diode |
| T54/C/22 | S37 / S38 | crossed 8.2K/12K | held | 8.2K |
| T54/C/25 | S37 / S38 | crossed 56K/diode | held | 56K |
| T54/C/26 | S37 / S38 | crossed diode | held | diode |

T53/B/14 (String 4 VCF decay): board page S36 native r0c0 prints **75K(39K)** at the
footprint provisionally traced to B/14. The T51 and T53 boards share paired artwork;
outside values match T53 and parenthesised values T51 at three classified pairs —
C/12 20K(13K), A/9 750(1K) and B/14's own parenthesised 39K, which equals the
classified T51/B/14. Evidence class: **declared inference** (no printed legend for the
pairing). Alternatives: the schematic's 25K-looking glyph; the v1 median (x = 0.502).
The cell stays `unknown` in `HardwareControl.h` until a second reader traces the
footprint (gate unchanged from `PRESET_PCB_UPDATE_REVIEW.md`).

Visible-candidate sensitivity (`CS80_BOUNDARY_VERBOSE=1 build-core/cs80_source_factory_boundary_tests …`,
ideal V2 source volts, median → candidate): String 1 out20 5.00→8.93 V, out25 0→10 V;
Brass 1 out25 0→5.00 V; E. Piano out22 10→4.55 V; Clavichord 1 out22 10→7.35 V, out25 0→6.41 V;
Organ 1 out9 6.81→4.55 V; Guitar 1 out22 10→4.55 V; Funky 3 out20 5.00→8.93 V;
String 2 out20 1.96→8.93 V, out25 0→10 V; String 4 out14 5.02→1.18 V, out20 1.96→8.93 V;
Brass 2 out20 1.96→8.93 V; Bass out23 3.12→3.85 V; Clavichord 2 out22 10→4.55 V;
Guitar 2 out22 10→5.49 V, out25 0→1.52 V (diode cells that already sit at x = 1 do not move).
The 1.2K VCA-sustain revisions (out20) are the largest audible lever: sustain ratio
0.20/0.50 → 0.89.

### BA driver impedance and diode I–V: versioned refinement

`source_factory::compileToneRefined(rank, selector, out, BoundaryModel)` — **opt-in;
the engine and `compileTone()` keep the ideal 10 V / 0 Ω source and ideal diode**, so
the seven complete tones' V1 numerics are byte-identical (checked with the V1 probe
after every edit, and by `scripts/check-compat-gates.sh`). Provenance of a refined tone
drops `Bridge::idealSourceAndDiode` and records `boundaryVersion = 1`.

Laws and evidence:

| Element | Law | Evidence class |
| --- | --- | --- |
| Selected column driver | 2SC458 NPN emitter follower, collector on +10.6 V (BA27/BA28) | topology: service page BA27/BA28 |
| Nominal driver state | saturated against the rail: 10.45 V (Vce(sat) 0.15 V), 20 Ω | **declared inference** — selector-line voltage not traced. Alternative `forwardActiveBoundaryV1`: 8.95 V, 50 Ω (selector at +10.6 V, base 9.64 V Thevenin) |
| Shared emitter | all rows of the selected column draw from one emitter: joint solve (`solveColumn`) | topology: BA27 ("all selected matrix branches draw from the same emitter") |
| Isolation diode | 1S1555 (T51/T53/BA28 notes) as Shockley: Is 2.52 nA, n 1.752, Rs 0.568 Ω, Vt 25.852 mV | part: service notes; **law: declared inference** (1N4148-class surrogate, no 1S1555 curve obtained). Alternatives `lowDropDiodeBoundaryV1` (Is 10 nA, n 1.6), `highDropDiodeBoundaryV1` (Is 1 nA, n 2.0) |
| Bus | one assembled 10 k shunt (unchanged) + the compiled receiver conductance at the reference point (rows 7/9: 8 cutoff receivers, 8/10: 8 Q ports, 22: 8 LC ports) | existing sourced graphs |
| Compiled path | per-row Thevenin linearised at the reference operating point; emitter-node impedance = Rout ∥ every other conducting row | model construction |

Truth vs compiled (`cs80_source_factory_boundary_tests`, tolerances pre-registered in
`tests/parity/tolerances-v2/ws6-library.json`, commit `b49cf89`, before evaluation):

| Gate | Tolerance | Measured (22 tones) |
| --- | --- | --- |
| reference-point bus volts | 1e-9 V | 1.8e-15 V |
| cutoff buses over the drive range (all voices z ∈ [−3, 3], velocity/pressure ∈ [0, 1], 64 trials/tone) | 1e-3 V | 2.7e-4 V |
| Newton residual | 1e-12 A | 3.8e-14 A |
| linearised impedance = truth tangent (added after mutation check, see below) | 1e-6 relative | pass |

Sensitivity versus the ideal V2 boundary (same tool, `CS80_BOUNDARY_VERBOSE=1`): the
shared emitter sags 0.14–0.31 V; LP cutoff buses fall by 0.03–0.42 V (at the declared
1 V-per-octave alignment slope: up to −0.42 octave, String 3), HP buses by up to 0.26 V,
AL by up to 0.40 V, level buses 10.0 → 9.55–9.68 V. The forward-active driver
alternative is the dominant uncertainty (LP buses another 0.6–1.7 V lower); the diode
corners move buses by ±0.1 V. Wiring the refined path into audio would be an
intentional circuit-engine change for factory-tab selections (V2 re-pin) and is left to
the integrator's decision; the cutoff alignment offsets would not need refitting (they
are defined in the bus domain).

Open limitation: the Q rows (8/10) are gated only at the reference point (master RES
source 0 V). With RESONANCE driving their shared node up to 10 V, a weak matrix branch
can be reverse-biased, which neither the ideal V1 model nor a linearised Thevenin
represents.

### Tests and mutation checks (step 1)

`tests/SourceFactoryBoundaryTests.cpp` (`cs80_source_factory_boundary_tests`):

| Mutation (temporary, restored) | Result |
| --- | --- |
| Thevenin not referred to the loaded operating point | 39 failures (reference-point gate) |
| diode dynamic resistance omitted | **not caught** by the 1 mV drive gate (0.6 mV effect); the tangent identity check was then added → 39 failures |
| other rows ignored in the emitter-node impedance | caught (tangent identity) |
| shared emitter not solved | caught ("shared emitter sags") |
| receiver load ignored when compiling (Q rows) | 36 failures |

The review record is checked too: all 25 cells still `unknown` in `HardwareControl.h`,
each with hypothesis, pages and candidate; only T53/B/14 has a board candidate; the
three paired-artwork consistency pairs hold; the candidate scenario overrides exactly
each tone's unknown cells.

---

## Step 2 — memory panels (M1–M4 and PANEL)

### State: done as a compiled model with a truth model and switch/loading tests. Not wired into the plugin's memory recall (which stays the digital programmer of `MEMORY_AUTOMATION.md`).

Model (`src/core/MemoryPanels.h`, control-rate) and truth
(`src/reference/MemoryPanelNetwork.h`, offline, analytic-Jacobian Newton over the full
network: 3 rails, 66 wipers, the selected emitter and 24 buses, plus every inactive
source branch):

| Element | Law | Evidence class |
| --- | --- | --- |
| Selection | acts on supply endpoints: BA I12/I13/I14 drive the selected memory/PANEL rail (rank I: M1, M3, PANEL; rank II: M2, M4, PANEL); factory tabs drive their column | BA27/BA28, MBK63 (`PRESET_BA_CUTOFF_BRANCHES.md`) |
| Pot | each control a pot between that rail and the reference endpoint | MBK63 |
| Wiper path | through an isolation diode to the row bus; cutoff rows 7/9 first buffered by an NJM4558 follower (memory diode after the feedback point; PANEL IC5/IC6 into the primed inputs, T51 alternate-input diode) | BA27/BA28, MBK63, `PRESET_AL_RECEIVER_GRAPH.md`, `RANK_II_RECEIVER_PORTS.md` |
| Pot value / taper | 50 k linear, 1 Ω ends | **declared inference**; alternatives 10 k, 100 k |
| Reference endpoint, follower | ideal 0 V; unity gain, 1 Ω output | **declared inference** |
| Rail drivers | the same 2SC458 follower model as step 1 (`nominalBoundaryV1`); unselected emitters at 0 V | declared inference (as step 1); alternative: open emitter — equivalent here, the rail then sits at the reference through its pots |
| Switch rows 4/5 | logical: only the selected source decides (factory: matrix direct/open; memory/PANEL: the bank's switch) | M-card G1/G2 chains (`RANK_II_RECEIVER_PORTS.md`) |
| Compiled path | selected source only; shared rail by 4-pass fixed point; each row's diode branch solved exactly | model construction |

Truth vs compiled (`cs80_memory_panel_tests`, pre-registered tolerances, both ranks,
selectors 0/3/7/10 (tabs), 11/12 (memories), 13 (PANEL), 4 random pot sets each):

| Gate | Tolerance | Measured |
| --- | --- | --- |
| bus volts, every analog row | 2e-3 V | 2.8e-4 V |
| unselected source branch current (diode leakage) | 1e-8 A | 2.5e-9 A |
| truth Newton residual | 1e-12 A | 6.5e-15 A |

Switch tests: memory and PANEL pots cannot move a factory tab's buses (< 1 µV);
unselected rails stay at the reference; factory switch rows come from the matrix column;
memory/PANEL waveform switches follow the selected bank; rank/bank pairing matches the
plugin (rank I memories 1/3, rank II 2/4).

Loading results (compiled, all sliders at mid travel, memory 1): the selected rail sags
to 10.31 V (50 k pots; 9.90 V with 10 k); the unbuffered AL bus reaches only 2.06 V
against the buffered LP bus 4.55 V, because the pot's wiper impedance (12.5 k at mid
travel for 50 k) works into the 10 k bus shunt. Pot-value sensitivity: AL/VCA-sustain
bus 3.53 V (10 k), 2.06 V (50 k), 1.34 V (100 k). This S-shaped, sagging slider law is
the main audible consequence a future integration would have to adopt; it depends
strongly on the undocumented pot value.

Mutation checks (all caught, see "Mutation log"): compiled rail ignoring branch
currents, pot wiper impedance ignored, every rail energised (no selection switching),
switch rows taken from the bank for a factory tab.

---

## Step 3 — curated factory library on the circuit engine

### State: done. 301 programs; the 222-program bank is now the read-only Legacy Library.

- `src/core/CuratedLibrary.inc` (included by `FactoryPresets.cpp`; no core source-list
  change) defines the programs with a small fluent helper and eleven technique bases.
  Every value is chosen by hand for the technique the program demonstrates; nothing is
  random or generated. Categories (counts): Brass 33, Strings 31, Pads 32,
  Polysynth 25, Keys 25, Organ 15, Winds 14, Plucked 17, Bass 25, Leads 32, Bells 23,
  FX 24, Sequences 4, Init 1 — 18 of these are modern performance programs tagged
  `modern` (FX slots, LFOs, arpeggiator; inert in PURE CS-80).
- Techniques: brass swells with the VCF EG (IL below / AL above the cutoff) and
  after-touch brilliance; resonance swells; PWM string ensembles; footage layering of
  the two ranks (16'/8'/5⅓'/4'/2⅔'/2'); ring-modulated bells (sine voices through the
  ring modulator, ring-envelope sweeps); SUB-modulated effects with the original six
  SUB functions (random/sample-and-hold, square trills, saw sirens, tremolo on the VCA);
  touch-expressive leads (initial/after brilliance and level, pressure vibrato through
  SUB AT VCO, initial pitch bend, glide/glissando, ribbon); keyboard contours.
  The cinematic film-score lead/pad technique is described generically ("Noir Swell
  Brass", "Noir Lead", "Neo-Noir Pad"); no trademark, film, artist or product name
  appears in a program name (checked by the audit).
- Per-program policy: every curated program sets `source_performance = 1` (circuit
  engine) and the original SUB (`sub_original_mode = 1`). The engine default flip is
  untouched (ws2).
- Level matching: `CuratedLibraryLevels.inc` holds only VOLUME, generated by
  `cs80_library_audit_tests --calibrate` (target loudest-50 ms RMS −18 dBFS, −22 dBFS for
  single-line Leads/Winds/Bass/Sequences; limited to phrase peak ≤ −3 dBFS and 8-voice
  stress peak ≤ −1.5 dBFS). Re-run after any voicing or engine gain change (e.g. ws2 or
  ws5 output/FX gain work). After merging ws4 (OE1 tremolo, ring-modulator offsets and
  feedthrough, OE2 chorus, PRA cell) the uncalibrated table measured stress peak −0.3 dBFS
  (Tremolo Pad) and RMS up to −15.7 dBFS; the table was regenerated (63 entries moved) and
  the audit is back to the values below.
- Legacy Library: `factoryPresets()` is the shipped bank, byte-identical; pinned by an
  FNV-1a digest `0x3628dd9acc109b2a` over names, categories, descriptions and values of
  indexes 0..`P_sourcePerformance` (computed from `6719ce4` before any change).
  Parameters appended by other workstreams stay at their (neutral) defaults in legacy
  programs, and that is checked too.
- Program numbering (`programLibrary()`): host programs 0..221 keep their shipped
  meaning (legacy, original order), the curated library follows at 222..522, and new
  curated programs are only ever appended. Old sessions/presets therefore recall a
  legacy program by its old index and name without remapping, and the existing host
  tests that pin "program 0/22 recalls the legacy model" stay unchanged. (A
  curated-first order was considered; it would have required a state remap and
  changes to legacy-pinned tests.)

Library audit (`tests/LibraryAuditTests.cpp`, `cs80_library_audit_tests`, ~3–4 min,
44.1 kHz circuit engine; criteria fixed before measurement): legacy digest, program-list
order, MIDI mapping, every parameter in range, switches 0/1, discrete parameters
integral, policy and physical setup, categories and tag vocabulary, unique names that do
not shadow legacy names, no trademarks, modern-layer use ⇔ `modern` tag, an audible
source per program, pairwise normalised-L1 distance ≥ 0.08, and rendering of every
program (category phrase, velocity 0.9, pressure ramp; and an 8-note full-velocity,
full-pressure stress chord).

| Measure | Criterion | Result (301 programs) |
| --- | --- | --- |
| phrase peak | ≤ −1 dBFS | max −4.3 dBFS |
| 8-voice stress peak | < 0 dBFS | max −1.5 dBFS |
| not silent: loudest 50 ms RMS | ≥ −42 dBFS | min −29.9 dBFS |
| level window | −30 … −12 dBFS | −29.9 … −18.0 dBFS |
| closest pair | ≥ 0.08 | 0.097 (Flute Organ 8+4 / Organ Vibrato) |

Mutation checks (data gates with `CS80_LIBRARY_AUDIT_DATA_ONLY=1`, level gates full):
legacy value changed → digest fails; ring speed 520 Hz → range fails; policy unset →
301 failures; duplicate program → distinctness fails; modern program untagged → tag
check fails; trademark in a name → name check fails; calibrated volume 0.05 → silence
and level window fail; level CV driven to the 2.6 clamp → phrase peak (−0.2 dBFS) and
level window fail.

UI smoke: one loop in `src/tools/ui_snapshot.cpp` ("every shipped factory program
explicitly retains legacy SUB behavior") now applies to programs 0..221 and asserts the
opposite (original SUB on the circuit engine) for 222+. Intentional change, commented.

---

## Step 4 — MIDI Program Change

### State: done.

- `getNumPrograms()` = 523, `getProgramName`, `setCurrentProgram` cover the whole list.
- Program Change is handled sample-accurately in `processBlock` (the 0xC0 filter is
  lifted): the program's values are written through parameter objects resolved once in
  `initialiseLibrary()` (no allocation, no ID strings on the audio thread; the same
  path as host automation write-back), selectors return to PANEL, physical setup and
  isolation are untouched, the control frame is re-read at the event position, and a
  policy change (legacy ↔ circuit) is handled exactly like the block-boundary
  transition (engine reset, keyboard handoff revoked, the pre-transition UI batch
  dropped). The name and index publish through one atomic (`getPresetMetadata`), the
  host display updates from the message thread (`AsyncUpdater`).
- Bank select: CC0 (MSB) 0 = curated library, 1 = Legacy Library; CC32 (LSB) = page of
  128 programs; MIDI 1.0 semantics (each byte holds until sent again). So bank 0/0 PC 0
  is the first curated program, bank 1/0 PC 22 the old program 22, bank 0/1 PC 5 curated
  133. Unassigned numbers change nothing.
- MPE: only the master channel (1) selects banks and programs. PURE CS-80 ignores bank
  select and Program Change (an interface extension; the original has no MIDI), like
  MPE and CC11.
- Host tests (`tests/ProgramLibraryHostTests.h`, run in the UI smoke): program list,
  legacy recall by old index/name (including a pre-library state), PC in bank 0, bank
  MSB/LSB paging, persistence of bank select, unassigned numbers, same-engine PC keeping
  a held note with new values from the event position, MPE master/member channels,
  PURE CS-80, rename after PC.

## Step 5 — library UX (`PresetBrowser`)

### State: done.

- **Legacy Library section**: the scope chips are ALL PATCHES (factory library + user),
  FAVOURITES, FACTORY LIBRARY, LEGACY LIBRARY, USER. Legacy rows show slots L001–L222
  and ORIGIN "LEGACY PROGRAM/STUDY"; they are read-only (cannot be deleted; saving makes
  a user patch). Opening the library with a legacy sound (an old session) switches to
  the Legacy section once so the loaded patch is visible.
- **Category / tag filtering**: curated categories map to instrument chips (new POLY and
  BELLS types besides the existing ones); the third facet switches between STYLE (derived
  character) and TECHNIQUE (library tags: AFTERTOUCH, VELOCITY, SWELL, GLIDE & BEND,
  RING MOD, SUB MOD, FOOTAGE, MODERN) with live counts; tags are searchable. The chip
  column tightens its pitch (≥ 33 design units) instead of overflowing.
- **Audition**: an AUDITION key between PREV and NEXT; when on, choosing a row (click,
  arrows, PREV/NEXT) loads it at once; filtering, searching and reopening never load;
  REVERT still returns to the sound loaded when the library opened. Remembered with the
  favourites (`#audition=on|off`, `#facet=technique|style`).
- **Shared art** (after merging ws7): the browser now uses `ArtCommon.h` for the wrinkle
  tile (`panelTile`), `fillRelief`, `textWidth` and the embedded panel typeface
  (`displayFont`, Barlow Condensed) instead of its own copies; its derived relief overlay
  and monospace slot font stay local.
- **Prev/next through the current filter, user patches included**:
  `PresetBrowser::loadAdjacent(±1)` steps from the sounding patch through the current
  filter (scope, types, facets, search) and loads, wrapping at the ends; it works while
  the library is hidden, for the toolbar.

## Step 6 — A/B compare and randomise (processor API)

### State: done.

- `abToggle()`, `abCopyCurrentTo(int slot)`, `getAbSlot()`, `isAbSlotStored(int)`
  (message thread). A snapshot is the whole sound: panel values (engine policy
  included, so A/B can compare the legacy and circuit engines), both tone selectors, all
  104 memory controls and the preset name/index. Physical setup and isolation stay live.
  The first toggle copies A into the empty B, so it is silent. The inactive slot is saved
  with the session as `<AB_COMPARE version="1">`; a state without it clears both slots;
  it is stripped from the live APVTS state and never read from user preset files.
- `randomiseWithinOriginalRanges(uint32 seed)` applies `cs80::library::randomiseWithinOriginalRanges`
  (`src/core/Randomise.h`) to the current sound, names it "Random XXXXXXXX", and leaves
  it a PANEL sound. Rules:
  - **R1** one splitmix64 stream per seed, fixed draw order (no `<random>` distributions;
    exp2/log2 may differ in the last float bit across C libraries).
  - **R2** every rank has an audible source: saw, pulse, both, or sine ≥ 0.6.
  - **R3** open passband: LPF ≥ 2.5 × HPF and ≥ 250 Hz; HPF 20–600 Hz.
  - **R4** audible duration: attack ≤ 1.5 s; sustain ≥ 0.3, or decay ≥ 0.4 s for percussive draws.
  - **R5** level: LEVEL 0.5–0.85 and LEVEL + initial + after level ≤ 1.45.
  - **R6** resonance: rank RES ≤ 0.6, RESONANCE ≤ 0.55, combined Q control ≤ 0.85.
  - **R7** ring modulation in 25 % of draws, amount ≤ 0.35.
  - **R8** only original panel controls move (`isRandomisedParameter`): policy, VOLUME,
    physical setup, tuning, ribbon, sustain, glide, pan, filter bypass, the modern layer
    and legacy SUB controls are copied unchanged.
- Tests: `tests/RandomiseTests.cpp` (400 seeds: determinism, ranges, every rule; 64
  renders on circuit and legacy engines: peak ≤ −1 dBFS, never below −45 dBFS) and the
  A/B and randomise sections of `tests/ProgramLibraryHostTests.h`.
- Mutation checks: R2 (silent sine allowed) → 59 failures; R3 removed → caught; R5
  removed → caught; R8 (volume randomised) → caught; R6 cap alone removed → **not
  caught**, because the sub-ranges already guarantee it (0.6·0.8 + 0.55·0.6 = 0.81 <
  0.85; the cap is a backstop); widening RESONANCE to 1.0 without the cap → caught.

---

## Mutation log (all temporary, restored byte-for-byte by script)

| Gate | Mutation | Caught |
| --- | --- | --- |
| library audit | legacy value 0.68 → 0.67 | yes |
| library audit | ring speed 520 Hz | yes |
| library audit | circuit policy unset | yes (301) |
| library audit | duplicate program | yes |
| library audit | modern program untagged | yes |
| library audit | trademark name | yes |
| library audit | calibrated volume 0.05 | yes |
| library audit | level CV to its clamp | yes |
| randomise | R2 / R3 / R5 / R8 / widened R6 | yes |
| randomise | R6 cap only | no — implied by sub-ranges (documented) |
| boundary | operating-point term, emitter-node coupling, shared emitter not solved | yes |
| boundary | diode dynamic resistance | only after the tangent identity check was added |
| boundary | receiver load ignored (Q rows) | first caught by the reference gate; after the joint-column rewrite the truth shared that load, so an independent check against the existing graphs' load was added → caught |
| memory panels | compiled rail ignores branch currents | yes (bus error 8.6–15 mV > 2 mV) |
| memory panels | pot wiper impedance ignored | yes (errors up to 2.7 V) |
| memory panels | every rail energised (no selection switching) | yes (inactive-current and bus gates) |
| memory panels | switch rows from the bank for a factory tab | yes |
| Program Change (host) | bank MSB ignored | yes (2 UI-smoke failures) |
| A/B (host) | toggle without capturing the active slot | yes (3 UI-smoke failures) |

## Intentional UI change (golden images)

The toolbar's program counter reads "OF 523" instead of "OF 222" (ws7's main-view
goldens). After merging ws7 the 1600x1000 golden failed on one 8x8 block (mean 8.59 >
8.0); the only differing pixels are that counter (bounding boxes 649,12–671,19 at
1600x1000 and 451,7–470,13 at 1120x700, checked against the previous goldens), so both
goldens were refreshed with `CS80_REFRESH_GOLDENS=1`.

## Intentional circuit-engine sound changes

None. No engine, DSP or default-render change: the curated programs are new data that
select the existing circuit engine; the boundary refinement and the memory-panel model
are opt-in/offline.

`scripts/check-compat-gates.sh` after merging the integration branch (`bd524f9`):
`legacy default`, `legacy tails` and `v1 circuit numerics` byte-identical; `v2` differs
from the `f6b126d` pin because of ws4's documented circuit changes, and the v2 probe
output of this branch is byte-identical to the integration branch's own (`bd524f9`,
built from `git archive`), so ws6 adds no v2 difference. Before the merge all four gates
were byte-identical.

## Verification (2026-10-31, after the merge)

- Core: 52/52 (`ctest --test-dir build-core -j2`), including `cs80_library_audit_tests`,
  `cs80_randomise_tests`, `cs80_source_factory_boundary_tests`, `cs80_memory_panel_tests`.
- Plugin: builds (`linux-release`); `cs80_ui_smoke` (with the program-library host tests
  and ws7's goldens) and the four locale tests pass.
- Plugin mutation checks of the host tests: Program Change ignoring bank MSB → 2 UI-smoke
  failures; A/B toggle without capturing the active slot → 3 failures.

## New parameters

None (no core or host parameters added; AU hint 13 unused).

## Entries for HARDWARE_COVERAGE.md (2026-10-31)

- *Two tone-selector rows (3)*: 2026-10-31 ws6 — 25 unknown cells re-examined from
  recorded native inspections (new crops unreachable: proxy 403); all remain labelled
  hypotheses with pages (`factory_hypotheses::review`); T53/B/14 gains a board-artwork
  candidate 75K (declared inference). Versioned BA 2SC458 follower + 1S1555 Shockley
  boundary with a shared-emitter column solve (`compileToneRefined`, opt-in, V1
  unchanged); truth-gated to 2.7e-4 V.
- *Four memory panels (46)*: 2026-10-31 ws6 — electrical model of the M1–M4 and PANEL
  source networks (selected-endpoint rails, pots, followers, isolation diodes, shared
  bus) and switch states, truth-gated to 2.8e-4 V; pot value is a declared inference;
  not yet driving audio.
- *Factory programs / library*: 2026-10-31 ws6 — 301-program curated circuit-engine
  library replaces the 178 generated variants in the user-facing library; the 222
  shipped programs are the read-only Legacy Library at their old host indexes; audit
  test for level, silence, duplicates and ranges.
- *MIDI*: 2026-10-31 ws6 — Program Change with bank select (MSB library, LSB page),
  sample-accurate; inert in PURE CS-80.
- *Editing*: 2026-10-31 ws6 — A/B compare (session-persistent) and randomise within
  original ranges (rules R1–R8).

## Integration notes

- Memory editing: store/copy memory and copy rank are ws7's `cs80::panel::MemoryLid`;
  ws6 adds no memory editing functions (A/B snapshots read and restore the memory banks
  through the existing parameters only).
- Toolbar (ws8): wire PREV/NEXT to `PresetBrowser::loadAdjacent(-1/+1)` (the editor's
  existing prev/next in `PluginEditor.cpp` still steps through the whole host list,
  legacy first), A/B to `abToggle()` / `abCopyCurrentTo(1 - getAbSlot())`, RANDOMISE to
  `randomiseWithinOriginalRanges(seed)` (pass a fresh seed, e.g. from `juce::Random`).
  Undo (ws8) can wrap these calls; they use plain parameter writes.
- `getFactoryPresets()` now returns the 523-program list; the editor's INIT key searches
  it by name ("Init Patch", legacy) and still finds the legacy init.
- After ws2 flips the engine default, consider starting fresh instances on a curated
  program (e.g. "Init Circuit", host 522) — not done here because it changes the default
  render.
- After ws2/ws5 change output or FX gain, re-run
  `build-core/cs80_library_audit_tests --calibrate src/core/CuratedLibraryLevels.inc`
  twice and commit the table.

## Open items

- New native crops of S31–S38 (blocked here); second-reader trace of T53/B/14.
- Selector-line voltage (PN2) to decide saturated vs forward-active BA drivers — the
  largest uncertainty of the refined boundary (up to 1.7 V on LP buses).
- A 1S1555 I–V source; memory/PANEL pot values and taper; reference-endpoint wiring.
- Q-row diode cut-off under RESONANCE drive (both ideal and refined models are linear there).
- Wiring the refined boundary and the memory loading law into audio (circuit-engine
  sound change; integrator decision).
