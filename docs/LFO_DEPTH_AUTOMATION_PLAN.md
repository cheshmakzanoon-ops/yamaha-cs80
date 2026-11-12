# Proposed bounded modern LFO depth automation

Design only. No production edits or runtime. Base is published main
`3451eddf9f7b7aae6682e796446a9c21ca8da136`, including the verified 83-lane
mod-wheel checkpoint. Rates, destinations and waveforms remain static; no rate,
phase-law, FX, macro, controller or discrete-switch redesign is included.

## Parameter semantics and production scope

Append existing `P_lfo1Depth`, `P_lfo2Depth`, `P_lfo3Depth` at zero-based lanes
83, 84, 85 respectively, total 86. Preserve all preceding 83 entries in order.
Require literal appended order and parameter IDs `lfo1_depth`, `lfo2_depth`,
`lfo3_depth`, not only a total-count check. Each existing amount is linear
normalized 0..1, default 0. Engine.cpp assigns
these values at lines 673–675 and already smooths them through the continuous
settings bank at line 30. No new DSP is necessary.

At Engine.cpp:2600–2614 each deterministic LFO output is multiplied by its
amount. Static destinations map to pitch (*2 semitones), filter brilliance
(*1.6 octaves), and level CV (*.25). The level path enters both manual and
factory-source LC gain at lines 3017–3026, rather than a new output-gain law.
PURE already forces depths/destinations to zero. Legacy timing remains its
existing final-point-at-block-start behavior.

## Deterministic fixture and claimed coverage

Pin sine waveform 0 and rates 8, 11, 14 Hz for LFOs 1, 2, 3 respectively.
Preserve the existing sine float-phase implementation; do not automate/reset
rates or phases. Four scenarios:

1. LFO1 depth alone, static pitch destination 1; other two depths/destinations 0.
2. LFO2 depth alone, static filter destination 2; other two depths/destinations 0.
3. LFO3 depth alone, static amp destination 3; other two depths/destinations 0.
4. All three amounts together, the same pitch/filter/amp destinations. This
   checks simultaneous queues on distinct live musical paths, not all possible
   LFO/destination combinations.

Each scenario uses manual/manual sources 13/13 and factory/manual sources 0/13,
and 8- and 16-voice capacities (the unchanged fixture emits 2 notes in the
8-voice case and 16 notes in the 16-voice case). At 44.1/48/96 kHz and realtime/offline, this is
4 × 2 × 2 × 6 = 96 full cases. Each render is 4096 stereo frames; ordinary
held notes and release from the unchanged historical native renderer.

Start enabled depths at .15; hold amount endpoints at frames 0 and 1023,
then .8/.25/.9/.4 at 1535/2047/3071/4095. Inactive depths stay zero. Require
finite output, exact initial 1024-frame byte prefix against a matched scalar
constant-depth history, scalar block1 versus blocks64/127/256 within the
unchanged host audio tolerance, and positive effect in held frames[1024,3072).
The three isolated cases witness each depth/destination individually; the
combined case does not stand in for those witnesses.

Destination-off control sets all three destinations to 0 while retaining the
same static rates, waveforms, amounts and note history. Compare automated
versus constant-depth renders byte-for-byte in that off configuration. Do not
compare destination-off sound to destination-on sound as an inertness claim.

## Neutralization

Pin modern circuit source policy with zero drift, dispersion and analog noise.
All four macros are .5 (especially motion, which otherwise adds LFO depth).
All 16 modulation-matrix amounts remain zero and modern/MSEG routing remains
default unrouted. This matters because raw LFO values also feed the matrix. Other oscillator/ring/chorus/FX modulation is off:
SUB VCO/VCF/VCA/touch/external amounts 0; ring amount 0; chorus off; all FX
mixes 0 and types off; rank PWM depths 0; wheel SUB/VCF amounts 0, no CC1 or
pitch/pressure events. Disable arp/MPE/KAS timing, ribbon, glide/footswitch
assignments; zero initial-bend/keyboard contours and rank touch amounts.
Use deterministic pulse-only manual ranks, no noise/sine extras, fixed PWM
width, stable filter headroom and nonzero VCA sustain with ordinary attack/
release. Factory rank behavior remains its existing source authority, not
rewritten manual rank values. Quality/tuning/source selections remain static.

## Compatibility, counts and retained evidence

Freeze the exact published 83-lane module and original source identities before
production edits, including all 54 historical numerical contracts. New runner
must compile twice: candidate86 headers and physically separate frozen83 headers
and historical renderer, not one candidate executable loading both modules.
Use a named-parameter finite-curve renderer confined to this new test so the
frozen83 build can deliver new-depth queues through its existing Legacy/PURE
fallback without indexing beyond its compile-time roster. Historical renderer
and UiComponents/faders are untouched.

Each candidate case:
- 2 noqueue renders (empty versus null changes), 1 captured baseline.
- 1 representative preceding83 automation render, 1 capture. Drive prior volume,
  tuningRef, masterTune and rank1pan (no CC1 driver); no claim
  of rerunning every old lane suite.
- 2 Legacy renders: actual offsets versus final point moved to block start,
  exact PCM; 1 capture.
- 2 PURE renders: varying versus constant depths, exact PCM; 1 capture.
- 4 full renders: scalar plus three partitions.
- 1 constant scalar for initial hold and positive held-window comparison.
- 2 destination-off renders for exact amount-inertness.

Totals: 14 renders/full case, 7 renders/frozen83 compatibility case. Across
96 + 96 cases: 1344 candidate + 672 baseline = 2016 renders, 12 native
processes. Four captures/case/side give 384 actual byte-equal comparison pairs
from 768 captures. Record pairs distinctly from capture counts. Protected
Legacy/PURE cases deliberately send new depth curves; noqueue and preceding83
preservation captures do not.

Preserve the 54 old contracts and all old evidence unchanged. Inventory exact
old source/test changes separately; forward-compatible roster guards require
literal prior-prefix identity, never a silently weakened count. Candidate
controls not exercised by this matrix remain outside its runtime claim.

## Required review sequence

Independent design approval, then implementation/frozen baseline; independent
source and bounded run-script review before any build/runtime. No malformed
input, scanner/broad host suite, GUI, benchmark, security reproduction or
hardware-fidelity claim. Rates and FX remain explicitly deferred. Publication
requires successful finite gates and independent actual-pair evidence review.

## Implementation prepared for source/script review

Only production edit: the three depth roster entries. The old mod-wheel runner
has a narrowly changed forward guard: contract stays83, runtime count>=83 and
all83 literal prefix entries required. Original historical native renderer,
DSP and UI files remain untouched. That guard is source-reviewed only, not a
new claim to have rerun its old runtime suite.

Frozen published83 module and copied src/tests live in `../lfo-depth-frozen83`.
Module SHA256 `04adbf33704b1b053847d4e6f5532b9d75b0b960584f087f1a9864338b0f4182`.
4662 tracked identities were frozen before edits, inventory SHA256
`7f9c9c65919082a05db0ac23d6de53454d4afade807d07ec5899673bdeb75db6`.
The independent baseline audit additionally pins54 contracts and564 source/test
identities. The only files added to frozen83/tests are the new LFO runner and
literal83 header, so its relative renderer include resolves to original source.

Exact proposed command after independent approval:
`python3 scripts/qualify-lfo-depth-automation.py`

The script verifies reviewed-source freeze and frozen/retained identities before
building only `cs80_vst3_lfo_depth_automation_tests`. It separately compiles the
baseline83 executable from frozen83 source (not candidate86 reused as loader),
runs exactly six full/six compatibility processes, checks logged compiled86/83
and per-process16 cases/224-or112 renders/64 captures, then compares384 actual
PCM byte pairs and repeats identity checks. It records binary/log/capture hashes
and refuses existing evidence/capture directories so failures are preserved.
No build/runtime has run yet. AST parsing and whitespace checks only have passed.
