# Separately implemented bounded host services

Preparation-only design accepted by independent reviewer; implementation/tests are
next checkpoint. This is **not** a patch, alias or admission of official SDK
HostApplication. Official overwrite/empty allocation controls remain unchanged.

Use the exact pinned IHostApplication/IMessage/IAttributeList interfaces and a
separately named host. Host, message and attribute-list objects have owning atomic
SDK reference counts and shared health/budget state; context owner destruction
must not invalidate retained SDK refs. Message owns one attribute-list ref;
getAttributes returns a borrowed pointer (QI/addRef needed for retention).

Frozen policy: 1,024 live service objects, 65,536 global keys, 4,096 keys/list,
8 MiB final charged bytes/list, 1 MiB/value, 1,024-byte attribute identifier,
4,096-byte message identifier, 64 MiB global charged payload including transient
replacement. Counts bound container overhead, but charged bytes are **not RSS**.
All size checks use subtraction or bounded scans before arithmetic. UTF16 byte
charge includes the terminator; binary size zero allocates no payload.

Storage uses typed variants and owned copies. Under each object's mutex, reserve
global charge for the complete new payload before allocating. On replacement,
keep old payload/charge until noexcept swap; on insertion reserve one key and key
bytes, and roll back on any allocation failure. Replacement consumes no new key.
Per-list final size is checked independently of global transient charge. Shared
budget uses atomic counters, so there is no shared mutex lock-order cycle; no
external SDK callback occurs while an object mutex is held.

ABI methods catch allocation/other exceptions, latch nonallocating health failure
and return explicit SDK errors. Invalid arguments return kInvalidArgument;
unsupported interfaces/classes and absent/type-mismatched keys are ordinary
negative results, not internal failures. Getter outputs reset consistently.
setMessageID has no result, so limit/OOM failures preserve old ID and latch
health. A null ID clears the prior ID allocation-free, matching the pinned helper. UTF16 gets copy min(stored bytes, caller bytes), including odd/truncated
byte lengths as pinned SDK does; truncated output need not be terminated.
Binary/ID pointers are borrowed until corresponding mutation/object destruction.
Per-method locking does not extend pointer validity beyond return. Concurrency
tests use copy-returning getters, not unsafe concurrent borrowed-pointer access.

Reference overflow/observable live-counter misuse latches health without throwing.
As with SDK interfaces, release after destruction or without an owned reference
is caller undefined behavior; no claim to make dangling-pointer misuse safe.
Final acceptance is an explicit quiescent check BEFORE dropping the context owner:
healthy, only owner's host reference, one service object, zero keys/payload.
Destructor cleanup is not acceptance and cannot fabricate a successful receipt.

Tests required before review: all variants/mismatches; overwrite/type change and
empty binary allocation balance; UTF16 odd/zero/truncated/full buffers; binary
copy ownership; QI/null/unsupported create; independent policy boundaries; old
value preservation under near-full transient budget and injected OOM; insertion
key rollback; message ID rollback; retained host/attributes after owner loss;
concurrent setters/copy getters; explicit final accounting refusal and success.
No actual plugin, native loader CLI, or production admission in this scope.
