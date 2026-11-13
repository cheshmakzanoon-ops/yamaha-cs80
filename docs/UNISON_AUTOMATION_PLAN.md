# UNISON DETUNE120 source qualification plan

Source-only until stages run. Append existing P_unisonDetune at index 119 while
preserving literal 119-entry prefix. No DSP, ID, normalization, UI or voice-mode
policy change. Dispersion fixture forward guard is source-reviewed only.

## Note-on-latched semantics

Amount is sampled when applyNoteOn assigns each accepted unison circuit; later
edits do not retune that group's stored offset. The prior independently tested
core contract covers x2/x4/x8 and Legacy/source, with 40 assertions. This native
fixture covers fixed x2/x4 modes, not automation of discrete mode switches or
voice stealing. PURE forces single-circuit mode and thus masks unison amount.
Legacy keeps its existing block-final parameter behavior.

## Histories and controls

32768 stereo float32 frames; notes60,67,72 on17,12288,24576 and off8192,20480,30720.
Only one key is held at a time. Fixed manual/manual sources, neutral macros,
ARP/scanner/glide off, dispersion/drift/noise zero, bypassed fixed drive FX.
Scenarios0/1 use x2;2/3 use x4, each amount alone/with volume.

Amount10 cents holds through1023, ramps to40 at16383,20 at24575,0 at32767.
Second onset12288 occurs inside the ramp, proving the onset consumes its
sample-position amount rather than the process block's endpoint. Third onset
uses the later amount. Combined volume holds .60 through 1023, then .75/.50/.70/.55/.60 at
4132/8228/16420/24612/32767.

Moving amount versus held amount (with identical volume motion) must remain
byte-identical for first12288 frames, including all first-note held/release audio.
A positive difference over frames16384..20479 proves a later accepted onset uses
changed amount. This is aggregate output evidence, not direct host-state readback
or isolated frequency measurement. Core diagnostic tests separately prove latching.

Five captures per scenario compare actual frozen119 plugin: noqueue/null/prior
lanes/Legacy prior/PURE prior. No new amount queue in these comparisons. Fixed
ARP is enabled only in the previous-lane compatibility capture; mode remains fixed.
Full candidate adds scalar/constant, blocks7/64/256/1024, Legacy flatten pair,
PURE held-amount pair; combined cases add reversed queues and individual controls.
The held amount capture is reused, not rendered twice. 72 candidate +20 baseline
per configuration across44.1/48/96kHz × realtime/offline gives552 finite captures.

## Frozen criteria and execution

120 byte-identical actual frozen119 pairs;96 scalar/block checks <=1e-8;
48 exact queue-order comparisons;24 initial1024-frame holds;24 exact pre-next-onset
12288-frame holds;72 exact empty/null/Legacy/PURE controls. 24 overall effects,
24 individual-lane effects and24 later-onset effects >1e-6. No threshold changes.

Freeze reviewed sources, runner, plan and baseline before execution. Build only
cs80_vst3_unison_automation_tests and frozen119 helper; fresh build/output/capture
directories, six named configurations and collector, stop on failure, no overwrite.
No broad suites, malformed input/parser runs, CPU benchmark, host/OS coverage,
model switches, state-readback, hardware, full-range or release claim.
