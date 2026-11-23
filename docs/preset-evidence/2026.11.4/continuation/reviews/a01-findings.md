# Agent 01 — Continuity and release audit

Repository: cheshmakzanoon-ops/yamaha-cs80; source 59d489d1a1f0065ae2a0f42a08c74edc43e94159. Read-only audit.

## Observed CI

Build run 45237136125: Windows job 137911734571, Linux 137911734834 and Intel Mac 137911734881 building; Apple Silicon job 137911735171 queued. No observed failure. Exact-source DSP run 45237136162 and screenshot run 45237136123 in progress; filter batch 45237136207 passed.

## Required qualification for new links

- Four native suites and packaging/upload jobs, pinned clean JUCE 8.0.11/c352e2489000ffeb9e397051377d75eef2efe641. Count real CTest results, formal skips separately.
- 13 pluginval 1.0.4 strictness-10 invocations: Linux VST3 headless, GUI and GUI seed 0x2da72db; Windows VST3 headless and seed 0x5037a0c; each Mac VST3, AU headless, AU GUI and AU GUI seed 0x2b4eef5. A validator SUCCESS line followed by nonzero process exit does not pass.
- Verify native distribution bytes: artifact API identity/size/hash/run/attempt/source, inner manifest/SOURCE.txt/inventory/checksums, executable headers and JUCE provenance. Windows/Linux VST3 payload must match separate validator-input archives. New Mac workflow now uses diff -qr on AU_SOURCE/AU_DEST before validation and packaging, and preserves before/after binary hashes in pluginval artifacts.
- Exact-source DSP/sanitizer and UI screenshot results must pass; do not relabel older source evidence.
- Actual audition archive should contain expected 138 float32 stereo WAVs / 74 programs / 870 seconds plus full 523 x 200 inventory; inspect actual payload before advertising it.

## Publication

Current public README links still point to cbbee6a / revision 2026.11.3. Main 59d489d is revision 2026.11.4. Existing .github/workflows/publish-preset-refinement.yml is pinned to old 2026.11.1 run 45044040694 and source 6d3cc220; it cannot publish the current work automatically. Ordinary main pushes cancel obsolete per-platform jobs, so new audio commits require their own fresh qualification.

Publication targets: README.md, docs/CI_DISTRIBUTION_STATUS.md, docs/CIRCUIT_LIBRARY_2026_11_4.md (or next revision report), docs/PRESET_GUIDE.md and docs/INSTALL_AND_QUICKSTART.md as relevant; include exact-source JSON qualification and current artifact expiration. Keep earlier outcomes historical. Native bundles have no publisher signing/notarization; no AAX.

## Stale documentation

docs/release/RELEASE_QUALIFICATION.md still claims pre-plan sessions reopen on Legacy sounding identical, contrary to current single-Circuit engine and 2026.11.4 report. That row should be reconciled when editing release docs. Its 3-platform formal installer workflow produces different rebuilt artifacts and must not inherit build.yml validator results. Current qualified public route is four architecture-specific CI bundles.

Sources: .github/workflows/build.yml at 59d489d; docs/CIRCUIT_LIBRARY_2026_11_4.md; docs/CI_DISTRIBUTION_STATUS.md; prior source-cbbee6a/release-qualification.json and native-validator-qualification.json; GitHub Actions APIs.
