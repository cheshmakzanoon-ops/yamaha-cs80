# Hardware programmer automation

The original 122 parameter IDs and their host indexes are unchanged. The plugin
now exposes 228 parameters. Legacy parameters retain JUCE version hint 1; all
106 appended parameters use version hint 2. AUv2 orders parameters by version
hint and then identifier hash, so merely appending to the layout with the same
hint could change old Logic/GarageBand automation indexes. The higher hint keeps
new controls after every legacy parameter in AUv2 as well as layout order.
See [JUCE's parameter compatibility contract](https://github.com/juce-framework/JUCE/blob/8.0.6/modules/juce_audio_processors/processors/juce_AudioProcessorParameter.h).

Appended in order:

1. `hardware_source_1`, `hardware_source_2`: 14 discrete choices, default PANEL.
   Values 0–10 select the corresponding rank's hardware factory studies;
   11 selects Memory 1/2, 12 selects Memory 3/4, 13 selects PANEL.
2. `memory_1_<channel-control-id>` through `memory_4_<channel-control-id>`:
   each bank's 26 original controls, in channel-definition order, skipping
   feet, pan, HPF bypass and LPF bypass. Saw/pulse are boolean switches;
   the remaining controls retain the main panel's physical ranges, defaults,
   units and skew mappings.

APVTS is the only mutable authority. The audio callback reads cached raw atomic
pointers, then resolves the selected source into its private effective parameter
snapshot. Miniature changes never overwrite the manual panel and neither UI
callbacks nor state loading mutate the engine. These controls take effect at the
next audio block, not with sample-accurate interpolation.

JUCE slider/button attachments carry host notification and balanced gestures for
miniature GUI edits. Source-tab API edits use complete begin/end gestures.
Existing get/set APIs read/write those same parameters. Excluded controls are
not exposed as miniature parameters and stay live on the main panel.

## State and presets

New session/user-preset states serialize only APVTS parameter children, not a
duplicate HARDWARE subtree. Restoring first completes a detached tree: missing
core parameters take declared defaults; missing miniature controls take their
declared defaults; missing selectors take PANEL. Nonfinite values are rejected
and ranges/discrete values are constrained before APVTS replacement.

Legacy version-1 HARDWARE children migrate sourceI/sourceII and each MEMORY bank's
pN properties into the appended parameters. Explicit new parameter children take
precedence over legacy copies. The legacy subtree is removed. Unknown hardware
versions are not interpreted. Older states without programmer data restore PANEL
and default miniature values, never stale controls from the previous session.

Factory/digital parameter-preset recall selects PANEL while retaining all four
physical miniature panels. Saved user-preset/session recall restores the stored
programmer state.

## Verification

The UI smoke executable includes assertions for all 228 parameters and preserved
legacy order; new IDs/order, ranges and skew; host automation to effective audio
settings without an editor; all 104 controls and two selectors roundtripping;
legacy migration and missing-state defaulting; GUI-to-APVTS and host-to-GUI updates
with balanced slider/switch gestures. These additions require CI execution;
source inspection alone is not a test pass.
