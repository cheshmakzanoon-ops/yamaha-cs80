# MN3001 clock-event reference: graph and bounded design

Conditional offline reference, 2026-10-29. No production-DSP change.

## Decision

An **explicitly normalized signal-packet routing reference** is defensible as an offline experiment. A reference claiming physical MN3001 pin voltages, exact sampling aperture, or actual OE2 reconstructed output is **not** established by these drawings. The distinction matters: a pair of permanently held delayed taps is not the pictured output circuit. Do not implement a generic fractional delay or infer its output timing from stage count alone.

## Primary evidence and reproducibility

Source: manufacturer-authored Panasonic BBD Series handbook, third-party-hosted at <https://www.ka-electronics.com/images/pdf/Panasonic_BBD.pdf>. Local PDF SHA-256: `a99dc2cbb7aa735ff2f69a1cc06d863ba6ac25798eada44b6558748aee445dec` (rechecked). Physical pages 5/6/7 are printed pages 3/4/5: description/block diagram, pin assignment/clock waveforms, transistor circuit respectively.

Detailed transistor inspection used physical page 7, PDF-point rectangle `[120,65,555,320]`, PyMuPDF matrix `(5,5)`, producing `/tmp/mn3001-circuit.png`, SHA-256 `8961e041986701f0449695e1eaa6e6b6b2b6741c224b1ad094db323a127812a8`. This temporary inspection image is not a publication artifact. The full-page sources remain untracked research artifacts. OE57 native crop rectangles/hashes and source URL are recorded in [OE2 evidence](OE2_CIRCUIT_EVIDENCE.md), especially `oe57-clock-bbd-wires.png` and `oe57-input-bbd.png`.

## Device pin map and OE57 connections

| Function | Chain A | Chain B | OE57 wiring |
|---|---:|---:|---|
| Input | 3 | 5 | Directly tied; same biased/coupled input |
| CP1 | 2 | 6 | Directly tied; same clock phase |
| CP2 | 12 | 10 | Directly tied; same complementary phase |
| OUT1 | 13 | 9 | Separate pair recombination for each chain |
| OUT2 | 14 | 8 | Separate pair recombination for each chain |

Shared package terminals: VBB 1, VGG 4, GND 7, VDD 11. The manufacturer allows independent clock/input terminals; the **OE57 board does not use independently clocked or independently driven chains**. The complementary clock nets come from the existing BA617/TC4049 chain; this design neither synthesizes that oscillator nor assigns its frequency.

OE57 recombines A pins 13/14 through equal 2.2 kΩ resistors, and separately B pins 9/8 through equal 2.2 kΩ resistors. Each common node has a 100 kΩ branch to the common return and feeds its own coupling/filter circuit. These are passive loaded nodes, not ideal arithmetic adders. The later IC5/IC6 gain differences remain outside this experiment.

## Transistor-level graph that must not be flattened

The input switch gate is CP2. Its following storage capacitor returns to GND. The numbered stage-1 switch gate is CP1, stage 2 CP2, stage 3 CP1, continuing odd/even alternation. Between clock-controlled switches, the drawing includes VGG-gated devices; their analog behavior is not modeled here.

The visible terminal graph is:

```text
input --[CP2 input switch]--> front storage
      --[CP1 stage1]--> ... --> upper509
                                  |\
                                  | +--> lower510 --> lower511 --> lower512
                                  |                                  |
                                  |                              lower513 --sense--> OUT2
                                  |                                  |
                                  |                              lower514 --> VDD
                                  |
                                  +--> upper510 --> upper511 --> upper512 --sense--> OUT1
                                                                         |
                                                                     upper513 --> VDD
```

The 509 fork is a **physical parallel branch**, not evidence of unity-gain duplication of signal voltage. Upper/lower stages 510 and 512 have CP2 gates; 511 and 513 have CP1 gates; lower514 has a CP2 gate. OUT1's output-transistor gate connects to upper512's storage node. OUT2's output-transistor gate connects to lower513's storage node. Both output transistors connect to VDD on their other side. Upper513 and lower514 terminate the respective transfer paths into VDD; they are not additional user-accessible held audio taps.

Consequently, `OUT1 = held stage512, OUT2 = held stage513` with an unconditional average is **not justified**: that would introduce an invented two-tap interpolation filter and ignore clearing of the sensed storage nodes. Likewise, naming the device a 514-stage FIFO is incorrect.

## Conditional idealization, not a pin-voltage model

A useful abstract convention is to represent each transferred audio deviation as a normalized packet, with an empty/reset bucket represented by zero. It assumes instantaneous, complete transfer without normalized-signal attenuation, no clock feedthrough, unity normalized signal duplication at the terminal fork (not conservation of physical charge), and identical unity normalized sensing. These are declared idealizations, not measured device properties. Initialization is an explicitly empty state, not a claimed physical power-up condition.

Under that convention:

- A completed CP2 phase acquires the caller's new input into front storage and advances all even numbered stages simultaneously from their pre-event predecessors.
- A completed CP1 phase advances all odd numbered stages simultaneously from their pre-event predecessors.
- CP2 places a packet in upper512 and lower512; OUT1 then carries the upper packet while OUT2 is empty after its terminating514 transfer.
- The following CP1 transfers the lower512 packet to lower513 (OUT2) while upper513 clears upper512 (OUT1).
- The next CP2 clears lower513 via514 and introduces the next packet into upper512.

The normalized packet sum therefore carries the same sample on adjacent CP2/CP1 slots. This is a consequence of the explicit graph plus the declared idealization, **not** proof of a physical output waveform or gain. Do not label the normalized sum the OE2 output. Even with ideal stiff pin-voltage sources, the real equal-resistor junction is load-dependent; the coupling/filter path must be solved separately.

The API event is a **completed phase transfer**, not a claim that acquisition occurs at a particular physical rising/falling edge. The manufacturer shows negative-going P-channel clocks but provides no input-aperture timing specification here. If callers require actual voltage-edge events, aperture/settling semantics are an unresolved prerequisite.

## Implemented API and tests (only for that declared abstraction)

`src/reference/Mn3001Packet.h` provides `Channel::step(time, completed_phase, input)` and `CommonClockDual::step(time, completed_phase, input_a, input_b)`, returning normalized packet pairs with explicit per-output `present`/empty state. `normalizedSum()` exposes the declared normalized sum. Input values are consumed only on CP2 acquisition. The common-clock adapter can be supplied the same input for the OE57 tied-input topology; separate `Channel` instances permit independent clocks. Neither result is a pin voltage or calibrated audio output.

Require finite strictly increasing timestamps, legal phases, strict phase alternation, and explicit initial phase/state. No assumed uniform clock period, sample-rate interpolation, clock oscillator, transfer loss, noise, bias, calibration or filters. The caller owns how continuous input is sampled to provide phase-event values. State updates must be simultaneous, not an in-place cascade through all stages on one event.

Implemented acceptance tests (`tests/Mn3001PacketTests.cpp`):

1. With initial CP2 event indexed 0, an impulse acquired there reaches upper/lower512 on CP2 event512, then lower513 on CP1 event513. OUT1/OUT2 alternate with empty opposite output; their normalized sum repeats that impulse on those two slots only.
2. Tagged input sequence: OUT2 on CP1 carries the same tag as preceding OUT1 on CP2, not the previous cycle's tag. This rejects a held-two-tap averaging implementation.
3. Constant alternating event intervals give 256 complete clock periods between the first acquisition and upper512 arrival under this convention. This is a consistency check, not a substitute for the event test.
4. Irregular timestamps leave packet/event order unchanged; arrival at caller-supplied event times follow supplied events, not `256 / instantaneous_frequency`.
5. Device chains remain independent for different inputs; OE57 adapter yields identical normalized A/B outputs for identical initialization and shared events. This does not predict identical measured board return amplitudes.
6. Reject nonfinite inputs/times, nonincreasing times and repeated phases; verify reset empties all stored packets. An explicit validity flag distinguishes an empty bucket from an actual zero-valued sample.

## Remaining gates

Before claiming physical terminal or board output: establish input sampling aperture relative to clock edges, transistor/capacitor transfer and fork behavior, source-follower transfer/loading, reset pedestal and output clock cancellation, and the actual coupled filter/load response. Before claiming installed CS-80 behavior: BA617 law/trim and measured clock/control waveforms are still needed. Excluding loss/noise/calibration does not by itself establish these timing and output-topology semantics.

Independent review must specifically check the 509 fork, 512/513 sense nodes, 513/514 VDD terminations, and whether the proposed empty-packet abstraction is acceptable for the intended offline use. Independent graph/timing review accepted this conditional abstraction and verified the crop byte-for-byte. Independent implementation review also passed: strict C++20 warning-clean build and 97,650 checks, including simultaneous transfers,509 fork, drains, atomic invalid-input handling and reset. This is still a normalized-signal abstraction, not analog validation.

## Local verification

Direct standalone build only (no CMake registration or production linkage):

```sh
c++ -std=c++20 -Wall -Wextra -Wpedantic -Werror -Isrc tests/Mn3001PacketTests.cpp -o /tmp/mn3001-packet-tests
/tmp/mn3001-packet-tests
```

Result: 97,650 checks, zero failures. The tests use an independent acquisition-history oracle, not a duplicate stage-update algorithm. Uniform and irregular event timestamps exercise impulse, constant and ramp inputs. Stopped clocks preserve state; explicit reset empties it; invalid phase/time/input/reset requests are checked for full-state atomicity across both channels.

Five temporary-header mutation builds were rejected by the suite: OUT1 off-by-one sense index (19,605 failures), omitted OUT1 draining (9,794), omitted OUT2 draining (9,774), omitted fork (9,794), and fork sourced from stage508 instead of509 (9,794). These mutants were built outside the repository and are not retained.

Address/undefined sanitizers also passed all 97,650 checks with `ASAN_OPTIONS=detect_leaks=0`. The default leak-sanitizer exit was blocked by the environment's ptrace restriction, not a reported model leak; leak checking is not claimed. No permission escalation was requested. `git diff --check` passed.
