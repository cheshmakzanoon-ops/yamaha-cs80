# Nonadmitting metadata from a failed terminal child

This does not relax any query, topology, parameter-binding, lifecycle, unload or
success-admission gate. The terminal child still exits24 on failure. The normal
supervisor must reject that attempt even when this diagnostic frame is complete.

After a successful Connected metadata capture, the owned instance retains exact
encoded bytes. A private noexcept, no-copy accessor exposes a const view only on
the idle UI thread, and only if the original capture returned successfully. A
later failure preserves that view in the quarantined native graph. No plugin
query, metadata regeneration or native cleanup is retried to obtain diagnostics.
The view remains valid because the whole graph is retained until process exit.

The failure handler may write one best-effort diagnostic frame to its already
exclusive output descriptor. It first checks original private ownership/mode,
regular/single-link/empty file and position0. Nonempty or partially written
success/failure output is never overwritten. Metadata size is checked against
16MiB before copying or encoding; oversized, empty or unavailable metadata is
not written. Failure of allocation, write, fsync or final close cannot change
exit24. A partial frame must not be treated as complete evidence.

## Failure frame v1

Framing remains u64LE body-size, body, then text SHA256(body). Text/blob means
u64LE length followed by exact bytes. Body cap is16MiB+4096; no trailing data:

1. text `cs80-process-lifetime-failure-metadata-v1`, text `rejected`, u32 exit24.
2. text nonce, u64 actual PID, text arm.
3. text expected-manifest digest, text preload evidence digest, text module SHA256.
4. text `preload-identities-not-postfailure-verification`.
5. text outer stage<=64, text module stage<=64, text history operation<=64,
   text primary exception<=256.
6. text SHA256(metadata), blob full original Connected metadata<=16MiB.

The unchanged Connected binary schema includes component/controller raw IDs,
all four bus inventories, and each parameter's ID, full title/short title/units,
step count, default normalized value, unit ID, flags and observed current value.

These identity fields describe pre-load verification only. The failure path does
not claim postfailure runtime closure or fresh input revalidation. A diagnostic
consumer must independently check the actual child exit24, PID/nonce/request,
frame/hash completeness and retained input identities. It must label the result
rejected, never an A-D success, qualification, sound-equivalence or timing result.

This records data already observed while a failed attempt ran. It does not repair
that attempt or authorize bypassing its blocking gate.
