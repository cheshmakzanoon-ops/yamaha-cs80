# Master-sustain host automation

## Append-only candidate capability

This candidate appends existing `P_masterSustain` at lane36, for 37 lanes.
The entire preceding36-lane roster stays fixed. The sole production change
is the capability list; parameter ID, range0–1, zero default, linear conversion,
DSP, UI and shared renderer remain unchanged. All29 old contracts are retained.
Historical portamento/glissando gates accept a larger runtime roster while
retaining exact contract36 and complete preceding-order assertions.

## Existing live release semantics

`Engine::refreshEnvelopeTimes` uses
`max(rankRelease, keyRelease, sustainGate * masterSustain * 10)` multiplied
by existing envelope-time scale and tolerance. Modern control-frame updates
refresh these timing ports for active amplitude envelopes and filter envelopes.
The existing `setTimes` preserves stage, capacitor-state proxy and elapsed time.
Note Off also sets this same maximum, but release timing is **not latched only
at Note Off**. No new DSP law or measured hardware-potentiometer law is implied.

The fixture holds sustainMode0, uses no pedal-held events, and neutralizes
other variation. Its initial master value .12 is held at samples0 and3100,
after every existing note-off at3072..3087. Targets are .04@3327, .20@3583,
.06@3839 and .15@4095. This isolates a finite already-released tail window.
Queued versus matched active-constant renders must be exactly equal before
sample3100 and show a positive PCM difference afterward. The same matched
control avoids different initial publication paths. This does not measure
complete release duration or hardware response.

### Source-derived release floors

PANEL13 and default MEMORY11 have filter release .30s and amplitude release
.28s; keyRelease is .18s. Factory0 uses T51/A in rank0 and T53/A in rank1.
`HardwareControl.h` output15 has 1.2k/direct branches, output21 has 1.3k/4.7k.
The engine uses ordinary `compileTone`, not the refined joint-column model:
`x=1/(1+R/10000)` and `release=10*10^(-3*x)`. Thus factory filter/amplitude
release values are approximately .02096180/.02213771s (rank0) and
.01/.09102982s (rank1). Every resistor is below5k, proving all these values
are below .1s independently of decimal rounding. The overall rank/key floor
is at most .30s, while the entire queued performance term is at least .4s.
These are source-model arithmetic facts, not measured hardware timings.

A separate fixed control uses sustainAssign1, footswitchPlugged1,
footswitchPressed0 and glideAssign0. The open assigned sustain gate removes
only the master performance term. Queued versus matched active-constant
master values are compared for PCM-output equality only; no internal-state
or later gate-switch equivalence is asserted.

## Bounded native plan

`cs80_vst3_master_sustain_automation_tests` registers 44.1/48/96kHz in
realtime/offline mode with CLI
`MODULE ORIGINAL_CONTRACT NEW_CONTRACT OUTPUT_DIRECTORY RATE MODE` and a
separate compatibility-only mode. Sources13/0/11 and capacities8/16 give
36 full cases over4096 frames; capacity8 uses two notes, capacity16 sixteen.

The plan is756 renders in12 native processes:36 full cases with15 renders
each, plus36 frozen36 compatibility cases with6 each. Full cases contain
six no-queue/policy observations, scalar plus five alternative partitions,
one matched active-constant render and two gate-off renders. The108 actual
candidate/frozen36 no-queue and Legacy/PURE capture pairs require byte equality.
Original numerical thresholds remain unchanged.

Registration or compilation alone is not runtime evidence. Broad, malformed-input, fuzz, negative-baseline,
GUI/DAW, AU ramps, CPU/benchmark, full release, hardware and release-wide
qualification remain excluded. No historical fixture rerun is implied.

## Verified local result

The candidate VST3 and supported host build completed with exit0. All12 direct
native runs passed:36 full cases and36 compatibility cases,756 renders total.
The108 actual candidate/frozen36 PCM pairs are byte-identical. Maximum observed
partition error was0 (unchanged threshold2e-6). Released-tail positive effects
ranged from .00140975 to .00932625; all36 pre3100 prefixes and all36 assigned-open
gate PCM controls were exact. No failed attempts or runtime retries occurred.

[Retained verification](experiments/master-sustain-automation/verification.json)
records commands, full logs, source/binary identities and actual capture hashes.
SDK, compiler, Python and source identities were recorded before compilation
and checked after runtime; newly built host and candidate/frozen module identities
were recorded before runtime and checked afterward. Final documentation was
updated only after that provenance check. All29 old contracts, prior exporters,
shared renderer and production files other than the capability header remain
byte-identical to the base. The two historical fixture guard changes were reviewed
but those historical suites were not executed here. CMake registration is distinct
from these direct runs; no CTest execution is claimed. Raw PCM and executables
remain in the local ignored build directory, so the committed evidence is not
a complete replay bundle. Independent final evidence review gates publication.
