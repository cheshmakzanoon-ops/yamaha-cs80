# Draft1x bounded sanitizer qualification

Fresh GCC 14.2 build with AddressSanitizer and UndefinedBehaviorSanitizer, strict FP (`-fno-fast-math -ffp-contract=off`), O1 and frame pointers. Exact commands in `run.sh`, `run-address-undefined.sh` and their logs. Prototype source was not modified; all 193 source-file hashes reverified afterward.

## Result

Address/undefined checks passed: 3,965 structural checks and 72 capture cases (draft/off/HQ, six scenes at 44.1/48/88.2/96 kHz). All processes exited 0; all PCM finite; no address/undefined sanitizer diagnostic. Structural checks cover 288 rate/policy/mode/offline/flag combinations, 1↔2/4/8 decimator/EXT resets, special-value sanitization, prepared-flag rejection, and rendered core voice-clearing transitions. Each capture mode uses a fresh process and each scene a fresh Engine.

Initial structural attempt exited 1 with LeakSanitizer's fatal ptrace incompatibility diagnostic. The original log is retained as `structural.log`. The subsequent explicitly scoped address/undefined attempt disabled leak detection (`detect_leaks=0`) and did not suppress address/undefined findings. **Leaks are not qualified.**

This is bounded core-only functional coverage, not plugin lifecycle, production real-time/allocation, concurrency, host-state/PDC, sonic acceptability or performance certification. No binary timing claims. It uses a sanitized O1 strict-FP build, not the production artifact. Only the existing six capture scenes are covered, not the full prospective quality roster.
