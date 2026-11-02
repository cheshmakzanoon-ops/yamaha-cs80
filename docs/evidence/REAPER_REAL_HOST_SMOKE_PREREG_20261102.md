# Limited REAPER host smoke — preregistration, 2026-11-02

This contract is registered before running the new fixture. It adds actual DAW
coverage; it does **not** replace native wrapper tests, pluginval, AU validation,
the original 885-row audio audit, CPU qualification, or manual DAW acceptance.
No result is claimed by the existence of this document or workflow.

The pinned host is official REAPER 7.84 Linux x86-64. The publisher provides a
fully functional 60-day no-registration evaluation and publishes the archive
SHA-256 on its [download page](https://www.reaper.fm/download.php?hashes=1).
The workflow is bounded to the evaluation period beginning 2026-11-02 and stops
before 2026-12-31. Fresh CI profiles must not extend this period. Continuing
afterward requires separately authorized licensing, not a modified timestamp.
The host binary is not included in project release artifacts.

`tests/hosts/reaper/contract-v1.json` fixes the inputs and limits. A ReaScript
running **inside the real host**, using its [public API](https://www.reaper.fm/sdk/reascript/reascripthelp.html),
must load the built VST3, verify identity and a fresh Circuit instance, construct
four MIDI notes and a volume envelope, save projects, perturb and reopen the
saved state, and read back parameters/MIDI/automation. Four independent offline
render invocations produce baseline, restored, volume-automated and no-MIDI WAVs.
The verifier checks audible baseline, repeatable restored output, automation's
silent interval, unchanged early audio, silence without MIDI, finite/bounded
samples, and exact duration/rate/channels. These are smoke checks, not musical
fidelity, all-parameter automation or sample-accurate-host acceptance limits.

Noise, drift and dispersion are disabled solely for this **new deterministic
host fixture**. No original acceptance fixture or default is changed. The real
host's raw PDC query is archived, but offline compensation correctness remains
unclaimed until independently measured. A screen capture is taken only from the
actual host with the actual plugin editor open; its visual review remains a
separate required observation, not a fabricated or pixel-count-derived PASS.

Malformed WAVs, wrong rate/channel/frame counts, silence in the active window,
audio without MIDI, lost automation, changed restored output and provenance
mismatches must fail the verifier. Synthetic negative controls test the
**verifier only**, never stand in for real-host evidence. Any bootstrap/API
assumption is provisional until a native Actions run demonstrates it.
