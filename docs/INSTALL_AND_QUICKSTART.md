# Install and play the CI development build

These are development artifacts, not a signed/notarized public release or a
promise of compatibility with every DAW. Read `DISTRIBUTION_STATUS.md` in the
package. No calibrated hardware-match claim is made.

This guide covers factory library source candidate **2026.11.8**: revoiced existing
programs and optional instrument bodies within one circuit instrument, the
unified **523-program library**, **Studio**, **PANIC** and the **SAMPLE PEAK**
readout. The cumulative work since 2026.11.4 revises all **521 existing musical programs**;
**Init Patch** and **Init Circuit** remain starting points, with all names and
program addresses preserved. This increment improves piano contact and treble
excitation, electric-piano pickup response and instrument-specific release
damping. It also calibrates existing EP recipes and stabilizes organ, Ocarina
and Crystal Strings layers. All 204 parameter definitions and the earlier
piano tuning, rate compensation, wind/brass response and material profiles
remain intact. See the [2026.11.8 sound revision](CIRCUIT_LIBRARY_2026_11_8.md).

At this source checkpoint, **2026.11.8 native builds and delivery qualification
are pending**. This guide does not announce a qualified download. The
[current online download table](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads) identifies the available artifacts
and their accepted source; a later source revision does not update those binaries.
Engine and isolation selectors remain removed. Older artifacts
may contain an earlier sound library. Use the guide bundled with your artifact
and compare its full manifest commit with the current-build table in the
repository README.
The repository retains earlier revision reports and download records. Their
bundled guides, manifest revision and `SOURCE.txt` identify those earlier
binaries; later source changes do not update an existing archive.
An archived guide's source-pinned links may lead to an older download table.
Use the live online table above for the completed qualification of the package's
full source commit; the archived guide remains its original source-time snapshot.

The project version string alone is not a build identifier: development builds
can share the same version while containing different source commits.

Native build, host/UI test and validator results belong to the exact source
commit linked in that table. A documentation-only update after a build does not
change its plugin binary; a later change under `src/` does. Do not treat a
historical green checkpoint, a screenshot, or an uploaded archive as evidence
that every gate for a newer revision passed.

## Choose and verify a build

Start with the [current online download table](https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/main/README.md#downloads),
which identifies the accepted source and packages. In the linked **Actions →
build** run, confirm that its required jobs passed and its exact-source
qualification is complete. Artifact availability alone does not establish that result.

| Artifact | Computer / host architecture | Included |
|---|---|---|
| `cs80-linux-install-bundle` | Linux x86-64 (built on Ubuntu 24.04) | VST3, Standalone |
| `cs80-macos-15-install-bundle` | Apple Silicon, native arm64 | VST3, AU, Standalone `.app` |
| `cs80-macos-15-intel-install-bundle` | Intel macOS, x86-64 | VST3, AU, Standalone `.app` |
| `cs80-windows-install-bundle` | Windows x64 | VST3, Standalone `.exe` |

Mac packages are separate native builds, **not universal binaries**. Match the
host process architecture, not just the computer's processor. Older OS versions
and emulation layers are not qualified by these runner builds.

1. Download and unpack the outer GitHub artifact ZIP. Keep `SHA256SUMS` beside
   the inner archive and manifest.
2. Verify hashes: Linux `sha256sum -c SHA256SUMS`; macOS
   `shasum -a 256 -c SHA256SUMS`. Windows PowerShell: run
   `Get-FileHash -Algorithm SHA256` for the inner ZIP and its matching
   `.manifest.json` sidecar. Compare both hashes with their entries in
   `SHA256SUMS`.
3. Unpack the inner `.tar.gz` with `tar -xzf "archive-name.tar.gz"` on Linux/macOS;
   use Windows **Extract All** for the inner `.zip`. Keep bundle directories
   intact. Tar packaging preserves Unix executable permissions and internal
   bundle symlinks; avoid copying only the binary inside a `.vst3` or `.app`.

`manifest.json` records the source commit, `preset_library_revision`, native
architecture, JUCE provenance and payload hashes. Checksums detect corruption;
they are not a publisher signature. `SOURCE.txt` identifies the corresponding
repository commit and preset library revision. To identify this candidate's
sounds, both must identify **2026.11.8**. Before treating a package as qualified,
also match its full source commit to the exact source SHA in the selected verified
download and its completed workflow qualification. Revision metadata alone does
not establish that qualification. The sound-library revision is separate from
the application's version string.

### Offline inventory verification (Python 3.10+)

Before unpacking the **inner** archive, a trusted source checkout can also check
its complete inventory, embedded/sidecar manifest agreement, file hashes, Unix
tar modes and safe internal bundle-link structure. From that checkout:

```sh
python3 scripts/verify-ci-bundle.py --archive "/path/to/cs80-reference-linux-x86_64-COMMIT_PREFIX.tar.gz" --expected-commit FULL_40_CHARACTER_COMMIT_SHA
```

Replace both placeholders with the selected artifact's filename and the full
source SHA from its trusted workflow. On Windows use `py -3` instead of
`python3`, and select the inner `.zip`. Keep its matching `.manifest.json` and
`SHA256SUMS` adjacent, or supply `--manifest` and `--checksums` explicitly. This
command needs both verifier and packager scripts from the same trusted checkout;
it does not need a compiler, plug-in host or any installed audio products.

Success reports **integrity and declared source consistency verified**. A
mismatch exits nonzero with the failed check. No files are extracted, installed
or executed; no network access or CI reruns occur. ZIP verification does not
assert Unix executable modes. Unsupported archive variants, resource-limit
violations and unsafe/ambiguous paths fail closed.

Symlink validation allows at most 64 nested links and 262,144 total path-component
visits across the manifest. The total-work limit also rejects shallow acyclic
alias graphs that repeatedly expand; a small inventory alone does not bound
resolution work. Exceeding this limit is a verification failure, not a reason to
extract or run the unverified bundle.

This proves consistency with the supplied checksum/manifest evidence, **not**
publisher authenticity, that the binary was built from the declared source,
signing/notarization, a CI pass, or DAW qualification. Obtain the checksum file
and expected SHA from a source you trust; an attacker can replace an archive
and its matching metadata together. Keep using the exact-commit CI gates above.

## Install only the formats you need

Quit the DAW and any running Standalone copy before copying. If replacing an earlier build, first keep a backup
outside the plug-in scan folders; do not keep two copies with the same plug-in
identity inside those folders.

| Format | Copy the entire item to |
|---|---|
| Linux VST3 | `~/.vst3/CS-80 Reference.vst3` |
| macOS VST3 | `~/Library/Audio/Plug-Ins/VST3/CS-80 Reference.vst3` |
| macOS AU | `~/Library/Audio/Plug-Ins/Components/CS-80 Reference.component` |
| Windows VST3 (per user) | `%LOCALAPPDATA%\Programs\Common\VST3\CS-80 Reference.vst3` |

Create the destination folder if missing. Some Windows hosts may require their
documented system location, `%COMMONPROGRAMFILES%\VST3`, instead of the per-user
development location; that copy may require administrator permission. Use the
host's normal plug-in rescan after restarting it. If the previous build still
appears, check for an older duplicate in its scan folders and follow that host's
documented rescan/revalidation procedure. Do not put these inside a DAW's
internal plug-in bundle. VST3 locations follow the [Steinberg developer documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html);
Apple documents the [Audio Units component folders](https://support.apple.com/en-ie/102239).

Standalone needs no DAW scan: launch the item under the package's `Standalone`
folder (`CS-80 Reference.exe` on Windows, `CS-80 Reference.app` on macOS, or
`CS-80 Reference` on Linux). Keep it in a folder you own; on macOS,
`~/Applications` is suitable for the `.app`. No helper
installs drivers, registers startup services, changes security settings or adds
persistent system access.

**Security prompts:** these packages have no release signing/notarization step;
macOS bundles may have ad-hoc build signatures. If macOS, Windows or your host
blocks them, stop and report the exact prompt. Do not disable Gatekeeper,
SmartScreen, antivirus, signature checking or remove quarantine to force a pass.
A locally trusted source build or a properly signed release is a separate path.

**Linux dependencies:** this is not a static/AppImage distribution. ALSA, X11,
font/rendering and other system libraries must match the build's requirements.
Report missing-library errors; do not assume Ubuntu 24.04 artifacts work on older
distributions. No additional audio driver is bundled on any platform.

## First five minutes

For a first comparison, start in a new empty project with a fresh instrument
instance. An older project can restore an earlier recipe under the same factory
name, along with its saved modulation and physical setup.

1. In Standalone, open its audio/MIDI settings and select your output device and
   MIDI keyboard. Start at **48 kHz / 256 samples** as a setup baseline, not a
   measured performance guarantee for your computer. In a DAW, create an
   instrument track, load **CS-80 Reference**, enable monitoring and choose the
   MIDI input. Start with low monitoring volume.
2. A fresh instance plays **Brass Swell I** on the circuit engine. Velocity,
   pressure, pitch bend, CC1 modulation, CC11 expression and CC64 pedal hold
   remain available. To choose another factory sound, release all keys and the
   sustain pedal, open **LIBRARY → FACTORY LIBRARY**, select a row and explicitly
   press **LOAD** (or **RELOAD**, if shown), then play fresh notes. If filters hide
   the sound, press **CLEAR ALL**, then select **FACTORY LIBRARY** again. The main
   toolbar's **< / >** arrows load adjacent programs. Library's **PREV / NEXT**
   buttons select rows and load them only when **AUDITION** is enabled. The
   hardware tone tabs choose separate electrical recipes. The default voice mode has eight circuits;
   an additional held key waits for an available circuit rather than stealing
   a held note. Expanded voice modes are available in Studio.
3. For MPE, enable it in the host/controller and instrument. Incoming MIDI
   configuration and RPN messages can set lower/upper zones and bend ranges;
   otherwise match the configured ranges manually. Studio's keyboard/tuning
   page shows the zones and ranges applied in the last audio callback.
   Polyphonic pressure also works without MPE; on screen, strike a key and
   drag farther down it for pressure.
4. Open **Studio** for effects, LFOs, macros, drawable envelopes, the modulation
   matrix, arpeggiation, MPE, microtuning and voice modes. These optional software
   features belong to the same circuit instrument. Factory recipes apply their
   sound parameters; separate MSEG shapes and modulation routing remain part of
   the session. The original six SUB functions are the factory default;
   **WAVEFORM SET** can select Extended functions independently. Keep normal
   output headroom when editing levels or playing dense chords.
5. Use **SAVE** (toolbar menu or Ctrl/Cmd+S) to name a user patch; an existing
   filename requires confirmation. User patches, factory programs, A/B comparisons
   and browser Revert all use the circuit engine. Preset recall preserves the
   live physical setup; a full DAW session also stores its session configuration.
   Transient pedal gestures are not saved.

Factory **LOAD / RELOAD** applies the recipe and PANEL sources. It preserves
physical setup, MSEG shapes and modulation routing, and existing host automation
can write later control values. It does not reset a complete session.
[Source-checked loading behavior](preset-evidence/2026.11.7/reviews/a43.md)

**Pressure Lead and Electric Leads 136 require aftertouch.** They are intentionally
silent before pressure is applied. Hold a key and add MIDI pressure, or, with **KAS Scan Timing** off
(the default), drag farther down an on-screen key. MIDI aftertouch also works
with KAS Scan Timing on. See the [preset guide](PRESET_GUIDE.md) for playing
suggestions across the revised bank.

### Stuck notes and output headroom

**PANIC**, in the software toolbar, requests a global hard stop of existing notes,
MIDI sustain holds and arpeggiator keys at the next audio callback. It clears stale
on-screen-key input without changing the patch, selectors or expression position.
It is **notes-only, not output mute**: retained effect/filter histories and analog
noise may remain audible. New host MIDI, including notes arriving in that same
callback after the stop, can play normally. If processing is suspended, the
request waits until it resumes. Use your host/output mute when complete silence
is required. See [PANIC details](PANIC_CONTROL.md).

The **SAMPLE PEAK** readout beside PANIC observes the final mono/stereo output
samples in dBFS. **OVER-RANGE** means a finite sample's absolute value exceeded
1.0 (0 dBFS), not proof that a floating-point host bus clipped. **NON-FINITE**
separately reports NaN/Inf; those values are excluded from the numeric peak.
Warnings hold briefly (one second / two seconds respectively), so they need not
match the decaying number at that instant. This is a **read-only sample-peak
indicator, not true peak, loudness measurement, a limiter or automatic gain
control**. It does not mute or repair audio. Leave headroom and keep monitoring
low; if NON-FINITE appears, report the patch, voice/quality settings and steps rather than treating
the indicator as protection. See [output-meter details](OUTPUT_METER.md).

### One circuit instrument

There is one rendering engine for all 523 factory programs and user patches.
**ALL PATCHES** shows the whole catalogue, including user patches;
**FACTORY LIBRARY** shows the factory entries. Their existing names and program
numbers are preserved. The first 222 addresses have new authored voicings,
including the original keyboard, bass and tone-study programs.

**Older sessions using Legacy or Pure intentionally change sound.** Stored
controls remain, but schema 4 converts the obsolete engine fields to the circuit
engine with normal MIDI and optional Studio features. A saved mode cannot disable
MPE, effects, tuning or expression, and writing an obsolete mode field cannot cut
held notes. The two host parameter identities are retained as fixed,
non-automatable compatibility fields. See the [session contract](PARITY_ISOLATION.md)
for the exact IDs and migration behavior.

The 7 `SOURCE-COMPLETE` and 15 `SOURCE-INFERRED` factory tabs use the
circuit models. “Complete” describes the
classified source column, not measured hardware equivalence. PANEL and memories
retain their programmer identity; they do not become factory source columns.

The original hardware tone tabs and the similarly named browser programs serve
different purposes. A panel **E. PIANO** tab selects the original CS-80 electrical
tone recipe; **Tone I: E. Piano** in Library loads the enhanced factory instrument
voicing. Use **Library** to hear the revised named instruments. The tabs' tooltips
identify the corresponding browser programs. Instrument bodies are available on
PANEL sources and remain suppressed on the hardware-tone and memory paths.

### Screen size and performance limits

Initial editor size fits the logical usable display where possible, preserving
the existing 8:5 layout and **1120x700 logical-pixel minimum**. Smaller usable
screens, or tight screens with large host chrome, may not fit the editor. This
initial fit does not override later host/manual resizing, remember window size,
or create a smaller layout. OS/host scaling can reduce the available logical area.

The [expressive Source Performance benchmark](performance/2026-10-30-source-performance/SUMMARY.md)
measures immutable DSP commit `d7b6e85`: eight voices/both ranks, all 22 tabs,
live MPE/controllers and two modest FX combinations at 44.1/48 kHz and
128/256 samples. Typical held-callback medians used roughly **24–30% of the
callback budget**, but **127 of 256,402 callbacks** across all measured modes and
phases exceeded budget; one held callback took 29.143 ms. This is core-only
cloud-machine evidence, excluding the plugin wrapper, UI, driver and DAW, and
predates the redesigned UI and later BBD/FX/envelope changes as well as the
PANIC/meter helpers. It is not a current-plugin performance measurement or a
glitch-free guarantee, nor a
64-frame/high-rate qualification. Start host checks at 256 samples and test your
intended patches; the older `932c6d4` dry-isolation audit remains separate.

Standard quality reports **67 samples of plug-in latency**
(**1.40 ms at 48 kHz**). Other quality settings can report a different
value; this excludes the audio interface and host buffer delays. Tail metadata
is unbounded because supported noise and retained effect histories have no
universal finite silence deadline. Use an explicit export end/selection with sufficient audible
release; do not depend on automatic finite-tail export termination. Hosts may
continue processing idle instances.

## Update, uninstall, or report a problem

- Back up sessions and user presets before changing builds. With the host closed,
  replace only this instrument's complete bundle/file; rescan afterward.
- To hear the revised factory sounds, open **LIBRARY → FACTORY LIBRARY**, choose
  the desired factory entry, then explicitly press **LOAD** (or **RELOAD**, if
  that is the label shown). Release all keys and the sustain pedal before loading.
  Enter or double-click also loads it. Play fresh notes
  afterward, and save the project
  if you want it to retain that recipe. An older project can show the same
  factory name while restoring its older settings; clicking an already-loaded
  row with **Audition** enabled may leave those settings in place.
- Sessions, user presets and A/B states retain their saved controls, with
  obsolete engine settings migrated as described above. **REVERT** restores
  the sound present when Library opened, which can be the older recipe; it does
  not load the latest factory settings. Keep the older binary and a reference
  bounce when an old project's exact sound must be retained. A delivered
  factory selection by program number loads the current voicing at
  that address. VST3 host changes sent only through the Program processing queue
  retain a [documented controller dependency](PRESET_REFINEMENT_2026_11_1.md#native-recall-and-host-program-qualification);
  **Library** selection does not use that limited route. See the bundled
  [preset guide](PRESET_GUIDE.md).
- To uninstall, remove only the installed **CS-80 Reference** bundles/application
  you copied. Do not remove shared plug-in folders or user presets. Restoring a
  backup does not prove old/new session audio is identical.
- Report commit/package SHA, OS, CPU architecture, DAW/version, format,
  sample-rate/buffer, voice/quality settings and exact steps/error. If the editor loads but is
  silent, check track/device routing, MIDI input, volume/expression, selected
  rank/source, and whether the selected program needs aftertouch to sound.
- Automated validation is not a substitute for a real-device session. Record
  installation, MIDI/pedal, session recall and long-tail bounce results for the
  particular host before relying on the build in a performance.
