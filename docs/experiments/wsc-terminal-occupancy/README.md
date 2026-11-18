# Which oscillator operations actually coincide across four ranks?

**Stock-only diagnostic. No changes to the instrument, new vector implementation, measured CPU saving, or DAW result.** The question is narrower than the oscillator family's 42–45% sampled share: after each rank's original event scheduler has done its work, do four fixed adjacent ranks still have the same *kind* of final job to perform?

| Late workload | All four actually ticked | Unchanged, uninterrupted whole tick | Ordinary final node request in all four |
|:--|--:|--:|--:|
| Factory, held | 262,144 | 218,870 (83.49%) | 262,144 |
| Factory, moving | 262,144 | 29,482 (11.25%) | 262,144 |
| Panel, held | 262,144 | 0 | 262,144 |
| Panel, moving | 262,144 | 0 | 262,144 |
| Pure, held | 262,144 | 219,267 (83.64%) | 262,144 |
| Three voices, held | 65,536 | see [counts](observed-counts.csv) | 65,536 |
| Legacy, held; quiet factory | 0 | 0 | 0 |

These are **retrospective counts**, not a cheap predictive admission test. Pitch, gates and comparator threshold can differ between lanes. Late means host blocks 1024–1535 at 48 kHz, 64 frames, 2× internal rate; each internal sample offers exactly four fixed groups of four ranks. No grouping across samples, repacking, or exclusion of difficult pitches. The moving fixtures receive pressure, bend and CC74. The normal control frame precedes the fresh MIDI in every callback. The [protocol](PROTOCOL.md) and [raw per-group onset/late rows](observed-counts.csv) also cover release/wake, resets and policy transitions. Panel's threshold changes during PWM every tick: zero in the strict column does not mean an idle oscillator. Three voices make one full group, one two-lane group and two empty groups per sample.

**Earlier events still matter.** In factory-moving, 232,658 late complete groups had earlier work before their ordinary final node calculation. A diagnostic event count of zero falsely suggested no boundary for 775 factory-held and 423 factory-moving complete groups: zero-residual decisions are not necessarily retained by that count. A separate genuine boundary case put one lane at reset end exactly one time step away. With four ordinary lanes, the terminal count was 1/1; with that restored lane, all four ticks completed but only three had an ordinary terminal request, so the count was 0/1. The reference and observed arm still agreed on all 7,031 serialized bytes. A separate reporting-only mutation added one to an aggregate after rendering: the changed count was detected, with identical traces. These two controls test different failure modes.

**Trace and execution provenance.** The original three-object shipping core is [`e9e8175`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/e9e8175f4278d2457279fcb8809cbba1ddb7be2b) (full identity in [evidence](evidence.json)). The reference's test-only friendship does not alter its release `Engine.cpp` object, SHA-256 `3267ee443c5612ac8babea935882c7de541facfc67d78b01c8149f5eddf2960e`. Added integer hooks observe actual decisions in the other arm; they do not repeat floating-point predictions. Nine paired full-length PCM/public/FP streams plus nine paired sampled private WSC state/cache streams were byte-identical: **18 streams and 46,558,089 bytes per arm**. Extra streams for the reporting mutation are excluded from that total. The file publishes hashes and counts, not the large raw binary traces; `verify.py` validates the saved receipts, not possession of historical audio. No long-double root-fallback or bounded-failure branch was seen in these nine runs.

**What the profile says.** Reclassifying the [previous six shipping release captures](../shipping-core-profile/README.md) including inline descendants attributes **10.60–10.85%** of held factory samples to all `nodes()` calls, and **12.46–12.68%** moving. These include nonterminal evaluations. Separate output-epilogue attribution is 3.10–3.34% held / 2.24–2.39% moving; the distinct endpoint-materialization annotation is 6.52–6.78% / 4.57–4.59%. [All six partitions](profile-scope.json). Don't multiply these shares by group occupancy to estimate speed. Historical four-wide state advance is a different operation and did not implement this terminal node evaluation or an Engine caller.

## Verify or reconstruct

From this directory, `python3 verify.py` checks the group matrix, parity receipts, actual reset control, reporting control and a fresh regrouping of the earlier saved profile. No renderer is executed. `python3 test_verify.py` exercises invalid-evidence rejection.

To *independently reconstruct* the experiment on Linux x86-64, use the exact shipping `src` and a new output directory:

```sh
python3 reproduce.py --run-new-study --src ../../../src --out /tmp/cs80-wsc-new-result
```

Add `--semantic-only` for the small two-case reset-boundary check. The script requires `c++`, `ar`, `patch`, Python 3 and the [already-published fixture](../lp-cross-lane-reuse/cost/fixture/). Normal compiler was GCC 14.2 (Debian 14.2.0-19), standard release flags; only `Ig00156Batch.cpp` additionally uses `-ffp-contract=off`. To preserve their historical source hashes, the driver keeps the fixture include and the direct boundary control reuses the exact prefix of its observer; the reconstruction materializes both in the new scratch directory. It authenticates the two source patches, both arm inputs, scene definitions, and parity byte receipts. Object hashes are compiler-specific; differences must be reported, not labeled shipping identity. Baseline floating-point environment is masked nearest with FTZ and DAZ on; other statuses and platforms are not qualified by this census. The original protocol predates this retrospective packaging.
