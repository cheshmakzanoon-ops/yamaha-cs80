# Structural Linux provider checks, not loaded-provider authority

`native_provider.hpp` parses bounded ELF64 little-endian x86_64 ET_DYN bytes and
Linux maps text. There is no module load, function call, verified capability or
token mint here. Caller-provided stamps, base addresses and export addresses are
untrusted structural inputs until joined to the actual retained-file/loader path.

ELF checks include exact header/program-entry sizes, versions/type/machine,
program count/table bounds, supported section-table metadata (extended forms
rejected), overflow, file/memory sizes, alignment power-of-two and congruence,
and non-overlapping executable load segments. Export addresses must be inside
file-backed executable bytes, never merely BSS/memory extent. The mapping must be
uniquely identified, executable, file-backed with target device/inode, and yield
exactly the ELF segment-derived file offset. Returned values are three structural
file offsets, not a claim that the caller obtained real exports.

Maps input is bounded to 4 MiB, 32768 lines and 16384 bytes per line, must end in
newline and have ordered nonoverlapping page-aligned ranges. Leading numeric,
permission and device fields are checked exactly with overflow rejection. Path
text is deliberately ignored. Input vector/layout structures are not immutable
capabilities: production integration must use parser results from fresh actual
sources, not caller-constructed equivalents.

**Inode preload rejection is insufficient.** A stale loaded image may retain an
old inode while the same pathname has been replaced. `stale-path-not-covered`
explicitly demonstrates this structural inode helper accepts that scenario; it
is NOT admissible as complete stale-provider rejection. Actual loaded-object /
link-map / pathname inventory rejection is a separate mandatory pre-load gate.
Do not use `/proc/self/fd` as the load path without reviewing plugin resource
semantics. Quiescent owned roots, pinned FD identity, actual dlinfo/dladdr/export
binding and post-load/post-unload revalidation remain integration requirements.
Loaded runtime/dependency closure is not proved by these checks.

47 inert controls pass, including actual self-maps parsing without loading any
module, independent fixed ELF file-offset expectations and malformed-input
negatives. ASLR positive control uses a different base with the same expected
file offsets: per-load virtual addresses are not disk identity and must not be
required to match across unload/reload or cross-arm work. This checkpoint is not
production qualification, sound equivalence or performance evidence.

File-backed mapping provenance is not in-memory code-byte integrity: private COW
text changes, relocations and instrumentation are outside these offset checks.
Only a trusted process/loader and a separately reviewed code-memory policy can
address that boundary. Structural success alone does not authorize native use.
