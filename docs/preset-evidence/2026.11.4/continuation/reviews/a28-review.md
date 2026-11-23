# Agent 28: numerical and timebase review

Source: `e32728b31d97fc356cac291cb98935fdcd38f803`.
Header SHA256: `d33f34f5cff6341abc9bc2c85056ce381b492d141a0fad856e0c7cbd4f06422e`.
No production files changed; no commit or push.

## Decision

No additional numerical, array-bounds, or timebase defect found in the tine and bowed corrections.

## Bound and indexing proof

- Both bowed cache arrays have `harmonicModeCount` (24) entries. Non-piano harmonic generation uses that same constant; the six extra coupled courses are created only for piano. Bowed count therefore cannot exceed 24, whereas the general mode store remains 48.
- The bowed response has an additive 0.32 floor and finite positive terms. Onset notes clamp to MIDI 0–127 and size to 0.5–2.5, so every inverse response is positive and finite. Live positive finite frequency is clamped to 0.01 through 0.49 times the internal sample rate before response evaluation.
- Let M_i=max(dark_i,bright_i), r_i=currentResponse_i/onsetResponse_i, B0=sum(M_i), and B=sum(M_i*r_i). The new common factor s=min(1,B0/B) gives sum(s*M_i*r_i)<=B0. Color is a convex interpolation between dark and bright; antialias, onset and pickup factors lie in [0,1]. Consequently the modal target sum retains the former ceiling. The existing 16-sample ramps interpolate between bounded targets. This is a modal-excitation bound, not a limiter on the downstream filters, VCA, noise or complete plugin bus.
- Modal antialias weights use host output rate, with a smooth fade from 0.38 to 0.465 of that rate. The new positive formant factors do not bypass that weight. High pitched modes can fade back in on downward bends without an onset allocation or phase reset. This does not independently certify all modulation sidebands or the noise path as alias-free.
- Tine actual excitation weights cannot exceed the corresponding full-strike bound: strike is clamped to [0,1], and its edge factor lies in [0.2,1]. The change adds no time-varying normalization to the sample loop.

## Reset, timebase and invalid controls

`reset()` leaves the new arrays' bytes untouched, but disables the model and clears its mode count. Every subsequent bowed onset rewrites all used cache entries and both bound scalars. Non-bowed paths never read those arrays. Copy assignment includes the caches and ramps; no pointers or external storage were added. `prepare()` recalculates coefficients and resets the model, so a new note is required after a sample-rate change.

Nonfinite, zero and negative live pitch returns zero before changing phase, envelopes, RNG or the control countdown. Resuming with a valid pitch resumes the preserved state. This is the existing malformed-input behavior, not a new bowed-specific timebase. Malformed brightness and pressure use finite fallbacks.

## Focused numerical probe

A header-only ASan/UBSan probe exercised every model, MIDI 0/60/127/NaN, sizes 0.5/2.5/NaN, internal/host rate pairs including 8/8 kHz, 88.2/44.1 kHz, 96/48 kHz, 192/96 kHz and 1536/96 kHz, plus invalid-rate fallbacks. Live pitch visits 0.01 Hz, ordinary notes, 15 kHz and 1e30 Hz; NaN/infinite/negative/zero inputs are interspersed with valid calls.

Result: **6,881,280 paired sample positions, 7,173,600 checks, zero errors; maximum absolute sample 0.716935039**. No AddressSanitizer or UndefinedBehaviorSanitizer report. Leak scanning was disabled. The combined probe also compares reused, fresh and copied objects; those lifecycle checks overlap agent 26 and should not be counted as an additional independent qualification gate.

Evidence: `a28/probe.cpp`, `a28/probe.log`, `a28/summary.json` in the continuation scratch directory. No native host, realtime deadline, full-engine aliasing or acoustic-realism claim follows from this probe.
