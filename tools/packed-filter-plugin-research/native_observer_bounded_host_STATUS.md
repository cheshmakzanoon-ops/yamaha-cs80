# Custom bounded host: preparation, not plugin admission

This is an independent implementation of the pinned SDK's mandatory
IHostApplication, IMessage and IAttributeList interfaces. It does not modify or
link the official HostApplication helper. The official defect-control evidence
remains separate and inadmissible. No module or synth is loaded by these tests.

32 inert runtime cases compile with `-std=c++20 -Wall -Wextra -Werror -pedantic
-pthread`, with no SDK diagnostic exception. Explicit `CS80_OBSERVER_SDK` selects
the verified 18-header environment. Without it the suite reports a skip, not a
runtime pass. Reproduce:

```
CS80_OBSERVER_SDK=/tmp/cs80-observer-sdk python -m unittest discover \
  -s tools/packed-filter-plugin-research -p test_native_observer_bounded_host.py -q
```

All cases account for actual global C++ allocation blocks and bytes across
context/object destruction; every case returns to its initial baseline. Tests
cover string/binary/type-change overwrite, zero-size binary with allocation
forbidden, typed values, owned binary copies, odd/truncated UTF16 bytes, output
reset, interface ownership, retained objects after context-owner destruction,
message ID replacement/rollback/allocation-free null clear, independent caps, global versus list keys,
transient replacement budget, constructor/child/key/map/value OOM rollback,
nonallocating QI and guarded live reference counters. The concurrency case runs
four threads doing 1,000 setter/copied-getter cycles each; this is not proof of
arbitrary scheduling or a thread sanitizer result.

Caps in PLAN are frozen defaults; callers may only lower them. The effective
chosen Limits must be frozen in a future driver receipt. Quotas are accounting
bounds, not process RSS limits. SDK scalar attributes intentionally permit all
double bit patterns rather than inventing an audio-parameter range requirement.

Returned message-ID and binary pointers are borrowed only until mutation or
object destruction. Callers must serialize mutation against consuming such
pointers. getAttributes borrows the message-owned list; retain it explicitly to
outlive the message. Method mutexes protect map/string operations; global quotas
use atomic CAS, no shared budget mutex, and no external SDK callbacks are invoked
under object locks. ABI exception handlers return SDK error codes and set a
sticky shared health failure. Ordinary absent/type-mismatched values, unsupported
interfaces and invalid argument returns do not count as internal health errors;
void setMessageID failures do latch failure. Out-of-policy limits and OOM fail
closed. Normal reference-count ownership remains mandatory: release through an
already dangling pointer cannot be diagnosed safely. Tests of zero/overflow
counters use a live internal probe, not undefined dangling-pointer access.

`Context::finish()` is an explicit snapshot requiring externally guaranteed
quiescence: healthy, only the owner's host reference, one service object, no keys
or payload. It is not a terminal state transition; future driver must stop all
callbacks/work and bind the successful snapshot to the final receipt. RAII
cleanup is never admission. Retained references survive owner destruction but
there is then no context capable of issuing that acceptance snapshot.

Optional host interfaces (including IPlugInterfaceSupport) are not advertised.
This is an intentional visible host contract, not a claim of equivalence to the
official HostApplication's optional capabilities. Future real-module observer
integration, metadata/state/render lifecycle, host-policy identity, callback
quiescence, bus ownership, full original fixtures and validators remain gates.
No real plugin is qualified by these service controls.
