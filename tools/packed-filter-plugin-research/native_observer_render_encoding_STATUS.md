# Render fieldwise encoding v1: preparation only

Encode-only helpers, no parser/loader/CLI/real plugin processing. The encoder
validates object structure before emission; it does not authenticate how those
objects were produced. Independent Python `struct` golden-byte controls cover
both full variants; negative controls cover malformed/truncated object inventories,
not untrusted binary parsing. A future decoder needs its own bounds/version/
truncation/trailing-data gates. No universal qualification/completion flag exists.

## Exact format

All integers little endian. Signed values use fixed-width two's-complement.
`f32`/`f64` mean finite IEEE binary32/binary64 bit patterns, including preserved
negative zero. No SDK object padding, pointer bytes or host-endian copies occur.
No byte alignment or implicit padding between fields. Counts are u64. Enumerated
values are the pinned SDK numeric values. Version is u32 = 1. Any incompatible
field interpretation requires a new version, not a silently changed encoder.

Protocol record, in order:

1. Eight literal bytes `CS80RPRT`, version.
2. rate f64; mode i32; block i32; total frames i64; legacy CC convention u32 (0/1).
3. Event count; each event: absolute frame i64, bus i32, canonical offset i32,
   canonical PPQ f64, flags u32, type u32, then exactly its variant:
   - Note on: channel i32, pitch i32, tuning f32, velocity f32, length i32, noteId i32.
   - Note off: channel i32, pitch i32, tuning f32, velocity f32, noteId i32.
   - Explicit legacy CC convention: control number u32, channel i32, value i32,
     value2 i32. SDK variant is 65535; input-use caveat remains unchanged.
4. Input parameter point count; each: absolute frame i64, ID u32, value f64.

Observation record, in order:

1. Eight literal bytes `CS80ROBS`, version.
2. Protocol byte length u64, followed by the complete exact protocol record above.
3. Callback count; each: start i64, count i32, return result i32, silence flags u64.
4. PCM scalar count (not frame count); every interleaved stereo value f32.
5. Output parameter point count; each: absolute frame i64, ID u32, value f64.

The observation encoder requires the exact contiguous callback partition from
frame zero through final partial block, successful returns, stereo silence masks,
and exactly twice the protocol frame count in finite PCM scalars. It does not
normalize samples based on silence flags. Output points retain callback order,
then first-occurrence ID queue order, then each queue's nondecreasing frame order.
An ID cannot re-enter after a different queue in the same block. Cross-ID frames
may decrease legitimately. IDs are non-sentinel; values finite normalized [0,1].
Bounds remain 4096 points per callback and 65536 total. Empty output queues were
not retained by RenderObservation, so this is point-stream identity, not evidence
of output queue creation calls. Empty callback point streams remain represented
by the complete callback partition. All protocol validation is also enforced.

## Identity and evidence boundaries

Protocol identity is lowercase hexadecimal SHA256 over **all exact bytes returned
by encodeRenderProtocol**, with no filename, extra prefix, JSON serialization,
trimming or normalization. SHA256 implementation is delegated to the established
artifact pipeline; this header does not implement cryptography. Observation hash
likewise covers the complete observation bytes, including embedded protocol.
Signed zero differences intentionally yield different identity. Input stream
ordering, flags and legacy-convention choice are identity-bearing.

This protocol hash binds only these render inputs, not module/source/build, seed,
state bytes, lifecycle history, reset, latency/PDC, host notifications or workload
case identity. Those must be bound by the future complete history manifest. The
embedded protocol prevents accidental use of an arbitrary unrelated digest, but
does not prove the supplied observation was executed from it. The observation
struct has no recorded actual rate/mode history; authentic setup/execution and
same-attempt failure provenance remain external mandatory gates. Structural
success is not production qualification or evidence of sound equivalence.

All encoding and validation allocate bounded-by-protocol storage and are outside
performance measurement. This is not a realtime/timing helper. Existing render
STATUS limitations, including externally retained failures and checked teardown,
remain in force. No underlying reviewed render files were modified for this work.
