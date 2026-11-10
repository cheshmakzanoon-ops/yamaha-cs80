# Channel pan typed-value round trips

Both channel pan controls display directional percentages (`L50`, `R50`) but
previously could not parse most of those strings. Copying a displayed value into
typed entry/paste could fail; a short numeric prefix such as `L1` could also be
mistaken for the endpoint label `L100` by generic choice-prefix matching.

A pan-only parser now recognizes a case-insensitive L/R direction followed by a
complete unsigned decimal percentage from 0 to 100, before generic choice
matching. Whitespace around the amount and decimal commas are accepted. Missing
amounts, signed amounts, exponents, suffixes, nonfinite values, malformed decimals
and out-of-range values reject without changing the output. Center `C` and other parameters retain their existing paths; endpoint values
retain their existing meaning. No DSP or formatter
changes are made.

## Verification

The exact native fixture covers nine displayed positions for each channel,
malformed input, decimal-comma entry and real APVTS parameter gesture delivery.
Baseline: 116 checks, 16 failures. Fixed: 116 checks, zero failures. The baseline
failure count includes the attached-value rejection check following an unsuccessful
valid edit; it is not a count of independent defects.

The existing five-point-per-parameter formatter witness changes intentionally:
950 cases, 946 accepted, digest `ab1393115089aa5f` before; 950 cases, 950 accepted,
digest `8d7e609da2f21897` after. The four newly accepted cases are L50/R50 on both
channels. This witness is not a claim of exact round trips for quantized choice
labels or rounded display values.

One freshly linked Release native CTest invocation passed four targeted suites
(context-menu keyboard, switch context, typed-value units, undo/replay gestures),
zero failures, 2.68 seconds. This is direct component/processor coverage, not OS
clipboard, external DAW, plug-in wrapper or full-suite qualification.
