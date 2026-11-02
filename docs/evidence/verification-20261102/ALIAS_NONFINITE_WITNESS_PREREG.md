# Existing raw alias nonfinite witness — preregistration

The registered release contract already requires
`require_zero_alias_nonfinite_sentinel_rows: true`. The original frozen
`engine_audit.cpp` exports numeric **999** when an alias render encounters a
nonfinite audio sample; the immutable reporter lists such rows separately.

At validator commit `f5fc991`, a raw 999 at 44.1 kHz passes if a counterfeit
summary falsely lists no nonfinite alias rows. The validator only recomputes
alias target comparisons in the original Source / 48 kHz / ring-off scope.
This is a missing raw nonfinite check, not a request to broaden or change the
registered -70 dB alias target.

Before implementation, add controls for all Source/Legacy, three-rate,
ring-off/on combinations outside the already-targeted scope, using equivalent
CSV spellings 999, 999.0 and 9.99e2. Every control keeps the full 885-row
inventory, frozen headers and fixture identities, normal numerical targets,
and a deliberately false-green nonfinite summary. All must fail for the raw
sentinel, not an unrelated coverage or performance error. The positive fixture
and original archived real-audit rejection must remain unchanged.

The repair must inspect all 168 raw alias rows independently of summary
booleans. It must not change any source, frozen reporter, numerical threshold,
audit inventory, or original historical evidence. Mutation validation must
remove the actual guard, import successfully and recover ordinary assertion
failures for these controls; a crash or import failure is not a mutation kill.
