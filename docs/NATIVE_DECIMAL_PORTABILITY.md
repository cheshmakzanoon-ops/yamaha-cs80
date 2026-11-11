# Native macOS decimal conversion in offline reference tools

## Verified build failure

The first-attempt native build of `321eb7052f5bd737693d7c55aafb43dd792610e4`
failed on both macOS architectures:

- [arm64 job 130789484870](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43100308340/job/130789484870)
- [Intel job 130789484458](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43100308340/job/130789484458)

Xcode 16.4's libc++ provides integer `std::from_chars` but not its floating-point
overload. Both jobs report `truth_render.cpp:66:19: error: no matching function
for call to 'from_chars'`. The same unsupported operation was also present in
`vca_reference.cpp`. These are offline diagnostic exporters, not the audio
engine. The newly integrated static libxml2 built successfully on both runners
before this unrelated compile failure.

## Bounded correction

Both tools retain their existing ASCII decimal grammar, leading-plus policy,
full-token checks, finite-value requirement and application-specific bounds.
Only the conversion of already validated decimal bytes is shared through
`tools/DecimalConversion.h`. Integer parsing and `truth_render`'s other CLI
number parser are unchanged.

Non-Apple builds retain `std::from_chars`. Apple builds use `strtod_l` with an
owned NUL-terminated token and a private immutable C numeric locale. The helper
does not change the process locale. It temporarily selects nearest rounding
in the calling thread when needed and restores the caller's rounding mode.
This is not a claim that it preserves errno or floating-point exception flags.

Representable subnormal values remain accepted, even when the C library sets
ERANGE. A nonzero decimal significand that rounds to zero is rejected. Literal
zero, negative zero, decimal exponents, and complete consumption retain the
existing contract. Failed conversion leaves the output argument unchanged.
The helper is not a standalone grammar validator; the two existing callers
continue to reject whitespace, hexadecimal syntax and malformed tokens first.

Both generated artifact manifests include the helper's SHA-256, and CMake
reconfigures the provenance definitions if that file changes. No DSP, parameter,
fader, plugin UI or XML admission behavior is changed.

## Verification and limits

- The conversion fixture checks signed zero, minimum normal/subnormal values,
  overflow, underflow, midpoint rounding, bounded views and embedded NULs.
  It checks all four standard rounding modes and mode restoration, and tests a
  comma C++ locale plus a non-C system numeric locale when available.
- On Linux, 17,940 decimal corpus cases are compared with `std::from_chars`.
  The complete fixture reports 18,301 checks with the available German locale.
  GCC 14.2 and Clang 19 pass both the regular and forced C-locale branches.
- Both whole exporters pass the nine focused conversion, filter/VCA renderer,
  float WAV, reference-chain/clock and amplitude/filter-envelope CTests in the
  regular GCC build and a build forcing the fallback throughout both exporters.
- The fallback fixture also passes AddressSanitizer and UndefinedBehaviorSanitizer
  with leak detection disabled. LeakSanitizer's separate ptrace environment
  error prevents a leak-check claim.

The forced fallback is a Linux test of the same conversion path, not execution
of Darwin's C library or SDK. Native macOS/Windows success for the correction
must be established by the subsequent exact-commit CI run. The failed original
jobs are never replaced by the local passes described here.

## Production trace follow-up

The next native arm64 build, at
[`6eeda885d00ac5447723177250b6d178f1469968`](https://github.com/cheshmakzanoon-ops/yamaha-cs80/commit/6eeda885d00ac5447723177250b6d178f1469968),
passed the earlier double-conversion compilation point but failed in the offline
amplitude production observer. [Job 130803854371, run 43104414461](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/43104414461/job/130803854371)
reports `amplitude_production_trace.cpp:125:25: error: no matching function for
call to 'from_chars'`. Tests and plugin validation were skipped for that job.
The first correction was therefore incomplete, not a verified native build.

A repository-wide source audit found the two remaining direct floating-point
calls in `amplitude_production_trace.cpp` and `filter_production_trace.cpp`.
Both parse directly to `float`. The shared helper now selects `strtof_l` for
`float` and `strtod_l` for `double` on Apple. Converting through double and then
casting to float would incorrectly double-round some decimals near binary32
halfway points. Non-Apple builds continue to use the typed `from_chars`
overload. Integer calls and the other existing `strtod`/`stod` parsers remain
unchanged.

Both observers retain exact decimal domain checks, leading-plus handling,
finite and nonzero-underflow checks, diagnostics, and direct binary32 output.
Their compile-bound source manifests and the comparison tools' frozen source
sets now include the conversion helper. Tampering with that hash is rejected.
The DSP models, envelope thresholds, UI, faders and runtime plugin are not
modified.

Local verification covers both ordinary and forced-C-locale builds of all
four affected tools. The eleven focused CTests include both whole production
comparison suites, the double reference exporters, float WAV, reference-chain
and envelope-reference regressions. The helper fixture adds 19,695 float
differential cases and explicit midpoint neighbors, signed zero, normal and
subnormal limits, overflow/underflow, four rounding modes, bounded views,
embedded NULs and locale isolation. With a non-C system locale available, it
reports 38,353 checks. These Linux results do not prove execution against the
Darwin SDK or C library. The follow-up still requires its own exact-commit
native CI build and test results.
