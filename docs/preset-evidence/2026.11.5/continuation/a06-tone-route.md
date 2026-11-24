# A06 — Hardware-tone route audit

Read-only source audit at `c76ad288`; no DSP, host, parameter, or preset changes.

**Correction to the initial A02 hypothesis:** the Circuit plugin does not simply
play the provisional `hardwareTonePatch` recipes. `PluginProcessor::readParameters`
retains the selector in `ControlFrame`. `Engine::setControlFrame` compiles the
corresponding `SourceFactoryTones` descriptor; `Engine::applyParams` then overrides
waveforms, signal-path gains and envelopes with its nominal electrical model.
The provisional values also support settings illustrations and the legacy core
adapter; editing them alone would not revise the actual Circuit tab sound.

`Engine::applyParams` explicitly admits software bodies only for PANEL selector
13. Factory tabs 0–10 and MEMORY 11–12 suppress them. The all-source rendered
comparison in `tests/InstrumentBodyHostTests.h` verifies this deliberate boundary.
`docs/PRESET_GUIDE.md` documents it. The enhanced factory program “Tone I: E.
Piano” instead loads the authored Electric Piano I recipe through PANEL. The
similar labels therefore identify different intentional sound paths.

**Decision:** preserve electrical tab behavior, selector automation, untouched
rank/global controls, memory, and old states. Do not silently enable bodies on
source-driven tabs, change a tab click into a full two-rank preset load, or
materialize the tone into PANEL while displaying it as a latched hardware tab.

A small discoverability clarification can be made in
`src/plugin/CentreRow.cpp::buildToneSection`, at the existing `setTooltip` and
`setDescription` calls. For factory tabs only, suitable text is:
“Original CS-80 tone for Channel I. For the revised instrument voicing, choose
‘Tone I: E. Piano’ in the preset browser.” Generate the exact row/name for each
tab; retain PANEL and MEMORY wording. This identifies the audible choice without
adding modes or exposing internal implementation details.
