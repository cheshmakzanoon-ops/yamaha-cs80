# Preserve ARP mode identity across an engine reset

Base: `cfa9073`.

An engine reset cleared the ARP transition marker to false while leaving the
effective ARP parameter enabled. If a fresh chord was played after reset, a later
unchanged parameter synchronization was incorrectly treated as enabling ARP and
cleared that new chord.

Reset now synchronizes the transition marker to the effective mode. Reset still
clears voices, held keys, sequence, phase, gate and controllers as before. A real
subsequent enable or disable remains a performance-mode transition. This change
does not preserve the pre-reset performance or change any parameter value.

The regression covers all five ARP modes, Legacy/Circuit policies and scan option
off/on at 48 kHz. Each fixture confirms reset clears its previous voices, starts
a new two-channel chord, compares 9,000 frames after an unchanged parameter sync
against an untouched reference, and verifies an actual disable still releases
the pattern. The pre-fix test reports 22,553 checks with 20 failures, one audio
mismatch for each fixture.

The fixed fresh Release build passes all 22,553 tuning/MIDI checks and all three
targeted CTests: core, scanner ordering and tuning/MIDI. This increment does not
claim a full suite, native host, sanitizer or realtime-performance qualification.
