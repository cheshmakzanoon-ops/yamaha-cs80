# Playable release readiness

Product/host audit, 2026-10-30; CI evidence reconciled at 04:28 UTC. The initial
audit baseline was `0273031` (all 22 nominal factory-source tabs and official
JUCE 8.0.11). The latest fully green checkpoint recorded here is now
**`54a96f2af4bc5ce3fe8fd68695afa8ddaec8961e`**: original build/validator gates,
DSP measurements, editor screenshots and the separate official beta-validator
comparison passed. Its usability fixes and native packaging are CI-verified.
**Source performance began at `d7b6e8510e42cf8bd7f35edad49c81795bc48801`;
the corrected `ce65ea0` Linux/Apple Silicon lanes and `91ef916` source-mode
screenshots have passed, but all-platform qualification remains pending.** The new PANIC/output-meter batch also needs its
own native host/UI run. None inherits the `54a96f2` pass. No actual DAW
installation, DAW session or packaged Standalone launch is claimed here.

The current implementation adds the approved, default-off `source_performance`
parameter. Existing host IDs, ranges and defaults remain unchanged; the new
Boolean is appended at host index 244 (245 total), AU version hint 7. Neither
source mode is calibrated or establishes measured CS-80 equivalence. Missing
hardware access is not an implementation or beta-acceptance gate.

## Verified checkpoint and pending candidate

| Scope | Exact evidence | Result |
|---|---|---|
| `54a96f2` original build and stable validators | [Build 40995783558](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783558) | All six jobs passed: Linux, Windows, both native macOS builds and the separate Linux/Windows pluginval jobs; macOS VST3/AU/GUI validation also passed. |
| `54a96f2` numerical measurements | [DSP 40995783492](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783492) | Passed. |
| `54a96f2` editor evidence | [Screenshots 40995783622](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783622) | Passed; this is not the later source-performance/display-fit editor. |
| `54a96f2` separate prerelease comparison | [Beta 40997993577](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40997993577) | Both official beta3 lanes passed; not a replacement for the stable gates. |
| `d7b6e85` original source-performance candidate | [Build 41001038174](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41001038174), [screenshots 41001038061](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41001038061) | Failed; the original Apple Silicon and screenshot jobs reported a source-toggle visibility-fixture assertion. These failures remain recorded, not relabelled as passes. |
| `ce65ea0` real-editor-peer fixture correction | [Build 41001845570](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41001845570), [Apple Silicon job 123887820589](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41001845570/job/123887820589) | Linux and Apple Silicon lanes passed, including their validators and native packages; Intel/Windows still pending. The correction attaches a real editor peer in the UI test, not a DSP change. |
| `91ef916` refreshed screenshot trigger | [Screenshots 41002244175](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41002244175), [artifact 12953893654](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41002244175/artifacts/12953893654) | Passed; all 13 PNGs verified and visually inspected. This qualifies source-mode screenshots, not the next PANIC/meter batch. |

The beta run's workflow head is screenshot-only `a98057c`, **not its tested
plugin source**. Both job logs explicitly select Linux VST3 artifact
`12951866069` from build `40995783558`, source
`54a96f2af4bc5ce3fe8fd68695afa8ddaec8961e`. The
[GUI-inclusive job 123875529215](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40997993577/job/123875529215)
records `SUCCESS` and 31 verified tests; the
[headless job 123875529400](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40997993577/job/123875529400)
records `SUCCESS` and 25 verified tests. This provenance is verified from logs,
not inferred from the comparison run's head SHA.

All four native build logs verify clean JUCE **8.0.11**, SHA
`c352e2489000ffeb9e397051377d75eef2efe641`. CTest passed 37/37 on Linux,
33/33 on each macOS architecture and 31/31 on Windows, including the linked
`cs80_ui_smoke` checks for the checkpoint's metadata, save feedback and route
clarity. Native packaging then validated the actual built products and reopened
its archives to check inventory, hashes, paths, types and Unix permissions/link
targets where applicable. The helper's synthetic self-tests also passed: 34 checks
on Linux/macOS, 22 on Windows (Unix chmod/symlink fixtures explicitly excluded;
Windows packages reject symlinks).

| `54a96f2` install bundle | Artifact ID | Packaged formats |
|---|---:|---|
| Linux x86_64 | [12951870661](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783558/artifacts/12951870661) | VST3, Standalone |
| macOS arm64 | [12951946583](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783558/artifacts/12951946583) | VST3, AU, Standalone |
| macOS x86_64 | [12952031695](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783558/artifacts/12952031695) | VST3, AU, Standalone |
| Windows x86_64 | [12952426100](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/40995783558/artifacts/12952426100) | VST3, Standalone |

All four uploads are present and unexpired at this snapshot, with matching
source/build metadata. These are **native package checks**, not evidence of
end-user download/extraction, DAW installation/scan, audio-device setup or a
Standalone playing session. Signing, runtime-library and distribution limitations
still apply. The `54a96f2` bundles do not contain the later source-performance
switch.

## Verdict and current status

The baseline already had a substantial playable instrument: eight dual-rank
voices, panel/memories, 222 library programs, expressive input, an editor, state
migration, latency reporting and strong numerical regressions. The identified
product gaps are now being closed directly rather than deferred for analog
research:

- **Source-performance/controller separation is implemented; qualification is
  pending.** The nominal source models can now be combined with ordinary MIDI,
  optional MPE and modern overlays without relaxing strict isolation. The
  original default-off sound path remains the compatibility target.
- **Mode/source clarity passed checkpoint GUI tests; corrected source-performance
  UI tests and screenshots now have Linux/Apple Silicon evidence, with
  all-platform completion pending.** Both
  requested rank identities/routes and
  effective mode policy are visible.
  Pedal help, library/PANEL behavior, precedence and note-reset warnings are
  explicit. Readouts use parameter atomics and immutable tables, not render-thread
  Engine diagnostics.
- **MPE/tail metadata passed checkpoint linked tests; real DAW handling remains
  unverified.**
  MPE is advertised as a static capability without enabling it. Tail metadata
  is positive infinity: voice-independent analog noise and retained bypassed FX
  histories prevent a universal finite silence deadline. Idle CPU processing
  may continue; exports need an explicit end/selection.
- **Safe patch saving passed checkpoint linked tests; a full DAW player workflow
  remains unverified.**
  Saving returns a result, stages beside the destination and publishes atomically
  without a truncate/copy fallback. Filename collisions require confirmation;
  cancellation/errors do not announce success. Atomic visibility is not a
  power-loss durability guarantee.
- **Native packaging/upload passed at `54a96f2`; end-user installation remains
  unverified.** CI now includes advertised
  VST3/Standalone products and native macOS AU, without replacing validator
  artifacts or weakening gates. See [install/quickstart](INSTALL_AND_QUICKSTART.md)
  and [distribution limitations](CI_DISTRIBUTION_STATUS.md).
- **PANIC and read-only output telemetry are implemented for the next checkpoint;
  native host/UI execution is pending.** PANIC stops existing notes, sustain holds
  and arpeggiator keys without changing the patch; retained histories/noise and
  newly received MIDI can still sound. SAMPLE PEAK observes final host output,
  with separate OVER-RANGE and NON-FINITE warnings, not limiting or normalization.
  The local core-only CTest run passed **38/38 suites**; that is not a linked
  execution pass for the new plugin/GUI helpers.
- **Initial display fitting is implemented; it is not an undersized-screen
  solution.** Initial size uses the logical usable display area, with room for
  host chrome where possible, within 1120x700–1600x1000. Below **1120x700 logical
  pixels**, the supported layout may not fit. Later host/manual sizing remains
  authoritative; this does not add persisted window size or shrink the minimum.

The next bounded work is final-commit core/host/visual qualification, actual
package installation and controller/session/bounce workflows, plus actual-host
underrun checks informed by the new `d7b6e85` expressive benchmark. No newer
increment is called release-ready
until its own relevant evidence exists.

## Current mode, source and controller contract

Both switches are in **Advanced → About**. Choose the effective mode before
playing; the toolbar reports the requested state, not audio-thread acceptance.

| Behavior | Legacy performance | Source performance | Strict comparison isolation |
|---|---|---|---|
| Switches | Both OFF (fresh default) | `source_performance` ON, `parity_mode` OFF | `parity_mode` ON; source request is masked |
| Hardware factory tabs | Legacy voicing-table adapter | All 22 source/hypothesis adapters: 7 complete columns, 15 inferred | Same 22 source/hypothesis classifications |
| PANEL / memories | Existing programmer adapter | Programmer identity retained; shared source-policy improvements apply | Programmer identity retained; source-policy improvements plus isolation masks |
| MIDI bend, CC1/11/64/74 | Active | Active | Ignored |
| MPE | Optional; initially OFF | Optional | Disabled, stored setting retained |
| Pressure, ribbon, on-screen PEDAL | Available | Available | Available |
| Modern FX/LFOs/macros/arp, pan/bypass, variation | Available | Available software overlays, not original hardware behavior | Stored but masked under the existing comparison policy |
| Library recall | Both selectors to PANEL; source flag OFF | Returns to legacy/PANEL | PANEL/source flag OFF; isolation remains ON |

**Precedence and transitions.** Isolation wins when both switches are ON; it
retains rather than clears the source request. Changing only that masked request
while isolation remains ON does not reset notes. Turning isolation OFF applies
whichever source request is then stored. An effective policy change clears
voices, controllers and FX histories; the plugin reapplies its shared physical
pedal position. This is not a seamless live morph. Changing a tone tab within a
policy retains its existing state-continuity contract.

**Recall and migration.** Source performance is a sound-model choice: new user
presets and full sessions save/restore it. The unchanged 222 factory/library
programs reset it OFF and return both rank selectors to PANEL. Old user presets
and sessions with a missing, malformed or non-Boolean saved source flag restore
OFF, never a live prior opt-in. Isolation remains independent: factory/user
preset recall preserves its current value; full session restore recalls it.
Thus loading a user preset may change the source model and stop notes, while it
cannot silently disable isolation.

**What the switch selects.** Both opt-in source policies use the existing nominal
factory-column adapter, state-continuous live envelopes/tail handling, PRA wah
and output-network path. The source-performance policy retains ordinary MIDI and
modern software overlays. PANEL/memories do not become factory columns; their
programmer controls and identities remain intact. This does **not** integrate the
separate experimental oscillator or claim recovered device laws. Current source
outputs remain unnormalized and may exceed digital full scale.

A selected factory tone or memory owns its 26 programmer controls; main-panel
faders are retained for PANEL. Footage and global controls remain live.
`SOURCE-COMPLETE` means no unresolved cells in that source-matrix column, not
complete hardware equivalence; even those columns retain inferred device laws.
`SOURCE-INFERRED` additionally uses explicit unresolved-cell hypotheses.

CC64 remains the MIDI pedal-hold software convention, separate from the original
footswitch release/glide assigners. Source performance makes that existing
convention usable with the newer models; it does not reassign CC64 to a different
physical switch. Defaults, strict comparison masks and old program content are
not silently rewritten.

## Remaining practical priorities

| Priority | Bounded outcome |
|---|---|
| P0: qualify the exact candidate | Finish corrected source-mode all-platform evidence, then run the PANIC/meter batch's linked host/UI checks, every original platform/validator gate and actual-editor screenshots at its own commit. Pending or failed is not passed; do not transfer `54a96f2` or one platform's results. |
| P1: controller/session workflow | Exercise source performance with bend, expression, sustain, pressure/MPE, paired tabs, modern overlays, policy transitions, PANIC, patch saving and full-session recall; verify strict isolation remains inert to its masked controls and the meter does not alter sound. |
| P1: native package/install workflow | Download/hash/extract each advertised package, verify permissions/bundles, scan/open in a named host or Standalone, and record architecture/signing/library limitations. Package presence alone is not a successful run. |
| P1: CPU/headroom on the new path | Preserve the separate `932c6d4` isolation and `d7b6e85` expressive core benchmarks, including every overrun. Check actual target-host underruns and the later telemetry batch's cost; neither core run qualifies the host/UI/driver. Keep explicit monitoring headroom. |
| P2: remaining UI conveniences | Pressure amount display and broader MIDI program/reset handling remain candidates; PANIC is now implemented, awaiting native qualification. Initial fitting retains the 1120x700 minimum; do not claim universal small-screen accessibility. These are separate behavior changes, not mandatory analog research. |

## PANIC and output indication: bounded helper scope

The toolbar's [PANIC control](PANIC_CONTROL.md) queues an all-channel hard stop
for the next audio callback. Existing voices, MIDI sustain latches, arpeggiator
keys and stale on-screen-key ownership are cleared; patch parameters, selectors,
expression and other controller positions are not rewritten. This is **notes-only,
not output mute**: effect/filter/output histories and analog noise remain.
New host notes are processed after the stop and can sound, including in the same
callback. A suspended host must resume processing to consume the request.

The [SAMPLE PEAK display](OUTPUT_METER.md), beside PANIC, reads final output
samples after decimation and mono/stereo mapping. **OVER-RANGE** means finite
absolute sample magnitude **greater than 1**, not proven hard clipping in a
floating-point host. **NON-FINITE** reports NaN/Inf separately from the finite
numeric peak. Warning holds are one/two seconds respectively; the numeric peak
decays on GUI time, so number and flags need not describe one instant.
It is not true peak, loudness measurement, a limiter, normalization, automatic
mute or audio repair. No new host parameter or serialized meter state is added.
PANIC does not clear genuine output telemetry: retained tails/noise remain
observable. New primitive/display/core tests passed locally; native final-buffer
oracles, helper UI/layout and player workflows still require the new commit's
linked run.

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
do, not dynamically toggle with a patch or isolation. The implemented fix returns
constant true. `tests/PluginHostMetadata.h` checks that queries do not mutate
parameters, that capability is stable with MPE off/on and isolation, and that
member-channel behavior and isolation masks are retained. These tests ran in
`54a96f2`'s passing linked `cs80_ui_smoke` suite on all four native builds; the
later source-performance candidate still needs its own run.

The implemented convention is channel 1 as master and other channels as members,
with manually configured bend ranges; system messages/RPN negotiation are not
handled. Document this supported lower-zone-style setup rather than promising
arbitrary MPE zoning/automatic negotiation. Verify two simultaneous member notes,
independent pressure/bend, master/member sustain, and channel-scoped panic in a
real supported host. No hardware CS-80 is needed.

### Original tail under-report and implemented policy

At the baseline, `getTailLengthSeconds()` returned 8.0. The legal per-rank VCA
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

The implemented fix changes only host metadata to positive infinity, leaving releases,
FX and the 63-sample output latency unchanged. A finite replacement such as 20 s
would still under-report supported slow-macro releases (roughly 61 s before FX
decay). More fundamentally, supported analog noise can continue without any
voice, and bypassed FX retain histories that may sound when re-enabled after an
arbitrary interval. These are explicit reasons for a stable capability-level
unbounded policy, not a claim that every dry note actually rings forever.

The added metadata tests cover the original long-release reproduction,
slow-macro release, ordinary FX decay, continuous noise and retained histories.
Checkpoint linked execution passed; actual DAW bounce handling remains
unverified. The policy lets a host keep idle instances
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
- The [new dry source-route audit](performance/2026-10-30-source/SUMMARY.md)
  measures DSP commit **`932c6d4` only**, with eight notes/two ranks, all 22 tabs,
  moving controls, idle/release cases and 44.1/48 kHz at 64/128/256-frame blocks.
  Source-held medians were 0.238–0.290 RTF, with 152 deadline exceedances across
  67,584 held callbacks. These cloud-core timings show typical margin and real
  outliers, not a DAW/driver guarantee. They do **not** qualify the later source-
  performance/MIDI/FX policy. Preserve this baseline separately; no 96/192 kHz
  recommendation follows from it.
- The [expressive Source Performance audit](performance/2026-10-30-source-performance/SUMMARY.md)
  measures immutable DSP **`d7b6e85`**, eight voices/two ranks across all 22 tabs,
  at 44.1/48 kHz and 128/256 frames. Held medians were **0.236–0.274 RTF dry**,
  **0.246–0.294 with MPE + chorus/reverb**, and **0.244–0.296 with MPE +
  phaser/delay/drive**: roughly **24–30% of callback budget**, not whole-machine
  CPU utilization or isolated FX cost. The full 156-case/400-row run retains
  **127 budget exceedances in 256,402 callbacks** across all modes/phases
  (88 in 135,168 source-held callbacks), including a 29.143 ms held callback.
  MPE, moving CC11 and actual CC64-held releases passed integrity checks. Zero
  nonfinite/full-scale samples at fixed master 0.12 do not establish headroom
  for arbitrary settings. This cloud-core test excludes the plugin wrapper, UI,
  driver and DAW, predates PANIC/meter, and does not qualify 64-frame/high-rate,
  many-instance or long-session use. Start actual-host checks at 256 frames;
  even that buffer had observed overruns, so it is not a guaranteed setting.
- APVTS/state migration and keyboard handoff already have extensive tests.
  Factory recall preserves physical setup and isolation but resets source
  performance OFF/PANEL; user recall restores physical setup and source choice
  but preserves isolation; host state restores both mode requests.
  Transient pedal position survives prepare/editor reopen but is deliberately
  not serialized. Explain these distinctions rather than promising every
  visible control is session-persistent.
- UI pressure is a usable one-pointer stand-in, with same-block onset and
  poly-pressure tests. UI/host same-channel/same-pitch ownership ambiguity is
  already documented. It is not a reason to rewrite the keyboard before
  qualifying the narrow product fixes and actual performance workflow.

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
Do not claim every DAW is supported because pluginval passed. The controller-
friendly **source-performance implementation now exists**, but its own interaction,
native-host, GUI and target-machine evidence must qualify that release. Keep the
strict comparison fixture distinct; do not borrow its narrower timing/neutral-
overlay results as proof of every source-performance combination.

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
Compile-time assertions cover the route-label helper's classifications and policy precedence.
The existing hardware control positions and colors are unchanged.

A mode-policy line states the isolation controller masks and note-reset policy.
Rank/mode hover help explains the matrix/device distinction, retained programmer
values and library-to-PANEL behavior. The PEDAL description now states the CC11
exception, and a local TooltipWindow override makes that description actual
hover help without intercepting wheel gestures. Advanced/About describes the
same restrictions, nominal source tabs, non-calibration and output headroom.

The original clarity increment's two translation units passed syntax-only compilation against the
unmodified official JUCE 8.0.11 checkout with GCC 14.2.0. Two pre-existing
enum-conditional warnings in `syncSubMode` remain. `git diff --check` passed.
Linked GUI smoke on all four native builds and the screenshot workflow later
passed at `54a96f2`. Neither the historical syntax check nor that checkpoint
verifies the later **source-performance/display-fit or PANIC/meter edits**.
The source-mode real-peer fixture correction passed the `ce65ea0` Linux/Apple
Silicon lanes and `91ef916` screenshots; Intel/Windows completion and the next
helpers' own native run remain pending.

Host-test hooks are component IDs `performance_mode_status`,
`source_route_status_1`, `source_route_status_2` and `expression_pedal`. Required
UI scenarios: fresh PANEL; isolated rank-I String 3 complete and rank-II String 2
inferred; source-performance routing and strict-isolation precedence;
both-flags-off legacy classification; each memory's correct bank;
library recall returning both to PANEL; timer-driven host automation refresh;
source choice recall/migration and masked-request continuity; no status-refresh
parameter mutations; pedal hover/accessibility text; readable
toolbar and About page at both supported screenshot sizes. These checks are
coordinated with the existing `ui_snapshot` suite, not duplicated by a second GUI
test binary.
