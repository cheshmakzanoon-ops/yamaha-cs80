# HIGH keyboard-level automation

## Append-only candidate

Existing `P_kbdLevelHigh` is appended at lane39, for40 capability lanes.
The previous39-lane prefix, metadata, physical -1–1 range, zero default and
linear conversion remain unchanged. Production changes only the capability
list; DSP, smoothing, UI and shared renderer are unchanged. All32 earlier
contracts are preserved. The historical LOW fixture accepts a larger runtime
roster while retaining exact contract39 and full prefix/order checks.

The global HIGH lever is smoothed. Its changing value refreshes held KBC
nodes and existing direct or factory shared-bus LC receiver paths. It is not
latched only at note onset and does not introduce a new circuit equation.
Every engine's eight nodes contribute to its shared loading, so no
individual-key invariance or calibrated hardware response is assumed.

## Responsive physical-key cohort

The unchanged supported renderer explicitly uses `noteBase=62`: capacity8
has notes62/63, capacity16 notes62..77. All are inside the physical24..84
keyboard range. Mapping subtracts24, giving physical38..53:

- Physical38..48 use G4/state1/taps1..11. Output is
  `(2*m/33)*(r-H)`, strictly nonzero for nonzero HIGH travel.
- Physical49..53 use G5/state5/taps0..4. Output is
  `(r-H)*(1-(11-m)*1000/(31000+VR2))`, with VR2=4136.2862 and a
  positive factor at least .6869.

These formulas come from `KbcContour.h` and the existing keyboard-contour
reference. Cold unused MIDI60 is physical36/G3 and remains neutral. Both
engines' active cohorts have same-sign excitation; positive own/shared
factory receiver coefficients do not cancel it. MIDI61 is deliberately
excluded because it maps to G4 tap0, an insensitive endpoint. These are
source-model facts, not hardware calibration or independent numerical proof
of every contour point.

## Bounded gate

LOW stays0. HIGH physical0 (normalized .5) is held at samples0 and64, then
physical targets .4/-.3/.25/-.4/.3/-.2 occur at255/767/1535/2303/3583/4095.
The existing normalization helper converts signed physical values. A matched
active-constant queue must retain exact PCM before64 and a positive later
aggregate difference; this does not attribute every shared-bus contribution.
Scalar and five partition renders retain the original threshold. No
unsupported inert-control or full keyboard-response claim is added.

`cs80_vst3_kbd_level_high_automation_tests` registers44.1/48/96kHz,
realtime/offline, with standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` and
separate compatibility-only mode. Sources13/0/11 and capacities8/16 give36
full cases over4096 frames. The plan is684 renders in12 processes:36 full
cases with13 renders each (six compatibility observations,six scalar/partition
renders,one matched activeconstant), plus36 frozen39 compatibility cases
with6 each. All108 actual no-queue/Legacy/PURE capture pairs require byte
comparisons. No helper change or default-helper historical rerun is included.

Registration/build alone is not runtime evidence.
Broad, malformed-input,fuzz,negative-baseline,GUI/DAW,AU ramps,CPU/benchmark,
hardware,full contour and release-wide qualification remain excluded.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:36 full cases and36 compatibility cases,684 renders total.
The108 actual candidate40/frozen39 PCM pairs are byte-identical. Maximum observed
partition error was0 (unchanged threshold2e-6). Later positive effects ranged
from .00139232 to .0276816; all36 pre64 stereo prefixes were exact.
No failed attempts or runtime retries occurred in this HIGH qualification.

[Retained verification](experiments/kbd-level-high-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; newly built host and candidate/frozen module identities
were recorded before runtime and checked afterward. Final documentation was
updated only after that provenance check. All32 old contracts, prior exporters,
shared renderer and production files other than the capability header remain
byte-identical to the base. The historical LOW fixture guard was reviewed but
that historical suite was not executed here. CMake registration is distinct
from these direct runs; no CTest execution is claimed. Raw PCM and executables
remain in ignored local build directories, so the committed evidence is not
a complete replay bundle. These aggregate waveform results do not qualify
isolated-key response or calibrated hardware contours. Final independent
binary/evidence review gates publication.
