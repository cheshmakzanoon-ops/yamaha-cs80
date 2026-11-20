# Continued exact VST3 hot-path work

Compared with source baseline `5499056a60ee5044275ba17eb7dddded4c96e90d`.
This pass makes no changes to the synth quality modes, voices, audio sample
order, or the latency. It combines these independently checked changes:

- The [output FIR](../packed-output-decimator/) packs independent 2× tap
  products, in the existing proven domain, and preserves scalar summation.
- The held oscillator [screens crossings in pairs](../held-crossing-pairs/).
  A non-empty candidate set still uses the original scalar roots/order.
- The [shared receiver solver](../../certified-shared-receivers/) reuses an
  unsaturated same-sample cutoff admission and computes each rank Q once.
  Ordered fallback is retained outside its validated math/runtime domain.
- The exact IG00156 AVX2 batch certificate compares grouped binary64
  coefficient and history words and keeps the original ordered edge path.
- The [processor](../../host-callback-projections/) reads only the required
  active-voice flags and does not examine unattached automation curves;
  required normalized parameter reads retain their order.
- The [classic delay reader](../linear-delay-neighbor/) replaces an integer
  remainder for an adjacent sample with a last-cell check, with unchanged
  floating-point cursor/wrap and interpolation logic.
- The Engine evaluates the output FIR after the factor's final internal
  sample. Engine rate configuration prepares both decimators with the
  engine factor, and every valid factor retains exactly its final phase.

## Proof scope

Matched Linux x86-64 Release/PIC, GCC 14.2.0, pinned official JUCE 8.0.11
(`c352e2489000ffeb9e397051377d75eef2efe641`). The archived
[replay procedure](../resumed-exact-hotpaths/harness/REPRODUCE.md) uses
genuine modules and verifies the selected real process call, duplicate
counts, one-more-callback additivity, and public PCM, FP and state data.
Each translated work row is a 256-stereo-frame prefix at 48 kHz. These are
not CPU clock time, latency in a DAW, or physical low-end CPU qualification.

For the separate enabled classic plate cell, run
`prepare-plate-host.py CHECKOUT VST3_SDK VALGRIND_INCLUDE NEW_DIRECTORY`.
Use its generated `work-plate.py` and `guest-plate` with the same replay
procedure and `--scenes plate-held --frames 64 --certify`. The dry
`varied-held-short` scene is an audibility control, not a replacement
baseline for counting the same plate scene.

## Direct measured changes

| True VST3 host block | Fresh quiet instructions | Eight varied held instructions |
| --- | ---: | ---: |
| [16 frames](16.json) | -0.262% | -5.917% |
| [32 frames](32.json) | -0.143% | -5.977% |
| [64 frames](64.json) | -0.078% | -6.008% |

All eight cells reduce instructions, data reads and writes. At 64 frames,
equal held is -6.040% and varied wake is -4.277% (guest instructions).
The [enabled classic plate](plate.json) row changes by -5.841%
for the whole combination, not the individual delay-reader change.
Counted module SHA-256: `ac936092bceb9f5afce9f098397e61583d9c349eb2dfe20f763690b942fd637c`.

The [27 main native histories](native.json) (82,944 genuine callbacks per
arm), [six connected MIDI histories](midi.json), [196 runtime selectors](selector.json),
[six pressure workloads](pressure.json), and [six native HQ 8×/4× histories](hq.json)
also matched tested public audio, state and FP observations byte for byte.
Standard still reports 67 samples (1.396 ms at 48 kHz); Legacy reports 63.
