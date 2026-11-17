# Terminal process-lifetime child, preparation only

This is a separately preselected `ProcessLifetime` profile. Public standalone
`SealedLoader` and `ModuleHistoryOwner` retain the existing strict-unmap default;
no public returning constructor exposes the process-lifetime profile. Only
`ProcessLifetimeChild::run` selects it before any native call. The child runner
never returns, including on failure. No E restore, state transfer, production
launch, CPU timing or plugin qualification is authorized by this helper.

All actual A-D, private worker join, factory release, ModuleExit, host reference,
callback, service-health and exact baseline/post-exit task-inventory gates remain.
A successful native `dlclose` return is separate from provider removal. After it,
bounded maps and loaded-object inventories must agree: either the target pathname
and inode are absent, or the entire original target file-backed mapping roster,
raw map rows/permissions and all three export offsets remain unchanged. Partial,
mixed, malformed or unknown snapshots fail. This does not prove in-memory code
integrity or arbitrary thread quiescence. Full declared disk identity and the
retained module FD are reverified after teardown, including declared runtime/input
files; completeness of runtime closure remains external.

On retained success the whole native ownership graph stays alive. The runner
also retains the graph on removed success: it writes its receipt then immediately
calls `_Exit(0)`, never returns to arbitrary caller code or graph destructors.
Failure preserves/quarantines the graph and exits24; no native cleanup is retried.
A bounded best-effort stderr failure line records the outer stage, module step,
primary exception text (truncated), native open/close counts, confirmed provider
absence, host references/registrations and failed module-call count. It cannot
admit any run; diagnostic write failure still exits24. This is not full partial
PCM evidence or a retry of cleanup. The parent MUST reject nonzero exit even if a full plausible receipt exists (for
example, a close/fsync failure). Hung native calls still require an external
process-group watchdog; the worker deadline alone is not a wall-clock bound.

## Output contract v1

The supervisor supplies the exact requested output path and exactly64 lowercase
hex characters representing32 fresh random bytes. The runner opens anchored
parent directories without symlinks, creates the final file `O_EXCL|O_NOFOLLOW`
0600, validates it is regular, owned, single-link, empty and position0 before
load and before writing. No native code supplies the output destination. It owns
write, fsync and close; any failure produces nonzero termination. The trusted
supervisor must enforce owned/quiescent directories and independently bind the
fresh output roster, actual child PID/exit, argv/profile/case/nonce, tool/runtime
identity, process-group emptiness and overall deadline. Echoed facts and file
hashes alone do not prove authentic execution. No directory durability/power-loss
claim is made.

One binary file: u64LE body size, body, then text body SHA256 (64 ASCII lowercase
hex). Text/blob is u64LE length followed by exact bytes; no padding or trailing
bytes. Body is ordered:

1. text `cs80-process-lifetime-child-v1`, text `process-lifetime`, text nonce,
   u64 PID, text arm, expected-manifest digest, evidence digest, module SHA256.
2. text `cs80-instance-history-A-D-v1`, blob exact case identity, u32 native-close
   success (1), text `provider_removed` or `provider_retained`.
3. text canonical-history SHA256, blob FULL canonical A-D payload.
4. Three text maps snapshots (before, loaded, after); two object-path vectors
   (before/after), each u64 count then text entries.
5. u64 loaded base, three u64 actual export addresses, three u64 file offsets.
6. Two thread vectors (baseline/after), each u64 count then u64 TID/start-time pairs.
7. u64 module-call count then text name/u64 returned-value pairs; all calls must
   have actually returned normally and succeeded.

Full payload cap64MiB, body cap96MiB, final framing overhead80 bytes. Variable
fields are accounted before receipt assembly. Allocations/copies occur and can
fail; no realtime safety claim. The complete post-A state is retained as part of
canonical A-D evidence, not described as default initial state or a DSP reset.
Compare matched stages across arms only, never infer A=C or A=E. This schema means
process-lifetime history completion, not strict in-process unmap completion.

## Inert coverage

Actual tiny SDK-fake library with ordinary unload, explicit NODELETE, and genuine
GNU_UNIQUE anchor (readelf confirms UNIQUE and no explicit NODELETE flag). Positive
controls independently decode full length/hash/PID/payload/ledger evidence. Strict
unmap still rejects retained libraries. Negatives cover malformed/partial/mixed/
permission-drift maps, native-close failure, post-teardown disk drift, output
write/fsync/close, existing/symlink output, invalid nonce, failed ModuleExit,
registered callbacks and unregistered thread retention. Real library positive
history covers one frozen case, not the entire grid. No CS80 module is loaded and
no production compiler/linker flags are changed.
