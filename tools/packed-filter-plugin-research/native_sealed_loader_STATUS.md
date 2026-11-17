# Joined pinned-file and concrete loader proof: preparation only

SealedLoader joins the reviewed immutable VerifiedIdentity/PinnedModule with
actual canonical-path DlopenOps. It loads only caller-selected pinned bytes; the
controls compile tiny inert libraries with no synth/factory instance/audio.
There is no CLI/default CS80 path, HistoryOwner, ReloadToken or qualification flag.

Before dlopen it rechecks the retained read-only descriptor and entire sealed
identity, parses ELF from that descriptor, snapshots bounded actual self maps and
loaded-object inventory, and rejects BOTH target device/inode mappings and target
path/provider entries. The latter covers replaced same-path/new-inode stale images,
which pure inode rejection misses. Paths with control characters or backslashes
are rejected rather than guessing proc escaping. Existing unrelated loaded object
paths must be resolvable; kernel vdso and empty main-executable names are handled
explicitly. Mapped `(deleted)` suffixes are checked against target path. Owned,
quiescent filesystem/process-loader state remains required during checks/load.

The normal canonical module path is passed to dlopen, not `/proc/self/fd`, so no
new resource-location semantics are introduced. Actual dlinfo must return this
exact path and base. Existing DlopenOps requires each mandatory symbol be a
function exported by that exact module base, not a dependency. Actual symbol
addresses are then checked against actual executable file-backed maps and held
ELF offset relationships. Held-file and whole-identity checks run again. Exact
maps snapshots, loaded-object names, base, export addresses and offsets are
retained read-only for later evidence packaging. Per-load addresses may differ
on fresh reload; immutable disk identity and function file offsets are separate
from ASLR virtual addresses.

All post-open failures retain the primary error and immediately attempt exactly
one owned dlclose, retaining secondary cleanup failure separately. Any error is
permanently terminal even when cleanup succeeds. Explicit close calls actual
dlclose once, verifies target path/inode/provider absence, then rechecks held and
whole identity. Failed dlclose is not retried or called unloaded; retained external
references that leave mappings make close admission fail. Destruction is only
best-effort cleanup, not a replacement for explicit admitted close. Caller must
close plugin instances/ModuleExit before invoking loader close and must not call
open/resolve again while native instances are live. This loader does not itself
establish module-entry/factory/instance lifecycle or worker/UI shutdown.

`providerClosedCleanly()` has private truth and describes ONLY successful provider
open/resolve/close proof. It does not establish complete history or production
admission. Even exact module proof does not bind all loaded runtime/dependency
closure; trusted expected inventory completeness, whole runtime mapping closure,
callback/thread quiescence, native state and release tests remain mandatory gates.

16 joined inert controls pass, including same-inode preload and replaced-path
stale preload, fresh reload, post-open link-map failure, post-resolve provider and
identity failures, resource mutation by constructor and before close, missing /
dependency-provided export, unreadable/malformed/truncated maps snapshots, failed
dlclose (explicitly simulated then cleaned outside the assertion), and retained
outside dlopen reference. Every post-open failure asserts one native open/close;
preload/map precheck failures assert no native load. Combined with47 pure structural
controls,63 pass. No real CS80 module loaded and no sound/performance claim.

## Code-memory and lifecycle boundary

Mapped device/inode/file-offset agreement and current disk hashes establish the
file-backed provider relationship, NOT byte-for-byte in-memory code integrity.
Private COW text modifications, constructor patching, instrumentation and runtime
relocations are not detected by these checks. Trusted loader/process state and
any required code-memory integrity policy remain separate admission gates.

SealedLoader owns only a loader reference, not native component/controller or
worker/callback lifetimes. Its error path may auto-close after an invalid repeated
open/resolve attempt. The future sole lifecycle owner must therefore keep these
methods inaccessible while any native instance, interface or callback can exist;
it must close those objects and call ModuleExit before the loader close boundary.
Public exposure of LoaderOps during live-instance operation is not safe and is
not authorized by a successful provider proof.

### Explicit-only cleanup for a guarded child owner

The default standalone policy remains Automatic. `SealedCleanupPolicy::ExplicitOnly`
is an additive contract for the private joined child owner: failures in either
underlying dlopen/link-map checks or later sealed proof/resolve do NOT dlclose;
the owned handle remains live for whole-graph quarantine. Its destructor also
does not unload. This is not permission to discard a failed object and continue:
the child must retain every native/host lifetime together and terminate nonzero.
An explicit close still runs the usual checked close proof. The joined owner
places its lifecycle/thread/callback gate in front of that explicit call.

The process-lifetime child adds a privately selectable retained-provider
classification; the public constructor and strict-unmap outcome are unchanged.
`nativeCloseReturnedZero`, `ProviderDisposition` and strict
`providerClosedCleanly` are distinct. Only the terminal child can reach the new
profile via its private module owner. Complete classification and limitations
are in `native_observer_process_child_STATUS.md`.
