# Skip exact positive-zero voice control smoothing

The source engine temporarily smooths each voice's pressure, MPE bend and
timbre on every internal sample. After the stock new-note/snap handling,
a voice can skip those three recurrences only when all three float targets
and all three double histories have the **raw bits of positive zero**. Negative
zeros, subnormals, nonfinite values, live controls, and an unsettled return to
zero take the original path. Target capture, age handling, release, global
smoothers and later target restoration remain unchanged.

In the admissible case stock produces exact zero throughout and snaps to
the target; the coefficient is positive finite normal within the guarded
engine-rate range (and initialized to one before preparation). No floating
exception or changed sign of the externally retained zero is required by
the fast path. V2 checks target bits first and visits the second set of three
histories only if it still might qualify.

## Work and scope

[Measured counts](integration-work.json) compare the exact oscillator-only
main `ecf3ef538e85ca0abd90fa0806276b23725d7796` with two controller candidates
in genuine VST3 callbacks. The independently reproduced Linux x86-64 GNU
14.2 Release stock ELF is `21513fd48aa95593ce44e52352d83434419ab27c3a10e6cbd786eb98755d4095`;
V2 is `cdc825de45605e73b865564310463071db930416cb3b4da959fd889ab94cbc89`.

| Four 48 kHz/64-frame callbacks | V2 guest instruction change |
| --- | ---: |
| Fresh quiet | −3.154% |
| Eight held notes, equal / varied velocity | −0.733% / −0.728% |
| Varied wake | −0.545% |
| One moving key | −0.609% |
| All eight keys moving | +0.080% |

The earlier combined classifier added 0.208% in the last case. The separate
eligibility build observed 4,096 of 4,096 groups on static histories, 3,584
with one moving key and zero in the all-moving case. A/A and the exact
extra-callback checks passed; no native elapsed CPU or physical-DAW
improvement is claimed.

## Equivalence exercised

- [48 kHz native histories](integration-long-compat.json): 112,640
  callbacks, 7,208,960 stereo frames and 113 public-state snapshots per arm
  matched with equal and varied held/released/wake histories.
- [Other rates](integration-cross-rate.json): 44.1, 88.2, 96 and 192 kHz
  in realtime and offline, including automation and restore.
- [Directed actual-module cases](integration-module-exceptions.json):
  1,600 cases, including 448 actual floating-point interruptions, matched
  exception classes, interrupted mnemonics, controller/voice state and
  continuation; 3,462,400 compared bytes per arm.
- Smoothing, MPE master smoothing and bend routing, per-note pressure,
  member onset and released-target tests passed on the selected source.

These are tested finite workloads plus directed exceptional cases, not
an exhaustive certification for all compiler builds and histories.
