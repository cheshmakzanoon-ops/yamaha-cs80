# Existing native fixture capture roster (preparation only)

Mapped from the 569-test configuration receipt of run44123289466 at revision6ef4d10, not from executed plugin output. The complete module-loading subset has294 cases:49 original drivers × six rate/mode configurations. Exact original argv (including default48000/realtime cases without explicit trailing arguments) is frozen. No test was run or altered.

The source-derived roster covers11,436 PCM files and393,609,216 bytes per arm, across276 cases with exposed captures. Three drivers (18 cases) expose no capture directory: base automation, CC1 fixture, and Sub waveform. They are explicitly retained as assertion-only cases, not skipped. All full feature assertions, process exit/unload, native module provenance, sealed canonical restoration and separate resource/state/PDC checks remain mandatory.

Most historical captures cover neutral/noqueue and Legacy/PURE behavior; successful byte equality is not a substitute for active automation assertions. LFO families also expose their named preceding-lane/scalar witnesses. Capture expectations use exact named-file rosters, fixed float32 stereo lengths and finite values. No filename discovery from the measured arm is used to define completeness.

The frozen JSON binds62 local fixture/include/CMake source files. SDK/compiler identities and complete source/build inventories are independent gates. Changes require a new source review and frozen roster, not relaxing the checks. All configured native drivers are supported; new/missing cases or altered original arguments fail closed.

## Driver derivation table

Counts below are per rate/mode case. Every driver has six cases. Source loops/names are frozen by SHA256 in the JSON; filename formulas were transcribed from each original capture call and its enclosing loops, not inferred from a baseline directory.

| Driver | Source | Files per case | Bytes per file |
|---|---|---:|---:|
| `cs80_vst3_after_touch_automation_tests` | `tests/Vst3AfterTouchAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_automation_tests` | `tests/Vst3AutomationTests.cpp` | 0 | none |
| `cs80_vst3_cc1_fixture_tests` | `tests/Vst3Cc1FixtureTests.cpp` | 0 | none |
| `cs80_vst3_chorus_automation_tests` | `tests/Vst3ChorusAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_envelope_attack_automation_tests` | `tests/Vst3EnvelopeAttackAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_envelope_decay_automation_tests` | `tests/Vst3EnvelopeDecayAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_envelope_decay_release_combined_automation_tests` | `tests/Vst3EnvelopeDecayReleaseCombinedAutomationTests.cpp` | 24 | 32768 |
| `cs80_vst3_envelope_release_automation_tests` | `tests/Vst3EnvelopeReleaseAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_filter_al_automation_tests` | `tests/Vst3FilterAlAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_filter_il_automation_tests` | `tests/Vst3FilterIlAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_glissando_automation_tests` | `tests/Vst3GlissandoAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_initial_bend_automation_tests` | `tests/Vst3InitialBendAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_initial_touch_automation_tests` | `tests/Vst3InitialTouchAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_kbd_brilliance_automation_tests` | `tests/Vst3KbdBrillianceAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_kbd_contour_combined_automation_tests` | `tests/Vst3KbdContourCombinedAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_kbd_level_high_automation_tests` | `tests/Vst3KbdLevelHighAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_kbd_level_low_automation_tests` | `tests/Vst3KbdLevelLowAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_key_release_automation_tests` | `tests/Vst3KeyReleaseAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_lfo_depth_automation_tests` | `tests/Vst3LfoDepthAutomationTests.cpp` | 64 | 32768 |
| `cs80_vst3_lfo_native_random_tests` | `tests/Vst3LfoNativeRandomTests.cpp` | 32 | 131072 |
| `cs80_vst3_lfo_rate_automation_tests` | `tests/Vst3LfoRateAutomationTests.cpp` | 64 | 32768 |
| `cs80_vst3_lfo_rate_depth_combined_tests` | `tests/Vst3LfoRateDepthCombinedTests.cpp` | 64 | 32768 |
| `cs80_vst3_lfo_static_shapes_tests` | `tests/Vst3LfoStaticShapesTests.cpp` | 64 | 32768 |
| `cs80_vst3_master_sustain_automation_tests` | `tests/Vst3MasterSustainAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_mod_wheel_automation_tests` | `tests/Vst3ModWheelAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_portamento_automation_tests` | `tests/Vst3PortamentoAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_pwm_automation_tests` | `tests/Vst3PwmAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_release_floor_interaction_tests` | `tests/Vst3ReleaseFloorInteractionTests.cpp` | 36 | 32768 |
| `cs80_vst3_ribbon_automation_tests` | `tests/Vst3RibbonAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_ribbon_release_modes_automation_tests` | `tests/Vst3RibbonReleaseModesAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_ring_automation_tests` | `tests/Vst3RingAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_ring_chorus_combined_automation_tests` | `tests/Vst3RingChorusCombinedAutomationTests.cpp` | 48 | 32768 |
| `cs80_vst3_ring_combined_automation_tests` | `tests/Vst3RingCombinedAutomationTests.cpp` | 24 | 32768 |
| `cs80_vst3_ring_depth_automation_tests` | `tests/Vst3RingDepthAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_ring_retrigger_automation_tests` | `tests/Vst3RingRetriggerAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_ring_times_automation_tests` | `tests/Vst3RingTimesAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_sine_noise_automation_tests` | `tests/Vst3SineNoiseAutomationTests.cpp` | 72 | 32768 |
| `cs80_vst3_sub_automation_tests` | `tests/Vst3SubAutomationTests.cpp` | 41 | 32768 |
| `cs80_vst3_sub_depth_automation_tests` | `tests/Vst3SubAutomationTests.cpp` | 41 | 32768 |
| `cs80_vst3_sub_legacy_combined_automation_tests` | `tests/Vst3SubLegacyCombinedAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_sub_legacy_speed_automation_tests` | `tests/Vst3SubLegacySpeedAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_sub_touch_automation_tests` | `tests/Vst3SubTouchAutomationTests.cpp` | 54 | 32768 |
| `cs80_vst3_sub_touch_combined_automation_tests` | `tests/Vst3SubTouchCombinedAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_sub_touch_legacy_automation_tests` | `tests/Vst3SubTouchAutomationTests.cpp` | 54 | 32768 |
| `cs80_vst3_sub_waveform_automation_tests` | `tests/Vst3SubWaveformAutomationTests.cpp` | 0 | none |
| `cs80_vst3_tremolo_automation_tests` | `tests/Vst3TremoloAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_tuning_ref_automation_tests` | `tests/Vst3TuningRefAutomationTests.cpp` | 18 | 32768 |
| `cs80_vst3_vca_sustain_automation_tests` | `tests/Vst3VcaSustainAutomationTests.cpp` | 36 | 32768 |
| `cs80_vst3_vcf_level_automation_tests` | `tests/Vst3VcfLevelAutomationTests.cpp` | 36 | 32768 |

Sub and Sub-depth each have36 single-lane/noqueue-policy files plus five old-lane block witnesses (old20 versus old22). Additional Sub-depth mixed-source feature checks intentionally emit no new file. Combined Sub touch/Legacy use six ordered source pairs, whereas combined Ring/Envelope use four. The LFO native-random renderer emits16384 stereo frames (131072 bytes); other mapped outputs emit4096 stereo frames (32768 bytes). `capture_case` checks only already-sealed capture bytes; it neither executes a fixture nor claims that expected exit/assertion gates occurred.
