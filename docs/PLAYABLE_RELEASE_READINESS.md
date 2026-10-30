# Playable release readiness

Product/host audit, 2026-10-30. Baseline: `0273031` (all 22 nominal
factory-source tabs; official JUCE 8.0.11 update). This is a bounded musician-use
audit, not another analog-parity roadmap. Initial inspection was read-only;
subsequently authorized usability fixes are summarized below. Their host
parameters/defaults, audio and validation gates are unchanged. Checkpoint
`0273031` has now passed all original CI gates and the separate official
beta-validator comparison, including the locale update. That completed checkpoint
does **not** qualify the newer local usability batch. No DAW was launched for this
audit.

## Verdict

There is already a substantial playable instrument: eight dual-rank voices,
panel/memories, 222 library programs, expressive input, a real editor, state
migration, reported latency, and unusually strong numerical/host regressions.
The next useful work is product plumbing, not another speculative circuit study.

The principal mismatch is that **the latest source models are available only in
a comparison mode that intentionally disables ordinary performance controllers**.
This is an intentional fixture contract, not an accidental broken CC handler.
It nevertheless prevents calling that path a controller-friendly production
instrument. The default legacy path remains playable and must not be silently
changed to solve this mismatch.

## Current local fix status

- **Mode/source clarity implemented; GUI verification pending.** Both requested
  rank identities/routes and the controller policy are visible; pedal help,
  library/PANEL distinction and note-reset warnings are corrected. Official
  JUCE 8.0.11 syntax compilation passed; final linked UI smoke/screenshots remain
  required.
- **MPE/tail metadata implemented locally; host verification pending.** MPE is
  now advertised as a static capability (`supportsMPE() == true`), without
  enabling MPE or relaxing isolation. Tail metadata is positive infinity rather
  than 8 s: supported voice-independent analog noise and bypassed-but-retained
  FX histories mean there is no universal finite silence deadline. Hosts may
  therefore continue processing idle instances; offline export must use an
  explicit end/selection rather than automatic finite-tail termination. See the
  original evidence and local-policy details below.
- **Safe patch saving implemented locally; host verification pending.** Saving
  returns an explicit result, stages output beside the destination, and uses
  atomic publication without a truncate/copy fallback. A same-name or sanitized
  filename collision prompts for overwrite; cancellation/error is visible and
  does not announce a successful save. Atomic visibility is not a power-loss
  durability guarantee. The new storage/UI regressions need final-commit CI.
- **Next product priorities remain source-performance/controller separation and
  complete artifact packaging/installation.** The first three local fixes do
  not make the new source models respond to pitch bend/CC11/CC64/MPE, nor do they
  produce all advertised installable formats. These remain explicit work, not
  analog-research or hardware-access gates.

No new batch is called release-qualified here until its own CI and required
host/visual results are established.

## Priorities and bounded completion criteria

This register preserves the **baseline findings**, not a claim that the locally
implemented items above are still unedited. Pending verification is distinct
from both an unfixed implementation and a passed release gate.

| Priority | Concrete gap at the baseline | Smallest useful completion |
|---|---|---|
| P0: qualify the candidate | The dependency/source checkpoint `0273031` is now green, including the separate beta comparison; the newer usability batch still needs its own completed evidence. | Record final commit, resolved JUCE SHA, all current CTests/builds/stable pluginval exits and relevant comparison results. Failed processes remain failed; do not change gates or transfer an old pass to new code. |
| P1: make the actual instrument mode legible | Source tabs require isolation; isolation masks common controllers. Library recall resets both selectors to PANEL. The PEDAL description unconditionally says CC11 does the same. | Main-panel mode plus rank-I/rank-II routing status; accurate controller caveats beside the mode and pedal; explicit distinction between library studies and source tabs. Automated UI checks and new actual-editor screenshots at supported sizes. No parameter/DSP/default changes needed. |
| P1: protect user patches | `saveUserPreset` ignores directory/write failure, can replace the same sanitized filename, and updates the displayed preset name even if writing failed. The browser always reports the normal post-save path. | Return/handle success and failure; do not mutate success metadata on failure; explicit overwrite choice or non-destructive unique-name policy; atomic replacement; test duplicate/sanitized-name collision, cancellation and unwritable destination. Keep state format and preset recall policy unchanged. |
| P1: correct host capability/tail metadata | The baseline inherited `supportsMPE() == false` and reported an 8 s tail despite valid longer releases. Local fixes now advertise MPE and an explicitly unbounded tail. | Run the added processor-level assertions and native AU/host verification; retain exact legacy audio regression. Verify infinite-tail handling and document idle-processing/export-end consequences. See evidence below. |
| P1: review a source-performance path | No existing mode combines the new source tab models with ordinary pitch bend/CC11/CC64/MPE. Changing the isolation masks would invalidate its explicit contract. | Agree a small versioned model-selection design, then test its compatibility matrix. Do not add a parameter or change defaults merely on this audit's authority. See proposed policy below. |
| P1: qualify current CPU/headroom | Published timing is for `8bb9b81`, before source integration. Source stress peaks can exceed 1; no output meter or clipping cue is present. | Benchmark the current source route, not `setParams`/PANEL only; publish an honest initial rate/buffer support recommendation. Provide a visible headroom warning and a reproducible level-controlled audition, without altering source levels or adding a hidden limiter. |
| P1 for distribution: deliver installable artifacts | CI uploads Linux/Windows VST3, but no macOS plugin bundle or any Standalone application. Build success is not an installation workflow. | Package all advertised formats/architectures, include commit/checksums and install/uninstall instructions, then verify fresh installation and launch. Explicitly state signing/notarization status. Do not change OS security settings to make installation pass. |
| P2: everyday UI recovery/discovery | Editor always opens at 1600x1000 (minimum 1120x700); there is no visible panic, pressure amount readout, MIDI program-change support or CC121 reset handling. | First address small-display fit and a bounded non-audio-to-audio panic action. Document current MIDI limits. Treat program changes/CC121/new mapping as separately reviewed behavior, not silent compatibility fixes. |

P0 is about whether the candidate is verified, not a claim that a new defect was
observed. P1 items are actionable product gaps; not every P2 item must block a
clearly scoped beta. Missing hardware measurements block no item in this table.

## Mode, source and controller truth table

Evidence: `PluginProcessor::applyValues/readParameters`,
`Engine::setControlFrame/buildSettings/pitchBend/controlChange`,
[source runtime](SOURCE_FACTORY_RUNTIME.md) and
[isolation contract](PARITY_ISOLATION.md).

| Input/behavior | Isolation off (fresh default) | Isolation on |
|---|---|---|
| Hardware factory tabs | Existing legacy parameter-table adapter | All 22 nominal source/hypothesis adapters: 7 complete source columns, 15 inferred columns |
| Library program recall | Resets both hardware selectors to PANEL | Also resets both selectors to PANEL; **does not select a source tab** |
| PANEL / four memories | Stored manual/programmer values | Still PANEL/programmer adapter, not a factory-source column |
| MIDI pitch bend, CC1/11/64/74 | Active software transports/extensions | Ignored; UI PITCH/MOD use the same ignored path |
| MPE | Optional; off initially | Disabled regardless of the retained setting |
| Poly/channel pressure, relative ribbon | Available | Available |
| On-screen PEDAL, physical footswitch controls | Available | Available; PEDAL reaches a separate direct-expression API |
| Modern FX/LFOs/macros/arpeggiator | Available | Stored but masked |
| Switching isolation | Clears voices/controller/effect state | Same transition reset; switch before playing |

A player can currently audition the new tabs with note velocity, MIDI pressure,
the ribbon, and UI/host-automated physical controls. They cannot drive those new
tabs with an ordinary CC11 pedal or pitch wheel. CC64 pedal hold is explicitly a
different software convention from the original footswitch release/glide
assigners; mapping it to the original switch without a policy would change its
meaning, not merely fix a cable.

The baseline UI had useful SUB-mode status and source-tab lights, but those did not
explain source-complete/source-inferred versus legacy/PANEL routing. The local
clarification adds those distinctions and the ownership cue described below. Avoid
overloading the single last-clicked preset name as a two-rank model readout.
When a source owns the 26 programmer destinations, make clear why moving a main
rank fader does not change that selected source; the fader value is retained for
PANEL. Keep those retained values visible/editable with an ownership cue rather
than silently overwriting them.

### Compatibility-safe product policy for review

Keep the existing `parity_mode` semantics and migration exactly as they are.
Do **not** make CC11 exceptions inside isolation, reinterpret the Boolean as a
three-way mode, alter existing program contents, or enable new audio by default.

One coherent future design is an explicit, independently versioned **source
performance model** selection, with Legacy as the unchanged fresh/missing-state
default and the existing strict isolation policy taking precedence. That would
provide three understandable user situations:

1. **Legacy performance:** old sounds/controllers and existing compatibility.
2. **Source performance:** selected factory tabs use the nominal source model,
   with ordinary controller transports and declared modern-feature interactions.
3. **Strict comparison:** the existing isolation fixture, unchanged masks and
   reset policy, regardless of stored performance-model selection.

This is a design proposal, **not approval to add a parameter**. Before any such
change, review the appended host ID/AU version hint, missing-state migration,
factory/user-preset policy, mode-transition behavior, output headroom and which
live-envelope/wah/output improvements follow the model rather than the isolation
mask. New source/modern combinations need interaction coverage; enabling the
source gate alone is insufficient. Preserve existing default-off and strict
fixture corpora, and separately record intentionally new source-performance
audio. A fresh-default change would require a further explicit product decision.

## Two narrow host findings

### MPE capability discovery

At the baseline, `PluginProcessor.h` contained no `supportsMPE()` override. The pinned, unmodified
JUCE 8.0.11 source defaults that method to false in
`modules/juce_audio_processors_headless/processors/juce_AudioProcessor.h:879`.
The AU client directly uses it for `kAudioUnitProperty_SupportsMPE`
(`juce_audio_plugin_client_AU_1.mm:604-605`). Therefore the baseline AU capability
discovery reported no MPE even though the core and UI implemented an optional path.
This is source-verified; no claim is made that a particular DAW was observed
refusing or remapping input. The capability should describe what the plugin can
do, not dynamically toggle with a patch or isolation. The local fix now returns
constant true. `tests/PluginHostMetadata.h` checks that queries do not mutate
parameters, that capability is stable with MPE off/on and isolation, and that
member-channel behavior and isolation masks are retained. These added host tests
are not declared passed until the new batch's linked smoke/CI actually runs.

The implemented convention is channel 1 as master and other channels as members,
with manually configured bend ranges; system messages/RPN negotiation are not
handled. Document this supported lower-zone-style setup rather than promising
arbitrary MPE zoning/automatic negotiation. Verify two simultaneous member notes,
independent pressure/bend, master/member sustain, and channel-scoped panic in a
real supported host. No hardware CS-80 is needed.

### Tail metadata under-reports audible output

At the baseline, `getTailLengthSeconds()` returns 8.0. The legal per-rank VCA
release is up to 20 s, before the modern envelope-time macro. A fresh ordinary
core render confirmed audible output **after** the declared tail:

| Mode | RMS, 8–9 s after note-off | Peak, same window | Active voices after 9 s |
|---|---:|---:|---:|
| Legacy | 0.00817450861 | 0.0128406705 | 1 |
| Isolated PANEL | 0.00651808687 | 0.0102488380 | 1 |

Fixture: GCC 14.2.0, C++20 `-O3 -DNDEBUG -Wall -Wextra -Wpedantic`, fresh
`Engine.cpp`; 48 kHz/256 frames; default `ParamValues`, MIX 0, rank-I saw off,
sine 1, VCA sustain 1/release 20, chorus/analog noise/drift/dispersion off,
macro2 0.5; note 60 velocity 1 on channel 1 held one second. Both windows were
finite. The final partial block was rendered at its actual size. This is core
audio evidence of under-reporting, **not a reproduced DAW bounce truncation**.
Engine SHA-256:
`4d0c2277c4f4ad8ec3062aeb9116413940904af0a538b661e65c9cbefbd11f35`.

The local fix changes only host metadata to positive infinity, leaving releases,
FX and the 63-sample output latency unchanged. A finite replacement such as 20 s
would still under-report supported slow-macro releases (roughly 61 s before FX
decay). More fundamentally, supported analog noise can continue without any
voice, and bypassed FX retain histories that may sound when re-enabled after an
arbitrary interval. These are explicit reasons for a stable capability-level
unbounded policy, not a claim that every dry note actually rings forever.

The added metadata tests cover the original long-release reproduction,
slow-macro release, ordinary FX decay, continuous noise and retained histories.
Final host execution remains pending. The policy lets a host keep idle instances
processing and means an offline exporter needs an explicit end/selection rather
than waiting for an automatic finite tail. Validate VST3/AU host handling and
old/new bounces; do not shorten DSP releases or mute supported noise just to fit
a convenient finite number.

## Headroom, timing and state

- The existing source stress fixture reports peaks up to 2.405104. That is not
  NaN/runaway behavior, but it is above digital full scale. A floating-point host
  can carry it; the final device/export may clip. A safe audition uses a low
  master setting, low monitoring level and explicit downstream headroom. Do not
  normalize individual source tones or conceal peaks inside a fidelity path.
- Current library tests render every program, but the broad all-program check
  is a single note for 2048 frames. It is not evidence of balanced loudness,
  long-attack audibility, eight-voice headroom or musical preset curation.
  A common phrase with enough attack/release time plus a concise listening pass
  is more useful now than adding hundreds more patches.
- Fixed output latency is 63 host samples (about 1.31 ms at 48 kHz), already
  reported at construction and prepare. This is not a major playability blocker.
  Driver/host buffer latency is additional. EXT's separate 63.5-sample control
  interpolation delay must not be confused with output compensation.
- The prior timing audit is explicitly cloud-core-only and predates this source
  adapter. Measure `setControlFrame` with both selected source ranks and eight
  notes, sustained and changing tabs/pressure/wah, at 44.1/48 kHz and
  64/128/256-frame blocks. Record CPU/compiler/commit, median/p99/max and budget
  misses. Then verify actual audio operation on a named target configuration.
  Treat 96/192 kHz as separately qualified options; existing 192 kHz numbers
  do not support a blanket real-time recommendation.
- APVTS/state migration and keyboard handoff already have extensive tests.
  Factory recall preserves physical setup and isolation; user recall restores
  physical setup but preserves isolation; host state restores isolation.
  Transient pedal position survives prepare/editor reopen but is deliberately
  not serialized. Explain these distinctions rather than promising every
  visible control is session-persistent.
- UI pressure is a usable one-pointer stand-in, with same-block onset and
  poly-pressure tests. UI/host same-channel/same-pitch ownership ambiguity is
  already documented. It is not a reason to rewrite the keyboard before fixing
  patch saving and host metadata.

## Release acceptance, not an endless gate

For a **clearly scoped playable beta**, close the P0 qualification and the narrow
P1 saving/host/discoverability items, ship the advertised artifacts, and record
one complete player workflow per advertised platform/format:

1. Fresh install, scan and launch; audio/MIDI device setup in Standalone.
2. Notes, velocity, poly/channel pressure, configured MPE where supported,
   pitch wheel and pedals in the documented mode; no stuck notes after panic,
   stop/start, editor close/reopen or sample-rate change.
3. All 22 source tabs and a representative brass/string/bass/pad/lead/keys set;
   source ownership and PANEL/memory restoration visible and correct.
4. Save a user patch, decline an overwrite, handle failed saving honestly,
   reopen it, then save/reopen a DAW session with selectors/automation intact.
5. A 48 kHz/256-frame eight-voice session with the editor open, note/controller
   automation and a long release; no observed dropout, truncated tail or crash
   in the recorded run. State machine checks also cover repeated transitions.
6. Offline bounce and real-time playback agree within a declared deterministic
   scope; reported latency/tail and mono/stereo routing verified.

Record the DAW/version, OS/architecture, rate/buffer, artifact SHA and result.
Failure to access a DAW is a named unverified cell, not permission to call it
passed; it need not block useful implementation or a narrower qualified beta.
Do not claim every DAW is supported because pluginval passed. A controller-friendly
**source-performance release** additionally needs the reviewed model/controller
separation above; until then label new-model access as comparison/audition.

The repository has no tracked LICENSE/COPYING/NOTICE file at this baseline;
the README's applicable-JUCE-license sentence is not a complete distribution
packet. Establish the intended project/dependency licensing basis and include
appropriate notices before public binary distribution. This audit makes no
legal compliance determination and chooses no license for the owner.

Deferred analog research includes exact oscillator/filter nonlinearities,
calibrated BBD/optical/touch laws, free-running idle state, component tolerances,
scanner and sample/hold behavior, unresolved matrix-cell recovery, and measured
instrument comparisons. These are fidelity refinements, **not prerequisites for
saving a patch, using a pedal, installing a plugin or completing this beta**.

## Narrow UI clarification increment

`PluginEditor.cpp` now shows both requested rank identities and their
`SOURCE-COMPLETE`, `SOURCE-INFERRED`, `LEGACY STUDY`, `PANEL` or `MEMORY` route in
the software toolbar. This reads the existing parameter atomics and immutable
route table on the message thread, **not render-thread Engine diagnostics**.
It describes requested state, not an audio-frame acceptance acknowledgment.
Six compile-time assertions cover the route-label helper's classifications.
The existing hardware control positions and colors are unchanged.

A mode-policy line states the isolation controller masks and note-reset policy.
Rank/mode hover help explains the matrix/device distinction, retained programmer
values and library-to-PANEL behavior. The PEDAL description now states the CC11
exception, and a local TooltipWindow override makes that description actual
hover help without intercepting wheel gestures. Advanced/About describes the
same restrictions, nominal source tabs, non-calibration and output headroom.

Both changed translation units passed syntax-only compilation against the
unmodified official JUCE 8.0.11 checkout with GCC 14.2.0. Two pre-existing
enum-conditional warnings in `syncSubMode` remain. `git diff --check` passed.
This is **not linked GUI execution or visual verification**; final-commit UI
smoke and actual-editor screenshots remain required.

Host-test hooks are component IDs `performance_mode_status`,
`source_route_status_1`, `source_route_status_2` and `expression_pedal`. Required
UI scenarios: fresh PANEL; isolated rank-I String 3 complete and rank-II String 2
inferred; isolation-off legacy classification; each memory's correct bank;
library recall returning both to PANEL; timer-driven host automation refresh;
no status-refresh parameter mutations; pedal hover/accessibility text; readable
toolbar and About page at both supported screenshot sizes. These checks are
coordinated with the existing `ui_snapshot` suite, not duplicated by a second GUI
test binary.
