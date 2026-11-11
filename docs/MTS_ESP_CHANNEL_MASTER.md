# Channel-specific MTS-ESP master tuning

Conventional MIDI now queries the pinned MTS-ESP client for all 16 channel tables,
including independent note-filter flags. Previously the processor queried channel
-1 only and broadcast that global result. MPE retains channel-independent polling,
as recommended by the pinned upstream API. PURE also retains global polling and
continues to ignore tuning in audio rendering.

A complete bank is staged before publication. Invalid mapped frequencies or an
observed disconnect reject the whole update, retaining the prior accepted bank.
Filtered invalid frequencies retain finite note-number fallbacks. One discarded
channel-filter query primes the upstream client's remembered channel capability
before the first frequency query after global polling. Equal safe frequencies
reuse only their pitch conversion within one note; filtering is always queried.
There is no persistent change cache: an unchanged master still restores its table
on the next block after a local SysEx message.

The accepted bank updates active and dormant engines. Held voices retune through
the existing engine policy; future extension activation retains all channel tables.
Block-start master polling and sample-position SysEx ordering remain unchanged.
This does not provide an atomic snapshot of concurrent master edits: the upstream
API exposes no revision token, and rapid disconnect/reconnect can escape detection.

## Regression evidence

The initial processor regression against the old global-only implementation
reported 3,921 checks and 1,952 failures. The first implementation passed those
3,921 checks. The final fixture additionally covers held voice updates, local
SysEx precedence, MPE and PURE transitions, and extension activation after master
disconnection. This initial red count is not a claim that the expanded final
fixture was rerun against the old implementation.

The adapter fixture covers all channel pitches and mappings, bit-exact agreement
with direct pitch conversion, malformed frequencies at early and late bank
positions, observed disconnect, filtered invalid values, and global/channel
capability transitions. Tests use deterministic replacements for the external C
API with the production adapter and real processor. They do not test discovery,
a running external master, cross-process concurrency or a DAW session.

The final focused Release/Linux run passed **14/14 CTests in 64.49 seconds**:
core tuning/MIDI, six MTS adapter/backend fixtures, six actual-module VST3
rate/mode registrations, and the direct-processor allocation fixture. Detailed
counts include channel adapter **6,226/0**, processor **5,633/0**, tuning **23,473/0**
and allocation **193/0**. The allocation fixture does not exercise a live master.
This is not a whole-project or full platform qualification.

## Polling cost and limits

A precomputed mock-backend measurement pinned to CPU 8 ran nine alternating pairs
of 1,000 polls for each mode (54 rows). Median nanoseconds per poll were:

| Mode | Previous global-table API | New bank API |
| --- | ---: | ---: |
| Global / MPE | 1,220.516 | 1,560.758 |
| Conventional MIDI, common frequencies | 1,224.902 | 11,202.370 |
| Conventional MIDI, distinct channel frequencies | 1,193.365 | 21,917.888 |

This is a functionality/cost tradeoff, not a speed optimization. Conventional
polling performs 2,048 frequency and 2,049 filter queries instead of 128 each.
The global path retains 128 of each query but fills a bank. Processor storage grows
by 9,600 bytes and staging uses an approximately 10 KiB bank on the stack.
The microbenchmark excludes real external-library calls, engine publication,
wrapper processing and master contention. It does not establish deadline safety,
lock freedom, CPU-target compliance or live-service performance. Missing-master
polls do not populate a bank. Existing bank/program limits remain. [Scale-name display](MTS_ESP_SCALE_NAME.md)
is implemented separately on the message thread.

A later [within-poll validation reuse](MTS_ESP_VALIDATION_REUSE.md) reduces the
common-frequency mock-backend cost while preserving all external queries. The
measurements above remain the initial feature checkpoint, not the later result.
