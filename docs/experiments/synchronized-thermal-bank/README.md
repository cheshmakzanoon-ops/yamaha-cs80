# Synchronized thermal drift: interrupted core study

**Decision: do not move this candidate into the native plug-in or production.**
The core timing run ended before its prespecified sample count. It produced no full-run CPU estimate or confidence interval. Even a perfect result in all unobserved slots could not have satisfied the original admission rule. The shipping DSP was not changed.

## What was tested

Circuit runs a thermal-drift generator for each of its 16 voice/rank pairs. On the tested GNU x86-64 Release build, the [research patch](tail/candidate.patch) replaces the independent counters with one lockstep clock. The random draws still run separately and in their original order on control ticks. Between ticks, the patch can retain the previous float conversions if the floating-point controls, sticky precision flag, exception masks and held-value range allow it. Pure and Legacy do not clock this bank. The baseline is `e9e8175f4278d2457279fcb8809cbba1ddb7be2b`; its shipping DSP also matches `052ed457c00067d1265a972d7babddf718b82e0a`.

* The [standalone differential](exact-bank.cpp) compared 3,249,152 rank outputs over 203,072 effective ticks, with four rounding modes, varied DAZ/FTZ and preexisting exception flags, rate changes, pauses and copies. There were zero mismatching ticks. A negative control omitting the rounding-control check produced 3,038 mismatching ticks. These tests use masked exceptions; they do not certify arbitrary trapping or asynchronous environments.
* Six stock/candidate [core render traces](trace.cpp) each matched byte for byte (1,249,792 bytes per pair), including stereo audio, voice occupancy and selected per-rank/shared readouts. Cases include factory quiet/held/moving, release and wake with policy changes and a rate/prepare/reset transition, panel held and Pure held. [Common lengths and hashes](tail/exact-traces.json) and [qualification record](qualification.json). This is finite test coverage, not a claim about all states.
* The [frozen core timing plan](bench/plan.json) uses two stock-only A/A blocks and six stock/candidate A–B–B–A blocks per case. Its primary endpoint is the pooled reduction for **factory quiet and panel quiet** at 48 kHz/64, not a host meter or a sounding-voice claim. The other five cases cover held and moving voices, including Pure and Legacy. Thread CPU surrounds the actual `Engine::render` calls in a pinned fresh process; the [driver](cost.cpp) and [analysis method](bench-run.py) are preserved. No plug-in or DAW timing was run for this candidate.

## What can still be concluded

The [retained CSV](bench/raw.csv) is the ordered prefix of **187/224 observations**: all 56 A/A observations, plus 131/168 stock/candidate observations. There are four full seven-case comparison blocks, then 19 rows of the fifth; both primary cases completed in that fifth block. All observed repeats and arms agreed on their case's recorded PCM fingerprint and voice/signal occupancy. The run has no terminal summary. No missing row was sampled again, imputed or excluded from an efficacy estimate.

The lower endpoint of the planned 95% interval needed to beat a floor of **at least +5.9695%** (already set by A/A), and each primary case needed a positive lower endpoint. In the original 10,000 seeded resamples, **3,323** depend only on the five already-complete primary blocks. Give every draw involving the sixth block the most favorable value possible: the two order statistics used to interpolate the 2.5th percentile still lie among those 3,323 fixed draws. The best possible lower endpoint is approximately **−0.9572% pooled**, and **−3.7063% for factory quiet alone**. These are mathematical ceilings, **not measured gains, slowdowns, or complete confidence intervals**. No possible sixth-block cost can meet the primary rule. Further timing is not warranted.

## Replay the decision

From this directory:

```sh
python3 verify.py
python3 -m unittest -v test_verify
```

The replay checks the preserved plan, measured row bytes, slot ordering, affinity observations, audio fingerprints, occupancy, and the bound with the original seed and interpolation. It does not run a timing slot, load a plug-in or claim to recreate the original native binaries. Their frozen hashes are retained in the plan; the original paired trace bytes were checked before export but are not redistributed.
