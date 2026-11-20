# Packed exact 2× output decimator

On the already qualified strict GNU/Linux x86-64 widened-history route, an AVX2
CPU can form four independent symmetric tap terms at once. Each channel still
starts with the same center product, adds the same binary64 tap terms in the
original order, and converts to float once. The code does not change coefficients,
mirror contents, history indexing, retained phase, or the sample count.

The route retains the existing coefficient, finite/normal-or-zero history,
nearest-rounding and fully masked SSE gates. All widened finite float inputs,
paired sums, and full-range normal float coefficient products stay far inside the
binary64 exponent limits; exact cancellation and signed zero are preserved.
Independent products cannot trap or consume a sticky flag. Other rates, unsupported
CPUs/compiler flags, and exceptional histories retain the former routes.

## Matched Linux VST3 work and parity

GCC 14.2 Release/PIC; official JUCE 8.0.11, c352e2489000ffeb9e397051377d75eef2efe641;
source baseline 5499056a60ee5044275ba17eb7dddded4c96e90d. The [receipt](work64.json)
uses a true 64-frame VST3 host at 48 kHz and a 256-stereo-frame counted prefix
with duplicated and additive-window controls.

| Scene | Guest instructions | Guest data reads | Guest data writes |
| --- | ---: | ---: | ---: |
| Fresh quiet | no change | no change | no change |
| Eight equal held | −0.344% | −2.447% | no change |
| Eight varied held | −0.342% | −2.442% | no change |
| Varied wake | −0.243% | −1.780% | no change |

Audio, public state, callback results, and floating-point observations matched
byte-for-byte within each execution mode. Untimed native comparison passed 27
histories (82,944 callbacks per arm), six connected MIDI histories, and all 196
runtime rank-selector pairs. The focused CTest checked 389,760 output words and
SSE sticky-flag snapshots against the unchanged scalar FIR through rate, ring,
rounding, denormal and reset histories. The Standard plug-in still reports 67
samples of latency at 48 kHz. [Reproduction harness](../resumed-exact-hotpaths/harness/REPRODUCE.md).

These are deterministic translated instruction/data counts and untimed parity
checks, not native CPU time, DAW round-trip latency, or a hardware budget result.
