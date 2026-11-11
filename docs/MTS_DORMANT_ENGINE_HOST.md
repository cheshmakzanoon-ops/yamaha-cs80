# MTS tuning across eight/sixteen-voice transitions

## Reproduced host gap

The processor previously sent SysEx and live MTS-ESP tables only to active
Engines. Its dormant extension therefore missed tuning received in eight-voice
mode. Enabling sixteen voices reset that extension but retained its older tuning,
so newly allocated extension notes could disagree with the primary instrument.

Tuning is persistent configuration, not an active-voice event. The processor now
mirrors supported SysEx to the extension when it is inactive, after the existing
sample-position advance and active-engine dispatch. Active extensions are not
updated twice. The existing parser still validates each message and enforces
channel masks and RT/NRT policy. No new voice events or rendering are introduced.

The live-master branch likewise mirrors global MTS-ESP table replacements while
the extension is inactive. This preserves the last master table if the master
disconnects before sixteen-voice activation. That branch is source-reviewed only:
a connected live MTS-ESP client was not available for runtime qualification.

## Real processor verification

A new registered native fixture drives actual `processBlock` MIDI dispatch. Its
32 cases cross Legacy/Circuit, RT/NRT, one-/two-byte scale formats, zero-/64-frame
message blocks, and initial eight-voice mode versus a prior sixteen→eight switch.
Each case verifies both stored channel tables, enables sixteen voices, plays
sixteen notes across addressed/nonaddressed channels, and checks all allocated
voices' oscillator pitches.

- Unchanged host baseline on reviewed core `8b0f27d0`: **608 checks,160 failures**.
- Rebuilt fix: **608 checks,zero failures**.
- Six registered native CTests: **6/6 passed in13.39 seconds**: dormant MTS Engine,
  controller reset, MPE released targets, MPE member onset, sixteen-voice release,
  and keyboard handoff.

This was a fresh Release native build using pinned official JUCE8.0.11. Fontconfig
cache warnings were nonfatal. These are direct processor fixtures, not external
DAW/plugin-wrapper, display-server, live-device, live-master, full-suite or
hardware-fidelity qualification. The prior-active case exercises table staging,
not sounding extension release-tail continuity. Existing bank/program and scale
reference limits from [core channel tables](MTS_CHANNEL_TABLES.md) remain.
