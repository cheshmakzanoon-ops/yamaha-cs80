# Install and play the CI development build

These are development artifacts, not a signed/notarized public release or a
promise of compatibility with every DAW. Read `DISTRIBUTION_STATUS.md` in the
package. No calibrated hardware-match claim is made.

This revision describes the redesigned vector editor, opt-in **SOURCE
PERFORMANCE**, **PANIC**, and **SAMPLE PEAK** helpers. Older artifacts may not
contain these changes. Use the guide bundled with your artifact and compare its
full manifest commit with the current-build table in the repository README.
The project version string alone is not a build identifier: development builds
can share the same version while containing different source commits.

Native build, host/UI test and validator results belong to the exact source
commit linked in that table. A documentation-only update after a build does not
change its plugin binary; a later change under `src/` does. Do not treat a
historical green checkpoint, a screenshot, or an uploaded archive as evidence
that every gate for a newer revision passed.

## Choose and verify a build

From the repository's **Actions → build** run, choose a commit whose required
jobs all passed. Artifact availability alone does not establish that result.

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
   `Get-FileHash -Algorithm SHA256 .\cs80-reference-*.zip` and compare with the
   matching line in `SHA256SUMS` (also check the manifest if using it).
3. Unpack the inner `.tar.gz` with `tar -xzf "archive-name.tar.gz"` on Linux/macOS;
   use Windows **Extract All** for the inner `.zip`. Keep bundle directories
   intact. Tar packaging preserves Unix executable permissions and internal
   bundle symlinks; avoid copying only the binary inside a `.vst3` or `.app`.

`manifest.json` records the source commit, native architecture, JUCE provenance
and payload hashes. Checksums detect corruption; they are not a publisher
signature. `SOURCE.txt` identifies the corresponding repository commit.

## Install only the formats you need

Quit the DAW before copying. If replacing an earlier build, first keep a backup
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
host's normal plug-in rescan. Do not put these inside a DAW's internal plug-in
bundle. VST3 locations follow the [Steinberg developer documentation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Locations%2BFormat/Plugin%2BLocations.html).

Standalone needs no DAW scan: keep its file/application in a folder you own and
launch it there. On macOS, `~/Applications` is suitable for the `.app`. No helper
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

1. In Standalone, open its audio/MIDI settings and select your output device and
   MIDI keyboard. Start at **48 kHz / 256 samples** as a setup baseline, not a
   measured performance guarantee for your computer. In a DAW, create an
   instrument track, load **CS-80 Reference**, enable monitoring and choose the
   MIDI input. Start with low monitoring volume.
2. Leave both **SOURCE PERFORMANCE** and **PARITY ISOLATION off** for the
   unchanged default legacy sound: velocity, pressure,
   pitch bend, CC1 modulation, CC11 expression and CC64 pedal hold work here.
   Select a library brass/string/keys patch or edit PANEL. Eight simultaneously
   held voices are available; a ninth held note is intentionally ignored.
3. For MPE, enable it in both host and instrument, use channel 1 as master and
   member channels 2–16, and match pitch-bend ranges manually. Automatic RPN
   zone/range negotiation is not implemented. Polyphonic pressure also works
   without MPE; on screen, strike a key and drag farther down it for pressure.
4. To play the new nominal source tabs with your controller, turn **Advanced →
   About → SOURCE PERFORMANCE on**, leave **PARITY ISOLATION off**, then select
   the hardware tone tabs. MIDI bend, CC1/11/64/74, optional MPE and modern
   FX/macros remain usable. This is an explicitly hybrid software performance
   mode, not a calibrated hardware comparison. The toolbar identifies the
   requested policy and both source routes. Source-model output uses a fixed
   -6 dB reference and an inferred driver ceiling at full scale, so very dense
   loud chords can still reach 0 dBFS; keep normal monitoring headroom.
5. Use **SAVE** to name a user patch; an existing filename requires confirmation.
   Check the saved/error status. Save the DAW session for the full session mode
   and automation. A new user patch saves/restores SOURCE PERFORMANCE; factory
   library recall resets it OFF and returns both selectors to PANEL. Old patches
   without the flag restore it OFF. User/factory patch recall preserves the
   independent isolation policy; full session restore recalls both flags.
   Transient pedal gestures are not saved.

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
low; if NON-FINITE appears, report the patch, mode and steps rather than treating
the indicator as protection. See [output-meter details](OUTPUT_METER.md).

### Choose the mode before playing

| SOURCE PERFORMANCE | PARITY ISOLATION | Effective behavior |
|---|---|---|
| OFF | OFF | Legacy performance (unchanged fresh default) |
| ON | OFF | Source models plus normal MIDI, optional MPE and modern overlays |
| Either | ON | Strict comparison: bend, CC1/11/64/74, MPE and modern overlays masked |

Isolation retains the source request but takes precedence over it. Pressure,
ribbon and the on-screen PEDAL remain active in all three policies. An effective
mode change **stops sounding notes and clears controller/FX history**; the shared
pedal position is reapplied. Toggling only SOURCE PERFORMANCE while isolation
remains ON is masked and does not stop notes. Turning isolation OFF applies the
stored source request. Library recall never disables isolation; it selects
PANEL and clears the source-performance request, not a source-model tone tab.

The 7 `SOURCE-COMPLETE` and 15 `SOURCE-INFERRED` factory tabs share the current
nominal models in source performance and isolation. “Complete” describes the
classified source column, not measured hardware equivalence. PANEL and memories
retain their programmer identity; they do not become factory source columns.
The separate experimental oscillator is not enabled by these switches.

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

The host receives 63 samples of output latency. Tail metadata is unbounded
because supported noise and retained effect histories have no universal finite
silence deadline. Use an explicit export end/selection with sufficient audible
release; do not depend on automatic finite-tail export termination. Hosts may
continue processing idle instances.

## Update, uninstall, or report a problem

- Back up sessions and user presets before changing builds. With the host closed,
  replace only this instrument's complete bundle/file; rescan afterward.
- To uninstall, remove only the installed **CS-80 Reference** bundles/application
  you copied. Do not remove shared plug-in folders or user presets. Restoring a
  backup does not prove old/new session audio is identical.
- Report commit/package SHA, OS, CPU architecture, DAW/version, format,
  sample-rate/buffer, mode and exact steps/error. If the editor loads but is
  silent, check track/device routing, MIDI input, volume/expression, selected
  rank/source and whether an unsupported controller is masked by isolation.
- Automated validation is not a substitute for a real-device session. Record
  installation, MIDI/pedal, session recall and long-tail bounce results for the
  particular host before relying on the build in a performance.
