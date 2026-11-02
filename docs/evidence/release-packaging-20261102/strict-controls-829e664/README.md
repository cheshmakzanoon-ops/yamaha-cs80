# Corrected semantic controls and native-payload infrastructure

Clean local source `829e66468a810ec703cf713f5aa281a0eee4efdf`, tree
`34c266c94492ee7f99b14750f0855918eab17ecf`, passes five focused CTests,
34 synthetic archive checks, eight distribution-helper semantic mutations and
nine independent extracted-document/controller guard mutations. Every semantic
mutant has ordinary assertions and zero runtime errors; unmodified and restored
controls pass. Tests ran under the shared heavy-check lock; no tracked/untracked
worktree changes existed before or after. `receipt.json` hashes the exact logs.

## Correction of the earlier mutation claim

The old runner treated `ERROR:` as a kill. The archived wrong-digest mutant only
crashed, and three omission mutants mixed assertions with errors. Those original
logs are preserved, but the blanket claim that all eight were qualified semantic
kills is withdrawn. Nothing here retroactively changes the old results.

New preregistration `53116a5` specifies exact counts and failing test identities.
The new classifier contract first fails with eight ordinary assertions across
seven tests, then passes all seven. Required-file assertions now precede reads,
so missing payloads fail semantically instead of throwing FileNotFoundError.
The digest mutant corrupts the serialized manifest **after** the producer's
self-check; independent staging and extracted-Debian checks each detect the bad
hash. It cannot masquerade as a missing build result. Eight mutants now produce
the preregistered counts **7, 4, 4, 3, 1, 1, 1, 2**, with zero errors or skips.
A separate deliberate RuntimeError source mutant is rejected by the classifier
and explicitly **not** counted as a semantic kill. The production notice helper
and all notice requirements are unchanged.

## Bounded native infrastructure

The new independent verifier compares extracted notice bytes with tracked source,
not merely a self-consistent manifest; source commit/tree/URL, inventory, hashes
and limited claims are checked. CI now runs actual native package creation and
document inspection: Debian extraction, macOS pkgutil expansion, and a disposable
Windows Standalone-only install followed by uninstall. Installer names explicitly
say CI-UNSIGNED. The macOS filename/host architecture derives from actual lipo
slices instead of advertising a universal binary unconditionally. Existing UI
candidate/diff PNGs are added to test-evidence uploads without changing goldens
or thresholds.

Windows partial-install failure/timeout still attempts cleanup and preserves its
primary error. Preregistered `f5c8115` additionally demonstrates an ordinary failed
assertion when a successful installation omits its uninstaller; the repair now
rejects that condition rather than emitting an install/uninstall success receipt.
The controller suite passes seven tests; guard deletion produces its intended
single ordinary assertion. The nine verifier/controller mutants all pass their
intended rejection controls. CMake registers the five focused Python suites in
normal CI without modifying DSP or numerical/release qualification workflows.

**Execution boundary:** local Debian containers have synthetic non-loadable
binaries; Windows control-flow tests are mocks. No actual macOS/Windows installer
execution, native binary identity, signing, numerical acceptance, physical
measurement, real-DAW success or release is established by these logs. Native
Actions must supply that separate package-execution evidence.
