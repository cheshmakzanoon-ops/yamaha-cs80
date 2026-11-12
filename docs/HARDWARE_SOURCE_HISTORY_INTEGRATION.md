# Combined tone-selector history verification

The reviewed correction `28b5e423` is integrated onto main `47c0a05b`, which
already includes preset row-click repair `8d8bddfa` and its screenshot refresh.
The source integration commit is `3f84dc71`.

All 189 runtime-source files were compared byte-for-byte. The three changed
processor/library files match the reviewed history correction; every other
runtime source matches the main baseline. The two overlapping test-registration
files contain only the reviewed additive changes. The original history evidence
and unrelated panel, fader, MEMORY and screenshot changes are preserved.

A fresh Linux Release build in a new build directory passes:

- Hardware source history: **307 checks, 0 failures**.
- Existing A/B and Randomise history: **106 checks, 0 failures**.
- Preset row-click audition: **41 checks, 0 failures**.
- Preset preferences: **85 checks, 0 failures across seven processes**.
- **10/10 CTests**: the eight original safe history/adjacent suites plus preset
  click and preferences.

The final rebuild reports no work, and all 11 frozen source/test hashes match.
The combined binary SHA-256 is
`53b7abdc465ac52888a5b66d98a25289018d6a0483599e86f57b11ae980410b9`.
Independent review verified that exact binary and all 11 source hashes before
and after repeating every listed check. All repeats passed.

This is combined verification, not a further behavior change. No native-peer,
DAW, full UI smoke, malformed-input, cross-platform, remote CI or release
qualification is claimed. Original reproduction evidence remains separate.
Commands, hashes and outputs are in the
[combined manifest](evidence/hardware-source-name-history-integration/manifest.json).
