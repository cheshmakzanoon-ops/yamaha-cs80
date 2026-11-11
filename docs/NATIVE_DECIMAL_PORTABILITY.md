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
