# Reduce exact FIR tap access work without changing the sample rate

No quality mode, factor, taps, arithmetic order, phase or algorithmic delay
changes. The Engine-owned path reuses the original binary32 coefficient values
after exact widening, and in the 2× path retains a widened copy of normal
and signed-zero inputs. All sums are still binary64 in the same order.
Subnormal/nonfinite input receives the original raw processing while present.
Directed rounding and unmasked exceptions keep the original scalar evaluation;
unsupported compilers and floating-point builds retain the established path.
8× uses the original coefficient/history evaluation (no widened scratch).
A same-sign zero history may skip duplicate ring stores while its cursor
advances normally.

The widened data lives in **unused** parts of the fixed scalar arrays (no
allocation or aliasing through a double pointer). Coefficient slots are
[255,511) at 2× and [511,1023) at 4×; the 2× shadow history is [510,1530).
Copies and resets keep the two histories coherent. The 16-bit certificate
occupies existing padding in the admitted Linux targets and the exact layout
is checked there. This code path is limited to strict GNU/Linux x86-64 builds
on GCC 13.3.0 and 14.2; no measured claim is made elsewhere.

## Direct incremental whole-VST3 counters

| Matched compiler / 48 kHz 64 frames | Fresh quiet Ir | Equal held Ir | Varied held Ir | Varied wake Ir |
| --- | ---: | ---: | ---: | ---: |
| [Ubuntu GCC 13.3.0, Debian runtime](gcc13-work.json) | −0.005% | −2.314% | −2.280% | −1.690% |
| [Debian GCC 14.2](gcc14-work.json) | −0.035% | −2.388% | −2.370% | −1.707% |

GCC 13 also cut instructions in the 1-, 6- and 120-queue cases. The separate
[16/32-frame GCC 13 comparison](gcc13-small-buffers.json) removed 2.231%/2.264%
held guest instructions. GCC 14 removed 2.321%/2.353% incrementally at those
buffer sizes. Held reads and writes increase slightly: about +0.13–0.14% and
+0.15–0.18% respectively. See all retained counters, including the mixed
quiet/wake tradeoffs. The [cumulative GCC 14 64-frame](gcc14-composed-work.json)
and [16/32-frame](gcc14-composed-small-buffers.json) comparisons start at
the prior-pass baseline product 0832721, not at the repository's original state.

All compared real VST3 audio/state/FP, latency and lifecycle/quality-switch
checks matched within runtime. Standard at 48 kHz still reports 67 samples.
Separate ledgers covered exceptional history, rounding and denormal controls
and 912 genuine unmasked FIR faults with matching continuation. A whole
header-dependent rebuild produced the exact measured GCC 14 ELF; GCC 13
built both full affected dependency closures using the same pinned JUCE and
link inputs. These are deterministic translated instruction and memory counts,
not measured native CPU percentages, deadline/DAW latency, or low-end hardware
qualification.
