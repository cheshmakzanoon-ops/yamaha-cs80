# Circuit preset refinement 2026.11.1

## Result and publication status

This refinement gives **twelve Circuit programs** stronger defining details or
more useful stereo separation. It builds on the earlier **300-program 2026.11
redesign**, retaining the bank's 301 entries including Init Circuit and the 222
frozen Legacy programs. The comparison baseline for this refinement is
`7f55a95a7534b389927defe46866456560b3a878`, which already contains that redesign.
The [original revision report](PRESET_REVISION_2026_11.md) documents the larger
change from the older library.

**All four native builds and installation archives are verified, and the
actual 24-WAV audition passed its artifact checks. The complete CI run failed
because the Linux randomized GUI validator terminated by SIGSEGV after SUCCESS.
The gated publisher was skipped; no qualified 2026.11.1 prerelease is published.**
The download links below are explicitly unsigned development candidates.

Binary source is `6d3cc220edef5a4a2c7cd968ea8a786c7c83198c`,
[build 45044040694, attempt 1](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/45044040694).
Its predecessor was cancelled after all four native runners recorded the same
stale workflow-snapshot test failure. The reviewed repair changed that snapshot;
the sound bank, packaging code and required validator commands were unchanged.
[Integration lineage](preset-evidence/2026.11.1/ci/source-lineage.json)

| Deliverable | Observed result |
| --- | --- |
| Twelve authored refinements | Implemented and measured against the frozen 2026.11 baseline |
| Character regression suite | 128/128 final-bank checks pass; the baseline fails 16 of the identical checks |
| Final calibration and library audit | Two applied passes converge for the twelve targets; 301 Circuit + 222 Legacy programs, 136 voicing checks and 9 capture-tool tests pass |
| Windows x64 VST3 / Standalone | **Native/platform candidate passed:** 219 of 221 registered CTest entries passed, zero failed, two skipped; both strictness-10 VST3 gates passed. [ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763872941.zip) · [Native record](preset-evidence/2026.11.1/ci/native-windows-x86_64.json) |
| macOS Apple Silicon VST3 / AU / Standalone | **Native/platform candidate passed:** 241 of 243 CTest entries passed, zero failed, two skipped; VST3, AU and AU GUI strictness-10 gates passed. [ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763761497.zip) · [Native record](preset-evidence/2026.11.1/ci/native-macos-arm64.json) |
| macOS Intel VST3 / AU / Standalone | **Native/platform candidate passed:** 241 of 243 CTest entries passed, zero failed, two skipped; VST3, AU and AU GUI strictness-10 gates passed. [ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14763644216.zip) · [Native record](preset-evidence/2026.11.1/ci/native-macos-x86_64.json) |
| Linux x86-64 VST3 / Standalone | **Native/archive passed; GUI validator failed:** 578 of 579 CTest entries passed, one SKIP_RETURN_CODE=77, zero failed; native CTest took 9,492.20 seconds. [Experimental ZIP — GUI validation failed](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14768807632.zip) · [Gate and exit record](preset-evidence/2026.11.1/final-ci/linux-x86_64/ci-proof.json) |
| Four installation archives | **Archive/installation checks passed:** complete inventories, native formats and architectures, executable/bundle structure, source identity, guides and notices. This does not qualify the failed full release. [Record](preset-evidence/2026.11.1/final-ci/installation/artifact-proof.json) |
| Actual 24-excerpt CI audition | **Artifact passed:** 117 seconds / 5,616,000 stereo frames at 48 kHz; all 24 parameter digests and 32 hashes verified; no nonfinite or full-scale samples. [ZIP](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip) · [Record](preset-evidence/2026.11.1/final-ci/auditions/artifact-proof.json) |
| Five public candidate downloads | **Byte checks passed:** full unauthenticated GETs matched fresh API sizes/digests and cached artifact bytes. They remain candidates from a failed build. [Record](preset-evidence/2026.11.1/final-ci/public-candidates/summary.json) |
| Complete CI and qualified prerelease | **Failed / withheld:** the required Linux randomized GUI gate failed; publisher run 45060638607 and its publish job were skipped. No public `presets-2026.11.1` release was available in the recorded check. |

The native counts distinguish executed passing cases from formal skipped
entries. The native records separately retain embedded Python subtest skips.
Archive and download verification establish their stated byte/inventory scope;
they do not reverse a validator failure or establish arbitrary DAW compatibility.

### Linux validator failure and diagnostic limits

| Required lane | Seed | Original child exit | Result |
| --- | --- | --- | --- |
| Headless VST3 | `0x3e72560` | 0 | Passed |
| Randomized GUI under Xvfb | `0x4369262` | −11, normalized wrapper 139 | **Failed after printing SUCCESS** |
| Fixed GUI regression | `0x2da72db` | 0 | Passed |

There were **zero internal wrapper retries**. The same-seed GDB and fault-thread
diagnostics both exited 0; these are nonreproductions and do not replace the
original failed invocation. No original crash core or stack was available.
The evidence does not establish the crash's ownership or root cause, and no
repair is claimed. [Original exits and diagnostic evidence](preset-evidence/2026.11.1/final-ci/linux-x86_64/ci-proof.json)

A separate [teardown source audit](preset-evidence/2026.11.1/ci/teardown-source-audit.md)
preserves the production-source comparison and lifetime review; it establishes
neither crash ownership nor a repair.

A separate upstream pluginval 2.0.0-beta3 comparison passed its three recorded
lanes on the same plug-in artifact. Its validator, test inventory and seeds
differ from the required stable invocation. Those passes add bounded
compatibility evidence; they do not repair the failed stable gate or identify
its cause. [Independent failure review and actual comparison](preset-evidence/2026.11.1/final-ci/failure-review/README.md)

The publisher's success and first-attempt requirements remain unchanged. The
public candidate links expose existing native artifacts with this failure
disclosed; they are not a qualified release or seven published release assets.
They retain source `6d3cc220` even after documentation changes on `main` and are
subject to GitHub artifact retention. The package application version alone is
not a build identifier. These bundles have no release signing or notarization.

The older **2026.11** source `d0c3c23`, its original Linux headless failure and
later successful retry remain documented in the
[original report](PRESET_REVISION_2026_11.md#linux-validator-history-and-remaining-limitation)
and [distribution history](CI_DISTRIBUTION_STATUS.md). Those records are separate
from this run's GUI failure; no common cause or fix is inferred.

[Evidence index](preset-evidence/2026.11.1/README.md) ·
[Additional native and performance evidence](preset-evidence/2026.11.1/INTEGRATION.md) ·
[Evidence provenance](preset-evidence/2026.11.1/provenance.json) ·
[Current 60-agent assignment review](TEAM_REVIEW_2026_11_1.md)

[Consolidated qualification](preset-evidence/2026.11.1/ci/release-qualification.json) ·
[Acceptance of the reviewed snapshots](preset-evidence/2026.11.1/ci/acceptance/FINAL-ACCEPTANCE-2026_11_1.md)

## What changed and why

The audit measured the contribution of each rank at the program's actual
balance. A separately programmed breath or strike can exist in isolation yet
remain too subordinate in the finished voice to express its intended role.
Likewise, chorus does not by itself establish a useful stereo image. The changes
address those specific relationships while retaining the existing synthesis
algorithms, family helpers, registrations and program identities.
[Authoring identity comparison](preset-evidence/2026.11.1/identity/summary.json)

### Sustained air: Flute and Breathy Flute

The flute body contains a strong sine that bypasses both filters. Its noise rank
therefore needed its own source, filter-output, VCA, sustain, touch and balance
adjustments. Flute now carries restrained sustained air; Breathy Flute has a
stronger aspirated component. Their existing body timing and pitch registration
remain intact. The already corrected Pan Flute keeps its different role: a
short chiff that disappears while the pitched body continues.
[Wind authoring](../src/core/curated/Winds.inc)

At **48 kHz, MIDI 67, velocity .8, pressure 0**, the weighted air/body RMS over
**300–500 ms** changes as follows:

| Program | Baseline | Refined authoring |
| --- | ---: | ---: |
| Flute | −50.77 dB | **−26.85 dB** |
| Breathy Flute | −34.64 dB | **−19.62 dB** |

[Matched center captures](preset-evidence/2026.11.1/winds/center.json)

Across MIDI **55/67/79**, velocities **.3/.9** and pressure **0/1**, Flute's
refined ratio spans −28.93…−25.02 dB and Breathy Flute's spans
−21.64…−17.45 dB. These pressure settings are fixed endpoints, not performed
crescendos. Weighted isolated ranks reconstruct the complete signals within
−142.3 dB relative RMS or better. An additional two-second, eight-note chord at
full velocity and pressure peaks at −2.506 dBFS for Flute and −4.353 dBFS for
Breathy Flute, using the baseline master volumes.
[Wind measurements and conditions](preset-evidence/2026.11.1/winds/summary.json)

Final-bank performance checks add 24 captures / 96 seconds of channel and
polyphonic pressure ramps, soft/hard repeats, overlap and release. Every voice
retires, all samples are finite, and the highest peak is **−7.824 dBFS**. Matched
pressure-step/control replays produce no immediate output or touch-node jump;
polyphonic pressure affects only its addressed note. These performed phrases
complement the fixed-endpoint authoring measurements.
[Performed wind evidence](preset-evidence/2026.11.1/wind-performance/summary.json)

### Defining attacks: Harp, Kalimba, Sine Bass and Upright Pluck

Harp and Kalimba retain their body and upper-rank envelopes. Increasing only the
upper sine and rank balance gives their existing octave glint and ping more
weight. Sine Bass and Upright Pluck retain their waveform sources, timing and
touch routes; a modestly lower strike high-pass, stronger filter output and
revised balance make the short upper component less thin.
[Plucked voices](../src/core/curated/Plucked.inc) ·
[Bass voices](../src/core/curated/Bass.inc)

| Program | Increase in weighted upper/body RMS, 4–20 ms | Conditions |
| --- | ---: | --- |
| Harp | **8.85–8.99 dB** | MIDI 48/60/76; velocities .25/.9 |
| Kalimba | **8.08–8.21 dB** | MIDI 48/60/76; velocities .25/.9 |
| Sine Bass | **7.89–8.16 dB** | MIDI 36/48/60; velocities .3/.9 |
| Upright Pluck | **7.31–7.45 dB** | MIDI 36/48/60; velocities .3/.9 |

All use **48 kHz and zero pressure**. These are relative layer measurements
before final master calibration. The detail remains subordinate and continues
to decay ahead of the body. For example, at MIDI 60 and velocity .9, Kalimba's
upper/body ratio falls from −18.07 dB during 0–20 ms to −51.51 dB during
200–400 ms. The change does not turn its brief strike into a sustained layer.
[Pluck measurements](preset-evidence/2026.11.1/plucks/measurements.json) ·
[Bass measurements](preset-evidence/2026.11.1/bass/summary.json)

### Late texture: Ether Pad

Ether Pad increases its noise amount from .65 to .95 and rank balance from .19
to .28. Its sine, filters, envelopes and pressure routes stay unchanged. Across
MIDI **48/60/76** and pressure **0/.5/1**, velocity .8 at 48 kHz, the late air/body
ratio increases **7.693–7.696 dB**. At MIDI 60 without pressure it moves from
−32.67 to −24.97 dB over 1.5–2.5 seconds. The air still develops later than the
body; the dominant pitch bin remains unchanged in every comparison.
[Ether measurements](preset-evidence/2026.11.1/ether/summary.json)

### Stereo spread: five ensembles

These programs previously rendered almost mono despite descriptions of broad
ensembles. Individual opposing rank pans now supply a restrained spread while
the original chorus settings remain active. The measurements compare **MIDI 60,
velocity .8, pressure 0, 48 kHz**, over **1.0–2.8 seconds** of a held note.

| Program | Baseline side/mid RMS | Refined side/mid RMS |
| --- | ---: | ---: |
| Noir Swell Brass | −39.47 dB | **−19.28 dB** |
| Brass Ensemble 16 | −37.84 dB | **−21.79 dB** |
| Neo-Noir Pad | −38.92 dB | **−19.95 dB** |
| Arctic Pad | −38.07 dB | **−23.09 dB** |
| Synthwave Chords | −40.42 dB | **−20.14 dB** |

Additional captures at MIDI 48/72 and pressure 0/.85 retain maximum absolute
channel imbalance of **0.663 dB**, overall mono loss of **0.103 dB**, and
fundamental-band mono loss of **0.116 dB**. Mono is `(L+R)/2`, compared against
averaged stereo power. These are bounded single-note results. A final-bank performed check also
rendered five twelve-second, eight-voice chord sequences and matched controls.
The factory programs retained at least **3.817 dB** sample headroom; worst
active-window mono loss was **0.317 dB overall** and **0.481 dB in the combined
fundamental bands**. All voices retired by twelve seconds; the engine output
floor remained near −82 dBFS, so this does not mean digital silence.
[Stereo comparisons](preset-evidence/2026.11.1/stereo/initial.json) ·
[Register checks](preset-evidence/2026.11.1/stereo/summary.json) ·
[Performed chord evidence](preset-evidence/2026.11.1/stereo-chords/summary.json)

**Pure CS-80 centers both rank pans.** This authored stereo spread belongs to
ordinary Circuit mode; Pure intentionally removes it. The [mode contract](PARITY_ISOLATION.md)
and [preset guide](PRESET_GUIDE.md) explain that behavior.

## Regression sensitivity and compatibility

The new [character tests](../tests/PresetCharacterTests.cpp) measure the finished
sound: sustained breath, present but subordinate strikes, earlier disappearance
of the attack layer, stereo information, channel balance and mono retention.
They also require weighted isolated ranks to reconstruct each complete patch
within **1% RMS**, both over the full capture and the exact measurement windows.
This prevents a local diagnostic mismatch from being hidden by a long average.

| Identical suite against | Passed | Failed |
| --- | ---: | ---: |
| Frozen 2026.11 baseline | 112/128 | 16 |
| Final calibrated 2026.11.1 bank | **128/128** | **0** |

The same 31.4-second, 44.1 kHz fixture covers eleven programs; Ether Pad has its
separate authoring matrix. No character threshold was relaxed after execution.
The final integrated run also passes the full 523-program audit, 136 voicing
checks and nine capture-tool tests. Two applied level-calibration passes converged
for the twelve refined programs;
only Ether Pad, Kalimba, Sine Bass and Upright Pluck needed master-level changes.
These bounds describe the intended roles of these programs,
not universal hearing thresholds or acoustic-instrument similarity scores.
[Character evidence](preset-evidence/2026.11.1/character/summary.json) ·
[Independent review](preset-evidence/2026.11.1/review/summary.json) ·
[Final integrated results](preset-evidence/2026.11.1/final/summary.json)

The final compiled-bank identity comparison finds exactly **38 voicing values
and four master levels changed across twelve programs**, with the other **289
Circuit value vectors** intact. UFO Landing also has a description-only
correction, leaving 288 complete Circuit records unchanged. All names,
categories, tags, ordering and host indexes remain stable. All **222 Legacy
records**, including their complete 190 float bit patterns and metadata, match
the baseline. All **49,152 tested MIDI addresses**, 830 index lookups, the
296-parameter application layout and state schema 3 remain unchanged. The native
VST3 wrapper separately exposes 2,378 parameters including wrapper and MIDI
parameters; that is a different count from the application layout.
[Final identity evidence](preset-evidence/2026.11.1/final-identity/summary.json)

Saved sessions, user presets, A/B and browser Revert restore their serialized
sound values. A saved factory index is metadata and does not reload the bank.
Explicit factory selection or a later MIDI Program Change loads the current
voicing at its stable address. A host's program-number recall must reach the
factory-selection path; VST3 Program processing-queue-only changes retain the
controller dependency documented below. Startup remains Brass Swell I.
[Restore control-flow review](preset-evidence/2026.11.1/compatibility/summary.json)

A separate audio screen compared **45,150 pairs across all 301 programs** and
found no exact raw or gain-normalized duplicates in its stated fixture. Ten
follow-up captures distinguish the closest pairs through register response,
decay or onset. This is a bounded distinctness check, not a perceptual quality
score or proof that related instruments should sound unrelated.
[Distinctness evidence](preset-evidence/2026.11.1/distinctness/summary.json)

## Native recall and host-program qualification

Four native JUCE processor/browser fixtures pass **6,607 checks** covering
factory selection, serialized sessions, user presets, A/B, Revert and loaded
identity. A separate external host loads the actual local Linux VST3 module and
passes **45,115 assertions**: all 523 program names and their order, plus
**4,560 normalized core-value comparisons** across selection and state restore
of the twelve revised programs. All 42 revised values agree with the compiled
factory inventory in both phases. That successful path explicitly synchronizes
the VST3 controller and processing queue; it is not a named-DAW certification.
[Native recall](preset-evidence/2026.11.1/native-recall/summary.json) ·
[External VST3 evidence](preset-evidence/2026.11.1/external-vst3/summary.json)

**A pre-existing Program automation limitation remains.** A Program value sent
only through the VST3 processing queue does not select the factory sound in
either tested native module. Installing a component handler, processing audio
on a separate thread and allowing message-thread time do not change that
observation; explicitly setting the controller Program value does select it.
Ordinary parameter automation works in the same diagnostic. The Program
parameter is advertised as automatable, so the failing queue cannot simply be
dismissed as invalid. The relevant bridge/processor code and pinned JUCE wrapper
are unchanged by this preset refinement. The earlier binary's exact compile
commit is unknown and is not inferred from its workspace. The diagnostic
records its binary hash and distinguishes its older factory data directly.
[Program queue diagnosis](preset-evidence/2026.11.1/program-queue/method.txt)

Library selection, controller-based selection and saved-state restoration are
supported by their passing fixtures. Queue-only Program automation, sample-accurate
program switching and arbitrary host integration are not certified. No runtime
or pinned-wrapper repair is claimed in this preset revision; MIDI Program Change routing
is a separate source path and was not exercised by this external host fixture.

## Additional performance checks

The [additional evidence index](preset-evidence/2026.11.1/INTEGRATION.md) records
bounded checks of the final bank at representative sample rates, performed
pressure and velocity, long sustained chords, repeated strikes, phrase gates,
glide modes and all four arpeggiators. Six refined layer roles retain their
measured behavior at 44.1, 48 and 96 kHz. Twelve representative slow programs
remain finite and below full scale through eight-note, ten-second holds with a
pressure ramp and eight-second release; the highest sample peak is **−2.939 dBFS**.
All four arpeggiators follow their documented order, timing and source-key gate
behavior in the recorded free-running fixtures.
[Sample-rate checks](preset-evidence/2026.11.1/sample-rates/summary.json) ·
[Long sustained chords](preset-evidence/2026.11.1/long-sustain/summary.json) ·
[Arpeggiator checks](preset-evidence/2026.11.1/arpeggiators/summary.json)

These records retain each fixture's source or binary identity, event sequence,
measurement window and limits. The original failed observations remain alongside
the follow-up results: the old Linux validator crash, an Xvfb setup failure,
too-short initial ring-release observations and the queue-only Program failures.
Passing checks are not combined into a subjective sound-quality percentage.

## Playing instructions and audition delivery

UFO Landing's description now correctly explains its shared ring sweep. In it,
Gong, Temple Bell and Ring Sweep Bell, overlapping notes continue the same
carrier envelope. Release all keys and any sustain hold before the next attack
to restart the sweep. Matched continuous/overlapping/detached phrases confirmed
identical carrier histories for overlap and a fresh envelope for detached
attacks; the sustain qualification follows the source gate logic.
[Ring evidence](preset-evidence/2026.11.1/ring/summary.json)

The [build workflow](../.github/workflows/build.yml) now defines **24 audition
WAVs**, retaining the original twelve examples and adding every refined program.
Seven slow examples use five-second holds and two-second release excerpts; the
others use three-second holds and one-second releases. Captures retain the
factory settings without normalization. Provenance includes source commit,
binary hash, selection, timing, completion status and checksums. A pipeline
smoke check produced 24 WAVs and verified 32 checksummed files; that check used
the frozen baseline executable solely to validate delivery.
[Pipeline evidence](preset-evidence/2026.11.1/audition/summary.json)

The actual revised-bank CI artifact now verifies **24 WAVs / 117 seconds**,
all 32 checksums and all 24 selected factory parameter digests. Its highest
sample peak is **−14.576 dBFS**; all samples are finite and below full scale.
It identifies source `6d3cc220`, run `45044040694`, attempt 1. Its producing
Linux build/capture steps succeeded, while the later GUI validator failure
prevents complete release qualification.
[CI audition verification](preset-evidence/2026.11.1/final-ci/auditions/artifact-proof.json) ·
[Public candidate download](https://nightly.link/cheshmakzanoon-ops/yamaha-cs80/actions/artifacts/14769036590.zip)

The earlier local replay retains source
`99f5ebd6b71283a56810d801df89c49ef62f3512`, run `local-assignment-32` and its
original capture binary hash. It is not relabeled as the CI artifact. The older
twelve-WAV **2026.11** package is also a separate recording set.
[Local replay](preset-evidence/2026.11.1/final-auditions-local/summary.json) ·
[Local provenance](preset-evidence/2026.11.1/final-auditions-local/preset-audition/provenance.json)

Instrument names describe CS-80 synthesized interpretations. These measurements
establish specific software behavior and reproducible differences; they do not
establish a listening-panel verdict, acoustic-instrument identity, calibrated
physical-CS-80 matching or compatibility with every DAW and performance history.
