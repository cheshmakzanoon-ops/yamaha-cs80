# ADJ evidence intake: DC-acquisition consistency preregistration

2026-11-02. Scope: the test-only schema-v1 importer at
`tests/bench/adj_intake.py`, reviewed at
`058224fad844b8f51e211e3b820c0ae2d8f25762`.
This is a software metadata contract, not an acquired CS-80 observation,
calibration change, hardware safety procedure or manufacturer acceptance.

## Defect hypothesis and fixed acceptance

The importer distinguishes `vdc` from waveform quantities but does not
currently relate that distinction to the supplied acquisition coupling.
A submitted DC reading supported only by an AC-coupled scope must be
rejected: AC coupling removes the DC component. This is established by
the instrument manufacturer's [Tektronix oscilloscope systems and controls
primer](https://www.tek.com/vn/documents/primer/oscilloscope-systems-and-controls)
and [Keysight AC-coupling explanation](https://www.keysight.com/blogs/en/tech/bench/2018/10/18/when-to-use-ac-coupling-on-your-oscilloscope).
Those sources describe acquisition behavior, not a Yamaha service limit.

Before evaluating the repair, commit the following exact tests:

1. Each permitted DC node (`FH`, `FL`, `IC10_VFC4`, `IC10_QADJ6`)
   with a `vdc` observation and only an AC-coupled scope must raise
   `IntakeError`, including the synthetic zero-voltage sentinel. Zero
   cannot be inferred merely because the measuring path blocks DC.
2. The same node/quantity with a DC-coupled scope remains accepted.
3. A declared `DC_V` meter without a scope remains accepted for DC.
4. Mixed CP3 waveform and FH DC observations with an AC-coupled scope
   **and** a valid declared DC meter remain accepted. Removing that
   meter must reject the same payload; waveform-only AC acquisition
   remains accepted. No actual measurement authenticity is inferred.
5. A `not_acquired` DC node alongside an observed AC waveform must not
   demand a DC instrument or turn into an invented numeric reading.
6. CLI rejection uses exit 2 with a diagnostic and no output report.

## Method and stopping condition

Retain the failing unmodified-importer run. Implement only the missing
DC-acquisition consistency check, preserving all other v1 fields and
statuses. Run the complete existing suite plus new tests. In an isolated
temporary copy, delete the new guard and require the negative tests to
fail; run a restored positive. All fixtures are explicitly synthetic,
in-memory schema sentinels and are not published as hardware evidence.

The study stops after that red/green/mutation/restored sequence and an
independent review. No DSP, V1 hypothesis value, original p11 CP3 target,
tone-board revision or physical calibration may change. Schema-valid
metadata remains unverified external testimony, never a hardware PASS.
