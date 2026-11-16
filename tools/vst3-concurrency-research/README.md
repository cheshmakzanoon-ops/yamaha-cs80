# Frozen 12-case VST3 concurrency diagnostic

This is diagnostic tooling, not a production scheduling or DSP change. The existing
`build` workflow and all its gates remain unchanged. This additional job builds
only the unchanged generic/PWM VST3 fixtures and module, then runs their twelve
existing cases serially and with concurrency two. It does not claim the remaining
CTest suite passed; the normal build continues to own that qualification.

Source and CMake are frozen against `f9dc1586bf8bfbecd51d776f97c6ed0100750d20`.
Actual CTest JSON inventory must contain exactly the expected twelve case names,
arguments and original 180-second timeouts. Unknown properties fail closed.
Per-case HOME/XDG paths isolate persistence; they must remain empty. External MTS
libraries must be absent. PWM uses the original unique CMake capture paths; each
completed capture directory moves into the result folder, without deletion or
overwrite. Captures are preserved separately for both phases.

Acceptance requires all 24 exit-zero results; full PWM feature mode; 24 scenarios
and 72 PCM files of 32768 bytes per PWM run; identical per-case stdout and capture
sets/hashes; empty homes; and unchanged executable/module hashes. Any failure
returns nonzero and preserves receipts. A single fixed serial-first run can show
feasibility and observed elapsed time, not repeatable CI savings, complete hidden
state equivalence, whole-suite concurrency safety or synthesizer CPU reduction.

## Prior local evidence

The same 12 cases passed locally on the successful exact-main CI module, using
locally rebuilt unchanged fixture sources and pinned JUCE SDK. All 432 PCM pairs
and 12 log pairs were byte-identical; all homes empty. Observed wall times were
678.83s serial and 359.40s parallel. An independent raw-file audit passed. Local
syscall tracing was unavailable, so no syscall-proven isolation is claimed. The
local inputs/environment differ from this fresh-runner diagnostic; its result is
required before considering any production workflow change.

## Operation

The separate workflow runs on relevant `main` pushes (and allows manual dispatch).
Each run permits only attempt one; the protocol forbids unreviewed repetition. It uses read-only repository permission, a 75-minute ceiling and
failure-preserving artifact upload. Future source/CMake drift rejects this frozen
experiment instead of silently changing its scope. Do not rerun or widen the
experiment to select a favorable result. Review a new protocol first.

```
python3 -m unittest discover -s tools/vst3-concurrency-research -p 'test_*.py' -v
python3 tools/vst3-concurrency-research/run.py --repo "$PWD" \
  --build "$PWD/build-diagnostic" --out "$PWD/vst3-concurrency-results"
```

`results.json` is authoritative only when `accepted` is true and the raw receipts
agree. Existing build CI success and this diagnostic success are separate claims.
