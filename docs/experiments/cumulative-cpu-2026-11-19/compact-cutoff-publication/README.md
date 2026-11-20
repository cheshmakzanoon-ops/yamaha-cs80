# Compact exact uniform cutoff publication

The [independent solver check](../../certified-shared-receivers/compact-cutoff.md)
explains the guarded change: write only fully validated uniform cutoff output,
not a second eight-receiver record containing the same values. No arithmetic,
per-sample scheduling, quality or latency is changed.

## Direct VST3 result versus the start of this pass

Linux x86-64 Release/PIC GCC 14.2, official JUCE 8.0.11, baseline source
`5499056a60ee5044275ba17eb7dddded4c96e90d`. Same
[short-buffer replay procedure](../resumed-exact-hotpaths/harness/REPRODUCE.md)
as the [prior combined image](../continued-exact-hotpaths/). Each
row has a true VST3 host, repeated acquisition and one-more-callback control.

| True buffer | Fresh quiet | Eight varied held |
| --- | ---: | ---: |
| [16 frames](16.json) | −2.212% | −5.957% |
| [32 frames](32.json) | −2.237% | −6.018% |
| [64 frames](64.json) | −2.251% | −6.049% |

These are translated guest instructions, not CPU usage percentages.
All eight cells also reduce guest data reads and writes. At 64 frames, equal
held improves 6.296%, varied wake 4.363%, and
[fresh quiet removes 13.287% of guest writes](64.json).
Versus the directly preceding combined 78b3f806 product, quiet alone
used 1.955%, 2.096% and 2.175% fewer guest instructions at 16/32/64 frames.
Both arms use the same engine settings.

[27 standard native histories](native.json) (82,944 callbacks, 3,286,656
stereo frames, 216 state snapshots per arm), [connected MIDI](midi.json),
[196 runtime selector pairs](selector.json), [pressure](pressure.json),
[enabled classic plate](plate.json), [six HQ realtime histories](hq.json)
and [four Standard offline histories](standard-offline.json) match exactly
within their public audio, state and callback/FP observations. Standard
realtime at 48 kHz still reports 67 samples. Offline HQ still reports
64/65 samples at 48/96 kHz, and Legacy Standard still reports 63.

## Pinned historical comparison

The earlier [direct original experiment](../resumed-exact-hotpaths/) measured
`db02cd8fa56892c6ea2cf7df084bcf190204968e`. The newly rebuilt starting arm
reproduced that experiment's later arm exactly in all eight measurement cells:
fixture hashes, Ir/Dr/Dw, and all native/translated public artifact hashes.
Comparing the pinned original counts with the new final counts gives
45.481%, 46.113%, 46.459% fewer instructions in fresh quiet and
23.681%, 23.476%, 23.370% fewer on eight varied held notes at 16/32/64 frames.
Those are ratios of matched recorded counts, not sums of percent changes or
a new simultaneous original/final acquisition. None of these figures
verifies physical CPU budget, DAW latency, or performance on other platforms.
