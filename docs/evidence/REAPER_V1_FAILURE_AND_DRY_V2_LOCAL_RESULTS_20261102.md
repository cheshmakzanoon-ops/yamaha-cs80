# REAPER V1 failure and dry V2 local results — 2026-11-02

## Actual host result: V1 remains FAIL

[Run 41439957224](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41439957224)
executed published source `2b50a62086087984a3b578f022c6e752d6a18e7e`, src
`7dd159d491012fd47b46c0aa651e86ea959ca289`. The actual four render processes
completed, but the original verifier failed **no-MIDI silence**. This failure
must not be replaced by a V2 result or a synthetic unit-test success.

Original artifact 13139541433 has SHA-256
`eaf3a50d149c001f6687075bb6e20d05d1b26036750b0a2b803c023b18b53825`.
Official REAPER archive identity is locked in unchanged `contract-v1.json`.
The built and installed plugin SHA-256 was
`0f2d7e29cf7d9ad87b1cdaa2b5c3b5c30990efb5c61c4762bf652dca196a16d8`.

The actual saved no-MIDI state retained String1's **CHORUS ON=1**, despite
analog/rank noise and drift being zero. Preserved BBD packet noise is independent
of ANALOG NOISE. A diagnostic core replay loaded all191 canonical parameter IDs
from that actual project; switching only chorus OFF made the cold no-note output
exactly zero, and restoring ON reproduced the first samples identically.
Downmixing the original core channels and discarding64 frames matched the
actual 24-bit host no-MIDI WAV to half a PCM step. Wrong tested offsets did not.
This is post-hoc causal attribution, not a newly invented PDC acceptance gate.

The actual screenshot was visually inspected. The plugin is loaded and its
full panel is present, but the initial tour overlays it, and the evaluation
about window is behind it. The live audio device was closed; this is offline
render evidence only. No clean-editor, live-device, all-DAW, or all-platform
acceptance is inferred from it.

## New explicit dry V2: local checks only

Preregistered at `60de75e6ecb320d1f2973f6bdc1b81e1597d226e`, V2 adds only an
explicit CHORUS ON=0 fixture state, with all V1 numerical limits, time windows,
host checksum and fixed evaluation dates unchanged. V1 contract SHA-256 remains
`e0fb81082eaac5989bc25655f0f9cef4a9c9930b9c2a1193624bdc469b1ec362`.
The new metadata-positive initially failed against the unmodified verifier,
as expected, because V2's additional state field was not yet supported.

Implementation `87bf4bd99d3716a613fdcecbf6e2d89a068712c3` and dispatch-test
amendment `6c3ef7705d2e6cb492087acb78cec767302e4c75` retain src `7dd159…`:

- Seven synthetic verifier tests pass, including genuine metadata positives
  before exact-reason negatives for V1 and V2. Wrong restored chorus is rejected.
- All15 actual AST guard-deletion mutants are rejected through ordinary
  assertion failures, with no unrelated execution-error masking.
- Three mock-only input-dispatch tests pass: exactly one Escape after unique
  instrument-window activation; no input on absent/ambiguous/invalid windows;
  and no Escape or after-capture claim when activation fails. No display or host
  is launched by these tests.
- Lua syntax is parsed by installed Lua5.4; five pure parameter-inventory
  assertions and the reintroduced duplicate-name defect control pass. This is
  not real-host Lua execution.

In a later authorized Actions run, V2 retains the actual before screenshot,
sends one real Escape in the uniquely identified instrument window, verifies
the editor remains open, and captures the actual result. Image review remains
required even if that sequence succeeds. No generated or edited screenshot is
accepted. The evaluation dialog and licensing clock are untouched.

Raw local/diagnostic records are in
[`reaper-dry-v2-20261102/`](reaper-dry-v2-20261102/), with a SHA-256 inventory.
The original full artifact and raw channel captures remain preserved in the
task's scratch evidence directory. **V2 has no actual DAW result yet.** Remote
publication/execution is held by root pending user approval for project
test/evidence payloads. That hold is not bypassed by the local results.
