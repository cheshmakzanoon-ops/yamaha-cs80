# Standalone process-lifetime request driver, preparation only

`native_observer_driver.cpp` is an unwrapped dedicated executable. It includes no
fixture, runtime fault-injection mode, environment-driven test seam, symbol
interposition, hardcoded CID or hardcoded case. It invokes the existing terminal
`ProcessLifetimeChild` after independently verifying the requested file identity.
It makes no timing or production-qualification claim.

Invocation is exactly `observer REQUEST SHA256 NONCE RECEIPT`. Request SHA256 is
64 lowercase hex characters supplied by the supervisor, not self-authorizing
content. The nonce and receipt contracts remain those of the terminal child.
Request rejection exits2 before loading. Native execution is terminal exit0 or
exit24. Neither success nor file hashing alone authenticates a parent supervisor,
a compiler, complete runtime closure or the expected manifest's source.

## Frozen binary request v1/v2/v3

Every integer is unsigned64 little-endian. Every text is a u64 byte count followed
by those bytes, with no NUL and no terminator. No padding or trailing bytes.
All limits are checked before field allocation/iteration. Ordered fields:

1. Text `cs80-process-lifetime-request-v1` or `cs80-process-lifetime-request-v2` or `cs80-process-lifetime-request-v3`, text `process-lifetime`.
2. Text arm, exactly `stock` or `candidate`.
3. Text32 lowercase hex representing the exact16 raw SDK TUID bytes, not a
   platform-reordered UUID display string. All-zero is rejected. Authentic CID
   discovery/binding remains the supervisor's responsibility.
4. Integers: workload policy0/1/2, rate44100/48000/96000, mode0/1
   (private workload enum, never cast to SDK), voices8/16, block64/256/1024.
5. Controller-state policy1 (`Supported`) or2 (`JuceUnsupported`), selected before
   native execution. Request-v2/v3 then carries query policy1 (strict SDK) or2
   (pinned JUCE false/null); v1 implies strict. Request-v3 then carries topology1
   (zero audio inputs) or2 (disabled mono external input); v1/v2 imply1.
   Worker timeout milliseconds1..600000.
6. Identity caps: entries1..16384, files1..entries, bytes1..8589934592,
   pathBytes1..4096. These are limits, not a claim all dependencies were declared.
7. Bundle root: text absolute path, integers mode, uid, gid. Then text relative
   module path.
8. Bundle entry count<=16384, followed by each entry: text relative path, integer
   directory flag0/1, mode, uid, gid, size, text SHA256 (empty for directory).
9. Input-root count<=1024, followed by roots in the same root encoding.
10. Input count<=16384, followed by: text input name, integer root index, entry.

Modes<=0777, uid/gid<=UINT32_MAX, each entry size<=8GiB. Text paths/names<=4096.
Canonical ordering, duplicate/root overlap, declared bytes/count limits and
full bundle/input equality use the existing strict identity verifier. Every
frozen case is validated against the original workload grid.

Request file cap8MiB. Each ancestor is opened without symlinks. The final file
must be regular, single-link, owned by effective UID, mode0600, positive bounded
size. The driver reads its retained descriptor, rejects growth, checks stable
metadata and hashes exact bytes before parsing. This is an owned/quiescent-root
model, not a hostile-writer sandbox or an assertion of parent authorization.

Worker timeout bounds worker waits, not arbitrary native calls. The independent
supervisor must still enforce an overall wall deadline, owned process-group exit,
exact argv/PID/nonce/output roster and actual tool/request/runtime identities.
The driver does not reset global RNG or DSP state, admit E transfer, or treat
process exit as physical `dlclose` unmapping. Full A-D evidence remains mandatory.

## Checks

41 driver controls pass. The dedicated executable builds with strict C++20 warnings and the explicitly
pinned SDK. Tests include actual inert-library execution for both arm labels at
one supported inert workload, parser-only coverage of all108 frozen cases,
truncation/oversize integer/trailing/NUL/schema/profile/CID/enum/deadline/cap
rejection, hash mismatch, ancestor/final symlink, hardlink, FIFO, oversized-file and
nonprivate-file rejection. A public-supervisor integration control binds the
exact unwrapped driver invocation and independent precomputed identity/protocol
expectations, then checks its actual inert-child receipt and process completion. The inert
library's native setup is deliberately fixed to one workload; parsing the grid
does not claim native interoperability across the grid. No CS80 plugin was run.

The standalone driver emits child-receipt-v3 with actual query policy/result/nullness and explicit input topology, including for older strict requests. See native_observer_juce_query_STATUS.md for the separate query compatibility contract and original diagnostic limitation.

See native_observer_input_topology_STATUS.md for frozen input-lifecycle and descriptor semantics.
