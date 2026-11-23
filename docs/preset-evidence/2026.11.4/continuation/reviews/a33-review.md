# Agent 33 — audition producer contract review

## Scope and result

Read-only source review at final native source `bcc86aa39a33a0162254614e5502705f0113d701`. Reviewed `scripts/capture-factory-auditions.py`, `scripts/test-capture-factory-auditions.py`, the float WAV writer and measurement path in `src/tools/preset_capture.cpp`, the Linux build/capture workflow, and the independent ZIP verifier at `docs/preset-evidence/2026.11.4/ci/verification-tools/verify-audition-zip.py`. Imported the producer without bytecode output and recomputed its roster against the existing local final inventory. No production edits, no new audio renders, no test execution, and no listening claims.

**The current roster meets the stated contract: 30 folders, 138 WAV requests, 74 distinct programs, 870 seconds (14:30), and all active body IDs 1–19 represented.** The inspected final inventory is revision `2026.11.4`, with 523 unique program names and 200 values per program. This review qualifies the producer contract and planned coverage; it does not certify a not-yet-downloaded final CI archive.

## Exact coverage

| Groups | WAVs | Seconds | Purpose |
|---|---:|---:|---|
| keyboards | 24 | 120 | All 24 curated keyboard names, velocity .85 |
| soft-keyboards | 5 | 25 | EP I, EP II, Tine, Reed, Stage at .30 |
| families | 22 | 198 | Brass, bowed, wind, organ, bass, pluck, mallet and expression examples |
| original-addresses | 8 | 40 | Existing original and earlier-library addresses |
| pressure-zero / pressure-full | 14 | 84 | Seven matched names at pressure 0 and 1 |
| instrument-bodies | 7 | 56 | Trombone, Theatre Organ, Oboe, Bassoon, guitars, Kalimba |
| textures-and-effects | 6 | 54 | Evolving/Breathing pads, Thunder, Rain, Surf, Wind |
| soft-kalimba | 1 | 5 | Velocity .30 attack comparison |
| tine-register 48 / 60 / 84 | 3 | 12 | Tine Piano register coverage, velocity .85 |
| piano-register 36 / 60 / 84, soft / firm | 12 | 72 | Analog Piano and Stage Keys, velocity .30 / .85 |
| sustained-effort, soft / firm | 20 | 140 | Seven note-60 and three note-72 sustained instruments at common C4 foundation |
| clarinet-register 60 / 64 / 68 / 72, soft / firm | 16 | 64 | Clarinet and Bass Clarinet across registers and velocity |
| **Total** | **138** | **870** | **74 unique programs** |

The two sustained lists are Solo Violin, Flute, Clarinet, Oboe, English Horn, Covered Horn, Muted Trumpet at MIDI 60, and Solo Cello, Bass Clarinet, Bassoon at MIDI 72. The second list uses the higher played key to account for the 16-foot main body. The clarinet register groups deliberately use the same *played* keys, so Bass Clarinet sounds an octave below Clarinet there.

### Active body coverage

Coverage was computed from actual selected program values, requiring positive body mix and positive audible rank weight: `(1-mix)` for rank I and `mix` for rank II. Merely having a nonzero model selector was not counted.

| Body ID | Representative selected program |
|---:|---|
| 1 | Analog Piano |
| 2 | Electric Piano I / Tine Piano |
| 3 | Reed Piano |
| 4 | Clavichord I |
| 5 | Harpsichord I |
| 6 | Nylon Pluck |
| 7 | Mandolin |
| 8 | Rubber Bass |
| 9 | Solo Violin / Solo Cello |
| 10 | Flute |
| 11 | Clarinet / Bass Clarinet |
| 12 | Oboe / English Horn |
| 13 | Bassoon |
| 14 | Covered Horn / Muted Trumpet |
| 15 | Theatre Organ |
| 16 | Toy Piano |
| 17 | Vibraphone |
| 18 | Marimba |
| 19 | Kalimba / Celesta |

## Source, output and test boundaries

The producer requires the recorded commit to equal `HEAD` and, when set, `GITHUB_SHA`; records `HEAD^{tree}`; rejects staged, unstaged and deleted tracked files; and repeats these checks after capture. It hashes the capture executable before and after capture and rejects mutation. The completed provenance records repository, run ID, run attempt, exact commands, settings, source/tree identity, binary hash, revision, counts and duration. The output directory must be new. Failed captures cannot receive the final README/checksum manifest. Untracked output is intentionally permitted.

The C++ WAV writer serializes each original IEEE float bit pattern directly into 48 kHz stereo 32-bit float WAVs. It performs **no export normalization or extra clipping**. The measurement path examines the same audio vector before export; nonfinite samples and samples exceeding absolute value 1.0 fail capture. This is an export claim: it does not claim the synthesis engine has no designed saturation/limiting.

The Python producer checks the metrics settings, expected per-folder names/counts, zero reported nonfinite/over-full-scale counts, and WAV presence/minimum size. It does not independently decode the WAV payload or enforce 200 parameters/revision .4. Those stricter checks belong to the independent downloaded-ZIP verifier, which does decode every sample, verifies actual frame counts, compares actual peak/RMS to the CSV, checks 523×200 inventory/revision, verifies render parameter digests against inventory, and checks all 19 active models.

The reviewed tests cover correct manifest/roster/checksum construction; unchanged fixture WAV bytes; wrong settings and invalid request groups; missing/extra/empty WAVs; missing/wrong metrics; child failures; source and binary changes; staged/unstaged/deleted tracked files; source/workflow mismatch; and duplicate requests after category default-note resolution. These are producer boundary and provenance tests using fixture audio, not DSP or perceptual listening tests. They were inspected here, not rerun.

The executable hash plus clean checkout provides traceability and immutability; by itself it cannot prove that an arbitrary supplied binary was compiled from that checkout. The actual Linux workflow closes the practical build linkage by checking out the run source, configuring/building the capture target, running tests, and then invoking that freshly built target. Final qualification should bind the downloaded artifact to that exact run and successful build/test/capture steps.

## Honest limits and missing coverage

- The archive contains 74 distinct program auditions; the inventory contains all 523. There is no basis to say all 523 were listened to, or that every preset is acoustically equivalent to its named instrument.
- Single held notes are the covered gestures. Chords, repeated strikes, stealing under polyphony, sustain-pedal phrases, live controller ramps and host-specific behavior are outside this archive.
- Pressure pairs start at fixed pressure 0 or 1. They do not demonstrate a time-varying aftertouch transition. Pressure Lead is intentionally silent at zero pressure.
- Current bowed presets and the current Tine model are represented, but the roster does not send explicit pitch-bend/glissando events to isolate the new stationary bowed-body formants during a slide. That needs the separate targeted model/glide tests, not an enlarged claim about this archive.
- Tine Piano has firm .85 register examples at 48/60/84 and a .30 default-register example. The .30/.85 multi-register piano pairs concern Analog Piano and Stage Keys. The audition roster is not a complete velocity sweep or hard-velocity-1 endpoint sweep.
- Soft Kalimba has a 4-second hold plus 1-second tail; the firm instrument-bodies example has a 6-second hold plus 2-second tail. Common attack windows are comparable, but these two files are not a strictly equal-duration A/B pair.
- One- or two-second tails can omit the complete decay of longer releases. Full-preset sustained effort comparisons do not isolate body-size effects independently from the rest of each preset.
- Safety/peak checks and coverage counts establish numerical behavior and evidence completeness; they do not by themselves establish musical realism or subjective improvement.

## Final actual-CI archive acceptance checklist

1. Obtain the audition artifact from the successful Linux capture job for source `bcc86aa39a33a0162254614e5502705f0113d701`, using the exact workflow run ID and attempt. Verify the artifact ZIP SHA-256 against GitHub's artifact API digest. An old e327/c4 artifact is not the final native-build source artifact.
2. Execute the source-bound independent verifier with that exact source/run/attempt/API digest. It imports the roster from `git show SOURCE:scripts/capture-factory-auditions.py`, so the checkout's later documentation commits cannot silently change the expected source roster.
3. Expect 173 regular members: five root files (`SHA256SUMS`, README, source commit, provenance, inventory), 30 metrics CSVs, 138 WAVs. Expect 172 checksummed members; the checksum manifest does not hash itself. Reject missing, extra, duplicate, unsafe or oversized ZIP members.
4. Expect schema-3 completed provenance, correct repository/source/tree/run/attempt, clean source both before and after, valid executable SHA-256, exact source-bound commands/settings/roster, and matching source-commit text.
5. Expect revision `2026.11.4`, inventory schema 1, exactly 523 ordered host/index addresses and unique names, 200 unique parameter IDs, 200 finite values per program, stable body parameter positions 190–197 and appended `body1_size` / `body2_size` at 198–199.
6. Expect every CSV program identity/settings to match inventory and roster; render parameter digests to match actual inventory float values; exact WAV filenames; 48 kHz stereo IEEE float with complete RIFF/fact/data framing; 138 WAVs and 41,760,000 frames total = 870 seconds.
7. Decode every audio payload. Require finite samples and no absolute samples above 1.0; recomputed peak/RMS must match CSV within the verifier's tolerance. Record the **actual** archive maximum peak rather than assigning a guessed headroom figure. Raw audio payload is 334,080,000 bytes; WAV headers add 8,004 bytes across 138 files.
8. Recompute audible rank/body coverage from the downloaded inventory and selected names: require IDs 1–19. Preserve the verifier JSON report with exact artifact/source/binary/script/verifier digests and its per-WAV results. Describe it as representative single-note evidence, separate from full-bank numerical tests and native plugin/host validation.

## Reviewed source hashes

| File | SHA-256 |
|---|---|
| `scripts/capture-factory-auditions.py` | `99150f4e685db3d3f0dde95ef2bc003a0b26d36499a76c957464af13ed80d9c7` |
| `scripts/test-capture-factory-auditions.py` | `ff526f51830fba1c1c755061a7e366c57b5647dd4a983b31d6aa447b15524605` |
| `src/tools/preset_capture.cpp` | `706e48efe50c785465426fbd2f1613d92a0b4178d1e29e9837cfde3b3ce32883` |
| `docs/preset-evidence/2026.11.4/ci/verification-tools/verify-audition-zip.py` | `75be3d29b566c9a88f9d301853ae8c60585aa604b3fc450ba23de1a6eaeebf62` |
