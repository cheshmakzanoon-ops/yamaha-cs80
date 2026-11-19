# Order-preserving hot-loop addressing

Group eight **integer address advances** in the admitted 2× FIR history loop.
Every binary64 tap still contributes in its original order. No new state,
coefficients, even/odd sums, vector arithmetic, factor or delay changes.

The three-pin terminal emitter uses direct, ordered fields only in the existing
borrowed oscillator path: masked nearest rounding on the qualified GNU/Linux
GCC 14.2 build. Every scalar or unsupported path keeps the original code.
A broader emitter was rejected because an actual GCC 13 unmasked invalid
fault changed one partial-state record and four continuation records, despite
normal audio tests passing. The admitted scope kept the normal Engine object
byte-identical on GCC 13 and did not compile the borrowed emitter there.

The [combined real VST3 work receipt](work.json) on GCC 14.2 relative to the
prefix-only parent shows −0.368%/−0.365% guest instructions on equal/varied
held notes and −0.264% in wake, with −0.012% in fresh silence. Reads did not
increase; writes were unchanged. The two repeat and next-callback-additivity
controls and independent native captures retained byte-exact audio, state,
events, latency and FP observations. A separate 22-case replay, including
seven actual traps and immediate continuation, matched the full capture.

The isolated group-eight change also passed real GCC 13.3 and 14.2 Linux
VST3 comparisons on b88b101, all 255 physical 2× ring positions and 912
matching actual FIR faults. Isolated held guest instructions fell about
0.15–0.18%. Isolated GCC 13 held reads rose 0.16% while writes fell 0.30%.
This is fixed translated work, not measured CPU use or a physical target test.
