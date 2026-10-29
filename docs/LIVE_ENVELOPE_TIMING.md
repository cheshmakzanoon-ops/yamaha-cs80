# Live envelope timing controls in isolation mode

2026-10-29. **Core implementation and independent regression review passed; host-level verification is tracked in exact-commit CI.** This is a correction to live control routing through the existing provisional numerical envelopes, not a calibrated IG00152/IG00159 model.

## Reproduced gap

The effective rank parameters change when PANEL controls, a selected memory or a tone selector changes, but active envelope objects previously copied attack/decay settings only at note-on and release settings at note-off or a sustain-gate cancellation edge. A later release-control change therefore did not affect an already releasing voice. The same audit found latched sustain/level targets; those remain outside this bounded timing-only correction.

## Primary-source basis

The [receiver ledger](PRESET_RECEIVER_PORTS.md) records the common M circuit and native source provenance. M41 shows continuously connected timing-control wires: IC12 AF→AT9, DF→1DT10, RF→2DT11; IC13 AA→AT9, DA→1DT10, RA→2DT11. Separate trigger pins control the stages. No external sample/hold is drawn on these timing inputs. The [release network](RELEASE_CONTROL_ROUTING.md) connects the raw rank release controls through SUB's switched selector before the M-card release inputs.

The M41 source SHA-256 is `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`. Native VCF-EG crop `[1550,2000,2430,2870]` has SHA-256 `4dc098b0695e917b067e4f93ee587708351f87acd83b0d004f247e867f1f55d3`; VCA-EG crop `[3200,1920,4030,2850]` has SHA-256 `32ca6d5d05d41a0e46326c0e6e4e95adb6536d08a37de7795aac28f77f4cbc7f`. These originals and crops were independently rechecked during this review. The source supports connected controls, not a measured transition time or exact numerical stage law.

## Bounded implementation contract

- Only opt-in isolation mode changes. Legacy-off automation/rendering retains its prior behavior.
- Refresh active rank envelopes' attack, decay and release times from the effective current settings without calling note-on, note-off or reset.
- Preserve current envelope value, stage and elapsed stage time at the setter boundary. Do not change voice allocation, held/pedal/sustained flags, oscillator phase, filter state or random-number state.
- Reuse the existing time clamps, mappings and release-floor policy. Release uses the current sustain gate, not a remembered enabled gate. A disabled gate must remain disabled on repeated unchanged parameter applications.
- Re-enabling the gate can alter a still-active release's coefficient, but never resurrects an inactive voice or restores hold flags. Existing sustain-II allocation policy is unchanged.
- Ring-envelope behavior is outside this patch.

## Explicit remaining limitations

The current `coefficientForTime` mapping, fast-bend windows, attack timeout and release threshold remain provisional. Changing attack time may cause the existing next-tick elapsed-time completion rule to fire; the setter itself must not reset or advance the stage. This behavior is not a measured analog transition.

VCA sustain level and filter initial/attack/steady targets are deliberately not refreshed by this patch. The existing Sustain-stage assignment would jump directly to a changed target; inventing an arbitrary slew or applying a decay law to that change would not establish the real circuit's behavior. Those live-target transitions remain an explicit open gap, requiring a justified state law and validation. A/D/R timing refresh does not close all live-envelope controls, all preset transitions, or W5/W10 parity.

## Verification checkpoint

`cs80_live_envelope_timing_tests` passes 91,672 checks at 44.1/48/96/192 kHz. Tests snapshot exact stage/value/elapsed state at the setter boundary, exercise both ranks during attack/decay/release, compare next-step behavior with the explicitly provisional law, test physical sustain gate cancellation/reopening, verify no inactive-note resurrection, and compare repeated setters and block partitions bit-for-bit. Deferred level targets are tested as unchanged.

Six targeted mutations were detected: leaving timing latched, enabling refresh in legacy mode, resetting elapsed time, resetting value, ignoring the sustain gate and resetting oscillator phase. Both complete Release and ASan/UBSan core-only CTest runs passed all 18 suites; local leak detection was off, so this is not a leak-clean claim.

Same-toolchain raw float comparisons against pinned baseline `d56911a7c5f6953cd244457226b126cc567a538b` matched exactly for all 222 shipped presets at three sample rates in both scenarios:

- Standard corpus SHA-256: `4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de`.
- Noisy reassignment/tail corpus SHA-256: `fe447882e3ea3560eefb7b5ae4a474b151efe5ced775b5ab36f2609c5fde0660`.

These hashes describe this compiler/build comparison, not a cross-platform hash promise. Default-off dynamic timing is separately checked against its existing latched behavior. None of these tests compares the provisional envelopes to hardware.

Host smoke regressions additionally exercise attack, decay and release with notes actually active, in both isolation and legacy modes and both ranks. PANEL→MEMORY→factory tone→PANEL switches, selected/unselected memory edits, zero-frame state continuity and separately advanced primitive histories test the processor's effective routing. The fixture explicitly sets the session isolation flag because ordinary preset application deliberately preserves it. These JUCE tests require the exact-commit CI build; local core-only results are not a host-test execution claim.
