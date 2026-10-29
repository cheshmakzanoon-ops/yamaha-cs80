# Isolation envelope numerical accuracy

Audit of pre-fix main `8662c3c`, reproduced with GNU C++20 Release builds. No hardware required: these failures violate the implementation's own exponential law and termination conditions.

## Valid domain
Params.h A/D/R = .001–20 seconds for both envelopes. Isolation sets macros neutral, hence envTimeScale=1. Host accepts 8–768kHz, internal 2x. The prior live-level primitive 42.87094s test is not a valid isolation maximum; all cases here use 20s and valid level ranges.

## Concrete Engine evidence
All parity enabled, default settings otherwise except masterSustain=0, both ranks VCA S=.5, filter IL=1/AL=1 (mapped -3/+3), D/R=20.

1. Host768000, A=20. After 10s both Attack outputs already plateau: VCA .791666746, filter2.16666698. At20s clocks have saturated16. At60s both still Attack, exact same values/clocks. Their fallback needs24.001s; float clock never reaches it.
2. Host48000, A=.001. At20s VCA Decay=.509765625; at60s identical value, still Decay (target.5, termination tolerance.0005). Filter zero-target decay completes, distinguishing the nonzero-target rounding failure.
3. Host48000, A=.001; noteOff at.1s. At20s filter Release=-2.96153855, still Release. It remains there until VCA deactivates the voice (~28s); frozen Release afterward is inactive voice state, NOT evidence of further ticks. VCA release completes normally.

The committed `tests/EnvelopePrecisionTests.cpp` exercises the corresponding valid-domain cases and analytic trajectories.

## Matching primitive results (valid .001s short attacks; pre-roll .02s)
After60s at internal96000: amplitude Decay .509765625; filter Release -2.96153855. At384000: .540865302 and -2.84615397. At1536000: .708333254 and -2.33333349. All remain in respective stages. Long Attack at1536000 exactly matches Engine findings.
Clock also drifts before saturation: at physical10s reports10.0532331 (96k),9.81790066 (384k),8.92562866 (1.536M). At1.536M physical20s clock16 forever. This can invalidate guard timing and live transition diagnostics even where double value accumulation exists.
Amplitude zero-target release does not stall, but differs from its own exact 20s exponential. At physical10s, ideal including .045s 5x bend is .047169867; observed .048357215 (96k), .048043821 (384k), .054295983 (1.536M); coefficient/value rounding introduces ~15.1% relative amplitude error at highest supported rate. This is algorithmic error, not hardware uncertainty.

## Implemented correction
Legacy-off arithmetic is retained in its original branch. `tick(rate, bent, reconstruction)` adds an explicit numerical path selected by Engine isolation for rank VCA/filter and ring envelopes. All stages use double state, coefficient computation and elapsed clocks. Public audio output/state snapshots remain float, but are never fed back into the reconstruction accumulator on a tick or live setter. New onset/reset initializes state; noteOff and the existing clock-resetting phase transitions reset the elapsed clock. Decay-to-Sustain and Release-to-Idle retain their prior elapsed-clock semantics. Engine mode transitions reset voices and ring state. Calling a primitive with a different reconstruction argument mid-note without reset is unsupported.
Each envelope has one reconstruction accumulator and clock, separate from legacy float state. Tick entry selects the numerical path. Existing phase policies, bend windows, endpoint thresholds, nominal Attack timeout and live-target rules are retained: this introduces no new physical envelope law. **Old isolation candidate bytes intentionally change even for untouched controls**, including ring-envelope trajectories. Legacy-off corpus bytes must remain identical.


## Verification scope

`EnvelopePrecisionTests.cpp` checks valid20s controls at96k/384k/1.536MHz internal rates, analytic trajectories, high-rate20s clock accuracy, endpoint arrival, reset hygiene, repeated setters and actual both-rank Engine Attack completion at768k host. Existing live-envelope suites retain continuity, block-partition and legacy behavior checks. Mutation checks separately reduce accumulator, coefficient and clock precision; each must fail.

Numerical accuracy does not establish the physical IC current law, threshold calibration or hardware-equivalent envelope timing. The retained exponential/bent model remains an explicitly provisional reconstruction. Hardware captures can refine that model later without blocking this numerical correction.

Local GNU Release compatibility checks retain exact pinned baseline hashes:

- Normal222-preset corpus: `4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de`
- Noisy eight-voice tail/reassignment corpus: `fe447882e3ea3560eefb7b5ae4a474b151efe5ced775b5ab36f2609c5fde0660`

Three deliberately broken numerical variants were rejected: rounded float accumulator (21 failing checks), float elapsed clock (8), and float coefficient (15). These are numerical regression controls, not hardware comparisons.

Final local verification: **22,732 focused checks**, **25/25 Release CTest suites**, and **25/25 ASan/UBSan suites** passed. Local leak detection was disabled because of the execution environment. The candidate-export suite passes all 13 tests with an independent double-recurrence oracle and explicit binary32 snapshot bounds. Core isolation checks total 125. Full JUCE/plugin execution remains an exact-commit CI responsibility.
