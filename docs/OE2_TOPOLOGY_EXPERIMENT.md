# Clean-main OE1/OE2 topology experiment

2026-11-03. Based only on main `db24a47170610f81236e8829ef5536af69dfc94d`.
**Experimental core API, not a production selection or calibrated hardware model.**
No integration commit/history, old evidence artifact, parameter, Engine route,
preset, state schema, existing test or tolerance is imported or changed.

## Two source-supported connections

[Yamaha detailed OE2 p57, KEC4454-6Z](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/57%20-%20OE2%20Circuit%20Diagram.jpg)
shows upper reconstruction **IC5 pin 2 feeding IC7/O1 and IC10/O4**; lower
**IC6 pin 13 feeds IC9/O2 and IC8/O3**, through their 1 uF / 100k input paths.
OE1 sums O1+O2 and O3+O4, so both reconstructed nodes reach both output sums.

[Detailed OE1 p55, KEC4453-6Z](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/55%20-%20OE1%20Circuit%20Diagram.jpg)
shows the 47k/47k IC4 stage inverting **LO1 -> LO3**, and IC6 similarly inverting
**LO2 -> LO4**. OE57 continues L1->IC7, L2->IC9, L3->IC8, L4->IC10 without a
further swap. The [overall foldout, identifier 002617](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/cs80_foldout_schematic.jpg)
independently corroborates both graphs.

For supplied U/V node samples and branch gains, the topology is therefore:

```
L = g1*U + g2*V
R = g3*V + g4*U
relative phase markers = [x, y, -x, -y] in O1, O2, O3, O4 order
```

Markers are dimensionless independent test inputs, not measured DO/LO voltages.
This does not establish absolute gain/sign, control scaling, bias, clipping,
loading, trims or a full analog transfer. The
[Yamaha CS Series Servicing Guide](https://www.goltsynth.com/resources/downloads/CS/cs_series_servicing_guide.pdf),
PDF pp49-50 / printed pp40-41, distinguishes IG00151's exponential pin-1 and
linear pin-2 controls. OE57 L1..L4 use pin 2; this qualitative identity does not
calibrate the installed network or establish a universal voltage/gain equation.

### Preserve the conflicting evidence

- Overall foldout 002617 grounds SPEED VR3 terminal 1; detailed
  [Panel66 KEC-4479-68](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/66%20-%20Panel%203%20Circuit%20Diagram.jpg)
  leaves terminal 1 unconnected and draws a separate C contact. Neither reading
  establishes installed continuity. No cause or revision chronology is inferred.
- OE55 revises oscillator control feeds to 180k while OE56 PCB printing retains
  100k; the foldout simplifies this region.
- OE57/OE58 show upper IC5 feedback 39k, versus 100k on the foldout; lower IC6
  is 100k in both. Output capacitor annotations and some connector numbers also
  differ, including detailed O3(8) versus foldout O3(38).

The two graph observations survive those differences. Arbitrarily combining
component values into a supposedly calibrated response does not. The
[Vintagesynthparts service PDF](https://vintagesynthparts.com/wp-content/uploads/2018/01/CS-80-Service-Manual.pdf)
repeats the detailed sheets at lower resolution, not an independent hardware
revision. No resolving Yamaha amendment or hardware measurement was obtained.

## Compatibility boundary and independent paths

Main originally has only `process(on, tremolo)`. It still selects the historical
same-chain grouping, crossed right phase pair and original arithmetic. The
pre-mix oscillator, clock, packet, filter, noise and state operations remain
main's own implementation, without integration optimizations.

The separate `processExperimentalIndependent` requires explicit chorus/tremolo
booleans and an `ExperimentalTopology` selection:

- `historicalGrouping`: controlled comparator using the old final mixer.
- `sourceGraph`: the documented crossover and relative inversion pairs.

Neither is wired to Engine, host parameters or serialized state. Both choices
share the same state equations and independent logical control inputs. **BOTH is
new experimental functionality on main, not an existing main-session state.**
The historical comparator is proved byte-identical to frozen main only in its
existing OFF/CHORUS/TREMOLO states. Physical panel switches appear cascaded;
these independent booleans do not resolve or reproduce that switching law.

Both choices retain the existing **unmeasured engineering assumptions**: affine
VCA modulation, full-depth index, wet gain/polarity, O1..O4 weights
0.65/0.58/0.58/0.65, normalized chain filters, noise model, clock law, SPEED
mapping and startup phase. Adjustment output amplitudes are not automatically
calibrated gain coefficients. Current equal normalized chain processing also
does not resolve the differing printed IC5/IC6 gains. No constant-mono,
improved-sound or hardware-parity target is introduced.

## Registered tests and fresh main results

The [clean-main plan](evidence/oe2-topology-experiment/PREREGISTERED_PLAN.md) was
saved before the candidate code edit, SHA-256
`a10d45ab465f7c29bb6fb9d4bd986cd81e91ab27495dc363c7145e49c4eabeb2`.
The registered route tests remain at hashes
`173a5d0747b8d61e9bbe32a121e296dcfd98430f68bb8df53efb304971a5e895` and
`fd17d2c9e613f7667dd267025a98ae6b8f09d219691effb7efaa7cefb7a6ef97`.
All reported measurements below are from this main-based candidate, not reused
integration qualification.

```
python3 tests/diagnostics/run-oe2-topology-experiment.py --output /tmp/oe2-main-check --audio
```

The runner verifies the frozen main header SHA-256
`559adda6ec5da6334ba19592e05189cb5f8dc953608937a483a3da9e4557c4c1`, renames
only its namespace and checks that the shared Dsp.h dependency is unchanged.
Route injection changes only supplied phase markers and wet nodes. A separate
observation-only test records actual oscillator/clock/wet values without
replacing them. Source, script, generated-header, binary, log and audio hashes
are recorded in the run manifest.

GCC 14.2.0, Linux x86-64, C++20, `-O2 -ffp-contract=off`:

| Check | Result |
|---|---|
| Literal route/phase basis and injected candidate | 61 checks passed |
| Unequal signed branches with simultaneous modulation | 8 checks passed |
| Wrong grouping / crossed phase / swapped O3-O4 mutants | All rejected by both route suites |
| Main-default and existing-state comparator vs frozen main | 2,468,160 channel checks, zero bit differences/nonfinite; state size unchanged |
| Actual independent-path observation | 144,000 samples, including 36,000 BOTH; zero failures |
| Disable clock modulation only in BOTH | Rejected, 36,000 failures |
| Disable VCA modulation only in BOTH | Rejected, 36,000 failures |

The independent-path test uses normalized clock/phase bounds of 1e-10 and output
bound 2e-7. Worst errors were 4.88943e-13, 3.54561e-12 and 2.15625e-8,
respectively. These are software arithmetic checks, not hardware tolerances.
Frozen comparisons cover 88.2/96/192 kHz, parameter changes, depth endpoints,
asymmetric input, idle, off/on, copy/reset and return from experimental selection.

The unchanged main BBD, OE2 filters and OE2 stereo CTest suites all passed;
main has no chorus-tabs test and none is imported. Separately compiled full Engine
renders cover all three policies and existing on/mode selections at 44.1/48/96 kHz
with note/control transitions. All 221,184 stereo frames match byte-for-byte, with
no nonfinite samples. Both used GCC 14.2.0, `-O3 -DNDEBUG -ffp-contract=off`; the
float-output SHA-256 is
`34597a6e2b874f5b4644ab7e5a3b058141f288d615db36acbfacfd19091a5319`.
Supplementary UBSan runs passed the frozen-main and independent-path suites.
A fresh ASan/LeakSanitizer run was blocked because the executor is ptraced; leak
checking was not disabled and no ASan qualification is claimed. This is not
complete host/platform, performance-budget or release qualification.

## Matched audio, not an improvement claim

The audio runner emits six-second 96 kHz stereo float WAVs: 0-2 s mono multitone,
2-4 s asymmetric decaying transients, 4-6 s silence. Historical comparator and
source graph use identical inputs, states and noise seeds, with no separate
normalization. Existing-state comparator samples are checked bit-for-bit against
frozen main while rendering; BOTH is explicitly a new controlled experiment.
Repeated renders match byte-for-byte, all outputs are finite and OFF is exact.
The analysis requires NumPy and reports RMS/peak/mid/side/correlation without a
listening verdict or real-instrument error metric. Fresh whole-render difference
RMS/peak values are: OFF 0/0; CHORUS 0.000029183/0.000132132; TREMOLO
0.00163813/0.0165640; experimental BOTH 0.00163804/0.0165600.

Under the retained model, static crossover plus the current weights makes the
same wet-noise sum reach both sides in CHORUS, so settled-noise correlation is
1 rather than the historical near-zero value. This is a consequence of assumptions
held constant, not a claim about a real CS-80. Existing legacy independence gates
remain unchanged. Future analog work needs an explicit mixing-covariance oracle,
not a widened old threshold.

## Source reproduction

| Full source | Dimensions | SHA-256 |
|---|---|---|
| OE55 | 5100 x 3300 | `524aaba53696004178b00f653ee2b69c736d957c589bb7641f67274859c76ef4` |
| OE57 | 5100 x 3300 | `9d3060b05899526469cbff8e65e5abbde116230fb4951580d9abd25902db72a9` |
| Foldout 002617 | 12185 x 5972 | `23de2fc4ef4bbbc6c171979aa586a2f1c1fb11d7f6116656153a752b31709b38` |

Native top-left-origin rectangles: OE57 crossover `(2700,850,4050,2350)`, VCA/control
`(2550,400,4750,2950)`; OE55 amplifiers `(2700,300,4700,3100)`; foldout OE2
`(7700,2050,9850,2850)`, OE1 controls/inverters `(8380,1150,9850,1900)`.
No hardware waveform capture or calibrated instrument comparison was made.
