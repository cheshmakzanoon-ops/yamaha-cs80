# Source-aware parameter classifier (pure research code)

No native module is loaded by this classifier or its focused controls. It is not
an admission receipt, production qualification, CPU result, or permission to
write residual parameters. The strict old native-ID policy is unchanged here.

## Frozen source schema

`sourceParameterExpectations()` constructs 2,378 expectations from source, not
from captured metadata. `classifySourceAwarePanel(roster, case)` requires the
complete unique set and bit-exact defaults and residual baselines, plus exact ID/title/shortTitle/units/stepCount/default/unitId/
flags for every parameter. UTF-16 strings must terminate within 128 units;
meaningless bytes after the terminator are not compared. Unknown, missing,
duplicate or colliding IDs fail. Native order may differ, but the captured ID
vector must exactly track the captured snapshot vector.

* 192 writable IDs: 190 `Params.h` controls plus two hardware-source selectors.
  Their current values may differ from defaults (the initial factory program
  does this); they must remain finite and normalized. The frozen case controls
  select Panel (`1.0`) for both selectors. This function does not itself flush.
* 104 observe-only memory IDs: four banks of 26 channel parameters, excluding
  feet, pan, HPF-on and LPF-on exactly as `isMemoryControl` does. Saw/pulse are
  canonical booleans, the others are float parameters.
* Two observe-only wrapper IDs: bypass and program.
* 2,080 observe-only MIDI-emulation IDs: 16 channels times 130 controller IDs.

All 2,186 residual current values must exactly equal the source-derived initial
baseline before being returned; memory defaults use JUCE float arithmetic, not
double approximations. Bypass/program/MIDI baselines are zero. Later baseline
readbacks and protocol/policy admission belong to the owned host integration,
not this pure function. No caller can supply replacement expectations.

The source layout translation follows `PluginProcessor::createParameterLayout`:
global contiguous group runs, channel groups, programmer/memory groups and
append-only extension groups, explicit bool/choice names/defaults, and ungrouped
modern controls. `Params.h` supplies ordinary names/labels/ranges. JUCE hashes
group IDs to unit IDs and parameter IDs with the frozen 31-bit hash; short names
are the first eight ASCII code units. Float steps are zero; bool steps one;
choice steps count minus one. Every source-derived ID is checked for collision.

Program count 523 is a frozen constant in the runtime header, but a separate
regression compiles and executes `programLibrary().size()` from the actual
`FactoryPresets.cpp`, including its two curated-library source fragments. It is
not inferred from the rejected native program parameter's step count.

## Primary source provenance

Pinned JUCE commit: `c352e2489000ffeb9e397051377d75eef2efe641`.
The companion provenance JSON records official source URLs and content hashes.
Relevant implementations: VST3 wrapper `Param::updateParameterInfo`,
`getUnitID`, `ProgramChangeParameter`, `initialiseMidiControllerMappings`,
wrapper-provided bypass; `AudioParameterFloat::getDefaultValue`,
`NormalisableRange<float>::convertTo0to1`, bool/choice constructors/defaults,
and `AudioProcessorParameterWithID::getName`. Local production source files and
classifier/test source are hash-bound by focused tests. No production file was
modified to create this classifier.

## Focused verification

70 controls (including signed-zero default/residual rejection): full set success, order independence,
workload-current variance, unknown/missing/extra/duplicate/ID-vector rejection,
nonfinite current and changed residual rejection, each full metadata field
mutated across all five roles (including panel, nontrivial memory default,
first/last MIDI), and the independently compiled 523-program source probe.
The retained rejected diagnostic is only a regression input: all 2,378 captured
metadata rows match the independently source-generated fields exactly; all
2,186 observed residual currents match expected defaults. It remains rejected
and does not qualify or admit a native run. The exporter fixture emits TSV
source expectations for independent parent-side comparison.
