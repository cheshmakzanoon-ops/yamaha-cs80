# REAPER dry-fixture V2 — preregistration, 2026-11-02

## Actual V1 outcome remains FAIL

Original source `2b50a62086087984a3b578f022c6e752d6a18e7e`, src
`7dd159d491012fd47b46c0aa651e86ea959ca289`, ran in
[Actions 41439957224](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41439957224).
Official REAPER 7.84 Linux x86-64, archive and installed/built plugin hashes were
verified. The real host passed 24,022 setup assertions, saved/reopened projects,
opened the actual editor, and produced all four 96,000-frame stereo/48-kHz WAVs.
The original verifier correctly failed the no-MIDI silence condition. The
original `contract-v1.json` bytes and its result are immutable in this amendment.

The original artifact is ID 13139541433, SHA-256
`eaf3a50d149c001f6687075bb6e20d05d1b26036750b0a2b803c023b18b53825`.
The actual no-MIDI project contains no MIDI items, disables all render dither
(`RENDER_DITHER=16`), and its decoded VST3 state has analog/rank noise and drift
zero, but **CHORUS ON=1** from the fresh String 1 program. The BBD model has a
separate packet-noise source, intentionally independent of ANALOG NOISE.
That enabled effect was not neutralized by the first fixture.

No-MIDI peak was 0.00023257732391357422 and RMS 0.000031138818984500537;
the floor persists throughout two seconds. Baseline pre-note audio has the same
floor, so it also cannot meet the original pre-note silence bound. Restored
samples are identical to baseline (maximum difference zero). No numerical bound
is increased and no production noise source is suppressed to obtain a pass.

## Causal control before the revised host run

Using exact PR6 core objects and all 191 core parameters decoded from the real
project, a no-MIDI cold render with chorus ON produced peak 0.000268811 and RMS
0.0000439854; changing only chorus ON to zero produced exact zero. Restoring
chorus ON reproduced the original samples identically. Two initial diagnostic
setup attempts with incomplete ID mapping failed before rendering and are not
counted as behavioral evidence; the final driver uses canonical `paramId`.

As a post-hoc attribution check, averaging the two original core channels and
discarding 64 frames agrees with the actual no-MIDI render within
5.960464477539063e-8 (half a 24-bit PCM step); wrong offsets do not. This strongly
identifies the preserved BBD output path. It is not promoted to complete offline
PDC acceptance: the original contract excludes that claim.

## Registered V2 changes

- Add a separate `contract-v2-dry.json`, with the same host archive/evaluation
  dates, four MIDI/automation/render cases, windows and **every original limit**.
  Only the new fixture's CHORUS ON is explicitly zero. Do not change the shipped
  preset or production model. Require its readback before save and after reopen.
- Retain the original synthetic verifier controls. Add a complete V2 metadata
  positive and exact-reason negative with restored chorus incorrectly enabled.
  Hash-lock the original V1 bytes and compare all shared limits/fixtures.
  First run these controls against the unmodified verifier to capture the new
  positive's expected missing-state-contract failure, then implement V2 support.
- The actual first screenshot was inspected: the loaded instrument is visible,
  but the first-run tour overlays it and REAPER's evaluation-about window is
  behind it. In the next Actions-only run, retain that genuine first screenshot,
  focus the unique instrument window, send one ordinary Escape key to dismiss
  the plugin's tour, and capture again. Do not edit/generate an image. Visual
  acceptance remains pending until the real new screenshot is inspected.

The host reported PDC 64 for plugin and chain, but its live audio device was
closed (`GetAudioDeviceInfo` unavailable); this is an offline-render smoke, not
live-device/realtime coverage. The evaluation dialog was not bypassed or its
clock reset. The unchanged fixed evaluation period still applies.

V2 has no actual host result at this registration. Remote publication/execution
is currently held by root pending the user's approval for project test/evidence
payloads after an unrelated packaging upload denial. Local investigation and
controls do not authorize an alternative remote upload path.
