# Publish the uniform cutoff result once

This is **separate** from the certified shared Q/level transaction. The
existing cutoff solver validates both sides (including that both buses, pins
and CVs are finite) before it publishes. At that point it holds one HP/LP pair
each for bus, pin and CV. The engine now writes those finished pairs directly
to their destinations instead of building, filling and copying a second
eight-voice result. If either side fails, nothing has been written. The scalar
math, check order and previous cutoff on rejection stay intact. The public
evaluator and non-qualified engine continue using staged publication. The
engine opt-out is `CS80_DISABLE_COMPACT_UNIFORM_CUTOFF`.

In an isolated GCC 14.2 Linux/x86-64 Release/PIC comparison, the prior
[certified receiver](README.md) is **present in both arms**. Four 48 kHz
64-frame **core renders** at fresh factory quiet changed guest Ir/reads/writes
from 3,877,208 / 991,829 / 525,888 to 3,781,976 / 957,013 / 454,208
(−2.46% / −3.51% / −13.63%). Equal-held and single-held instruction counts
fell 0.21% and 0.11%; Ir, reads and writes were unchanged in the other six
selected scenes. This is translated instruction work, not CPU time or an
absolute plug-in meter. See [the complete comparison](compact-cutoff-work.json),
including duplicate and exact four-plus-one controls.

The [directed check](compact-cutoff-check.cpp) compares direct versus staged
transactions for all 22 factory tones, rejected calibration/provenance and
non-finite late LP maps, saturated drives, signed zero, subnormals, all four
MXCSR rounding modes, DAZ/FTZ and sticky flags. All 65,536 returned decisions,
destinations, uniformity flags, MXCSR/x87 and errno matched, including
55,159 accepted transactions and 14,045 uniform entries. The 20,480 Engine
handoffs, 22 interruption cases (seven signals), six longer render histories
and nine native and translated core scenes also matched. Bounds are finite
tests of selected state, not universal numerical proof.

Run the direct test from repository root (Debian GCC 14.2 x86-64):
```sh
g++ -std=c++20 -O3 -DNDEBUG -fPIC -Isrc docs/experiments/certified-shared-receivers/compact-cutoff-check.cpp -o /tmp/check-cutoff-publication
/tmp/check-cutoff-publication
```
The [core probe and comparison recipe](../engine-lazy-records/README.md)
apply without changing the measured Engine objects. Do not combine percentages
from this controlled pair with those of a different arm or a whole host.
