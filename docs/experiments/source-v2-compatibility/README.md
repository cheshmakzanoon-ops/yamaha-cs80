# Preserve Source Audio V2 compatibility

Publication 822300353623513eb263f5de255bc2c50bedf42d introduced a strict historical-audio compatibility failure. The circuit comparison passed, but the source audio/numeric corpus first differed at byte 139501. Fresh GCC 14.2 builds reproduced the exact first mismatch reported by GCC 13.3 CI. The immediate pre-publication commit 28999229 was byte-identical to immutable Source Audio V2 (00ceccfa).

The first mismatch maps to block 40 at 44.1 kHz, first source pair, voice 7/rank 1 HP filter integrator history, following channel-2 all-sound-off. This is not merely an inaudible state change: 651 of 2716 PCM blocks later differed, with maximum absolute sample difference 0.0108993165. Do not describe the original publication as preserving V2 numerical/audio history.

Recovery is now explicitly experimental and default OFF. Both scalar and SIMD default arithmetic retain the historical capped Newton path. CMake option `CS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=ON` enables the changed-history solver consistently for the core and consumers. This is not a user preset migration or a new versioned production model. It must not be silently enabled in distributed builds.

Recovery fixtures explicitly compile the opt-in path in isolated targets, without linking contradictory core definitions. Historical compatibility assertions are not removed or weakened. No CI reference hash was changed. Existing published recovery experiments remain evidence about that experimental configuration, not the default production solver.

The 9,332,176-byte audio/numerical corpus SHA-256 is `08a381f4ac9de1709a9c9197a78baefb12ffac93cd79f76d4b1e92d4f329e195` for immutable V2, pre-publication baseline and repaired default with AVX2 enabled. The incompatible publication is `1e8a3ed83737b1f8706b35f5997f403f4fea5760ffac7badf60d318e8611fbd3`. All eight targeted AVX2 registered checks passed; see accompanying log.

No hardware fidelity claim. Full cross-platform CI must still qualify the repair.

Additional qualification: default scalar-only core produces the same corpus hash above. Explicit recovery ON with AVX2 ON reproduces the published experimental corpus exactly (the incompatible hash above), demonstrating the experiment is retained. Both configurations passed their two registered batch/engine checks. New default-history regression passes 768 captured states / 197,376 samples, comparing exact output bits and all 12 named history doubles, including clock changes and zero/driven continuations. All three registered valid/missing/truncated cases pass. AddressSanitizer + UndefinedBehaviorSanitizer pass the history regression (`detect_leaks=0`); leak checking is not claimed. Independent review checked macro propagation and isolated fixture targets. CI workflow and historical compatibility assertions remain untouched.
