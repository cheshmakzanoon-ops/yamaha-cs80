# Exact paired transfers in the IG00156 AVX2 stage batch

Compared the existing product `9208eb6323207b5054055dd2e0549e45324c539a` (from the same-source screenshot refresh `631b7708`) with an independently relinked Linux x86-64 Release VST3. Both use strict GCC 14.2.0, the same generated VST3 wrapper/objects, and JUCE `c352e2489000ffeb9e397051377d75eef2efe641`. The contender rebuilt only `Ig00156Batch.cpp`, with `-ffp-contract=off` unchanged. These measurements are scoped to this change, not the cumulative change from the original plugin.

- Two adjacent fields per scalar stage are transferred as object bytes and transposed into separate lane vectors. The corrected slope pair uses the inverse transfer. Layout assertions bind the field order; no arithmetic is moved into the transfers.
- In the Newton loop, an inactive lane's BP value is frozen by the existing result mask. The other three per-lane values are pure functions of that now-frozen word and unchanged inputs. Evaluating them produces the same words, so no separate blend is needed. The same FP expressions still execute in every lane. No admission gates or scalar fallback changed.
- Against the prior stage implementation, 43,520 cases matched per compiler with GCC 14.2 and GCC 13.3 (including 14,904 admitted cases). Comparisons covered the output words, capacitor and saved slope states, status and FP flags, with directed rounding, DAZ/FTZ modes and trap-rejection cases. The new CTest case staggers lane convergence and checks the next sample across every lane permutation and both signs. An intentionally incorrect variant that let a retired lane's BP advance failed qualification.

## Real VST3 work

At 48 kHz and a 64-frame host buffer, the recorded window is four actual VST3 callbacks per scene. The window excludes setup, warmup, the loader and the host stub. These are Callgrind translated guest instructions and guest memory operations, not physical retired events, elapsed time, or a percentage of real-world CPU. A native twin comparison checks the corresponding public outputs independently of Callgrind.

| Scene | Guest instruction change | Guest read change | Guest write change |
|---|---:|---:|---:|
| Fresh quiet | 0.000% | 0.000% | 0.000% |
| Eight equal held notes | −0.485% | −1.185% | −0.689% |
| Eight varied held notes | −0.483% | −1.183% | −0.689% |
| Varied wake | −0.352% | −0.897% | −0.482% |

The calculation uses the same host input and preserves Standard's 67 samples of reported latency. This is neither DAW round-trip latency nor validation of a physical CPU budget.

Both module images passed independent repeated callback and next-window additivity controls. The separate native receipt compared **27 histories**, **82,944 callbacks and 3,286,656 stereo frames per arm**, with **216 state snapshots per arm**, byte-for-byte PCM/FP capture, channel/event metadata, and reported latency. The matrix included source, legacy, external-input, effects, automation and MPE workloads, three sample rates and variable 1–64 frame block lengths. Standard reported 67 samples and Legacy reported 63 on both images.
