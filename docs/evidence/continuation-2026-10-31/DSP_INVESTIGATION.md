# DSP continuation: preregistered questions

2026-10-31. Starting revision `db24a47170610f81236e8829ef5536af69dfc94d`.
No candidate audio change has been evaluated at this checkpoint.

## Retrigger and release

The unchanged phrase audit reports Bass Stack's retrigger score as 1.51 at
44.1 kHz and paired Funky 1 + Funky 2 note-off as 1.65 at 48 kHz. A score is a
ratio of maximum sample-to-sample steps over a window; it does not identify a
physical discontinuity by itself.

The investigation will compare the amplitude and filter envelopes, filter
capacitor voltages, IG00151 LI node and output-coupling state at the actual
assignment/release. Counterfactual probes must isolate mechanisms; they are
not proposed tonal changes. A fix must preserve free-running phase and the
capacitor states, rather than masking an output step with a fade.

One source-review hypothesis is that a never-used PANEL/MEMORY voice warms
its filters with an untriggered filter envelope at zero, then `startVoice`
replaces that capacitor level with IL. In the selected EG topology, released
TR selects IL (see `ENVELOPE_TARGET_TRANSITION_REVIEW.md`,
`ENVELOPE_RC_MODEL.md` and `ACTIVE_TAIL_CONTINUITY.md`); the existing numerical
RC law is a declared inference. The hypothesis and alternatives (VCA pedestal,
amplitude-envelope restart, and coincident ordinary waveform edges) remain
unproven until traced.

The new continuity and phrase limits are in
`tests/parity/tolerances-v2/completion-dsp.json`. The existing score calculation
and its windows will not be changed. Any surviving score above one will be
reported, including a case proved to be a normal waveform edge.

## Filter numerical accuracy

`Ig00156Stage` already prewarps every capacitor at the requested cutoff.
The outstanding error is the curvature of the bilinear frequency map over
a broad band, not the absence of a prewarp. The current nodal test prints
whole-grid misses and asserts only the already-passing subgrids. Completion
requires asserting the existing limits over the entire registered grid.

No hardware-network or Q law will be adjusted to compensate for a numerical
frequency-warp error. A candidate correction must keep the continuous truth
model and use an independent derivation of the new discretization. The
limits in `ws1-voice.json` remain unchanged.

The ADJ p11 HP discrepancy is an evidence question. M41's LP 150 kOhm
interpretation (shunt in the inspected ledger, series in the current declared
model), HP input-switch loading and the actual adjustment setup need direct
inspection. Component or adapter values will not be selected to fit the
reported voltage.

## Release and retrigger evidence, baseline evaluation

The new `--suite click-causality` audit reproduces the original metrics on
`db24a47170610f81236e8829ef5536af69dfc94d` plus diagnostic-only changes. It
uses the original phrase, event rounding, 8 ms scoring window and denominator.
Only the first note-off is omitted in the release control. All samples before
that event are exactly identical. The reproducible CSV is
[`click-causality-baseline.csv`](click-causality-baseline.csv).

| Fixture / host rate | Original score | Controlled comparison |
| --- | ---: | ---: |
| Funky 1 + Funky 2, 44.1 kHz, note-off | 1.441274 | 1.441133 with note-off omitted |
| Funky 1 + Funky 2, 48 kHz, note-off | 1.647965 | 1.679875 with note-off omitted |
| Funky 1 + Funky 2, 96 kHz, note-off | 1.290939 | 1.326840 with note-off omitted |
| Bass Stack, 44.1 kHz, retrigger | 1.510516 | 1.179700 at the first strike |
| Bass Stack, 48 kHz, retrigger | 1.308168 | 1.017237 at the first strike |
| Bass Stack, 96 kHz, retrigger | 1.246674 | 1.014932 at the first strike |

At 48 kHz the largest Funky release-window step is 84 host samples after the
event. It is already present, with greater amplitude, when no release is sent.
The event therefore cannot be the cause of that flagged waveform edge. This
does not prove that every possible release is inaudible, and it does not
waive the existing score target. The score remains above one and open.

`NoteOffContinuityTests.cpp` checks both ranks of the actual Bass Stack and
Funky fixtures at 44.1/48/96 kHz, every oversampling quality and three note
durations. The pre-registered instantaneous state limit is zero: the amp EG,
filter EG, VCA LI/output-coupling capacitors and complete filter histories
must be identical before and after `noteOff`, before elapsed time is rendered.
The omitted-event control has byte-identical preceding audio, and the test
also verifies that the tested note really changes from held to released.
Baseline result: **540 checks, zero failures; maximum state delta exactly 0**.
Mutation check: inserting `r.vca.reset(0.0f)` in `releaseVoice` makes the same
test fail **108 of 540 checks**, with a maximum instantaneous delta of
2.659253597. The mutation was compiled from a separate temporary source and
is not present in the production tree.

The Bass Stack trace finds a separate initialization discrepancy: an unused
PANEL voice's filter EG can warm at zero and then be replaced by its negative
IL on assignment. At the 44.1 kHz repeated strike that is a 0 to -1.44 octave
replacement. This is inconsistent with a continuously released EG capacitor,
but it does not explain the score outlier. A diagnostic cold-IL initialization
experiment changes the retrigger score from 1.51052 to 1.51169; removing only
the previous release tail changes it to 1.50447. The largest output step is
about 7.98 ms after the new strike, with smooth amp EG and VCA node traces.
These are causal probes of this software, not proposed fixes or hardware
measurements. The cold-EG issue and attack metric remain separate open items.

No output fade, oscillator phase reset, VCA pedestal retuning or patch change
is justified by this evidence. The physical-state regression guards an
actual event-reset mechanism; the audit retains the unresolved metric.
