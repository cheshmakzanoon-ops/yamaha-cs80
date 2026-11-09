# Memory-pot resistance: parts-list evidence

## Source and visual reading

[Yamaha CS-80 Parts List](https://www.therogoffs.com/cs80/cs80_parts_list.pdf),
14-page scan, downloaded 2026-11-08, SHA256
`eaea3c1bdc542b8e0d057b9a7b711aeda22a6d258562f5b5e05cd732a4aabe28`.
The table identifies CS-80 serial numbers 1001 onward.

Printed page 8 (PDF page 9), reference 8, lists a **TPR B-10K slide variable
resistor** in black. Its remarks inherit “for Memory Bank” from reference 7.
Five following ditto rows specify gray, white, green, red and yellow variants.
The part-number suffixes are HP 50 01 50/60/70/80/90 and HP 50 02 00,
respectively, under the common prefix 40 10 00.
Printed page 7 locates references 7/8 at the memory assembly.

Printed page 9 (PDF page 10) separately lists reference 19 B-10K slides,
a C-10K sustain variant, and reference 21 B-10K portamento/glissando.
The photo places reference 20 at the left performance-control cluster. Parts-list
reference 20 is not electrical output row 20: do not assign its C code to the
memory amplitude-sustain control. These entries do not establish a universal taper.

**Conclusion:** nominal 10 kΩ, B-coded memory sliders have primary-source
support. The scan does not establish the installed resistance of a particular
unit, a measured travel curve, terminal orientation, endpoint impedance, or a
complete per-row wiring calibration. Preserve the literal B designation rather
than silently promoting it to a measured linear law.

## Consequences for this repository

The existing `MemoryPanels::nominalElectrical` uses 50 kΩ as a declared historical
inference. It remains unchanged in this evidence-only update; it should not now
be described as the best-supported memory-slider nominal. Its 10/50/100 kΩ study
cases remain useful sensitivity coverage, with 10 kΩ now distinguished as sourced
for the memory assembly, not merely an arbitrary alternative.

`Electrical::potOhms` currently applies one scalar to the whole modeled bank
family. The parts list does not by itself validate every PANEL row, any special
end connections, or the model's reference rail/driver/diode assumptions. A later
model revision must explicitly choose the bank/row scope, preserve a historical
comparison configuration, and rerun compiled-versus-reference and derivative
checks. Nothing here changes runtime audio or supplies a digital-value-to-wiper
conversion.

Two independent visual readers checked the memory table and its ditto linkage.
The scan itself is not redistributed in the repository. The source link, hash
and printed/PDF page numbers identify the evidence for reproduction.

## Regression scenario

`cs80_memory_panel_tests` now additionally gates 28 selected-memory cases at
10 kΩ: both ranks and both memory selectors, five uniform endpoint/quarter-travel
settings and two deterministic nonuniform settings per selection, with separately
varied inactive banks. All 24 analog buses, selected rail, convergence, KCL residual
and inactive-source leakage use the existing `memory_panel_v1` tolerances, without
loosening them. The bus limit remains 2 mV; observed worst discrepancy is about
0.297 mV. These compare two models, not a physical instrument.

The reference's shared scalar necessarily sets inactive PANEL pots to 10 kΩ in
this scenario too; that is an explicit test assumption, not newly sourced PANEL
calibration. Original default-50 kΩ coverage remains. A negative-control build
that accidentally leaves the compiled solver at 50 kΩ fails the new gate.
