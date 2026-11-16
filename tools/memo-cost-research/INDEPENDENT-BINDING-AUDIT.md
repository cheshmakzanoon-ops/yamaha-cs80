# Independent binding audit

Request02 artifact: 51 manifest-listed files independently rehashed with no mismatch; GOT/PLT validator independently rerun. Runtime binding and library/build fingerprints equal request01 apart from ASLR addresses. Supplementary grid: 2,097,168 calls, zero errors, stable binding. This grid is not the all-domain argument below.

Qualified fingerprint: libm SHA256 fce00b6f25f459cf4ae0b7fae4257909a1a8a86f9149e7c029a5d617baf1ccd0, build ID d53fcbff9f855463606b25cfbe2a2e10856ea3a7, resolved powf offset 0x7df50. Exact companion libc/loader and proof executable fingerprints are recorded in extracted/RESULT.json and must also match the proposed allowlist. No other binding is approved by this audit.

## All admitted exponent branch argument

For the fixed float base 2 (bits 0x40000000), the entry base classification chooses the normal path. The table selection in 0x7df82 onward gives index9, reduced float1 and integer exponent1. Actual section bytes decode:
- constant0x99360 = -1 exactly;
- table0xb8010 = 1 exactly;
- table0xb8018 = 0 exactly;
- polynomial coefficients at0xb8080..0xb80a0 finite normal doubles.

Thus the first FMA yields exactly zero; polynomial products with this zero yield zero, and the logarithm accumulator is exactly1. Multiplication at0x7e00c therefore yields the converted input exponent exactly (DAZ can turn a subnormal input into signed zero, also within the bound). For every finite admitted float exponent in[-120,120], the magnitude high-word test at0x7e019 has low16 value at most0x80bc, below threshold0x80be. Hence branch0x7e01d to error/range handling cannot be taken. This is derived from actual constants, not interpolated grid observations.

Bitwise zero exponents take0x7e138 ->0x7e2f0 ->0x7e08c, returning1; fixed base2 passes its non-signaling classification. Other finite nonzero exponents take the main return through0x7e023..0x7e090. Those reachable paths consist solely of integer/register operations, table reads and SSE/AVX scalar arithmetic; there are no x87 instructions, memory stores, calls or error-helper jumps. Therefore this bounded path cannot mutate errno or x87 state. MXCSR arithmetic sticky effects remain observable and are covered by the memo's stored-post-mask containment gate, not waived by this audit.

## Scope

Approved as a binding-specific prerequisite for an isolated memoON qualification/cost experiment, provided every new run rechecks this exact fingerprint, actual ordinary powf call path and strict supported caller controls. This is not general glibc/Linux approval, production promotion or evidence of CPU benefit. Native/unsupported/raw fallbacks remain required. Full candidate correctness and frozen cost gates are separate requirements.
