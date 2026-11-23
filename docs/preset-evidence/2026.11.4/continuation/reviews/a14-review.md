# Agent 14 — Organ recipe audit

Scope: all 14 recipes in `src/core/curated/Organ.inc`, host indexes 360–373, starting source `59d489d`. Read-only production audit; no source or test edits.

## Decision

No additional definite recipe bug found in organ velocity routing, registration ratios, held/percussive articulation or modulation descriptions. The recipes support their stated intended organ colors. This is not a measured acoustic-organ match or a listening-based quality judgment.

## Complete checklist

| Existing program | Registration and articulation | Review |
|---|---|---|
| Flute Organ 8+4 | 8′ main / lighter 4′ upper; dark organ bodies, 10/16 ms entrance, held sustain | Correct octave pair and velocity-independent stops |
| Flute Organ 16+8 | 16′ / 8′; slower low entrance, dark organ bodies | Correct root octave and low-rank entrance |
| Percussive Organ | Sustaining 8′ body plus 4′ pure-sine percussion; upper sustain 0 and 0.82 s decay | Separate second-harmonic percussion actually dies while main stop holds; each accepted note retriggers |
| Reed Organ | 8′ / 8′; organ bodies blended with pulse edge; 22/39 ms entrance | No unintended octave or velocity layer |
| Church Organ | 16′ principal-like body / restrained 4′ upper; 55/38 ms entrance, 260/170 ms release | Description matches two-octave registration and dry, compact tail |
| Chapel Organ | 8′ / quieter 2′; upper entrance later at 61 ms | Correct two-octave upper stop and quiet registration |
| Theatre Organ | 8′ / 4′; SUB VCA depth .14 at 5.6 Hz plus shallow BBD tremolo | Whole-registration amplitude motion exists; stereo color is BBD tremolo, not a physical rotary-speaker model |
| Tremolo Organ | 16′ / 4′; SUB VCA depth .20 at 3.2 Hz plus shallow BBD tremolo | Low-frequency amplitude motion is actually routed; straight rank detune |
| Mellow Organ | Main 8′ rank only, mix 0; dark organ/sine blend | Upper rank correctly excluded; smooth compact release |
| Bright Organ | 8′ principal-like body / quiet 2′ stop; fast 4.5/6 ms gates | Brightness comes from registration, not velocity |
| Pipe Organ | 16′ / 4′; separate 42/22 ms entrances and 220/130 ms releases | Correct registration; no chorus or ensemble detune |
| Combo Organ | Electronic pulse 8′ / 4′; bodies off; shallow chorus, .18 cent detune | Deliberately electronic combo identity, with no inappropriate hammer velocity law |
| Fifth Organ | 8′ / 5⅓′; quieter upper mix .23 | Upper multiplier is exactly 1.5 (3:2), not an octave or a third |
| Organ Vibrato | 8′ / 4′; SUB VCO depth .035 at 5.9 Hz | Actual restrained pitch modulation; no pressure or velocity dependence |

## Velocity and controller verification

The final inventory has all eight rank initial/aftertouch brilliance/level destinations zero in every one of these 14 recipes. Global initial bend, SUB-aftertouch routes, mod-wheel routes, sustain override and key-release override are also zero. Both ranks hold sustain 1 except the intended Percussive Organ upper rank. The organ body's note-on forces a fixed excitation strike and seed, and its sustained modal losses are zero.

A full-preset render comparison tested each of the 14 programs at MIDI 60, 48 kHz, velocities 0.2 and 0.95, with a 0.5-second hold and 0.15-second release tail. **All 14 soft/firm pairs are byte-identical WAVs**. This verifies that the tested complete presets, including their effects and release, have no unintended velocity response. It does not establish identity at untested registers or every possible controller state.

Evidence: `a14/velocity-identity.json` records the executable SHA256, source file hashes, exact render scope and the 28 WAV hashes. The root-built executable contains the tine correction; the note explicitly distinguishes the starting commit from that uncommitted build.

## Non-blocking observations

- Eight active organ-body ranks carry non-neutral Partial decay values. As already documented for sustained models, these values do not change held modal losses and must not be counted as audible revoicing. No forced decay is recommended.
- The 14 programs do not promise a rotary cabinet. The Theatre/Tremolo descriptions correctly identify amplitude movement and stereo color. `Program::tremolo()` selects BBD tremolo, while their separately authored SUB VCA route provides whole-signal motion at the stated rate.
- Small authored drift/dispersion remains in the family. “Straight tuning” here means no intentional rank detune; it is not a claim of mathematically fixed pitch.
- Percussive Organ is a per-accepted-note synth percussion implementation. It does not claim Hammond-style single-trigger percussion behavior, and should not be described as one.

## Source paths checked

- `src/core/curated/Organ.inc` — all14 recipes and family defaults.
- `src/core/CuratedLibrary.inc` — feet enumeration, touch, SUB, chorus/tremolo and envelope helper meanings.
- `src/core/Params.h` — nominal feet ratios `.5, 1, 1.5, 2, 3, 4`.
- `src/core/Engine.cpp` — touch destinations, SUB amplitude/pitch routing and final BBD path.
- `src/core/InstrumentBody.h` — organ strike independence and sustained losses.
- `src/core/BbdChorus.h` — explicit BBD tremolo branch gains and Hz rate handling.
- `docs/preset-evidence/2026.11.4/development/final-bank/inventory.json` — final vectors, avoiding overwritten source-chain literals.
