# Modern LFO depth automation qualification

The candidate appends LFO1, LFO2 and LFO3 depth amounts to the unchanged
83-lane prefix, for 86 sample-accurate lanes. Production changes are limited
to the roster; no DSP or UI source changes are included.

## Verified native results

- Linux VST3, 44.1/48/96 kHz, realtime and offline.
- 96 full cases and 96 frozen-83 compatibility cases passed.
- 1,344 candidate and 672 baseline renders: 2,016 completed renders.
- 384 actual PCM pairs compared byte-for-byte, each 32,768 bytes.
- All measured block-partition errors were zero.
- All 96 held-note effect checks were positive (0.00261088 to 0.139784).
- Nine reviewed source-freeze identities, 618 frozen source/test identities,
  and 616 retained candidate source/test identities remain unchanged.

The qualification holds sine rates and destinations fixed. It covers each
individual depth and their combination with the plan's selected pitch,
filter and amplitude destinations. It does not qualify rate automation,
all waveform/destination combinations, other operating systems or DAW hosts.
Historical suites were not rerun. Forward compatibility guards were source
reviewed only, not executed as part of this qualification.

## Interrupted run and recovery

The original 48 kHz offline candidate attempt retained a command-only log
and partial capture directory. It is not counted as a passing run. The
completed 44.1 kHz realtime/offline and 48 kHz realtime batches were retained
and their logs and actual PCM pairs reverified rather than rerun. Recovery
used a fresh `candidate-48000-offline-resume.log` and `-resume` capture
directory, preserving the original attempt untouched, then completed the
missing baseline and both 96 kHz batches. The completed-render count above
excludes any work from the interrupted attempt.

`summary.json`, `compatibility-pairs.json`, and `sha256.json` contain the
completed-run totals, actual PCM comparisons, and native/evidence hashes.
The original design in `docs/LFO_DEPTH_AUTOMATION_PLAN.md` is historical;
its opening design-only status describes the pre-implementation checkpoint.

Candidate runner and module hashes identify the final recovered artifacts.
No pre-recovery candidate binary hashes were recorded, so this evidence
does not establish candidate binary identity across the interruption. The
frozen baseline module hash was verified before and after recovery.
