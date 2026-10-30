# Bounded active-tail continuity in parity isolation

This is a **Class A, source-informed assignment-policy correction**, not measured
hardware parity, a calibrated capacitor model, or full idle free-running behavior.
The inspected pre-change source was `145d4dbbb8482127768198f2ad0dd1b695129f95`.

## Verified topology

The original [M41 circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg)
routes TR terminal63 to the IC12/IC13 envelope gate circuitry. No traced branch
resets IC8 oscillator timing or IC10/11 audio-filter capacitor states. IC8 has
K37 at KV2, transposition at FT1, modulation at VIB15 and its local timing network
at TO14/TI12/RT11/CT10. The [IC42 description](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/42%20-%20M%20Circuit%20ICs.jpg)
identifies TI12 as the comparator input receiving the timing waveform from TO14;
it does not identify a note-on phase reset pin. The
[Yamaha IC guidebook](https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf),
PDF35, printed2-4, shows the KV-controlled current, comparator, one-shot and
switched reference-current loop. That **oscillator-cycle reset** must not be
confused with a **keyboard-triggered reset**.

M41 IC10/11 have continuous audio capacitor networks, including the 0.0015uF
C1/BP and C2/LP branches, without a trigger-operated discharge switch. A note
changes envelope/control conditions; the inspected topology supplies no basis
for instantaneously zeroing the audio memories on reassignment.

The [SH7 circuit](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/07%20-%20SH%20Circuit%20Diagram.jpg)
shows KI through a bilateral switch to K and switch control NAND(TR,SUS), with
SUS pulled up to +8.5V through22k. This does **not** establish unconditional
last-pitch holding on release. Panel-to-SUS polarity, upstream KI behavior,
acquisition/leakage and startup/unassigned key voltage remain unresolved. See
[initial-touch evidence](INITIAL_TOUCH_CIRCUIT_EVIDENCE.md).

## Implemented boundary

At entry to `startVoice`, preserve the oscillator phase and HP/LP numerical
integrator memories **only** if parity isolation is enabled and the selected
voice was active, unheld and not MIDI-pedal-held. Each rank still consumes its
legacy random-phase draw at exactly the former point, discarding the value if
preserving. The filter-EG and initial-touch retriggers are unchanged.

**Amplitude envelope (engine audit, 2026-10-30; inferred).** Under the same
condition the VCA envelope of the reused circuit is no longer zeroed at
assignment: its attack restarts from the level the release tail has reached.
Zeroing it removed the whole tail in one sample: the largest sample-to-sample
difference between an engine that steals four tails and a twin that does not was
0.0006 to 0.022 full scale under Source Performance (0.0003 to 0.027 under
legacy) over eight factory programs, audible as a tick under a passage, while
the oscillator phase and filter memories beside it were already continuous.
With the change the Source Performance range is 0.0006 to 0.0074 (the remainder
is the new note's different pitch); legacy is unchanged.
This extends the reasoning above (no traced reset path
for the envelope capacitor either); it is not a measurement of the original
envelope generator, whose retrigger-from-nonzero behaviour is unverified. Only
the amplitude envelope changes; the filter envelope and initial-touch retriggers
still reset, and the legacy policy keeps the zeroing exactly. This is an audible
change for the two source policies only. `ActiveTailContinuityTests` asserts the
level at assignment for both policies and that the attack does not dip below the
tail level; restoring the unconditional reset fails both assertions.

Default-off executes the old draw/reset operations in their old order. Cold or
fully inactive circuits still reset on assignment. Sustain-II termination makes
a circuit inactive before assignment, so it also follows the old reset policy.
Explicit engine reset, mode transitions and channel panic retain their existing
policies; this patch does not reinterpret panic as an analog operation. Allocation
still protects held and pedal-held circuits and ignores a ninth held key.

Inactive voices are still skipped by the audio loop. Preserving an active tail
is not a claim that inactive oscillators advance or that filters evolve during
silence. SVF `ic1/ic2` are **numerical memories**, not calibrated capacitor volts.
The existing waveform, filter transfer, envelope and S/H approximations remain.

## Verification and cost

`tests/ActiveTailContinuityTests.cpp` uses current, render-thread-only
`getRankAudioState` diagnostics (not stale last-render telemetry). The getter is
read-only, allocation-free and lock-free, and is **not** safe for concurrent UI
polling. It checks both ranks at44.1/48/96/192kHz; same/different note reuse;
nonzero HP and LP states at assignment; cold/inactive/sustain-II/panic exclusion;
held and pedal-held protection; reset and mode transitions; legacy RNG draw
consumption; high-Q pitch-jump finiteness; and exact block-partition invariance.

The phase, HP and LP preservation assertions separately reject a mutation that
restores each reset. These software tests establish the bounded policy, not
hardware waveform agreement. Exact-commit CI and baseline raw-capture comparison
remain the authority for release and default-off compatibility.

There is no added sample-loop work: only an assignment-time predicate and state
reads in diagnostics. Full inactive evolution would be a separate decision,
potentially clocking16 oscillator/rank chains and32 filters during silence, and
would require explicit idle-CV policy plus silent/one/eight-voice CPU benchmarks.

## Reproducible source provenance

Source SHA256:
- M41: `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080`
- IC42: `5698fdfd19600f6b0b8b8f9dfdfc70723248b4b33e8daad39b9f9c0ee11cc24d`
- Guidebook PDF: `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`
- SH7: `99ac2cbfe0551bbd0d536f94ab6e92bbf924e5bf21b6c646c07b17459495de0c`

Native PNG crops made using Pillow without resizing, boxes[left,top,right,bottom]:
- M41 VCO [200,500,1800,2200]: `ac66be34ea896029f6065162accaa96414a1dab265db32e03491b7ec84d698fa`
- M41 filters [1700,1000,3800,2200]: `143f24f91f0b5bc8a68ffcbf09ba5a452a74e43404932a1c2778274b4cde16ef`
- M41 TR [1300,2100,4200,3200]: `79ec4c3ad4c4e99bda40ca91786448af4aa71b718aa7a05e05621ca49649e7d2`
- IC42 VCO [250,200,1250,3250]: `f87c9e9bbf8f40898d06192c68f3f5e68e3cf56f3432f0e4bcf83dd3a5f614cd`
- SH7 first voice [110,400,1300,780]: `2203c1c51a6ae814637455e35836753f74d3f587ddee08b16b0f61ce176e7f99`
- SH7 SUS [110,2750,600,3050]: `897f02ccfa5bd3eba927a1ef42cf97ef4f9899f8d0c4c2ff932871bc0f0323ef`

Guidebook PDF35 render: `pdftoppm -f 35 -singlefile -r 150 -png`;
PNG SHA256 `74292d61f3c6beeacc2e6fcea2dc550d70d22e3f3ef71985f0e7a9085a81a00b`.
Rendered pixels vary with renderer settings; the source PDF hash matches the
existing oscillator evidence. Local inspection files are not committed manual
redistributions. No prior unmerged implementation was used as source authority.


## Default-off raw compatibility and CI gate

The same GCC14.2 Release core toolchain compared baseline
`d56911a7c5f6953cd244457226b126cc567a538b` with this change. Both raw-float
corpora matched byte-for-byte across the shipped222 programs at44.1/48/96kHz:

- Original24-block note/controller fixture: SHA-256
  `4a3483f77c9df9895386b98eaa169358d19b463c21283231f994c5d0f6c523de`.
- 72-block repeated release/reassignment fixture, with per-rank noise0.3 and
  global noise0.2: SHA-256
  `fe447882e3ea3560eefb7b5ae4a474b151efe5ced775b5ab36f2609c5fde0660`.

`src/tools/compatibility_probe.cpp OUTPUT [--tails]` defines the deterministic
fixtures. Compile it separately against each revision's own headers and core
library; never mix a baseline header with the current binary library. The DSP
workflow now rebuilds both revisions with one compiler and requires byte equality
for both scenarios. It archives compiler identity, source revisions and hashes,
not a false cross-platform universal hash or analog-fidelity score. Hash values
above describe the local toolchain; CI compares both sides directly instead of
assuming all compilers must produce those same bytes. Future appended programs
are outside this pinned222-program compatibility corpus.
