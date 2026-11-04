# Bass Stack retrigger: causal controls and remaining score failures

2026-10-31. These are deterministic software-model diagnostics, not hardware
measurements. The original physical-unit PANEL coordinate is retained in this
investigation. The production electrical programmer changes attack timing and
VCF drive, as quantified separately in [PROGRAMMER_INTEGRATION.md](evidence/continuation-2026-10-31/PROGRAMMER_INTEGRATION.md);
its lower click ratio cannot identify the cause of the old physical-PANEL edge.

## Omit only the6.25-second note-on

The audit now renders an omitted-retrigger control, with every earlier event
and RNG draw identical. The prefix before that event is exactly equal in all
three rates. All original windows and score denominators remain unchanged.

| Rate | Original physical PANEL score | Note-on omitted | Peak offset after event |
| ---: | ---: | ---: | ---: |
|44,100|1.519786127|0.030999552|352samples /7.982ms|
|48,000|1.301823122|0.027891511|370samples /7.708ms|
|96,000|1.246150373|0.066892453|740samples /7.708ms|

The large derivative depends on the new note. Zero-time capacitor continuity
alone therefore does not close the original metric. The measured peak occurs
several milliseconds after assignment; it is not an instantaneous reset.
The same diagnostic also keeps the electrical-programmer rows separate and
retains the original Funky omitted-note-off control.

## Separate VCA pedestal from full-rate idle history

Two temporary source variants affect only the still-unassigned voice6. One
disables that voice's inferred VCA offset/feedthrough while keeping its LI RC,
envelope and output coupling. The other continuously clocks that voice's
oscillator/filter at full internal rate from reset, with its normal idle VCA
path and shared RNG draw sequence preserved. No variant is production code.
Every variant has byte-identical audio before the6.25-second event.

| Rate | Baseline | Pedestal disabled for voice6 | Voice6 fully clocked during idle |
| ---: | ---: | ---: | ---: |
|44,100|1.519786127|1.519764831|0.645894568|
|48,000|1.301823122|1.301805408|1.293045136|
|96,000|1.246150373|1.246133018|1.246706364|

The pedestal is too small to explain the edge: at44.1k the peak derivative
changes by only1.72e-7 full-scale units, at the identical sample. Internal taps
place the dominant derivative in the filtered oscillator signal about6.5ms
after onset, followed by the63-host-sample decimator delay. The amplitude
envelope is about0.985 there and the LI node changes continuously. First
internal-sample rank outputs are only about-5.8e-5.

Full-rate idle does change the result materially at44.1k. It also reveals a
0.020-0.023-cycle cold phase difference, beyond the existing varying-control
limit. The original score remains above one at48/96k even with full-rate idle.
This is an unresolved timing finding, not permission to classify the old
phrase as passing. `COLD_IDLE_TIMING.md` describes the identified first-step
stagger/zero-frequency mechanism and its preregistered correction gates.

## Reproduction and evidence

Run `cs80_engine_audit --suite click-causality --rates44100,48000,96000 --out NEW_DIR`
(with spaces after `--rates` in the command). Its CSV includes the exact event
and peak sample, derivative magnitudes, coordinate, omitted-event score and
prefix equality. The renderer's single-sample diagnostic split was separately
compared with the ordinary64-frame rendering: all output floats are identical.

The compact evidence bundle under `evidence/continuation-2026-10-31/` contains
`omitted-retrigger-controls.csv`, `bass-physical-counterfactuals.csv`, onset/peak
rank taps, the standalone probe, and three diagnostic patches. To reproduce
the temporary variants, export `src` from audio commit `44e321c`, copy this
revision's `src/tools/engine_audit.cpp` into that export, apply
`bass-trace-base.patch`, then optionally one of the other patches. Compile
`bass-causal-probe.cpp`, the exported `Engine.cpp` and `FactoryPresets.cpp` with
C++20/O3 against the exported `src`; run with a new output directory. It emits
full sample/tap traces and rejects any trace split that changes the audio.

## Cold-clock repair checkpoint

The published cold-time repair is `46a7a5a2ce077e95b9025de81da85b3480134176`,
with separate pin `6ab46dc8e1f75f4b559b279c54a254ace0480fbd`. Its source is
identical to the retained isolated `bc41f23` causal qualification, as recorded
in [cold-idle-soa](evidence/continuation-2026-10-31/cold-idle-soa/README.md).
The physical-PANEL Bass score becomes 0.571043 at 44.1 kHz, while the 48 kHz
score is 1.440981 and the 96 kHz score also remains above one. The named
Bass phase error versus a full-rate voice is now at most 0.00010705 cycles,
within the unchanged 0.002 limit. Thus the identified cold scheduling error
is removed, but that does not close the original all-rate click metric.
The retained after CSV includes electrical-programmer rows and omitted-note
controls without changing any scoring window or denominator.

## Exact event continuation checkpoint, 2026-11-01

The [exact event regression](CLICK_EVENT_CONTINUITY.md) now covers all 72 named
exposed continuation fields per rank, including all antialias residuals. Its
original and early-release Funky fixtures, sounding-tail Bass fixture,
independently caught-up original inactive Bass fixture and cache-invalidation
oracle pass 7,438 assertions under both source policies. Fifteen actual-source
mutants and two test-strength controls qualify the gate. It adds no production
sound change.

At current source baseline `d06648c` / audio `18400c2`, the fresh original
physical-PANEL Bass scores are **0.567655911 / 1.440924029 / 1.365308570** at
44.1/48/96 kHz. The latter two remain above the unchanged limit of one; the
Funky note-off scores also remain above one at all rates. The new exact
continuation proof and passing electrical-programmer scores do not close
those original criteria. The linked report retains the full causal controls,
baseline comparison, mutation logs and the earlier preregistration red run.
