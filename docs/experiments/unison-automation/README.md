# UNISON DETUNE120 scoped qualification

Local qualification completed 2026-11-13. Publication is tracked separately.
Only the existing `P_unisonDetune` amount is appended at index 119; the literal
119-entry prefix remains unchanged. No DSP, UI, ID, metadata or normalization
change. Discrete voice-mode switching is not automated by this change.
The preceding dispersion fixture forward guard is source-reviewed, not rerun.

## Note-on-latched playing contract

This is not a live retune of held notes. Accepted unison note-on assigns the
amount to each circuit's stored offset. Parameter motion takes effect at a
later accepted strike. The separate core regression establishes the analytic
frequency contract across Legacy/source policies and x2/x4/x8; this native
fixture tests fixed x2/x4 histories, amount alone and with volume.

Notes 60, 67 and 72 start at frames 17, 12288 and 24576 and release at 8192,
20480 and 30720. Only one key is held at a time. The amount begins at 10 cents,
then moves toward 40 while the first note plays. The second onset lies inside
that ramp. All audio through frame 12287 is byte-identical to an amount-held
control with identical volume motion. Later-onset output differs measurably.
Thus current notes remain unchanged while subsequent accepted onsets receive
sample-position amounts, without changing the underlying latching law.

## Provenance

Qualified baseline source is DISPERSION119 local commit
`0b4e39474e3923c18c9370eaa1147d1ffea01440`. Copied plugin module SHA256
`e2e9dce58ca1a456497643a6f6ed80a8c8f2a13e39af1dcd30ae2c6f164b74e6`
matches that completed qualification. No baseline plugin rebuild is claimed.
The retained 5,776-file baseline inventory SHA256 is
`783f21355e8f1e11f492d495eb3ed239c996e5837574693da29b9ec297593a88`.
Identical fixture/header copies were added to the baseline after the inventory.
Eleven reviewed inputs, including pre-run plan and staged runner, are frozen
in `tests/unison-source-freeze.json`. Every stage verifies source, baseline,
and previously completed evidence identities before and after execution.

## Completed gates

Fresh candidate and helper builds, six Linux VST3 configurations
(44.1/48/96 kHz × realtime/offline) and collection passed once. No retry,
overwrite or threshold relaxation was used.

- 552 finite exact-size captures: 432 candidate and 120 baseline.
- 120 actual frozen119 compatibility pairs byte-identical.
- 96 scalar/block comparisons with maximum error 0 under fixed 1e-8 ceiling.
- 48 queue-order comparisons, 24 initial holds and 24 pre-next-onset holds exact.
- 72 exact controls: empty/null, Legacy block-final flattening and PURE masking.
- 24 overall effects: minimum 0.039200267754495144.
- 24 individual-lane effects: minimum 0.0022720862179994583.
- 24 future-onset effects: minimum 0.03780531231313944.

All effect floors remain 1e-6. Future-onset witnesses use frames 16384–20479,
following exact audio through 12287. The collector independently recomputes
all metric windows from retained PCM. Capture inventories/pairs and source,
binary and log identities are retained. PCM/binaries remain local, not in Git;
absolute provenance paths are not portable download links.

## Scope

Fixed manual/manual sources, neutral macros, scanner/ARP/glide disabled, zero
noise/drift/dispersion, fixed bypassed FX. Prior-lane compatibility alone uses
fixed ARP. PURE forces single-circuit mode, retaining identical volume motion
while holding only unison amount. No mode-switch, stealing, all-source,
full-range, x8 native-host, state-readback, hardware, DAW/AU/cross-platform,
CPU, broad-suite or release qualification is claimed. Historical evidence is
unchanged and not rerun. Cross-rate PCM identity is not asserted.
