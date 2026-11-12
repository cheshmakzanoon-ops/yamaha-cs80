# RING time updates at the next fresh trigger

## Test-only qualification scope

This candidate gate tests the existing 33-lane [RING time capability](VST3_RING_TIMES_AUTOMATION.md).
It adds no lane, parameter, DSP, wrapper, UI or production change. Existing
attack/decay are still latched, not live envelope reshaping. All 22 earlier
capability/numerical contracts remain byte-identical; the new
`host-automation-ring-retrigger-v1.json` is a **qualification** contract.

The remaining question is whether times changed after the initial latch are
consumed at the next eligible RING trigger. The old late-control witness covered
a window without a later trigger; it did not demonstrate this next-trigger
behavior. Both scopes remain distinct.

## Opt-in supported event sequence

The shared supported-input host appends a `twoNoteCycles=false` option to
`Instance::process` and `render`. All historical calls keep their prior default.
The false branch retains the exact existing note-on at `17+key` / note-off at
`3072+key` generation. Only the new fixture opts into:

- first onsets `17+key`, releases `1024+key`;
- second onsets `2048+key`, releases `3584+key`;
- two active keys for eight-voice capacity, sixteen for sixteen-voice capacity.

Every first key has been released by1039 and every second onset occurs by2063.
With pedals, scan timing and arpeggiation off, the existing engine last-held /
pedal-held release logic clears `ringKeyDown` separately in main and extension
engines. The next qualifying onset latches current global times. Release tails
may still exist; this is not a claim of zero audio or reset of all voice state.
There is no production latch change or new trigger mechanism.

## Separate witnesses

The main attack-only or decay-only trajectory holds its initial normalized value
through64, then reaches its target before the second onset (at1536). Its
comparator has the **same active parameter lane**, initial normalized value,
partition and two-cycle notes, but retains that value throughout. This avoids
confounding active conversion with no-queue parameter initialization.

- Interleaved PCM for sample interval **[0,2048)** must match exactly.
- A positive aggregate effect over **[2048,4096)** shows the later trigger can
  consume changed times. It is not a per-parameter global accuracy or hardware
  claim; actual active source and capacity cases must be recorded.
- A separate step trajectory publishes target at2048 and compares with target
  held since2047. Both hold target through2063, covering every second engine
  onset. This tests the existing same-sample automation-before-MIDI ordering,
  not the earlier target-by1536 trajectory.
- Per-sample references and bounded partitions use the original numerical
  tolerances. No normalized-to-seconds linear mapping is assumed.

The unchanged default event path is checked separately by no-queue and policy
PCM captures against its frozen historical host on the same frozen33 module.
Those compatibility observations are not a substitute for the fresh-trigger
witnesses. Historical malformed-input and broad bodies remain excluded.

## Build and evidence boundary

`cs80_vst3_ring_retrigger_automation_tests` has six CMake registrations at
44.1/48/96 kHz, realtime/offline, using the standard module/original-contract/
new-contract/output/rate/mode CLI. The independent fixture records exact33 and
complete roster assertions. As this is test-only, direct supported-host builds
against the official SDK can reuse the already qualified frozen33 module; a
new plugin build is not required or implied. CMake registration is not execution.

The bounded matrix is 18 processes / 1,944 renders, including 216
separate default-helper PCM pairs. Exact source/host/module identities, build
commands, outcomes and source/capacity settings must be retained before any
passing claim; the retained local result is below.
No production, GUI/DAW, broad, malformed-input, fuzz, CPU/benchmark or hardware
qualification follows from this scope.

## Verified local result

[Verification, commands and logs](experiments/ring-retrigger-automation/verification.json)
record direct official-SDK compilation of the new retrigger host and the
historical time host with the updated helper. An initial compilation failed
because the explicit-off parameter list mixed enum types; no native process ran.
The runtime guard rejected an invocation before launch. The original source and
logs are retained. Independent review approved a one-line `std::array<int, 8>`
type correction, after which both hosts compiled successfully. No production
binary was rebuilt, and no runtime failures or runtime retries occurred.

All 18 planned native processes exited 0. The 72 full cases cover isolated
attack/decay, sources 13/0/11, capacities 8/16 and 44.1/48/96 kHz in realtime
and offline modes. Every pre-second-trigger PCM prefix, frames [0,2048), was
exactly equal to the matched constant-active control. Every partition and
second-onset 2048-versus-2047 comparison had error 0. Post-trigger effects over
[2048,4096) ranged .0404978–.416059 for attack and .0320216–.55328 for decay.
These bounded observations do not establish a universal numerical bound.

All 216 actual no-queue/Legacy/PURE PCM pairs from the new and frozen historical
hosts matched exactly on the same frozen 33-lane module. This establishes only
the stated default-helper compatibility scope, not a rerun of every historical
automation assertion. Runtime source/host/module hashes were unchanged. All
production sources, prior scripts and 22 existing contracts match the base.

The compiler and SDK identities are recorded. The SDK hash inventory was taken
after compilation, during the matrix, not before compilation. CMake targets
were edited but no CMake configure/build or CTest execution was performed here.
Raw PCM and host executables remain locally in ignored build directories; the
committed hashes/logs are not a complete replay bundle. No negative baseline,
broad, malformed, fuzz, GUI, DAW, AU, CPU or hardware tests were run.
