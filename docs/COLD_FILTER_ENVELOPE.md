# Cold filter-envelope charge and IL selection

2026-10-31. Circuit-engine model correction; no hardware measurements claimed.
The two acceptance limits were committed in `85ccd9a`, before candidate
evaluation: cold released IL error at most `1e-7` after twenty release times,
and exactly zero filter-envelope capacitor jump at assignment. The existing
phrase metric and its limit of one are separate and unchanged.

## Mechanism and source boundary

The pre-change `reset()` clears each filter EG to zero. An untriggered Idle
state does not follow IL. `startVoice` treats `envelopeReset` as permission
to overwrite the filter state with IL before any sample elapses. Bass Stack
therefore gives `0 -> -1.439999938` in the physical-unit PANEL API, or
`0 -> -0.5906781554` with the electrical programmer, in either source policy.
The trace is in `evidence/continuation-2026-10-31/cold-programmer-before.csv`.

M41, freshly re-examined in the native `m-r1-c0.png` crop from inspection
workflow 41224106605, separates IC12 IL16/AL14/SL15 and LC13 from the stored
node IC1 OUT8 -> IC12 BI2 with its 0.22 uF capacitor. LC13 drives the external
active element through 100 kOhm; TP1 returns through a separate 100 kOhm
feedback branch. The IC guide, PDF page 33, printed 2-3, shows level selection
and sequence control separately from that buffered state. Source identities
and the guide's part-number discrepancy remain in
[ENVELOPE_TARGET_TRANSITION_REVIEW.md](ENVELOPE_TARGET_TRANSITION_REVIEW.md).
There is no traced gate-operated discharge branch on the capacitor.

IL is the selected drive, not a new affine offset to add at each trigger.
The existing unit conversion `TP1 = (5/3) * z` and the frozen cutoff receiver's
affine service alignment are unchanged; neither is used to replace `z`.
With bounded drive, the ideal capacitor's voltage is continuous. The precise
IC current law and sequence thresholds remain inferred, as documented in
[ENVELOPE_RC_MODEL.md](ENVELOPE_RC_MODEL.md).

## Declared initial condition and selected law

Keep reset's existing zero-charge initial condition. Under either source
policy, released/untriggered Idle selects IL and approaches it through the
already registered release RC law, `exp(-ln(1000)/(T*fs))`. This adds no new
time constant or output fade. Twenty release times settle the declared law.
An alternative is to initialize a powered-on circuit at a precharged steady
IL value; that stronger warm-state assumption is not needed for this fix.

At every source-policy assignment, retain the current filter charge,
including a first note before settling and a note immediately after IL changes.
SUSTAIN II may retire an amplitude tail as before, but the already released
filter capacitor remains connected and keeps its existing release phase/time.
Legacy keeps the old untriggered zero state, reset-to-IL assignment and
SUSTAIN-II reset. Explicit engine reset still resets stored state.

The new test covers three host rates, all three quality settings, both source
policies, physical-unit PANEL, and electrical M1/M2, M3/M4 and PANEL. It checks
untriggered settling, zero-time IL retarget, first assignment, tail reuse,
SUSTAIN-II retirement and Legacy behavior. Mutations must remove idle IL
selection, restore assignment reset, or force assignment to the new IL.
No result is accepted by changing the preregistered limits.
