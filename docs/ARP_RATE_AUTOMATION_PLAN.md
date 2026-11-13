# ARP RATE sample-offset automation: source review plan

Status: source only; no build, native execution or qualification claimed.
Base local `1e7113b4e040548471c5d42d80b24a57ffff48d9` represents the verified
main snapshot `6db7de3e`. The current 115-lane qualified baseline module is the
FX115 artifact, SHA256
`8c05a7273d12ff0fc7f6de8ae33cb8467cc7459a65290de03994d4147f291667`,
from local qualified source `2305421d50aab146cb348256234eaf71a2d0538e`.
Current production source differs from that qualification only by the separately
reviewed loudness comment; this does not claim a new whole-tree qualification.

## Capability and existing semantics

Append only `P_arpRate` at lane 115 (total 116) in HostAutomationBridge; preserve
the literal 115-entry prefix. The existing parameter ID `arp_rate`, range
0.25–16 Hz and normalization remain unchanged. No DSP, UI, state format, parameter
metadata or source authority changes. `buildSettings` applies the existing rate
and `updateArpeggiator` reads it each internal sample. Rate motion retains elapsed
clock time, and an already-open gate keeps its existing remaining duration until
expiry or replacement; an overdue shorter interval can trigger the next sample.
No external MIDI clock or new musical timing policy is introduced.

`Vst3Frozen115Roster.h` freezes the previous literal roster. Six historical
native FX fixtures change only their exact-115 full-mode guard to a minimum-115
literal-prefix guard; their old contracts, freezes and evidence are untouched.
These historical fixtures are source-reviewed, not requested to rerun here.

## New ordinary-input fixture

CLI: `MODULE OUTPUT HOST_CONTRACT ARP_CONTRACT RATE MODE [--compat-only]`.
`Vst3ArpRateAutomationTests.cpp` uses the existing supported helper only, with
its own bounded processing function. Frozen115 helpers can compile the same
fixture in compatibility mode; that mode never queues the new ARP-rate lane.
Fresh output directories are mandatory and every render is retained as stereo
float32 PCM (65,536 values / 262,144 bytes per capture).

Six configurations: 44.1/48/96 kHz, realtime/offline. Each history is exactly
32,768 frames, with manual/manual source selectors 13/13 and ascending one-octave
ARP held fixed throughout. Three notes 48/52/56 enter at frames 17/18/19 and leave
at 28,672/28,673/28,674. Fixed neutral macros, matrix, SUB, FX, tuning mode and
controller state isolate the rate and optional preceding volume lane. Source
performance is on for the active gate, scanner off; no model/type/mode switches
or malformed/state/parser/queue stress paths are used. Existing ordinary note
and queue inputs only. Current release floor and rank release times are fixed
at .02 s, with no live release edits.

Scenario 0 moves only ARP RATE. Scenario 1 combines it with the existing volume
lane. Rate starts at 8 Hz through frame 1023, then hits 12/16/10/14/8 Hz at
4095/8191/16383/24575/32767. The combined volume starts at .60 through1023,
then .75/.50/.70/.55/.60 at4132/8228/16420/24612/32767. Queues interpolate in
normalized parameter space, not linear physical Hz. Scalar references use the
same normalization. This is conservative partial-range behavior, not all-rate
coverage.

## Fixed captures, comparisons and acceptance

Each scenario retains five baseline-compatible renders, none with a rate queue:
`noqueue`, `null`, `preceding115` (volume/masterTune/tuningRef/mod16Amount/fx1Mix),
`legacy-prior`, `pure-prior`. The last two queue those same preceding controls
under the named policy. Baseline mode saves exactly 10 captures/configuration;
all 10 must pair byte-for-byte with the candidate's corresponding captures.
Representative preceding controls are not exhaustive coverage of the115prefix.
FX1 stays at classic drive type5/model0 throughout, initially mix0: normal ARP
histories leave it bypassed, while the preceding115 curve alone moves its mix
to exercise the most recently published capability. No type/model automation.

Full mode additionally retains per scenario:
- `legacy`, `legacy-flat`: two captures; original rate curves versus explicit
  per-block final-value queues must be byte-identical.
- `pure`, `pure-rate-held`: two; hold only ARP RATE while retaining volume motion.
- `arp-off`, `arp-off-rate-held`: two; same isolation with ARP disabled at setup.
- `scalar`, `block7`, `block64`, `block256`, `block1024`: five captures.
- `constant`: one capture with all active curves held at initial values.

Combined scenario also retains `reverse7/64/256/1024` (four) and `held-lane0/1`
(two, holding only rate or volume respectively). Therefore full mode saves
17+23=40/configuration: **240 candidate +60 frozen115 =300 retained PCM**, with
**60 actual compatibility pairs**. Per six configurations:
- 48 scalar/block comparisons, frozen host maximum-error tolerance `2e-6`.
- 24 exact reversed-queue comparisons (combined rate and volume).
- 12 exact initial1024-frame holds.
- 12 overall and12 individual combined-lane effects, each greater than `1e-5`
  maximum absolute difference over frames1024..28671.
- 12 exact Legacy flattening,12 PURE rate-inert and12 ARP-off rate-inert pairs.
- Empty/null equality in candidate and baseline, all samples finite and sizes exact.

The combined control does not wrongly assume volume is inert in PURE or with
ARP off: only the new rate is held and the same volume curve remains on both
sides. Per-lane witnesses prevent volume alone from satisfying the rate witness.
No claim of state readback, precise musical onset extraction, DAW/cross-platform,
CPU, hardware fidelity, full-range coverage or other render-policy qualification.

## Review and staged execution boundary

Exact CMake build target: `cs80_vst3_arp_rate_automation_tests`, using the existing
SDK helper linkage and candidate VST3 build dependency. No broad CTest selection
or plugin validator is requested. Qualification runner/baseline preparation,
source/binary freezes and independent review are separate before execution.
Do not overwrite attempts, retry a native failure, modify old evidence or relax
thresholds. Retained Git evidence will contain hashes/logs/manifests; PCM and
binaries remain local. Completion requires collector recomputation from every
capture, actual baseline pairing and independent final review.
