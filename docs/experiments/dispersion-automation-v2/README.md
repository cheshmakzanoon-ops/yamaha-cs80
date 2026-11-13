# VOICE DISPERSION119 scoped qualification

Local qualification completed 2026-11-13. Publication is tracked separately.

The only executable production change appends existing `P_voiceDispersion` at
index 118, preserving the literal 118-entry prefix. No DSP law, UI, MEMORY,
parameter ID, normalization or metadata changes. The drift fixture forward guard
is source-reviewed only, not rerun. Existing live tolerance-ratio/envelope-time
refresh behavior is unchanged.

## Attempts and provenance

Attempt 1 failed both candidate/helper builds because the inherited new fixture
missed one closing brace. No native captures ran. Original fixture, runner, plan,
freeze, logs and recorded exits remain in `../dispersion-automation/`, with
`attempt1-hashes.json`. Review initially missed this syntax defect.

V2 adds exactly that brace without changing histories or numerical thresholds.
The named candidate target was reconfigured/rebuilt incrementally in the same
CMake cache; it is not a fresh clean build. V2 helper, evidence and capture paths
are fresh. Both builds, six native configurations and collection then passed once.
No failed native stage was retried and no evidence was overwritten.

Baseline source is qualified DRIFT118 local commit
`803caea8157925da35d492681de30a568e741f2f`. Its copied module SHA256 is
`f1121774d2865a8f0516d97a2d99242924df700d9091a4139eb266cf304ff683`, matching
that qualification; no baseline plugin rebuild is claimed. The retained 5,721-file
baseline inventory SHA256 is
`49f364b37e030d6ee3261cf91f75a6d9edec3b20043cc07399601c5c763de4a3`.
Matching new helper fixture/header copies were added after the inventory.
Eleven reviewed v2 inputs are frozen in `tests/dispersion-source-freeze.json`.
Every stage verifies source, baseline and completed evidence identities.

## Completed gates

Six configurations: 44.1/48/96 kHz × realtime/offline, with four scenarios each.
Two sustain three notes until near the end, alone/with volume; two release earlier,
then change dispersion only after identical pre-release and initial-release audio.

- 552 finite exact-size stereo float32 captures: 432 candidate and 120 baseline.
- 120 actual frozen 118 compatibility pairs byte-identical.
- 96 scalar/block comparisons: maximum error 0 under the fixed 1e-8 ceiling.
- 48 reversed-queue comparisons, 24 initial 1024-frame holds and 12 extended 10240-frame
  release holds byte-identical.
- 72 exact controls: empty/null, Legacy flattening, PURE dispersion masking.
- 24 overall effect witnesses: minimum 0.001214061485370621.
- 24 individual-lane witnesses: minimum 0.0005280200857669115.
- 12 post-release witnesses: minimum 0.0011061535769840702.

Effect floors remain 1e-6. The post-release witnesses compare frames 16384..28671
following byte-identical audio through 10239, including noteoffs 8192..8194;
therefore they do not merely inherit different pre-release trajectories.
They are aggregate output evidence, not isolated envelope-time measurements.
`collect/complete.json` retains independently recomputable metrics; stage files
retain binary/log/PCM identities. PCM and binaries remain local, not in Git.
Absolute evidence paths are provenance, not portable download links.

## Scope

Fixed manual/manual sources, source-performance policy, neutral macros/modulation,
ARP/scanner off, drift/noise zero and fixed bypassed FX isolate the overlay.
PURE holds only dispersion and preserves matching volume motion. No all-source,
full-range, isolated-component, hardware, DAW/AU/cross-platform, state-readback,
CPU, whole-tree or release qualification is claimed. Historical suites are
unchanged and were not rerun. Cross-rate PCM identity is not asserted.
