# Keyboard mapping: constraints and identifiability review

2026-10-29. Independently reviewed against the evidence ledgers, reference implementation and fixtures. This review excludes several proposed explanations; it does **not** establish a 61-key map or change production audio.

## Inputs and provenance

The native KAS p6, foldout keyboard crop, and previously corroborated detailed KBC p15 path were rechecked against [code-path evidence](KEYBOARD_CODE_PATH_EVIDENCE.md) and [contour evidence](KEYBOARD_CONTOUR_EVIDENCE.md). Their original URLs, image hashes and crop coordinates remain the source manifest. The conditional circuit implementation is `src/reference/KeyboardContour.h`; `tests/KeyboardContourReferenceTests.cpp` explicitly supplies, rather than discovers, its calibration states.

Also re-read Yamaha IC guide PDF pages 15–18 (YM26600/YM26700 pin descriptions and block diagrams), from the existing downloaded guide PDF:

- Source: https://www.goltsynth.com/resources/downloads/CS/yamaha_ic_guidebook.pdf
- PDF SHA-256: `ff051aed0687cad4c4b246d145f85896c229e4cc679333328483118a3e7f8cc1`.
- These diagrams show octave encoder, code memory and octave decoder blocks, not an internal truth table. Their application illustrations are explicitly **CS-80 / KAS**; a CS-60-specific alternate mapping is not established here.
- KAS p6 describes MODE as selecting eight versus seven sounding notes; the guide calls it a pronouncing-mode switch. Neither inspected description documents a keyboard-range or octave-code mode. This is not proof that undocumented internal behavior cannot exist, but it supplies no basis to implement such a mode.

## Index convention without MIDI assumptions

Use the foldout's physical labels C1 through C6 and zero-based semitone position `k`, with C1 = 0. Then C2 = 12, C5# = 49, C6 = 60. This is an index into that drawing, **not** an assertion about scientific pitch notation or MIDI octave numbering.

The KAS p6 printed table, after its explicit negative-logic conversion and the source-traced polarity-preserving external B-to-Q path, gives:

| Physical label as printed | Index if read in the foldout convention | Electrical Q1 Q2 Q3 |
|---|---:|---|
| C2 | 12 | 100 |
| C2#–C3 | 13–24 | 010 |
| C3#–C4 | 25–36 | 110 |
| C4#–C5 | 37–48 | 001 |
| C5#–C6 | 49–60 | 101 |

The calibration procedure's VR1 adjustment at C2 requires Q110 **conditional on the detailed p15 gate/network interpretation**. VR2 at C5# requires Q101 on the same basis. The conflicting foldout rendition is not substituted for that network.

## Excluded repairs

1. **Changing bit significance/order alone cannot fix C2.** A permutation preserves Hamming weight. Printed-table electrical C2 is 100 (weight one); required 110 has weight two. This includes reversing the displayed bit order. Independent wiring drawings also corroborate the named bit order.
2. **A global polarity reversal, even with bit permutation, cannot satisfy both anchors.** It changes electrical C5# 101 to a weight-one state, whereas the required 101 has weight two. Negative-logic labels have already been converted once; converting again is not a remedy.
3. **Any fixed bit permutation plus per-bit XOR mask also fails with the labels held fixed.** The naive anchor pair 100/101 differs in one bit; the required pair 110/101 differs in two. A fixed permutation and XOR mask preserve Hamming distance, so none of these 48 transformations can map both anchors correctly. This stronger statement still does not cover a simultaneous note-label shift.
4. **A uniform note-name offset cannot satisfy both anchors within the printed table.** Let `d` be a constant semitone offset applied when interpreting the table labels. For C2 to land in Q110, `12+d` must lie in 25–36, hence `13 <= d <= 24`. For C5# to retain Q101, `49+d` must lie in 49–60, hence `0 <= d <= 11`. The intervals are disjoint. Ordinary octave-name shifts are a subset of this excluded case.

These are separate, precisely scoped exclusions, not a claim that all arbitrary joint recodings are mathematically impossible. Nonlinear label remapping, different decoder wiring or a replacement table would add unsupported hypotheses. Unknown entries outside the printed range cannot be filled merely to escape the interval proof.

A small enumeration of all six permutations, all 48 permutation/per-bit-XOR transformations, and shifts −60 through +60 found no solutions to the respective constraints above. The argument does not use sound, smoothness or fitted trim values.

## A stronger non-identifiability result: OC0 versus OC1

The detailed p15 conditional network and the reference both have

`OC0 = OC1 = L`

for every LOW/HIGH setting. In the reference, `selectedOc` affects the result only through the selected octave voltage `O`; Q and note tap are supplied separately. Therefore replacing a supplied OC0 selection by OC1 (or vice versa) while holding Q and note tap fixed leaves **every settled contour output identical**, across the entire lever and trim grid—not merely the two calibration anchors.

This is a proof about the conditional model, not a proposal that the physical chip can choose either input arbitrarily. It shows why even perfect agreement of those contour voltages cannot identify this part of the internal OC decode. The existing low calibration fixture supplies OC1; its successful −2.76 V result is equally compatible with supplied OC0. That success is not evidence selecting one of them.

The foldout's conflicting 100-ohm OC0–OC1 link does not justify breaking this equivalence in the detailed-p15 model without revision/connectivity resolution. Dynamic switch/contact differences likewise cannot be inferred from a settled ideal model.

## Exact remaining evidence requirement

There are two missing functions, not one binary-octave number:

1. Physical key/contact combination → actual aligned electrical B/Q code on the CS-80 application.
2. Actual electrical code → YM26700 selected OC input (and corresponding note selection).

The encoder/decoder block boxes do not expose those functions. The 49-key table contradicts one CS-80 trim-enable anchor when interpreted directly. The KAS input wiring identifies contacts but does not supply the internal encoding truth table. Two trim anchors cannot determine intervening keys; all-grid contour DC measurements alone cannot distinguish the OC0/OC1 equivalence above.

Resolving evidence would be a matching CS-80 encoder/decoder truth table or documented erratum; otherwise instrument captures of B1/B2/B3 and correctly aligned Q at every physical key, plus independent OC decode evidence that distinguishes tied contour taps. A suitable independent decoder characterization must distinguish the OC inputs rather than infer selection from equal KBC voltages. Do not modify a vintage instrument merely to manufacture distinguishable voltages without a separate safe measurement plan.

**Outcome:** the simple bit-order and uniform octave-naming explanations are excluded under stated premises; complete physical-key mapping remains unestablished. No new primary download, production change, mapping guess, or hardware-fidelity claim was made.
