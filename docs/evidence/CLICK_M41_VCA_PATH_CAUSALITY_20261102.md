# M41 CP3 → IG00151 differential click-path test (software evidence)

**2026-11-02 — preregistered path-causality trial: 7/7 predicted carrier suppressions observed; original full-source click scores remain FAIL. No audio equation, numeric limit, factory program, acceptance gate, or `src/**` file changed.** This result attributes the measured *software* derivative to a signal route. It does not establish that those peaks are perceptually objectionable clicks, that they are faithful Yamaha transients, or that a source V2 change is justified.

## Schematic distinction and falsifiable question

Yamaha [service M41 p41](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/41%20-%20M%20Circuit%20Diagram.jpg) (original-image SHA-256 `1278f17ff05cd82b2b84153838bf95109d3f1ea1e864119de0a3f4a4af4f0080` from the existing hardware ledger), independently corroborated in the circuit workstream, routes IG00156 LP pin 15 to CP8/CP3 and then through a **1 µF/50 V nonpolarized coupling capacitor and 100 kΩ** to IC4 IG00151 +IN3. LP1(27) enters the separate **100 kΩ/0.01 µF** branch to LI2; the downstream VCA output is CP4. The sine bypass meets the filtered audio path before amplitude control (Yamaha [owner manual pp35,42](https://www.therogoffs.com/cs80/CS80E_2.pdf)). The M41 amplitude-envelope drawing separately stores evolving charge on IC2 OUT8 → 0.22 µF → IC13 IG00159 BI2; the [IC service guide p52](https://www.therogoffs.com/cs80/manuals/CS80_Service_Manual/52%20-%20M%20Circuit%20ICs.jpg) does **not** supply a calibrated inner OTA or stage-comparator transient law. In particular, we have **not** independently certified an exact KAS net to the IG00159 gate from this M41/M44 subset.

In the current frozen engine (`src/core/Engine.cpp`), `vcaInput = filtered * rs.vcfLevel + sineWave * rs.sine`; `Ig00151Vca::process` clocks the modeled LI node, inferred envelope-controlled offset and feedthrough, and downstream 0.1 s coupling every active sample. This is a software equivalent path, **not** a measured reconstruction of CP3/CP4 voltages or the complete physical loads.

The preregistered `H_M41_AUDIO_CARRIER_DOMINATES_JUMP` predicts that, across seven named original-source events, suppressing **both** modeled audio carrier entries for the *entire phrase* while continuing every oscillator, IG00156 stage, envelope, VCA control/pedestal/coupling and output clock leaves at most **10%** of each original event-window absolute adjacent-sample jump in at least six of seven fixtures. This would favor a delayed harmonic/filter attack or held-tone edge over a charge reset or VCA-only spike in the software. A contrary result in two fixtures would falsify that numerical prediction. The original score denominator and 8 ms event window are never redefined.

## Chronology, including the failed first positive control

1. The full hypothesis, source tree, seven fixtures, 63-run matrix, 8 ms scoring, negative/positive controls and falsifier were committed **before any experimental render** as `b7b4599`, `tests/parity/tolerances-v2/click-m41-vca-path-causality.json`.
2. The first run **failed and stopped** at fixture `bass_panel_first_48000`: the preregistered in-engine CP3 switch-on positive had deliberately omitted the first Bass note at 6.0 s. No note was sounding to carry the switched CP3 signal, so the positive yielded exactly no PCM change. The file emitted only its header, not a valid first-fixture measurement. That failure was **not** dropped or relabeled a successful trial.
3. The narrowly scoped, separately committed amendment `ff05da3`, `tests/parity/tolerances-v2/click-m41-vca-path-positive-amendment.json`, required both Bass positive controls to retain the original note-on while comparing against the matching CP3-muted note-on. Funky positives keep its note-off omitted, which leaves the already-held tone sounding. No main variant, ratio threshold, window, score, source program or rendering equation changed.
4. The complete 63-render trial and its independently repeated 63-render replay both finished. The 77-row CSV files are **byte-identical** at SHA-256 `7fa78c747afeabf3ee25cf506c67c287a4cf0f5dbcf3e631be485566ca5b70c8` (78 lines with header). Both emitted `primary_audio_carrier_suppression_successes=7/7`.

## Direct observed PCM path controls

The percentages below are **absolute max adjacent-sample jump in the original 8 ms event window divided by the unmodified source event's absolute jump**. They are **not** new acceptance scores, sound-quality classifications or recalibrations. Every row uses the exact original source policy and coordinate, **Bass physical PANEL** (`useProgrammer=false`) or **Funky 1+2 original tone tabs**, with the source phrase's 64-frame event alignment. Filter/sine audio gains are forcibly zeroed only by the standalone test-only friend after each accepted ordinary control frame; all original core methods and histories execute unchanged.

| Original event, host rate | Replayed original score (FAIL) | Filter/CP3 muted | Sine bypass muted | Both audio carriers muted | Original event omitted |
|---|---:|---:|---:|---:|---:|
| Bass first strike, 48 kHz | 1.123291 | 3.35% | 100.06% | 2.43% | 2.43% |
| Bass retrigger, 48 kHz | 1.440924 | 2.38% | 99.57% | 1.67% | 2.18% |
| Bass first strike, 96 kHz | 1.106081 | 6.69% | 100.02% | 6.25% | 6.25% |
| Bass retrigger, 96 kHz | 1.365311 | 5.06% | 99.57% | 4.66% | 5.35% |
| Funky note-off, 44.1 kHz | 1.626889 | 2.05% | 98.18% | 0.42% | 100.00% |
| Funky note-off, 48 kHz | 1.291310 | 2.34% | 97.90% | 0.39% | 106.91% |
| Funky note-off, 96 kHz | 1.220958 | 1.95% | 98.62% | 1.07% | 101.47% |

All seven satisfy the preregistered **both-carriers-muted <=10%** criterion. More specifically, mute of the **filtered CP3-like** path alone leaves only 1.95–6.69% of the original absolute maximum; mute of the **sine** path alone leaves 97.90–100.06%. The source-card/PANEL runtime gains before interception were Bass `vcfLevel=0.8/0.8`, sine `0.24/0.24`; Funky `vcfLevel=1/1`, sine `0.52356/0`. In these modeled fixtures the filtered path overwhelmingly carries the high-slope PCM energy; this does **not** imply that the actual hardware CP3 analog voltage has an identical proportion, nor that VCA contour is physically perfect.

At Bass 48 kHz, the retrigger peak remains **374 host samples = 7.792 ms** after the MIDI event, matching the older rank-resolved delayed-attack investigation; at 96 kHz it is 750 samples = 7.813 ms. The filtered tone path is *necessary for that modeled high derivative*, but this intervention does not prove the waveform edge is intentional or inaudible. Funky's unmodified event omission has essentially the same 8 ms maximum; at 48/96 kHz the omitted-note-off tonal slope is actually higher. The event still changes the eventual waveform: its independent maximum event-minus-omit derivative is 0.010980/0.010769/0.008694 at 44.1/48/96 kHz. With both audio carriers suppressed those differentials fall to approximately `2.01e-7`, `1.85e-7`, `9.22e-8`, respectively, under the source model.

## Independent controls and exact state

- All **28 event/omission same-path pairs** have exactly **zero differing pre-event PCM bits**. In the twelve Funky note-off runs which actually apply the gate (3 rates × 4 audio-path variants), all **384 modeled VCA LI/coupling charge-field bit comparisons** at zero elapsed time are equal. Bass first/retrigger stale idle storage requires the separately qualified wake reference; the existing 72-field continuation test covers that rule. These limited fields do not claim to expose every internal circuit state.
- The corrected in-engine positive forces the CP3 audio gain from 0 to its original value at the target block with a sounding carrier. It produces nonzero event-window and postevent PCM difference against the correctly matched CP3-muted source timeline in all seven fixtures. The Bass positive uses an actual note-on; it is an artificial control switch, not an asserted Yamaha gate transient.
- An independent, explicitly synthetic **single-sample +0.20 FS** downstream PCM corruption changes exactly one left-channel sample at event +1 and yields a measured derivative ~0.20 FS in every fixture. A separate **C1-smooth** 4 ms Hann-onset 220 Hz sine at 0.10 FS starts with no zero-time discontinuity and is observed as an ordinary pitched negative control. Neither synthetic control changes Engine or the original audit reference. Missing/wrong MIDI-target identity, pre-event one-bit corruption and two-sample mutation are rejected by standalone oracles.
- The original seven full-source scores reproduce within the registered absolute `1e-5` replay tolerance; every original score above is **still >1 and FAIL**. This diagnostic cannot turn an existing red phrase/audit gate green. The separately published 88-case classifier study also showed many alerts on smooth pitched controls and misses on some true single-sample impulses; high score alone is not proof of a digital discontinuity.

## Reproduction and limitations

The full raw source-path matrix is [`m41-vca-path-causality-20261102.csv`](m41-vca-path-causality-20261102.csv), generated by [`ClickM41VcaPathCausalityProbe.cpp`](../../tests/diagnostics/ClickM41VcaPathCausalityProbe.cpp). Git `HEAD:src` SHA was `7dd159d491012fd47b46c0aa651e86ea959ca289`; linked Release core archive SHA-256 `ef757a7f944b81c837c5eb76e22ef536a3cdb0d5f7690e621ac87ce53349483c`, test executable SHA-256 `ebbf13654de6497c7e5682a9a289879ca51922321d062814d19d5ef14dd03b59`. The test is standalone, not a new original audit suite and does not register or relax CTest acceptance thresholds.

```sh
g++ -std=c++20 -O3 -DNDEBUG -Isrc -Itests \
  tests/diagnostics/ClickM41VcaPathCausalityProbe.cpp build-core/libcs80_core.a \
  -o build-core/ClickM41VcaPathCausalityProbe
build-core/ClickM41VcaPathCausalityProbe \
  --prereg tests/parity/tolerances-v2/click-m41-vca-path-causality.json \
  --amendment tests/parity/tolerances-v2/click-m41-vca-path-positive-amendment.json \
  --out build-core/m41-vca-reproduction.csv
sha256sum build-core/m41-vca-reproduction.csv
```

This cannot determine the physical noise NI source impedance, KAS gate pulse topology, actual IG00151 dynamic OTA current law, measured attack/switch transient, or the audibility of a waveform edge. It identifies **which modeled branch carries the existing derivative** under deliberately disconnected path inputs. It does not justify a fade, phase reset, arbitrary filter smoothing, a VCA pedestal retune or changing any original threshold. Physical/hardware correctness and a credible listening comparison are open; the source score gate stays **FAIL** pending a separately authorized, circuit-evidenced correction or explicit acceptance decision.

The existing, unmodified regression runs from this same frozen core/build tree also passed: `cs80_click_event_continuation_tests` (54.07 s), `cs80_click_bank_causality_tests` (7.02 s), `cs80_ig00151_vca_tests` (2.59 s), and `cs80_vca_tail_modulation_tests` (24.99 s), all **0 failures**. They protect their prior registered contracts; none accepts or waives the original high-scoring musical attacks.
