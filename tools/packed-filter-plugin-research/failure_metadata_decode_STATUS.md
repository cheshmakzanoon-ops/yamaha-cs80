# Read-only rejected-run Connected metadata decoder

This decoder consumes the distinct `cs80-process-lifetime-failure-metadata-v1`
frame only when its caller supplies independently observed exit24, PID, nonce,
arm and expected preload identity digests. These are checked exactly, along with
framing, hashes and the explicit `preload-identities-not-postfailure-verification`
scope. It never sets an admission/qualification flag, never retries or loads a
module, and does not change the successful observer supervisor.

The Connected metadata decoder retains each bus and every parameter in original
enumeration order: IDs, UTF16 titles/short titles/units (original code units and
hex plus display text), raw and signed step count/unit ID, default/current values
and exact f64 bits, and flags. It rejects duplicate/sentinel IDs, malformed order,
truncation, trailing bytes, excessive counts, nonfinite/out-of-range normalized
values and inconsistent hashes. Metadata cap16MiB, body cap metadata+4096 bytes,
1024 buses/section, 16384 parameters and127 UTF16 code units/string. The diagnostic
reader's parameter cap is intentionally tighter than the native producer's broad
one-million enumeration guard. Unsupported larger captures remain rejected,
never silently truncated. Synthetic controls cover framing, identity,
metadata and classification; actual plugin inventory is a separate observation.

## Source-backed classification, not permission to change parameters

`classify_roster` receives the separately generated control-name/ID list and
verifies its exact managed-parameter hash rule. Current `workload_manifest.hpp`
produces192 controls:190 synth parameters plus two hardware-source parameters.
The complete roster stays retained even when a row is classified.

Pinned JUCE wrapper SHA256:
`16031325b31be2e32b23689f07338bd6c631941475b9bbf87d176d1f2565c202`.
Relevant wrapper lines: internal IDs410–416, managed/bypass/program construction
650–735, program metadata1025–1041 and MIDI registration1766–1790.

Pinned SDK MIDI enum SHA256:
`c51527d390b09d62e71be172908b81990f77c25920047f729cfc9b5c69f73e4b`.
Aftertouch128 and pitch bend129 precede the implicit count130. The program-change
alias130 appears later and is not included in that count.

The source patterns are wrapper bypass ID0x62797073 with title/flag, wrapper
program ID0x70727374 with title/flag, and16×130 MIDI proxy IDs starting0x6d636d00
with exact channel/controller title, empty units/short title, flags0, unit0,
step0 and default0. Bypass requires exact flags65537/step1/default0. Program
requires exact flags32769/unit0/step522/default0, matching the source523-program
bank. Current values remain observed, not assumed equal to defaults. Classification explicitly
remains source-pattern-only, not runtime source attestation or write authority.
Unknown rows are retained and labeled unclassified, not discarded.

## Handling proposal, contingent on actual capture and separate review

Do not expand initial flush to every native ID. In particular, writing every
MIDI proxy at zero can generate controller events, reset/all-notes-off operations
or change sound. Writing the program proxy can select a preset and overwrite the
carefully frozen synth controls. Bypass is also not an ordinary synth parameter.

Source also defines104 memory controls: four banks of30 channel controls minus
feet, pan, HPF-enable and LPF-enable. The two hardware-source controls are already
in the generated192 and must not be double-counted. The source expectation is
192+104+2+2080=2378 total. Memory rows are synth controls, not wrapper parameters;
`classify_roster` accepts a separately source-derived memory name/ID map and keeps
them distinct. Their exclusion from active workload requires source/routing
review rather than an automatic extra-ID exception.

If the actual full roster exactly matches the192 generated controls,104 memory
controls and pinned wrapper inventory, a new explicitly selected workload-binding policy could
partition active workload controls, separately verified dormant memory controls
and observation-only wrapper rows. Preserve the current strict
full-ID-set default. Under the new policy, require exact complete metadata and
source-bound role/count/ID agreement; reject unknown/colliding/missing rows; write
only the frozen192 control domain; separately retain and compare wrapper values
and metadata across arms/histories. Any required bypass/program/MIDI value must be
preselected and verified, not silently rewritten or inferred from a failed run.
The exact semantics and points of re-observation need review after actual data.
This document grants no implementation or qualification authority for that policy.
