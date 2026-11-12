# Keyboard BRILLIANCE LOW/HIGH automation

## Append-only paired candidate

Existing `P_kbdBrillianceLow` and `P_kbdBrillianceHigh` are appended at40/41,
for42 capability lanes. The preceding40 entries, IDs, physical -1–1 ranges,
zero defaults and linear conversion are unchanged. Only the capability list
changes production; DSP, smoothing, UI and shared renderer are untouched.
All33 previous contracts remain intact. The historical HIGH level fixture
accepts a larger runtime roster while preserving its exact contract40 and
full original prefix/order checks.

## Existing live filter-control path

Both global levers are smoothed. Changing smoothed values refreshes KBC1
held-key nodes, not only future onsets. Their voltages feed separate HP/LP
cutoff-control receiver maps: PANEL/MEMORY have direct gains39/120 and1;
factory ranks use existing own-node and shared-bus contributions. Factory
map coefficients `a,t` and effective conversion `1/a` are positive; shared
bus response is nonnegative. Same-sign cohort excitation therefore does not
cancel at that map. Engine adds these per-side contour CVs to the existing
HP/LP cutoff controls. This is not the LC level path, and no circuit or
calibrated cutoff law is introduced.

## Source-proven responsive cohorts

The two levers are tested **in isolation**, not simultaneously. LOW uses the
unchanged helper with MIDIbase24: capacity8 has24/25; capacity16 has24..39.
These map to physical0..15. G0 response is `r-L`; G1 response is
`(r-L)*(1-m*1000/(33000+VR1))` for taps0..11; G2 response is
`(2/3)*(r-L)*(1-m/11)` for taps0..2. All factors are positive.

HIGH uses MIDIbase62: capacity8 has62/63; capacity16 has62..77, physical38..53.
G4 response is `(2*m/33)*(r-H)` for taps1..11; G5 response is
`(r-H)*(1-(11-m)*1000/(31000+VR2))` for taps0..4. These factors are also
positive. All notes are inside the actual24..84 keyboard range; cold unused
MIDI60 is neutral. The opposite brilliance lever and both LEVEL levers stay0.
No individual-key invariance is assumed because shared loading remains live.

These facts establish source-model excitation, not a mathematical guarantee
that any rendered PCM difference exceeds its threshold. The positive waveform
witness remains mandatory; any first failure is retained rather than weakening
the gate. No hardware calibration or full-keyboard response claim follows.

## Bounded native plan

Each physical lever starts0 (normalized .5), held at samples0/64, then follows
.4/-.3/.25/-.4/.3/-.2 at255/767/1535/2303/3583/4095 through the existing
normalization helper. A matched active-constant control must give exact PCM
before64 and a positive later aggregate effect. Scalar plus five alternative
partitions retain the original threshold. There is no unsupported inert control.

`cs80_vst3_kbd_brilliance_automation_tests` registers44.1/48/96kHz,
realtime/offline, standard CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` and
separate compatibility-only mode. Two isolated levers, sources13/0/11 and
capacities8/16 yield72 full cases over4096 frames.

The plan is1368 renders in12 processes:72 full cases with13 renders each
(six no-queue/policy observations,six scalar/partition renders,one matched
activeconstant), plus72 frozen40 compatibility cases with6 each. The216
actual no-queue/Legacy/PURE capture pairs require byte comparisons. No helper
change, default-helper historical rerun or simultaneous-control gate is implied.

Registration/build alone is not runtime evidence. Broad,
malformed-input,fuzz,negative-baseline,GUI/DAW,AU ramps,CPU/benchmark,hardware,
full contour calibration and release-wide qualification remain excluded.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:72 full cases and72 compatibility cases,1368 renders total.
All216 actual candidate42/frozen40 PCM pairs are byte-identical. Both isolated
lanes had maximum observed partition error0 (unchanged threshold2e-6). LOW effects
ranged .0100588–.104127 and HIGH effects .00195321–.0426371, with36 cases each.
All72 pre64 stereo prefixes were exact. No failed attempts or runtime retries
occurred in this qualification.

[Retained verification](experiments/kbd-brilliance-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; newly built host and candidate/frozen module identities
were recorded before runtime and checked afterward. Final documentation was
updated only after that provenance check. All33 old contracts, prior exporters,
shared renderer and production files other than the capability header remain
byte-identical to the base. The historical HIGH fixture guard was reviewed but
that historical suite was not executed here. CMake registration is distinct
from these direct runs; no CTest execution is claimed. Raw PCM and executables
remain in ignored local build directories, so the committed evidence is not
a complete replay bundle. Isolated control-lane tests are not simultaneous-lane,
isolated-key, hardware-cutoff or calibrated contour qualification. Independent
final binary/evidence review gates publication.
