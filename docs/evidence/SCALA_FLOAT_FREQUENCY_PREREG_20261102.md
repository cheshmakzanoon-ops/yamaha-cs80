# Scala nominal float-frequency safety — preregistration, 2026-11-02

`Tuning::buildTable` currently accepts any finite fractional MIDI pitch produced
from a finite double-precision frequency. The core's actual `midiToHz` uses
float arithmetic, including a float power before multiplication by 440; the
accepted pitch can therefore reconstruct to infinity or zero. This is a host
file-input representability defect, not a criticism of the Scala file format.

The [primary Scala format](https://www.huygens-fokker.org/scala/scl_format.html)
allows non-octave ratios, negative cent degrees and arbitrary trailing comments.
Do not tighten those syntax rules to make this safety test pass. The repair is
limited to rejecting a table whose nominal core float conversion is nonpositive
or nonfinite, before assigning the caller's output. Do not introduce a new
16-octave transposition/headroom policy: that requires a separate domain contract.

Before editing production, add three valid-syntax cases: one-degree 2400-cent
and 3600-cent repeated scales, plus 12TET with a finite 1e-44-Hz KBM reference.
Require an explicit build error and byte/value-preserved caller pitch/mapping
arrays. The test's diagnostic conversion calls the actual core `midiToHz`, not
a new production validation helper. Capture the baseline's accepted invalid
frequencies and failed rejection/atomicity assertions.

Positive controls retain 12TET, a five-note just scale, a non-octave tritave,
a negative interior degree and nonmonotonic interior degrees. All128 generated
pitches must have positive finite core conversions; 12TET keeps its existing
1e-5-semitone fixture bound. Save the positive pitch arrays before and after and
require byte identity. Replay KBM/count-MTS/original tuning tests and sanitizers.
No shipped preset, state interpretation, original tolerance or audio deadline
is changed. This does not claim safety under every possible subsequent pitch
modulation, or complete Scala/KBM format conformance.

Base `e56f03d10137cb5893627f630f52bffcf99d2e08`, src
`a1424ee1b4dc26d125805c9c482502ab2c790efc`. Original whole-source audit locks are
unchanged; a later composed-source qualification remains necessary.
