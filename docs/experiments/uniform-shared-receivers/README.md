# Reuse equal receiver proof inside one source-readout solve
The source engine already checks whether all eight input drive words within
a rank are identical. When both ranks pass that check, the shared receiver
network can compute one lane's Q and level values per rank, while retaining
the original ordered eight-term sum, both shared buses, all drive and
envelope ticks, and every per-receiver published field. No result is kept
from a previous sample. One differing lane, an inactive source rank, or an
unsupported FP environment retains the original solve.

This branch is only enabled in the tested ordinary Linux x86-64 GNU 14.2
SSE2/no-FMA arithmetic. It requires nearest rounding and masked
exceptions in both units. Same-invocation qualification independently
validates shared network inputs. It does not require the cutoff *output*
to be accepted: a rejected cutoff calibration cannot suppress or corrupt
the original independent shared receiver transaction.

## Work and verification
Exact normal Release VST3 stock 25281f677330f7e73a0fc695f43cdfc6eaf902ffb059218e0b3ac36188edae5a
from 5ef3cfee, candidate
0f848ae0c8eb7629262395d457ff31662d3d8806b498848a6a6ee868b5ed56d1.
The later screenshot and evidence main commits did not change any
product inputs. Four genuine 48 kHz/64-frame VST3 callbacks, with duplicate
and exact extra-callback controls on both arms:

| Scene | Guest instructions, default | Guest instructions, filter batch disabled |
| --- | ---: | ---: |
| Fresh quiet | −12.9401% | −13.0205% |
| Eight equal-velocity held notes | +0.1156% | Unchanged |
| Eight varied-velocity held notes | +0.1078% | −0.0064% |
| Varied wake | +0.1075% | −0.0030% |

The no-batch version is a software diagnostic; it is not an older physical
machine. No native CPU-time or DAW result follows from these work counts.

Published audio, source state and FP/history observations matched through
112,640 native callbacks per arm (7,208,960 stereo frames and 113 public
state captures), complete 48 kHz VST3 protocol, and 44.1/88.2/96/192 kHz
realtime and offline cases. A directed loaded-module experiment compared
4,608 controlled calls including 1,794 actual signals, interrupted partial
state and continuations. The separate 73,728-case direct receiver matrix
also included boundary rejection, saturation and exceptional values.

The [settled-release check](release.json) reached genuine note-off and
tail histories before the four callback window. Equal strikes used 12.650%
fewer guest instructions; varied strikes used 0.137% fewer, with 0.134%
more guest writes. Captures from all 9,222 callbacks matched stock within
each runtime, as did repeat and exact 4+1 controls. Native and translated
stock have tiny differences in approximately 102,000 PCM words in each release
scene (maximum about 1.187e−38); cross-runtime identity is not assumed.

[Normal work](full.json), [batch-disabled work](scalar-work.json),
[48 kHz history](public-parity.json), [cross-rate/rate mode](cross.json),
and [actual loaded module exceptions](exception.json) describe distinct
checks. The source integration regression compares the full named shared
readout to the public scalar calculation in uniform and nonuniform
situations, including pressure and release.

The strict final source guard reproduced the measured normal and
batch-disabled engine objects. [Compile profiles](engine-profile-hashes.log)
record byte-identical old/new engine objects with explicit feature-off,
non-trapping, altered rounding, signaling-FP, and other unsupported
settings. The 73,728-case direct comparison also passed on enabled GCC
O1 and O2 variants. These checks do not replace the real loaded-module
test for the actual tested Release image.

The [direct edge corpus](direct.json) and [mutation receipts](mutations.json)
distinguish reordering the sum, incorrect rank bits, a local level error,
and leaking a rejected transaction into the published result.

The independent Source Factory integration test passed all 135,799 checks
on both arms. Rebuilt O1 and O2 loaded-module pairs each passed the same
4,608-case interrupt/continuation matrix; the measured product work
figures above are only for the pinned Release builds.
