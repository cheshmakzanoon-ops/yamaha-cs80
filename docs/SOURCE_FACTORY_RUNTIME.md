# All 22 factory tabs: source-model runtime integration

2026-10-30. Public-knowledge **nominal reconstruction**, not measured hardware
parity. This increment connects `SourceFactoryTones.h` to the existing isolated
engine and existing rank-local plugin selectors. It does not integrate the
separate reconstructed oscillator and does not change the 222-program bank.

## Selection and compatibility contract

`ControlFrame` submits effective `ParamValues`, the two existing hardware-source
selectors, and fixed model version 2 as one render-thread transaction. The plugin
still reads the same 26 programmer controls, with the same PANEL/memory/legacy
substitution. It captures each selector once alongside that effective snapshot;
APVTS remains the stored authority. No new host ID, range, default, program,
selector order or serialized shadow source is introduced.

Only `parity_mode && source_factory::isSourceDriven(route(...))` activates the
source audio adapter. All 22 factory selectors now use it, without another host
flag or a hidden legacy override:

- **Seven `source_complete` tones:** rank I selectors 1 String 3, 3 Flute,
  9 Funky 1; rank II selectors 3 Brass 3, 7 Organ 2, 9 Funky 2, 10 Funky 4.
- **Fifteen `source_inferred` tones:** the remaining factory tabs use the separate
  `FactoryToneHypotheses.h` manifest for exactly 25 excluded source cells. These
  are explicitly inferred, not newly verified or recovered source values.
- PANEL and all four memories retain their existing adapter. Isolation off
  retains the legacy factory-table adapter for every selector. A valid move to
  any non-source route clears active source controls; no cached tone leaks out.

`unknownCells` preserves the original evidence exclusions. For inferred tones,
`inferredCells == unknownCells` intentionally overlaps it, while `knownCells`
is their complement within the 26 outputs. The source table remains unchanged.
The manifest uses a frozen same-rank/same-output median excitation of classified
cells, excluding unknowns, with explicit basis addresses, exclusion reasons,
visible alternatives and a non-probabilistic `[0,1]` sensitivity domain. Nominal
runtime frames have `hypothesisOverrideMask == 0`; offline sensitivity scenarios
are not a hidden player setting. Hypothesis version 1 has fingerprint
`e5f877a025fc52c6`; source fingerprint remains `a95212770767733c`.

Changing a source does not reset oscillator phase, audio filter memories,
envelope value/stage/elapsed time, or performance controllers. Source parameters
own all 26 programmer destinations while selected; footage and original global
controls remain live. Isolation's existing forced filter enables, neutral pan,
modern-feature mask and mono-selector suppression still apply.

`setControlFrame` rejects an invalid selector, unsupported model version,
nonfinite parameter or compilation failure **before applying any part of the
frame**. `getControlFrameStatus()` reports the reason. Rejection preserves the
previous parameters, source identities, compiled maps and audio states. Finite
core parameters retain existing range clamping. The legacy `setParams` method
retains its old sanitization and explicitly means PANEL/no source identity;
resource `prepare` retains an accepted frame. Mode-toggle resets remain unchanged.

Existing restore normalization remains responsible for old/malformed saved data;
the core never substitutes a partially new frame after rejection. As documented
in the proposal, an isolated saved factory selector intentionally adopts the new
source model after upgrade; default-off legacy audio is the compatibility target.
The former seven source-complete tones are additionally byte-identical between
V1 and V2 in the reproducible numeric/audio corpus below. Passing model version 1
to the V2 frame API is rejected atomically, not used as an unautomated opt-out.

## Electrical/readout boundary

`ToneControls` are compiled only when a selected source address changes; repeated
frames reuse cached maps and route classification. No solver, allocation, lock or
exception is added to the audio path.

For each internal sample, source-selected filter-envelope states are advanced
first, then all eight distinct receiver drives enter the common FH/FL map.
`evaluateShared` evaluates the two-rank, 32-Q/16-LC network once. A non-source rank
retains all its represented receiver ports through the documented stiff nominal
bridge, but only source-selected ranks consume the new readouts. Shared RES/AM/KB
are the model's explicit ideal boundaries, not separately invented per-voice
masters. No finite-impedance cross-rank coupling is claimed where the nominal
shared source is stiff.

- Envelope targets use `envelopeStateFromVolts`; the stored coordinate stays `z`
  across source changes. The source readout alone uses `(5/3)*z` volts.
- Source cutoff consumes loaded per-voice effective CV, without the old `z`/`0.5z`
  HP/LP readout or generic initial/after-brilliance multiplier being added again.
- Source Q bypasses the old normalized rank/master resonance mixer.
- Source LC gain, amplitude envelope, filter-path gain, post-filter sine gain,
  rank mix and final engine scale are each applied once. Original global
  effective-domain modulation/keyboard transports remain, once.
- Source LC plus global level modulation is floored at zero, **not capped at the
  legacy 2.6 gain**. There is no per-tone normalization, output limiter, or CP3/CP5
  voltage fitting. The existing filter nonlinearity and output circuits remain.
- Oscillation still uses the existing oscillator with the explicitly inferred
  `dutyProxy`/PWM bridge. `WscControls` is not evidence of a physical comparator
  having been integrated here.

`getSourceToneInfo`, `getSourceCutoffReadout`, `getSourceSharedReadout` and the
compile counter are render-thread diagnostics. Cutoff/shared readouts are the
last internal sample, cleared on an accepted frame until rendering resumes;
`ToneControls.busVolts` is only its documented compile reference operating point,
not those current loaded buses.

## Inactive control state and MIDI panic policy

A VCA becoming inaudible does not electrically disconnect TP1. Therefore an
inactive **source-selected filter EG continues advancing**, and its source target
and timing ports remain live for all eight circuits. This is a continuous-control
engineering inference using the existing envelope law, not a recovered chip
current law. Cold never-triggered Idle remains at zero. Inactive touch drive is
explicitly zero. Oscillators and audio filters of inactive voices remain frozen;
no voice is resurrected.

This supersedes the initially considered frozen-inactive-`z` approximation. That
approximation could freeze a short-VCA/long-filter tail and permanently bias the
other voices' common cutoff bus. The existing envelope state is neither rescaled
nor forced to IL when a tab changes; it relaxes using the selected timing law.
PANEL/memory/legacy-only inactive behavior is unchanged.

MIDI hard panic/CC120 has no direct original-panel equivalent. The explicit
source-only inference is: hard-mute audio as before, and gate a source filter EG
from Attack/Decay/Sustain into Release at its current value. Release/Idle is never
retriggered by repeated panic or repeated frames. If legacy hard panic previously
left an inactive envelope in a held stage, selecting a source preserves its exact
zero-frame state; its unheld gate is reconciled once at the **next control sample**.
Sustain-II's existing reset semantics are not expanded or changed.

## Retained uncertainty and verification scope

The underlying model/provenance contract is
[the integration proposal](FACTORY_TONE_INTEGRATION_PROPOSAL.md) and
`SourceFactoryTones.h`: source fingerprint `a95212770767733c`, seven complete
matrix columns, fifteen columns with a separate 25-cell inference overlay, and
named inferred device/adapter laws. Seven complete tones have no inferred source
cells; their device laws remain nominal engineering models.
Ideal diode/source limits, zero chip input currents, inferred touch/time/LI laws,
affine cutoff alignment, stiff shared boundaries, mixed-source proxies, old
oscillator duty bridge, effective global transports and omitted control-capacitor
dynamics remain explicit. None is a hardware gate or a hardware-equivalence claim.

`SourceFactoryToneTests` supplies the independent literal circuit oracles.
`SourceFactoryIntegrationTests` exercises all 22 source routes (seven complete and
fifteen inferred), original unknown/inferred masks and manifest provenance,
all 26 source-owned controls, receiver wiring/no double envelope/touch/Q,
PANEL/memory clearing, atomic rejection, prepared identity, exact live state,
callback partitions, active-tail reuse, inactive EG convergence, panic gating,
mono suppression, 222 default-off program paths, four audio rates, keyboard
extremes, and eight-voice finite releases. Host restore/selector tests live in
`ui_snapshot.cpp`; their results must be reported separately from core tests.

Initial local same-GCC14.2 `-O3 -DNDEBUG -fPIC` standard and noisy-tail corpora
were byte-identical to the fresh pre-increment snapshot
`4e2e4f58590d405565baba78f09bd5628713280e` (its Engine.cpp equals `8bb9b81`).
That snapshot's corpus hashes also matched the historical pinned compatibility
hashes; this run did not independently rebuild `d56911a7`. Exact final-commit CI
is a separate requirement. ASan/UBSan was run with leak detection disabled after
the container's ptrace boundary prevented LeakSanitizer's exit scan; no leak-clean
claim follows.

### Unnormalized eight-voice stress audition

A bounded numerical candidate check, not a reference recording: default frame,
isolation on, dry single-rank MIX 0/1, chorus off, other rank PANEL, MIDI notes
`{0,21,36,60,84,108,120,127}`, velocities `{.125,.25,.375,.5,.625,.75,.875,1}`,
zero pressure, 0.2 seconds from reset, then all notes released. Every source/rate
remained finite and all VCA voices retired within the following 1.5 seconds.
Across 44.1/48/96/192 kHz and all 22 source tabs (88 fixtures, pre-reset run):

|Quantity|Observed range|
|---|---:|
|Absolute stereo peak|0.233092–2.405104|
|Stereo RMS|0.020144–0.566099|
|Stereo mean/DC over the short onset window|−0.008916–+0.011536|

**Values above 1 are preserved**, not clipped or normalized: this exposes the
nominal linear-LI/output headroom limitation. These are engine normalized units,
not measured volts at a Yamaha output. This deliberately extreme chord is not a
loudness calibration or a claim about sustained DC, alias rejection, or a typical
musical performance. Source-specific candidate captures with complete metadata
are a separate export task; existing legacy candidate fixtures remain unchanged.

## Historical V1 local acceptance checkpoint

The preceding seven-tone V1 integration passed **31/31 Release CTest suites** and
**31/31 ASan/UBSan suites**, with local leak detection disabled. The whole runs
completed in77.12s and372.04s respectively. The focused integration suite has
**67,936 checks**; later comment-only corrections do not alter its assertions.
Both standard and noisy-tail legacy corpora, plus the extended legacy-isolation
corpus, match the pre-increment same-compiler baseline byte-for-byte. Five
separately compiled behavioral mutants are rejected: missing isolation gate,
doubled filter-envelope contribution, old Q mixer, source-tab state reset and
frozen inactive source EG. Independent review reproduced4,608 staggered-voice
receiver comparisons and the retired-voice shared-bus relaxation.

Host source-selection/onset/ownership/session tests pass a JUCE8.0.6 syntax
check; actual plugin execution is an exact-commit CI gate, not a local claim.
There is no new oscillator integration in this checkpoint.


## V2 integration checkpoint and reproducible seven-tone comparison

Before the execution workspace reset, the expanded V2 integration suite passed
**135,739 checks**, including all 22 source ownership/receiver/finite-audio
fixtures, four rates, every source transition, version-1 rejection, original
unknown/inferred mask preservation, PANEL/memory clearing and the retained
inactive-EG/panic rules. The new predicate is `isSourceDriven` for both compilation
and activation; `source_inferred` remains a separate diagnostic classification.
Full V2 CTest, sanitizer, host and validator outcomes are separate exact-commit
gates, not inherited from the V1 checkpoint or a lost temporary workspace.

`src/tools/source_factory_compatibility_probe.cpp` is a same-toolchain regression
fixture usable with both V1 and V2 core headers. Its four-rate, seven-tone corpus
includes staggered voices, paired complete sources, PANEL/memory handoffs,
pressure, original global modulation, expression/ribbon, panic, active tails and
prepare. It records audio and relevant numeric envelope/filter/oscillator and
receiver state field-by-field, not struct padding. Model-version/provenance
metadata is deliberately recorded separately, not expected to be byte-identical.
It does not select the fifteen intentionally changed V2 tones and is not hardware
evidence.

The pre-reset baseline was `22d6a32` (V1), extracted with `git archive` into a
separate temporary source tree before compiling V2. Both versions used GCC 14.2,
C++20, `-O3 -DNDEBUG -fPIC`, without fast-math/LTO/native tuning. The 9,332,176-byte
numeric corpus was exactly identical, with this expected regression SHA-256:

`038357d28e188020f7d61ab6aab4b87cdee818cda022b02844520507204003fb`

The restored fixture source SHA-256 has been rechecked as
`079cc78b1822c345d4b490597bbac8991e63da880d1fac3c2140dc253ee4862a`.
V1 uses model version 1 with no hypothesis manifest; V2 uses model version 2 and
manifest version 1/fingerprint `e5f877a025fc52c6`, while these seven tones retain
zero inferred/unknown source-cell masks. No binary audio archive is committed.
The earlier temporary binaries were lost in the workspace reset; historical
hashes are not substitutes for new executions against restored source.

To reproduce, compile this same fixture against each revision's own `src/core`
headers, `Engine.cpp` and `FactoryPresets.cpp`, run it with separate output paths,
and compare the outputs with `cmp` using the same compiler/options. The existing
`compatibility_probe.cpp` remains the distinct 222-program legacy regression
fixture; its corpus and parameter/program meanings are not replaced.

### Post-reset restoration check

The restored Engine/tests/fixture were newly compiled after the workspace reset.
The focused integration test again passed **135,739 checks, zero failures** with
GCC 14.2 and strict warnings. A fresh `git archive` of published `9725889` supplied
the unchanged V1 core baseline; newly rendered V1 and restored V2 numeric corpora
were each 9,332,176 bytes and compared exactly, both reproducing the expected
`038357d28e188020f7d61ab6aab4b87cdee818cda022b02844520507204003fb` hash.
This newly establishes the focused restoration check, not a fresh full sanitizer,
host, validator or 222-program compatibility run. Those remain separate final
commit/CI gates. No experimental oscillator was connected by this restoration.
