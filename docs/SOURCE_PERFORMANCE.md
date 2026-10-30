# Source Performance — policy v1

This opt-in mode combines the current **source-informed tone/circuit models**
with the existing expressive MIDI controls and modern software layer. It is not
a calibrated hardware match. Hardware recordings are not a prerequisite for
the software contracts and compatibility tests below.

## Player-facing contract

`source_performance` is one new default-off host Bool, appended at host index
244 (core index 138, AU version hint 7). All previous 244 host IDs, indexes,
defaults and version hints remain unchanged. `parity_mode` retains its original
Bool representation, index 243/hint 6, and isolation meaning.

| Parity isolation | Source performance | Effective mode |
|---|---|---|
| Off | Off | Legacy Performance — existing sound and controllers |
| Off | On | Source Performance — source models plus software overlays |
| On | Either | Comparison Isolation — existing isolated sound and masks |

- Choose Source Performance, then use the hardware tone tabs to select the
  source-informed factory models. PANEL and MEMORY retain their existing local
  parameter values but use the source-policy envelope/PRA/output processing.
- Source-selected tones still own their **26 programmer controls**. Main-panel
  edits are retained for PANEL; they do not secretly replace the selected tone's
  filter, envelope or touch targets. Footage, global controls, software pan and
  HP/LP bypass remain separately live.
- Enable MPE in the plugin and host. The current implementation uses channel 1
  as master, channels 2–16 as members and manually matched bend ranges. There is
  no automatic RPN zone/range negotiation or separate upper-zone implementation.
- Changing the **effective mode** stops sounding notes and clears controller,
  envelope and effect history. The expression-pedal mailbox is reapplied. A
  hardware tone change within a source mode keeps the established state-
  continuity policy instead. Toggling the masked source flag while Isolation
  is on does not reset notes; leaving Isolation uses the retained flag.
- Source output is not normalized per tone or limited to digital full scale.
  Allow monitoring/export headroom and adjust the existing VOLUME control;
  this mode adds no hidden normalization or safety limiter.

The new model choice is part of a sound preset. Loading an old factory/library
program recalls Legacy/PANEL; it does not silently modernize that program. A
user preset explicitly saved with Source Performance restores it. Isolation
remains an independent session policy, so preset recall cannot disengage it.

## Model selection is separate from feature isolation

`RenderPolicy` has `legacy`, `sourcePerformance` and `isolation` values.
`renderPolicy(parity, source)` applies the priority above, and
`usesSourceModels(policy)` selects the model behaviors. The editor derives a
**requested** policy from APVTS; it must not read audio-owned Engine state.

| Behavior | Legacy | Source Performance | Isolation |
|---|---|---|---|
| Hardware factory-tab routing | Legacy bridge | Source complete/inferred | Source complete/inferred |
| Precise/live envelope backend and active-tail audio-state continuity | Existing legacy | Enabled | Existing enabled |
| PRA EXP-WAH and loaded output circuit | Existing legacy path | Enabled | Existing enabled |
| CHORUS mode delay path | Generic delay line | OE2 BBD model ([BBD_CHORUS_MODEL.md](BBD_CHORUS_MODEL.md)) | OE2 BBD model |
| Existing output soft conditioner/scalar HIGH–LOW path | Unchanged | Not used | Not used |
| Pitch bend, CC1/11/64/74, optional MPE | Live | Live | Existing ignored/masked behavior |
| FX, LFOs, macros, arp, dispersion, drift/noise, tuning, mono | Existing | Existing software overlays | Existing isolation masks |
| Force Original SUB, filters on, centered pan | No | No | Yes, unchanged |

This does **not** promote the separate experimental oscillator, change
oversampling, or change latency: the current production oscillator and 2×
backend remain, with the existing 63-sample reported host latency. Source table,
model-v2 inference manifest, source-complete/source-inferred classification,
original unknown-cell masks and unmeasured provenance remain unchanged.

## What the performance overlays mean

These are deliberate software transports around the inferred source networks,
not assertions that original hardware contains these mappings:

- Velocity/pressure enter the selected source touch/loaded-target network once.
  The older local touch gain/cutoff additions are not added again. Shared loaded
  buses may couple responses between voices; MPE controller identity does not
  imply physically independent analog networks.
- Pitch bend moves the existing VCO pitch transport. It does not invent a new
  filter-key-voltage bend law. CC74 adds the existing per-member cutoff offset
  **after** source cutoff evaluation; it does not rewrite source target voltages.
- CC1 adds the existing global SUB-pitch and filter offsets. CC11 controls the
  global expression/PRA pedal proxy, not per-note expression. Master/member
  sustain, channel panic, poly/channel pressure and ribbon keep existing rules.
- Macro 1 adds the existing global cutoff offset after the loaded source CV.
  Macro 2 scales compiled envelope times and adds existing LFO depth; live
  changes keep envelope value, stage and elapsed time rather than retriggering.
  Macro 3 transports detune/dispersion/chorus width. Macro 4 transports ring
  amount and SUB filter/amplitude depth.
- LFO destinations remain existing pitch/filter/amplitude/pan/PWM overlays.
  Bypassing an audio filter does not remove a receiver from the loaded control
  network. The existing modern FX order is retained: after PRA, before the
  source output circuit. No software overlay is silently relabeled as a source
  electrical target or a measured hardware law.

Some tones intentionally have zero touch depths; an audible change from every
controller on every tone is not promised. Tests use active destinations and
check the intended transport/readout, not just different output samples.

## State and lifecycle compatibility

- Missing, nonfinite, fractional, out-of-range or partially parsed source-mode
  values restore to **0**, even if the live instance was previously enabled.
  Only complete numeric 1 enables the restored opt-in, matching the existing
  strict isolation-mode restoration policy.
- APVTS remains the sole serialized parameter authority. No second tone bank,
  source flag or mutable Engine state is serialized alongside it. This change
  does not revise the source-model-v2 data or inference manifest.
- Ordinary preset recall restores the model flag; only the existing isolation
  and physical-setup exceptions retain their prior semantics. Existing factory
  values naturally contain the new default-off value.
- The Engine compares the old and accepted new **effective policy**, not just
  the isolation Bool. Invalid control frames leave the entire old transaction
  intact. Mode changes are applied at the existing control-frame/block boundary.
- The processor compares accepted policies in both prepare and process paths.
  It reapplies retained expression after reset and clears old UI-held ownership,
  so a delayed on-screen key release cannot release a newly struck host note
  with the same channel/pitch. A masked source switch is not a mode change.

## Verification

`SourcePerformanceTests` checks explicit MPE/master/member/panic behavior;
CC1/74 cutoff and pitch ratios; CC11 versus the direct PRA pedal; macro transport
ratios and precise-envelope oracle; live state continuity; pan/bypass; FX and arp
at 44.1/48/96 kHz. Initial local execution: **485 checks, zero failures**.

The independent `SourcePerformanceAuditTests` checks source routes, neutralized
Source Performance versus Isolation, priority/reset/prepare behavior and
post-network modulation and a scoped post-prepare C++ allocation boundary.
Final local execution: **2,705 checks, zero failures**.
Its separate before/after regression compared **84 configurations** (three
rates × both pre-existing modes × 14 source pairs), including MIDI/pressure,
controller events, releases and live source/PANEL/MEMORY changes. The baseline
`932c6d4` and changed core produced byte-identical 1,448,832-byte float corpora:
`f83e99697ba048f9a3603aafc0445d2b21932b4d6f89c8a5f6f11ce1534b577c`.
The [preserved compatibility evidence](source-performance/compatibility/README.md)
includes the identical harness, input hashes, compiler/commands, results and a
reproduction script; it also records the separate ASan/UBSan result and the
explicitly unavailable leak-check result.

`SourcePerformanceHostTests.h` exercises append-only host metadata, host/user/
factory state recall, old-state and malformed-value migration, expression and
prepare/reset behavior, stale UI releases and isolation precedence. It is
integrated into the ordinary host smoke, not the fresh-process locale fixtures.
Native wrapper/GUI execution and performance qualification are separate gates;
these source-level/core results are not a claim of a measured hardware match or
universal real-time safety on every host.
